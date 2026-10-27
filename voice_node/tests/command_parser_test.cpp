#include "command_parser.h"

#include <iostream>
#include <string>
#include <vector>

using hoodseek::voice::Command;
using hoodseek::voice::Intent;
using hoodseek::voice::power_retry_candidate;
using hoodseek::voice::is_valid_command;
using hoodseek::voice::normalize_command_text;
using hoodseek::voice::parse_command;
using hoodseek::voice::repeated_command_candidate;

int main() {
  int checks = 0;
  int failures = 0;
  const auto check = [&](bool ok, const std::string& label) {
    ++checks;
    if (!ok) {
      ++failures;
      std::cerr << "FAIL: " << label << '\n';
    }
  };
  struct Accepted { const char* text; Intent intent; std::optional<int> value; };
  const Accepted accepted[] = {
      {"후드 약풍", Intent::FAN_SET, 1},
      {"후드 약풍으로 해줘", Intent::FAN_SET, 1},
      {"후드 팬 약풍으로 해 주세요.", Intent::FAN_SET, 1},
      {"후드 약풍으로 켜", Intent::FAN_SET, 1},
      {"후드 약풍으로 켜줘", Intent::FAN_SET, 1},
      {"후드 팬을 약풍으로 켜 주세요.", Intent::FAN_SET, 1},
      {"후드 풍량을 약풍으로 좀 켜줄래요?", Intent::FAN_SET, 1},
      {"후드 중풍으로 켜", Intent::FAN_SET, 2},
      {"후드 팬 중풍으로 켜줘", Intent::FAN_SET, 2},
      {"후드 강풍으로 켜세요!", Intent::FAN_SET, 3},
      {"후드 바람을 강풍으로 켜주세요", Intent::FAN_SET, 3},
      {"후드 중풍으로 해줘", Intent::FAN_SET, 2},
      {"후드 중풍 설정해줘", Intent::FAN_SET, 2},
      {"후드 강풍으로 해줘", Intent::FAN_SET, 3},
      {"후드 강풍으로 바꿔 줘.", Intent::FAN_SET, 3},
      {"후드, 강풍으로 바꿔줘.", Intent::FAN_SET, 3},
      {"후드, 팬, 강풍으로, 해줘.", Intent::FAN_SET, 3},
      {"  후드\t강풍으로  바꿔\n줘.\r\n", Intent::FAN_SET, 3},
      {"후드강풍", Intent::FAN_SET, 3},
      {"후드 약", Intent::FAN_SET, 1},
      {"후드 중", Intent::FAN_SET, 2},
      {"후드 강", Intent::FAN_SET, 3},
      {"후드 약으로 해줘", Intent::FAN_SET, 1},
      {"후드 중으로 바꿔줘", Intent::FAN_SET, 2},
      {"후드 강으로 해줘", Intent::FAN_SET, 3},
      {"후드 강풍으로 좀 바꿔줘", Intent::FAN_SET, 3},
      {"후드 약풍으로 좀 해주세요", Intent::FAN_SET, 1},
      {"후드 팬 꺼", Intent::FAN_OFF, std::nullopt},
      {"후드 꺼줘", Intent::FAN_OFF, std::nullopt},
      {"후드 팬 꺼 주세요!", Intent::FAN_OFF, std::nullopt},
      {"후드 정지", Intent::FAN_OFF, std::nullopt},
      {"후드 팬 정지해줘", Intent::FAN_OFF, std::nullopt},
      {"후드 팬 좀 꺼줘", Intent::FAN_OFF, std::nullopt},
      {"후드, 팬, 좀, 꺼줘!", Intent::FAN_OFF, std::nullopt},
      {"후드 좀 꺼 주세요", Intent::FAN_OFF, std::nullopt},
      {"후드 인덕션 꺼", Intent::IND_OFF, std::nullopt},
      {"후드 인덕션 꺼 주세요.", Intent::IND_OFF, std::nullopt},
      {"후드 인덕션 좀 꺼줘", Intent::IND_OFF, std::nullopt},
      {"후드 인덕션 켜", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 켜줘", Intent::IND_ON, std::nullopt},
      {"후드 인덕션, 켜", Intent::IND_ON, std::nullopt},
      {"후드, 인덕션, 켜.", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 켜 주세요", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 좀 켜 주세요", Intent::IND_ON, std::nullopt},
      {"후드 화력 올려", Intent::IND_UP, std::nullopt},
      {"후드, 화력. 올려.", Intent::IND_UP, std::nullopt},
      {"후드. 인덕션. 켜.", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 화력 좀 올려줘", Intent::IND_UP, std::nullopt},
      {"후드 불 높여 주세요", Intent::IND_UP, std::nullopt},
      {"후드 화력 내려", Intent::IND_DOWN, std::nullopt},
      {"후드, 화력, 내려.", Intent::IND_DOWN, std::nullopt},
      {"후드\t,\t화력 , 내려.", Intent::IND_DOWN, std::nullopt},
      {"후드 인덕션 화력 좀 내려줘", Intent::IND_DOWN, std::nullopt},
      {"후드 불 줄여 주세요", Intent::IND_DOWN, std::nullopt},
      {"후드 팬을 약풍으로 해줘", Intent::FAN_SET, 1},
      {"후드 풍량은 중풍으로 바꿔주세요", Intent::FAN_SET, 2},
      {"후드 바람을 강풍으로 맞춰줄래요?", Intent::FAN_SET, 3},
      {"후드 강풍으로 바꿔줘?", Intent::FAN_SET, 3},
      {"후드, 강풍으로, 해줘?", Intent::FAN_SET, 3},
      {"후드 강풍으로 바꿔줄래", Intent::FAN_SET, 3},
      {"후드 팬을 1단으로 설정해줘", Intent::FAN_SET, 1},
      {"후드 팬을 1단으로 켜줘", Intent::FAN_SET, 1},
      {"후드 풍량을 두 단으로 켜", Intent::FAN_SET, 2},
      {"후드 팬을 3단으로 켜줄래?", Intent::FAN_SET, 3},
      {"후드 한 단으로 팬을 켜 주세요", Intent::FAN_SET, 1},
      {"후드 풍량을 2단으로 설정해주세요.", Intent::FAN_SET, 2},
      {"후드 바람은 3단으로 맞춰줘", Intent::FAN_SET, 3},
      {"후드 팬 풍량을 3단으로 설정해줘", Intent::FAN_SET, 3},
      {"후드 팬 바람을 2단으로 해줘", Intent::FAN_SET, 2},
      {"후드 팬을 일 단으로 바꿔줘", Intent::FAN_SET, 1},
      {"후드 팬 한단으로 해주세요", Intent::FAN_SET, 1},
      {"후드 풍량 이단으로 맞춰줘", Intent::FAN_SET, 2},
      {"후드 바람 두 단으로 바꿔줄래?", Intent::FAN_SET, 2},
      {"후드 팬 삼단으로 설정해주세요", Intent::FAN_SET, 3},
      {"후드 팬 세단으로 해줄래요?", Intent::FAN_SET, 3},
      {"후드 1단으로 팬을 설정해줘", Intent::FAN_SET, 1},
      {"후드 이 단으로 풍량을 좀 바꿔줘", Intent::FAN_SET, 2},
      {"후드 3단으로 팬을 설정해줄래?", Intent::FAN_SET, 3},
      {"후드 삼단으로 바람을 맞춰주세요!", Intent::FAN_SET, 3},
      {"후드팬을3단으로설정해줘", Intent::FAN_SET, 3},
      {"후드, 3단으로, 팬을, 설정해줘.", Intent::FAN_SET, 3},
      {"후드 팬을 꺼주세요", Intent::FAN_OFF, std::nullopt},
      {"후드 팬은 끄세요", Intent::FAN_OFF, std::nullopt},
      {"후드 팬 전원을 좀 꺼줘", Intent::FAN_OFF, std::nullopt},
      {"후드 팬 꺼줄래?", Intent::FAN_OFF, std::nullopt},
      {"후드 팬 전원 꺼줄래요?", Intent::FAN_OFF, std::nullopt},
      {"후드 꺼주세요?", Intent::FAN_OFF, std::nullopt},
      {"후드 팬을 정지해주세요?", Intent::FAN_OFF, std::nullopt},
      {"후드 인덕션을 꺼주세요", Intent::IND_OFF, std::nullopt},
      {"후드 인덕션 전원을 끄세요", Intent::IND_OFF, std::nullopt},
      {"후드 인덕션을 꺼줄래요?", Intent::IND_OFF, std::nullopt},
      {"후드 인덕션은 켜주세요", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 전원을 켜세요", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 켜줄래", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 켜줄래요?", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 켜주세요?", Intent::IND_ON, std::nullopt},
      {"후드 인덕션 화력을 높여주세요", Intent::IND_UP, std::nullopt},
      {"후드 화력을 올리세요", Intent::IND_UP, std::nullopt},
      {"후드 불은 높이세요?", Intent::IND_UP, std::nullopt},
      {"후드 인덕션 화력 올려줄래요?", Intent::IND_UP, std::nullopt},
      {"후드 화력 좀 높여줄래?", Intent::IND_UP, std::nullopt},
      {"후드 인덕션 화력을 줄여주세요", Intent::IND_DOWN, std::nullopt},
      {"후드 화력을 내리세요", Intent::IND_DOWN, std::nullopt},
      {"후드 불을 줄이세요?", Intent::IND_DOWN, std::nullopt},
      {"후드 화력 내려줄래요?", Intent::IND_DOWN, std::nullopt},
      {"후드 인덕션 불 좀 줄여줄래", Intent::IND_DOWN, std::nullopt},
  };
  for (const auto& item : accepted) {
    const auto actual = parse_command(item.text);
    check(actual && actual->intent == item.intent && actual->value == item.value &&
              is_valid_command(*actual), item.text);
  }

  struct Corrected { const char* text; const char* normalized; Intent intent; int value; };
  const Corrected corrected[] = {
      {"호대 종풍", "후드 중풍", Intent::FAN_SET, 2},
      {"호등, 강풍.", "후드 강풍.", Intent::FAN_SET, 3},
      {"후등 화력 올려", "후드 화력 올려", Intent::IND_UP, 0},
      {"푸드 인덕션 켜", "후드 인덕션 켜", Intent::IND_ON, 0},
      {"후도팬을 약풍으로 해주세요.", "후드팬을 약풍으로 해주세요.", Intent::FAN_SET, 1},
      {"후들 약품", "후드 약풍", Intent::FAN_SET, 1},
      {"호드 2단으로 펜을 설정해줘", "후드 2단으로 팬을 설정해줘", Intent::FAN_SET, 2},
      {"포드, 약품.", "후드 약풍.", Intent::FAN_SET, 1},
      {"후둘 화력 내려.", "후드 화력 내려.", Intent::IND_DOWN, 0},
      {"호둬 화력을 줄여주세요", "후드 화력을 줄여주세요", Intent::IND_DOWN, 0},
      {"푸대 종풍.", "후드 중풍.", Intent::FAN_SET, 2},
      {"부드 펜 꺼", "후드 팬 꺼", Intent::FAN_OFF, 0},
      {"부대 팬 꺼", "후드 팬 꺼", Intent::FAN_OFF, 0},
      {"후다 감풍", "후드 강풍", Intent::FAN_SET, 3},
      {"호다, 중풍.", "후드 중풍.", Intent::FAN_SET, 2},
      {"후드 펜을 약품으로 해주세요.", "후드 팬을 약풍으로 해주세요.", Intent::FAN_SET, 1},
      {"후드 약풍프", "후드 약풍", Intent::FAN_SET, 1},
      {"후드 임덕션 켜", "후드 인덕션 켜", Intent::IND_ON, 0},
      {"후드 인덕장 꺼", "후드 인덕션 꺼", Intent::IND_OFF, 0},
      {"후드 인덕전 켜", "후드 인덕션 켜", Intent::IND_ON, 0},
      {"후드 팬 꺼져", "후드 팬 꺼줘", Intent::FAN_OFF, 0},
      {"후드 팬 꺼죠.", "후드 팬 꺼줘.", Intent::FAN_OFF, 0},
      {"후드 꺼져!", "후드 꺼줘!", Intent::FAN_OFF, 0},
      {"후드 인덕션 꺼져", "후드 인덕션 꺼줘", Intent::IND_OFF, 0},
      {"후드 인덕션 꺼죠.", "후드 인덕션 꺼줘.", Intent::IND_OFF, 0},
      {"후드 인덕션 켜져", "후드 인덕션 켜줘", Intent::IND_ON, 0},
      {"후드 인덕션 켜죠!", "후드 인덕션 켜줘!", Intent::IND_ON, 0},
      {"후드 팬 약풍으로 켜져.", "후드 팬 약풍으로 켜줘.", Intent::FAN_SET, 1},
      {"후드, 팬, 중풍으로 켜죠.", "후드 팬 중풍으로 켜줘.", Intent::FAN_SET, 2},
      {"후드 강풍으로 켜져", "후드 강풍으로 켜줘", Intent::FAN_SET, 3},
      {"푸드 펜 약품으로 켜죠", "후드 팬 약풍으로 켜줘", Intent::FAN_SET, 1},
      {"후드팬을3단으로켜져", "후드팬을3단으로켜줘", Intent::FAN_SET, 3},
  };
  for (const auto& item : corrected) {
    const auto normalized = normalize_command_text(item.text);
    const auto actual = parse_command(item.text);
    check(normalized && *normalized == item.normalized, "normalized command text");
    check(normalized && normalize_command_text(*normalized) == normalized, "normalization idempotent");
    check(actual && actual->intent == item.intent && actual->value.value_or(0) == item.value &&
              is_valid_command(*actual), item.text);
  }
  for (const auto* text : {"푸드코트에서 인덕션 켰대", "후드 펜션 예약해줘",
                           "후드 약품 설명해줘", "후드 인덕션 켜지 마", "푸드 인덕션 켜도 돼?",
                           "후도 펜 끄지 마", "그 사람이 호대 종풍이라고 했어",
                           "후들 강풍 후들 강풍", "후드 인덕장 거", "후드 인덕션 커",
                           "부드 팬 거", "후드 팬 버", "모두 화력 내려", "구두 팬 포",
                           "후드 인덕션 켜져 있어", "후드 팬 꺼져 있어", "후드 인덕션 켜져?",
                           "후드 팬 꺼죠?", "후드 인덕션 안 켜져", "후드 팬 안 꺼져",
                           "후드 인덕션 켜지 마", "후드 팬 끄지 마", "후드 인덕션 켜져라고 했어",
                           "후드 팬 꺼져 인덕션 켜져", "후드 팬 켜져", "후드 켜죠",
                           "후드 강풍으로 켜져?", "후드 팬 약풍으로 켜죠?"}) {
    const auto actual = parse_command(text);
    check(!actual || actual->intent == Intent::NONE, "no inferred action or extracted command");
    check(normalize_command_text(text) == std::optional<std::string>{text}, "unsupported text preserved");
  }
  const auto contracted = parse_command("후드" + std::string(195, ' ') + "약풍프");
  check(contracted && contracted->intent == Intent::FAN_SET && contracted->value == 1,
        "200 original codepoints before contraction");
  check(!normalize_command_text("후드" + std::string(196, ' ') + "약풍프"),
        "201 original codepoints cannot shrink past input limit");
  check(!normalize_command_text(std::string("푸드 팬 꺼") + '\0'), "original NUL rejected");
  check(!normalize_command_text(std::string("후도\x80")), "original malformed UTF-8 rejected");

  const char* none[] = {
      "후드 인덕션. 켜지 마", "후드 팬. 끄지 마", "후드. 인덕션 켜도 돼?",
      "후드 팬 꺼. 인덕션 켜", "후드. 화. 력 올려", "후드.. 화력 올려",
      "후드 켜지 마", "후드 인덕션 켜지 마", "후드 팬 끄지 마",
      "후드 인덕션 켜면 안 돼", "후드 인덕션 안 켜줘", "후드 강풍으로 하지 마",
      "후드 인덕션 켜지 말고 꺼줘", "후드 팬 꺼주지 마",
      "후드 바람 좀 바꿔줘", "후드 세게", "후드 켜", "후드 요리 냄새가 심하네",
      "후드 세게 했다가 꺼줘", "후드 강풍으로 하고 인덕션 꺼줘",
      "후드 인덕션 켜고 팬 꺼", "후드 팬 꺼 후드 팬 꺼",
      "후드 한 단계 올려줘", "후드 바람 좀 줄여줘", "후드 조금 더 세게 해줘",
      "후드 팬 올려줘", "후드 팬 내려줘", "후드 풍량 올려줘",
      "후드", "후드티 강풍", "후드 인덕션", "후드 인덕션 켜졌어",
      "후드 인덕션 켜?", "후드 강풍으로 바꿔줘...",
      "후드 강풍으로 바꿔줘 [음악]", "후드 [BLANK_AUDIO]", "후드 4단으로 해줘",
      "후드 강풍으로", "후드 강풍으로 바꿔줘 취소", "후드 인덕션 잠금 풀어줘",
      "후드 팬 꺼\n인덕션 켜", "후드 강풍으로 바꿔줘 꺼줘", "응 후드 강풍",
      "후드 팬 좀 끄지 마", "후드 인덕션 좀 켜지 마", "후드 약 말고 강",
      "후드 강으로 했다가 꺼줘", "후드 중 약",
      "후드, 인덕션, 켜지 마.", "후드, 팬, 끄지 마!",
      "후드, 약풍 말고, 강풍", "후드, 강풍, 아니 중풍",
      "후드, 강풍. 후드, 팬 꺼.",
      "후드, 인덕션, 켜?", "후드, 팬, 꺼, 인덕션 켜",
      "후드,, 강풍", "후드, , 강풍", ",후드 강풍", "후드 강풍,",
      "후드 인,덕션 켜", "후드 화력 내,려", "후드, 인덕션 켜지,마",
      "후드, 팬 꺼 후드, 팬 꺼", "후드, 화력, 내려라고 했어",
      "그 사람이 후드, 강풍이라고 했어", "지금 후드, 강풍으로 해줘",
      "후드 팬을 끄지 말아줄래?", "후드 인덕션을 켜지 말아줘",
      "후드 인덕션 전원을 켜지 마", "후드 팬 전원 끄지 마세요",
      "후드 인덕션 켜도 돼?", "후드 팬을 꺼도 될까요?",
      "후드 인덕션 켜져 있어?", "후드 팬 꺼진 거야?",
      "후드 인덕션 켜줄래도 돼?", "후드 인덕션 켜세요라고 했어",
      "후드 인덕션 켜 줄래 말래", "후드 팬 꺼줄래 아니 켜줘",
      "후드 팬을 꺼주고 인덕션을 켜줘", "후드 약풍 말고 강풍으로 해줘",
      "후드 팬을 1단 또는 3단으로 설정해줘", "후드 팬을 3단 약풍으로 해줘",
      "후드 팬을 4단으로 해줘", "후드 팬을 0단으로 해줘",
      "후드 팬을 -1단으로 해줘", "후드 팬을 1.0단으로 해줘",
      "후드 풍량을 30단으로 설정해줘", "후드 바람을 2단 더 올려줘",
      "후드 풍량을 한 단계 올려줘", "후드 3단으로 설정해줘",
      "후드 팬 3단", "후드 팬 일단 꺼줘", "후드 일단 팬 꺼줘",
      "후드 인덕션을 3단으로 설정해줘", "후드 3단으로 인덕션을 설정해줘",
      "후드 팬 전원을 켜주세요", "후드 전원을 꺼주세요", "후드 팬 켜",
      "후드 풍량을 꺼줘", "후드 인덕션 전원을 올려줘",
      "후드 가스 켜줘", "후드 모터 꺼줘", "후드 레인지 켜줘",
      "후드 팬 꺼?", "후드 꺼?", "후드 강풍?", "후드 정지?",
      "후드 팬 꺼줄래??", "후드 인덕션 켜줘?!", "후드 팬을 꺼주세요? 아니야",
      "팬을 3단으로 설정해줘 후드", "후드 3단으로 해줘 팬을",
      "후드 강풍으로 풍량을 바꿔줘", "후드 좀 팬을 꺼줘",
      "후드 팬 약풍으로 켜지 마", "후드 약풍으로 켜도 돼?",
      "후드 약풍으로 켜?", "후드 중풍으로 켜져 있어",
      "후드 강풍으로 켜고 인덕션 꺼줘", "후드 팬 4단으로 켜줘",
      "후드 팬 켜줘", "후드 켜줘", "후드 인덕션 약풍으로 켜줘",
  };
  for (const auto* text : none) {
    const auto actual = parse_command(text);
    check(actual && actual->intent == Intent::NONE && !actual->value &&
              is_valid_command(*actual), text);
  }

  std::vector<std::string> dropped{
      "", " \t\r\n", "요리 냄새가 심하네", "강풍으로 바꿔줘", "인덕션 켜줘",
      "후 드 강풍", "[BLANK_AUDIO]", "후,드 강풍",
      "후드" + std::string(199, 'a'),
      std::string("후드 팬 꺼") + '\0',
      std::string("후드\x7f"), std::string("후드\x01"), std::string("후드\x80"),
      std::string("후드\xc0\xaf"), std::string("후드\xe0\x80\xaf"),
      std::string("후드\xed\xa0\x80"), std::string("후드\xf0\x80\x80\xaf"),
      std::string("후드\xf4\x90\x80\x80"), std::string("후드\xf5\x80\x80\x80"),
      std::string("후드\xe3\x81"), std::string("후드\xc2"), std::string("후드\xe3"),
      std::string("후드\xe3\x28\xa1"), std::string("후드\xff"),
      "후드" + std::string(801, 'a'),
  };
  for (const auto& text : dropped) check(!parse_command(text), "drop: " + text);

  // 문자 수 200과 byte 수 800 경계. 출력은 NONE이고 잘라서 실행하지 않는다.
  std::string boundary = "후드";
  for (int i = 0; i < 198; ++i) boundary += "가";
  check(parse_command(boundary).has_value(), "200 Korean codepoints");
  check(!parse_command(boundary + "가"), "201 Korean codepoints");
  boundary = "후드";
  for (int i = 0; i < 198; ++i) boundary += "😀";
  check(parse_command(boundary).has_value(), "200 mixed codepoints");
  check(!parse_command(boundary + "😀"), "201 mixed codepoints");
  check(parse_command("후드" + std::string(198, 'a')).has_value(), "200 codepoints");

  // ASR가 말하지 않은 부분을 임의로 보완하지 않는다. 쉼표 외 부호·인용을
  // 지우지 않으며, 긴 공백 입력을 잘라서 명령으로 만들지 않는다.
  for (const auto* text : {"후드 인덕션 안켜", "후드 인덕션 못 켜", "후드 인덕션 켜지마",
                           "후드 팬 끄지마", "후드 강풍 말고 중풍", "후드 팬 꺼도 돼?",
                           "후드 인덕션 켜라고 했어", "후드 인덕션 켜줄래라고 물었어",
                           "\"후드 인덕션 켜\"", "\"후드, 인덕션, 켜\"", "후드 , , 강풍",
                           "후드 강풍\x1b", "후드 인덕션 켜; 팬 꺼"}) {
    const auto actual = parse_command(text);
    check(!actual || actual->intent == Intent::NONE, "additional rejection");
  }
  const auto spaced = parse_command("후드" + std::string(196, ' ') + "강풍");
  check(spaced && spaced->intent == Intent::FAN_SET && spaced->value == 3,
        "200 codepoints with whitespace");
  check(!parse_command("후드" + std::string(197, ' ') + "강풍"),
        "201 codepoints with whitespace");

  // 주변 문맥을 삭제하거나 키워드만 뽑아서 명령으로 승격시키지 않는다.
  for (const auto& item : accepted) {
    for (const auto* extra : {" 하지 마", " 취소", " 그리고 인덕션 켜줘", "라고 했어", "?!"}) {
      const auto actual = parse_command(std::string(item.text) + extra);
      check(actual && actual->intent == Intent::NONE, "suffix rejection");
    }
    // 알려진 활성어 별칭을 문장 처음에서만 해석한다. 임의의 활성어를
    // 삽입하지 않으며, 문장 뒤에 붙은 부정·질문·인용은 그대로 보존한다.
    for (const auto* activation : {"호대", "호등", "후등", "푸드", "후도", "후들", "호드", "포드",
                                    "후둘", "호둬", "푸대", "부드", "부대", "후다", "호다"}) {
      auto changed = std::string(item.text);
      changed.replace(changed.find("후드"), std::string("후드").size(), activation);
      const auto actual = parse_command(changed);
      check(actual && actual->intent == item.intent && actual->value == item.value,
            "known activation alias in an entire command");
      for (const auto* suffix : {" 하지 마", "라고 했어", " 그리고 인덕션 켜줘"}) {
        const auto rejected = parse_command(changed + suffix);
        check(!rejected || rejected->intent == Intent::NONE, "alias does not erase rejection context");
      }
    }
    for (const auto* activation : {"", "푸드코트", "포드자동차", "모두", "구두", "우즈"}) {
      auto changed = std::string(item.text);
      changed.replace(changed.find("후드"), std::string("후드").size(), activation);
      check(!parse_command(changed), "unknown activation is not inferred");
    }
    const auto quoted = parse_command("그 사람이 " + std::string(item.text) + "라고 했어");
    check(quoted && quoted->intent == Intent::NONE, "quoted command is not a request");
  }

  for (int value = -1; value <= 4; ++value)
    check(is_valid_command({Intent::FAN_SET, value}) == (value >= 1 && value <= 3),
          "FAN_SET value bounds");
  for (const auto* text : {"후드 강풍으로 켜. 후드 강풍으로 켜.",
                           "후드, 인덕션, 켜. 후드, 인덕션, 켜.",
                           "후드 팬 꺼! 후드 팬 꺼!", "푸드 약품 푸드 약품"}) {
    check(repeated_command_candidate(text).has_value(), "same repeated command can request re-transcription");
    const auto original = parse_command(text);
    check(!original || original->intent == Intent::NONE, "repetition alone cannot execute");
  }
  const Accepted equivalent_repeats[] = {
      {"후드, 인덕션, 꺼주세요. 푸드, 인덕션, 꺼 주세요.", Intent::IND_OFF, std::nullopt},
      {"후드 인덕션 꺼주세요.푸드 인덕션 꺼 주세요.", Intent::IND_OFF, std::nullopt},
      {"후드 중풍!호다 중풍.", Intent::FAN_SET, 2},
      {"후드 중풍 호다 종풍", Intent::FAN_SET, 2},
      {"후드 팬 꺼. 호다 팬 끄세요.", Intent::FAN_OFF, std::nullopt},
  };
  for (const auto& item : equivalent_repeats) {
    const auto candidate = repeated_command_candidate(item.text);
    check(candidate && candidate->intent == item.intent && candidate->value == item.value,
          "entire equivalent commands can request re-transcription");
    const auto original = parse_command(item.text);
    check(!original || original->intent == Intent::NONE, "equivalent repetition alone cannot execute");
  }
  for (const auto* text : {"후드 강풍", "후드 인덕션 켜 후드 팬 꺼",
                           "후드 인덕션 꺼. 푸드 인덕션 켜.",
                           "후드 중풍. 호다 강풍.", "후드 팬 꺼. 호다 팬 끄지 마.",
                           "후드 인덕션 꺼. 푸드 인덕션 켜지 마.",
                           "후드 인덕션 꺼. 푸드 인덕션 꺼도 돼?",
                           "후드 인덕션 꺼. 푸드 인덕션 꺼 주세요. 오늘 날씨 좋네.",
                           "후드 중풍.호다 중풍.후드 중풍.",
                           "후드 인덕션 켜지 마 후드 인덕션 켜지 마",
                           "후드 팬 끄지 마. 후드 팬 끄지 마.",
                           "후드 팬 꺼. 후드 팬 꺼지 마.", "후드 팬 꺼? 후드 팬 꺼?",
                           "후드 팬 꺼줄래? 후드 팬 꺼줄래?",
                           "후드 바람 바꿔줘 후드 바람 바꿔줘",
                           "후드 팬 꺼 후드 팬 꺼 후드 팬 꺼",
                           "그 사람이 후드 강풍 후드 강풍이라고 했어",
                           "후드 인덕션 켜져 있어 후드 인덕션 켜져 있어",
                           "후드 팬 켜줘 후드 팬 켜줘", "인덕션 켜 인덕션 켜"})
    check(!repeated_command_candidate(text), "no retry from negation, question, quote or mixed requests");
  check(!repeated_command_candidate(std::string("후드 팬 꺼 후드 팬 꺼") + '\0'), "repeat NUL rejected");
  check(!repeated_command_candidate("후드" + std::string(199, 'a')), "repeat 201 characters rejected");
  for (const auto* text : {"후드 인덕션 강", " 후드, 인덕션 강.", "후드인덕션강"}) {
    const auto retry = power_retry_candidate(text);
    check(retry && retry->intent == Intent::IND_OFF && !retry->value,
          "ambiguous power may only retry toward IND_OFF, never IND_ON");
    const auto command = parse_command(text);
    check(command && command->intent == Intent::NONE, "ambiguity itself is not an execution command");
  }
  for (const auto* text : {"푸드 인덕션 강", "후드 인덕션 강?", "후드 인덕션 켜지 마", "후드 인덕션 끄지 마",
                           "후드 인덕션 강이라고 했어", "후드 강풍", "인덕션 강",
                           "후드 인덕션 강 후드 인덕션 강", "후드 인덕션 켜도 돼?",
                           "후드 인덕션 강!", "후드 인덕션 강 꺼줘", "후드 인덕션 강\n켜지 마"})
    check(!power_retry_candidate(text), "no power retry from missing activation, question, negation or quote");
  for (const auto& text : dropped)
    check(!power_retry_candidate(text), "malformed text is not a power retry candidate");
  check(!is_valid_command({Intent::FAN_SET, std::nullopt}), "FAN_SET needs value");
  for (const auto* text : {" 푸드, 팬, 고", "후드 팬 고.", "후드팬고!"}) {
    check(hoodseek::voice::is_damaged_fan_text(text), "known damaged fan may request original-model retry");
    const auto command = parse_command(text);
    check(!command || command->intent == Intent::NONE, "damaged fan itself is not an execution command");
  }
  for (const auto* text : {"푸드 팬 고?", "푸드 팬 고 하지 마", "그 사람이 푸드 팬 고라고 했어",
                           "푸드코트 팬 고", "푸드 팬 고 인덕션 켜", "팬 고", "후드 팬 끄지 마",
                           "후드 팬 켜지 마", "후드 팬 고 후드 팬 고", "후드 인덕션 고"})
    check(!hoodseek::voice::is_damaged_fan_text(text), "no fan retry from noncommand or other target");
  for (const auto& text : dropped)
    check(!hoodseek::voice::is_damaged_fan_text(text), "malformed text cannot request fan retry");
  for (const auto intent : {Intent::FAN_OFF, Intent::IND_OFF, Intent::IND_ON,
                            Intent::IND_UP, Intent::IND_DOWN, Intent::NONE}) {
    check(is_valid_command({intent, std::nullopt}), "non-FAN_SET omits value");
    check(!is_valid_command({intent, 1}), "non-FAN_SET forbids value");
  }
  check(!is_valid_command({static_cast<Intent>(99), std::nullopt}), "unknown intent");
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures == 0 ? 0 : 1;
}
