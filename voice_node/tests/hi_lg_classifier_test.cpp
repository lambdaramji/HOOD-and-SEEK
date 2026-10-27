#include "hi_lg_classifier.h"
#include <iostream>
#include <utility>
#include <vector>

int main() {
  using namespace hoodseek::voice::hi_lg;
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"하이 에일지 인덕션 켜", "IND_ON"},
      {"하이힐지 인덕션 전원 켜져", "IND_ON"},
      {"하엘지 후드 강풍", "FAN_SET"},
      {"하이엘지 화력 낮춰져", "IND_DOWN"},
      {"하이엘지 팬 작동 멈춰져", "FAN_OFF"},
      {"하이엘지 인덕션 켜져", "IND_ON"},
      {"하이엘지 인덕션 꺼져", "IND_OFF"},
      {"하이엘지 후드 약으로 설정에", "FAN_SET"},
      {"하이엘지 인덕션 켜줘", "IND_ON"},
      {"하이엘지 인덕션 꺼줘", "IND_OFF"},
      {"하이엘지 팬거", "FAN_OFF"},
      {"하이엘지 펜거", "FAN_OFF"},
      {"하이엘지 팬 거", "FAN_OFF"},
      {"하이엘지 펜 거", "FAN_OFF"},
      {"하이엘지 펜꺼", "FAN_OFF"},
      {"하이엘지 펜 꺼", "FAN_OFF"},
      {"하이엘지 페꺼", "FAN_OFF"},
      {"하이엘지 패꺼", "FAN_OFF"},
      {"하이엘지 페 꺼", "FAN_OFF"},
      {"하이엘지 패 꺼", "FAN_OFF"},
      {"하이엘지 팬 거?", "FAN_OFF"},
      {"팬 거", "NO_EXEC"},
      {"펜 꺼", "NO_EXEC"},
      {"하이엘지 인덕션 거", "NO_EXEC"},
      {"하이엘지 팬 거지 마", "NO_EXEC"},
      {"하이엘지 펜 끄지 마", "NO_EXEC"},
      {"하이엘지 팬 거져 있어", "NO_EXEC"},
      {"하이엘지 팬 거도 돼?", "NO_EXEC"},
      {"하이엘지 팬 거라고 했어", "NO_EXEC"},
      {"하이엘지 팬 거 인덕션 켜", "NO_EXEC"},
      {"하이엘지 팬 거 내놔", "NO_EXEC"},
      {"하이엘지 화력 올려줘", "IND_UP"},
      {"하이엘지 화력 내려줘", "IND_DOWN"},
      {"하이엘지 인덕션 켜지 마", "NO_EXEC"},
      {"하이 에일지 인덕션 끄지 마", "NO_EXEC"},
      {"하이엘지 팬 멈추지 마", "NO_EXEC"},
      {"하이에일지 인덕션 켜져?", "IND_ON"},
      {"하이엘지 인덕션 꺼?", "IND_OFF"},
      {"하이힐지 인덕션 켜도 돼?", "NO_EXEC"},
      {"하이엘지라고 부르면 인덕션 켜", "NO_EXEC"},
      {"하이에일지라는 말 인덕션 켜", "NO_EXEC"},
      {"하이엘지 후드펜 꺼주고 인덕션 켜", "NO_EXEC"},
      {"하이에일지 후드 팬 꺼주고 인덕션 켜", "NO_EXEC"},
      {"하이엘지 인덕션 켜고 후드 팬 꺼", "NO_EXEC"},
      {"하이엘지 후드 강풍 했다가 꺼줘", "NO_EXEC"},
      {"하이엘지 후드 강풍 하이엘지 후드 강풍", "NO_EXEC"},
      {"하이엘지 인덕션 켜져 있어", "NO_EXEC"},
      {"하이에일지 인덕션 꺼진 상태", "NO_EXEC"},
      {"하이엘지 인덕션 켜짐", "NO_EXEC"},
      {"하이엘지 바람 바꿔줘", "NO_EXEC"},
      {"하이엘지 후드 팬 켜줘", "FAN_SET"},
      {"하이엘지 팬 한 단계 올려줘", "NO_EXEC"},
      {"하이엘지 조명 켜줘", "NO_EXEC"},
      {"하이엘지 음악 틀어줘", "NO_EXEC"},
      {"오늘 날씨 좋네", "NO_EXEC"},
      {"인덕션 켜줘", "NO_EXEC"},
      {"후드 팬 꺼줘", "NO_EXEC"},
      {"하이엘지 인덕션 커주세요", "NO_EXEC"},
      {"하이엘지 화력 맞춰죠", "NO_EXEC"},
      {"하이엘지 바람 바꿔죠", "NO_EXEC"},
      {"하이엘지 후드 약풍으로 맞춰죠", "FAN_SET"},
      {"하이엘지 인덕션 세이 울져", "NO_EXEC"},
      {"하이엘지 후두 화력군려", "NO_EXEC"},
      {"하이엘지 후드펜 꺼져어", "NO_EXEC"},
  };
  int failures = 0;
  for (const auto &item : cases) {
    const auto result = classify(item.first);
    const bool ok = item.second == "NO_EXEC" ? !executable(result)
                                             : result.intent == item.second;
    if (!ok) {
      ++failures;
      std::cerr << "FAIL " << item.first << " => " << result.intent << '\n';
    }
  }
  for (const auto &alias : aliases) {
    const auto result = classify(alias + " 후드 약풍");
    if (result.intent != "FAN_SET" || result.value != 1)
      ++failures;
  }
  for (const auto &body : std::vector<std::string>{"페꺼", "패꺼", "페 꺼", "패 꺼"}) {
    for (const auto &text : std::vector<std::string>{
             body, "하이엘지 " + body + "지 마", "하이엘지 " + body + "도 돼?",
             "하이엘지 " + body + "라고 했어", "하이엘지 " + body + "져 있어",
             "하이엘지 " + body + " 인덕션 켜"}) {
      if (executable(classify(text))) {
        ++failures;
        std::cerr << "FAIL fan-off alias boundary: " << text << '\n';
      }
    }
  }
  for (int level = 1; level <= 3; ++level) {
    const std::string word = level == 1 ? "약풍" : level == 2 ? "중풍" : "강풍";
    const auto result = classify("하이엘지 푸도 " + word);
    if (result.intent != "FAN_SET" || result.value != level) {
      ++failures;
      std::cerr << "FAIL hood alias: 푸도 " << word << '\n';
    }
  }
  for (const auto &text : std::vector<std::string>{
           "푸도 강풍", "하이엘지 푸도 강풍 하지 마",
           "하이엘지 푸도 강풍으로 해도 돼?", "하이엘지 푸도 강풍이라고 했어",
           "하이엘지 푸도 강풍 인덕션 켜", "하이엘지 푸도코트 강풍"}) {
    if (executable(classify(text))) {
      ++failures;
      std::cerr << "FAIL hood alias boundary: " << text << '\n';
    }
  }
  for (const auto &text : std::vector<std::string>{
           "하이엘지 후드 강풍 인덕션 켜", "하이엘지 인덕션 후드 강풍",
           "하이엘지 팬 인덕션 켜", "하이엘지 인덕션 펜 꺼"}) {
    if (executable(classify(text))) {
      ++failures;
      std::cerr << "FAIL multiple device targets: " << text << '\n';
    }
  }
  for (const auto &word : std::vector<std::string>{
           "세기", "색이", "세기이", "세이", "새기", "쎄기", "새이", "색기"}) {
    for (const auto &action : std::vector<std::pair<std::string, std::string>>{
             {"올려줘", "IND_UP"}, {"내려줘", "IND_DOWN"},
             {"줄여줘", "IND_DOWN"}}) {
      const auto text = "하이엘지 불 " + word + " " + action.first;
      const auto result = classify(text);
      if (result.intent != action.second) {
        ++failures;
        std::cerr << "FAIL " << text << " => " << result.intent << '\n';
      }
    }
  }
  const std::vector<std::pair<std::string, std::string>> boundaries = {
      {"하이엘지 불 세기 올리지 마", "NONE"},
      {"하이엘지 불 색이 내리지 마", "NONE"},
      {"하이엘지 불 세기 올려도 돼?", "NONE"},
      {"하이엘지 불 색이 내려?", "IND_DOWN"},
      {"하이엘지 불 세기 올려주고 팬 꺼줘", "NONE"},
      {"하이엘지 불 새기 올려달라고 했어", "NONE"},
      {"하이엘지 불 세기 지금 어때?", "NONE"},
      {"하이엘지 불 세기", "NONE"},
      {"불 색이 올려줘", "DROP"},
      {"하이엘지 후드 1단으로 내려줘", "NONE"},
      {"하이엘지 후드 2단으로 내려줘", "NONE"},
      {"하이엘지 후드 3단으로 올려줘", "NONE"},
      {"하이엘지 후드 켜지 마", "NONE"},
      {"하이엘지 후드 켜?", "FAN_SET"},
      {"하이엘지 후드 켜져 있어", "NONE"},
      {"하이엘지 켜줘", "NONE"},
      {"하이엘지 꺼", "NONE"},
      {"하이엘지 꺼줘", "NONE"},
      {"하이엘지 꺼주세요", "NONE"},
      {"하이엘지 멈춰줘", "NONE"},
      {"하이엘지 꺼?", "NONE"},
      {"하이엘지 후드 꺼", "FAN_OFF"},
      {"하이엘지 팬 꺼?", "FAN_OFF"},
      {"후드 켜줘", "DROP"},
      {"하이엘지 인덕션 꺼도 돼?", "NONE"},
      {"하이엘지 인덕션 켜도 돼", "NONE"},
      {"하이엘지 인덕션 꺼도 될까？", "NONE"},
      {"하이엘지 인덕션 켜져 있어?", "NONE"},
      {"하이엘지 인덕션 켜지 마?", "NONE"},
      {"하이엘지 인덕션 끄지 마？", "NONE"},
      {"하이엘지 인덕션 꺼달라고 했어?", "NONE"},
      {"하이엘지 인덕션 꺼주고 후드 강풍?", "NONE"},
      {"하이엘지 인덕션 꺼? 후드 강풍", "NONE"},
      {"후드 인덕션 꺼?", "DROP"},
      {"하이엘지?", "NONE"},
  };
  for (const auto &item : boundaries) {
    if (classify(item.first).intent != item.second) {
      ++failures;
      std::cerr << "FAIL " << item.first << '\n';
    }
  }
  int level_unit_checks = 0;
  const std::vector<std::vector<std::string>> numerals = {
      {"1", "일", "한"}, {"2", "이", "두"}, {"3", "삼", "세"}};
  for (int level = 1; level <= 3; ++level) {
    for (const auto &number : numerals[level - 1]) {
      for (const auto &unit : std::vector<std::string>{"단", "단계"}) {
        for (const auto &text : std::vector<std::string>{
                 "하이엘지 후드 " + number + unit + "으로 해줘",
                 "하이엘지 팬을 " + number + " " + unit + "로 설정해주세요",
                 "하이엘지 후드 " + number + unit + "으로 팬을 켜줘?"}) {
          ++level_unit_checks;
          const auto actual = classify(text);
          if (actual.intent != "FAN_SET" || actual.value != level) {
            ++failures;
            std::cerr << "FAIL absolute level unit: " << text << '\n';
          }
        }
      }
    }
  }
  for (const auto &text : std::vector<std::string>{
           "하이엘지 팬 한 단계 올려줘", "하이엘지 팬 한 단계 내려줘",
           "하이엘지 후드 1단계로 내려줘", "하이엘지 후드 3단계로 올려줘",
           "하이엘지 후드 0단계로 해줘", "하이엘지 후드 4단계로 해줘",
           "하이엘지 후드 팬 11단계로 해줘",
           "하이엘지 후드 팬 11단으로 해줘",
           "하이엘지 후드 팬 -1단계로 해줘",
           "하이엘지 조명 2단계로 해줘",
           "하이엘지 후드 2단계로 하지 마", "하이엘지 후드 2단계로 해도 돼?",
           "하이엘지 후드 2단계로 해달라고 했어",
           "하이엘지 후드 2단계로 해주고 인덕션 켜줘",
           "후드 2단계로 해줘", "하이엘지 단계를 알려줘"}) {
    ++level_unit_checks;
    if (executable(classify(text))) {
      ++failures;
      std::cerr << "FAIL absolute level boundary: " << text << '\n';
    }
  }
  int induction_level_checks = 0;
  for (int level = 1; level <= 3; ++level) {
    const std::string heat = level == 1 ? "약불" : level == 2 ? "중불" : "강불";
    std::vector<std::string> bodies = {
        "인덕션 " + heat, "인덕션 " + heat + "해줘",
        "인덕션을 " + heat + "로 설정해주세요",
        "인덕션 화력을 " + heat + "로 맞춰줘"};
    for (const auto &number : numerals[level - 1])
      for (const auto &unit : std::vector<std::string>{"단", "단계"}) {
        bodies.push_back("인덕션 " + number + unit + " 해줘");
        bodies.push_back("인덕션을 " + number + unit + "으로 설정해줘");
      }
    for (const auto &body : bodies)
      for (const auto &ending : std::vector<std::string>{"", "?"}) {
        ++induction_level_checks;
        const auto actual = classify("하이엘지 " + body + ending);
        if (actual.intent != "IND_SET" || actual.value != level) {
          ++failures;
          std::cerr << "FAIL induction absolute level: " << body << ending << '\n';
        }
      }
  }
  for (const auto &text : std::vector<std::string>{
           "인덕션 약불로 해줘", "인덕션 1단으로 해줘",
           "하이엘지 인덕션 0단으로 해줘", "하이엘지 인덕션 4단으로 해줘",
           "하이엘지 인덕션 11단으로 해줘", "하이엘지 인덕션 -1단계로 해줘",
           "하이엘지 인덕션 1.5단으로 해줘", "하이엘지 인덕션 일단 켜줘",
           "하이엘지 인덕션 한 단계 올려줘",
           "하이엘지 인덕션 1단으로 내려줘", "하이엘지 인덕션 3단으로 올려줘",
           "하이엘지 인덕션 약불로 올려줘", "하이엘지 인덕션 강불로 내려줘",
           "하이엘지 팬 약불로 해줘", "하이엘지 후드 강불로 해줘",
           "하이엘지 약불로 해줘",
           "하이엘지 인덕션 약불로 하지 마", "하이엘지 인덕션 강불로 해도 돼?",
           "하이엘지 인덕션 중불로 해달라고 했어",
           "하이엘지 인덕션 약불로 되어 있어?",
           "하이엘지 인덕션 1단으로 해주고 후드 강풍",
           "하이엘지 인덕션 약불 중불로 해줘",
           "하이엘지 인덕션 팬 1단으로 해줘"}) {
    ++induction_level_checks;
    if (executable(classify(text))) {
      ++failures;
      std::cerr << "FAIL induction absolute boundary: " << text << '\n';
    }
  }
  int punctuation_checks = 0;
  for (const auto &text : std::vector<std::string>{
           "하이엘지 후드 약풍", "하이 엘쥐 후드 중풍", "하이엘지 후드 강풍",
           "하이엘지 후드 팬 꺼", "하이엘지 인덕션 꺼", "하이엘지 인덕션 켜",
           "하이엘지 불 세기 올려줘", "하이엘지 불 색이 내려",
           "하이엘지 후드 약풍으로 맞춰죠"}) {
    const auto expected = classify(text);
    for (const auto &mark : std::vector<std::string>{"?", "？", "??", "? ？"}) {
      ++punctuation_checks;
      const auto actual = classify(text + mark);
      if (!executable(actual) || actual.intent != expected.intent ||
          actual.value != expected.value || actual.normalized != expected.normalized) {
        ++failures;
        std::cerr << "FAIL terminal punctuation: " << text << mark << '\n';
      }
    }
  }
  for (const auto &text : std::vector<std::string>{
           "하이엘지 후드 켜", "하이엘지 팬 켜줘",
           "하이엘지 후드 팬을 켜주세요"}) {
    const auto result = classify(text);
    if (result.intent != "FAN_SET" || result.value != 1)
      ++failures;
  }
  for (const auto &text : std::vector<std::string>{
           std::string(201, 'a'), std::string(1, '\x80'),
           std::string("하이엘지 ") + '\0' + "인덕션 켜"}) {
    if (classify(text).intent != "DROP")
      ++failures;
  }
  int tuning_checks = 0;
  // Recorded target/level errors: correction needs activation, a fan target,
  // one absolute level and a clear margin; it must not erase other clauses.
  for (const auto &item : std::vector<std::pair<std::string, int>>{
           {"하이엘지 후드 약쿠", 1}, {"하이엘지 호두 약통", 1},
           {"하이엘지 호두 중풍", 2}, {"하이엘지 후드 중 꿈", 2},
           {"하이엘지 호두 중궁으로 해줘", 2},
           {"하이엘지 후드 팬 약쿠로 설정해주세요", 1},
           {"하이엘지 후더팬을 약품으로 해주세요", 1},
           {"하이엘지 포두 강풍", 3}, {"하이엘지 푸두 강풍", 3}}) {
    ++tuning_checks;
    const auto actual = classify(item.first);
    if (actual.intent != "FAN_SET" || actual.value != item.second) {
      ++failures;
      std::cerr << "FAIL recorded level: " << item.first << '\n';
    }
  }
  for (const auto &text : std::vector<std::string>{
           "호두 중풍", "후드 약쿠", "하이엘지 약쿠",
           "하이엘지 인덕션 약쿠", "하이엘지 조명 중궁",
           "하이엘지 후드 궁풍", "하이엘지 후드 준",
           "하이엘지 후드 약쿠 중궁", "하이엘지 호두 과자 꺼내줘",
           "하이엘지 호두 중궁으로 하지 마", "하이엘지 호두 중궁으로 해도 돼?",
           "하이엘지 후드 약쿠라고 했어", "하이엘지 후드 약쿠로 켜져 있어",
           "하이엘지 후드 중꿈 했다가 꺼줘",
           "하이엘지 후드 중궁으로 내려줘",
           "하이엘지 부대찌개 레시피 알려줘",
           "하이엘지 포드 자동차 가격 알려줘"}) {
    ++tuning_checks;
    if (executable(classify(text))) {
      ++failures;
      std::cerr << "FAIL level boundary: " << text << '\n';
    }
  }
  ++tuning_checks;
  const auto corrected_level = classify("하이엘지 호두 중궁");
  if (corrected_level.normalized != "하이엘지, 후드 중풍" ||
      corrected_level.level_similarity < kTargetMinimum)
    ++failures;
  std::cout << "hi_lg_checks=" << cases.size() + aliases.size() + 3 + 24 + boundaries.size() + 3 + punctuation_checks
            + tuning_checks + level_unit_checks + induction_level_checks
            << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
