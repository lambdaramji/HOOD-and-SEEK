#include "command_parser.h"

#include <array>
#include <cstdint>
#include <regex>
#include <string>
#include <utility>

namespace hoodseek::voice {
namespace {

// 계약의 text 상한은 UTF-8 byte 수가 아닌 Unicode 문자 수 200이다.
bool valid_text(std::string_view text) {
  if (text.size() > 800) return false;
  std::size_t count = 0;
  for (std::size_t i = 0; i < text.size();) {
    const auto first = static_cast<unsigned char>(text[i]);
    std::size_t size = 1;
    std::uint32_t cp = first;
    if (first >= 0xc2 && first <= 0xdf) {
      size = 2;
      cp = first & 0x1f;
    } else if (first >= 0xe0 && first <= 0xef) {
      size = 3;
      cp = first & 0x0f;
    } else if (first >= 0xf0 && first <= 0xf4) {
      size = 4;
      cp = first & 0x07;
    } else if (first >= 0x80) {
      return false;
    }
    if (i + size > text.size()) return false;
    for (std::size_t j = 1; j < size; ++j) {
      const auto next = static_cast<unsigned char>(text[i + j]);
      if ((next & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (next & 0x3f);
    }
    if ((size == 2 && cp < 0x80) || (size == 3 && cp < 0x800) ||
        (size == 4 && cp < 0x10000) || cp > 0x10ffff ||
        (cp >= 0xd800 && cp <= 0xdfff)) return false;
    if ((cp < 0x20 && cp != '\t' && cp != '\n' && cp != '\r') || cp == 0x7f)
      return false;
    if (++count > 200) return false;
    i += size;
  }
  return true;
}

std::string normalize_separators(std::string_view text) {
  std::string result;
  bool space = false;
  bool comma = false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
      space = !result.empty();
    } else if (ch == ',' ||
               (ch == '.' && !result.empty() && i + 1 < text.size() &&
                (text[i + 1] == ' ' || text[i + 1] == '\t' ||
                 text[i + 1] == '\r' || text[i + 1] == '\n') &&
                text.find_first_not_of(" \t\r\n", i + 1) != std::string_view::npos)) {
      // ASR가 단어 사이에 넣은 쉼표만 공백으로 취급한다. 시작·끝·반복
      // 쉼표는 원문을 남겨 전체 문장 매칭에서 거부한다. 단어 뒤의
      // 마침표+공백도 구분자로 처리하되 마지막 마침표와 물음표는 보존한다.
      if (result.empty() || comma) return std::string(text);
      comma = true;
      space = true;
    } else {
      if (space) result += ' ';
      result += ch;
      space = false;
      comma = false;
    }
  }
  return comma ? std::string(text) : result;
}

struct Rule {
  std::regex pattern;
  Command command;
};

const std::array<Rule, 8>& rules() {
  static const std::string prefix = "후드 *";
  static const std::string particle = " *(을|은)? *";
  static const std::string fan =
      "(팬" + particle + "((풍량|바람)" + particle + ")?|(풍량|바람)" + particle + ")";
  // 물음표는 직접 요청 어미에만 허용한다. '켜?'와 '켜도 돼?'를
  // 부호 삭제 후 '켜'로 승격시키지 않는다.
  static const std::string request = " *(줘|주세요|줄래(요)?)[.!?]?";
  static const std::string set = "(해|바꿔|설정해|맞춰)" + request;
  static const std::string off =
      "(꺼[.!]?|꺼" + request + "|끄세요[.!?]?|정지[.!]?|정지해" + request + ")";
  static const std::string on = "(켜[.!]?|켜" + request + "|켜세요[.!?]?)";
  static const std::string up =
      "(올려[.!]?|올려" + request + "|올리세요[.!?]?|높여[.!]?|높여" + request + "|높이세요[.!?]?)";
  static const std::string down =
      "(내려[.!]?|내려" + request + "|내리세요[.!?]?|줄여[.!]?|줄여" + request + "|줄이세요[.!?]?)";
  const auto fan_set = [&](const std::string& named, const std::string& number) {
    const auto named_target = "(" + fan + ")?" + named;
    const auto setting = " *((으로|로) *)?(좀 *)?(" + set + "|" + on + ")";
    // ponytail: 숫자 단수는 대상과 설정 동사를 함께 요구한다. '일단'을
    // 연결말에서 무조건 1단으로 치환하거나 인덕션 단수로 해석하지 않는다.
    return std::regex(prefix + "(" + named_target + "[.!]?|" + named_target + setting +
                      "|" + fan + number + setting + "|" + number +
                      " *((으로|로) *)?" + fan + "(좀 *)?(" + set + "|" + on + "))");
  };
  static const std::string fan_power = "(팬" + particle + "(전원" + particle + ")?)?";
  static const std::string induction = "인덕션" + particle;
  static const std::string induction_power = induction + "(전원" + particle + ")?";
  static const std::string heat = "(" + induction + ")?(화력|불)" + particle;
  // 문장 전체 allowlist다. 부정·허가 질문·인용·복합·상대 팬은 매칭되지 않는다.
  static const std::array<Rule, 8> value{{
      {fan_set("(약풍|약)", "(1|일|한) *단"), {Intent::FAN_SET, 1}},
      {fan_set("(중풍|중)", "(2|이|두) *단"), {Intent::FAN_SET, 2}},
      {fan_set("(강풍|강)", "(3|삼|세) *단"), {Intent::FAN_SET, 3}},
      {std::regex(prefix + fan_power + "(좀 *)?" + off),
       {Intent::FAN_OFF, std::nullopt}},
      {std::regex(prefix + induction_power + "(좀 *)?(꺼[.!]?|꺼" + request + "|끄세요[.!?]?)"),
       {Intent::IND_OFF, std::nullopt}},
      {std::regex(prefix + induction_power + "(좀 *)?" + on),
       {Intent::IND_ON, std::nullopt}},
      {std::regex(prefix + heat + "(좀 *)?" + up),
       {Intent::IND_UP, std::nullopt}},
      {std::regex(prefix + heat + "(좀 *)?" + down),
       {Intent::IND_DOWN, std::nullopt}},
  }};
  return value;
}

std::optional<Command> literal_command(const std::string& normalized) {
  if (normalized.find("후드") == std::string::npos) return std::nullopt;
  std::optional<Command> matched;
  for (const auto& rule : rules()) {
    if (!std::regex_match(normalized, rule.pattern)) continue;
    // 나중에 규칙이 겹쳐도 첫 번째 명령을 임의로 고르지 않는다.
    if (matched) return Command{Intent::NONE, std::nullopt};
    matched = rule.command;
  }
  return matched.value_or(Command{Intent::NONE, std::nullopt});
}

}  // namespace

std::optional<std::string> normalize_command_text(std::string_view text) {
  if (!valid_text(text)) return std::nullopt;
  const auto normalized = normalize_separators(text);
  auto corrected = normalized;
  // 활성어는 문장 처음에서만 바꾼다. 문장 중간의 인용을 추출하지 않는다.
  constexpr std::string_view activation_aliases[]{
      "호대", "호등", "후등", "푸드", "후도", "후들", "호드", "포드",
      "후둘", "호둬", "푸대", "부드", "부대", "후다", "호다"};
  for (const auto alias : activation_aliases) {
    if (corrected.compare(0, alias.size(), alias.data(), alias.size()) == 0) {
      corrected.replace(0, alias.size(), "후드");
      break;
    }
  }
  constexpr std::pair<std::string_view, std::string_view> words[]{
      {"약풍프", "약풍"}, {"약품", "약풍"}, {"종풍", "중풍"}, {"감풍", "강풍"},
      {"펜", "팬"}, {"임덕션", "인덕션"}, {"인덕장", "인덕션"}, {"인덕전", "인덕션"}};
  for (const auto& [from, to] : words) {
    std::size_t pos = 0;
    while ((pos = corrected.find(from, pos)) != std::string::npos) {
      corrected.replace(pos, from.size(), to.data(), to.size());
      pos += to.size();
    }
  }
  // 알려진 요청 어미의 오인식만 문장 끝에서 교정한다. 물음표·부정·상태
  // 설명은 삭제하지 않고, 아래 전체 문장 검사로 지원 명령인지 확인한다.
  constexpr std::pair<std::string_view, std::string_view> endings[]{
      {"꺼져", "꺼줘"}, {"꺼죠", "꺼줘"}, {"켜져", "켜줘"}, {"켜죠", "켜줘"}};
  auto end = corrected.size();
  if (end && (corrected.back() == '.' || corrected.back() == '!')) --end;
  for (const auto& [from, to] : endings) {
    if (end >= from.size() && corrected.compare(end - from.size(), from.size(), from) == 0) {
      corrected.replace(end - from.size(), from.size(), to.data(), to.size());
      break;
    }
  }
  if (corrected != normalized) {
    const auto command = literal_command(corrected);
    // 단어 내부·일반 대화·부정·질문을 명령으로 만들지 않는다. 동작어
    // 거/커/버, 반복 문장과 누락된 단어는 임의로 보완하지 않는다.
    if (command && command->intent != Intent::NONE) return corrected;
  }
  return normalized;
}

std::optional<Command> parse_command(std::string_view text) {
  const auto normalized = normalize_command_text(text);
  return normalized ? literal_command(*normalized) : std::nullopt;
}

std::optional<Command> repeated_command_candidate(std::string_view text) {
  if (!valid_text(text)) return std::nullopt;
  const auto normalized = normalize_separators(text);
  if (normalized.find('?') != std::string::npos) return std::nullopt;
  for (auto split = normalized.find_first_of(" .!"); split != std::string::npos;
       split = normalized.find_first_of(" .!", split + 1)) {
    // 두 문장 전체를 각각 검사한다. '켜/꺼/켜지 마'를 글자 유사도로
    // 합치지 않고, 별칭·띄어쓰기가 달라도 같은 명령일 때만 재전사한다.
    const auto first = parse_command(normalized.substr(0, split + (normalized[split] != ' ')));
    const auto second = parse_command(normalized.substr(split + 1));
    if (first && second && first->intent != Intent::NONE &&
        first->intent == second->intent && first->value == second->value) return first;
  }
  return std::nullopt;
}

std::optional<Command> power_retry_candidate(std::string_view text) {
  const auto normalized = normalize_command_text(text);
  static const std::regex pattern("후드 *인덕션 *강[.]?");
  if (normalized && std::regex_match(*normalized, pattern))
    return Command{Intent::IND_OFF, std::nullopt};
  return std::nullopt;
}

bool is_damaged_fan_text(std::string_view text) {
  static const std::regex pattern(" *(후드|푸드) *,? *팬 *,? *고[.!]? *");
  return valid_text(text) && std::regex_match(text.begin(), text.end(), pattern);
}

bool is_valid_command(const Command& command) {
  switch (command.intent) {
    case Intent::FAN_SET:
    case Intent::IND_SET:
      return command.value && *command.value >= 1 && *command.value <= 3;
    case Intent::FAN_OFF:
    case Intent::IND_OFF:
    case Intent::IND_ON:
    case Intent::IND_UP:
    case Intent::IND_DOWN:
    case Intent::NONE:
      return !command.value;
  }
  return false;
}

}  // namespace hoodseek::voice
