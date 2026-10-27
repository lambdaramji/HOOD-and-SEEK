#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace hoodseek::voice {

enum class Intent { FAN_SET, FAN_OFF, IND_OFF, IND_ON, IND_UP, IND_DOWN, NONE, IND_SET };

struct Command {
  Intent intent;
  std::optional<int> value;
};

// 원문 경계를 먼저 검사한다. 알려진 오인식은 교정 후 문장 전체가 단일
// 지원 명령일 때만 반영한다. 부정·질문·인용·미지원 입력은 교정하지 않는다.
// nullopt는 잘못된 UTF-8·제어 문자·200자 초과다. ASR 품질/송신 허가는 별도다.
std::optional<std::string> normalize_command_text(std::string_view text);

// ASR 성공·품질 검사를 통과한 전사만 입력한다. nullopt는 전송 제외,
// NONE은 활성 단어가 있지만 지원 명령 하나로 확정하지 못한 경우다.
std::optional<Command> parse_command(std::string_view text);

// 전체 문장 두 개가 같은 intent/value일 때만 재전사 후보를 반환한다.
// 별칭·띄어쓰기 차이는 기존 파서로 검사하며 글자 유사도로 추측하지 않는다.
// 축약한 문장을 실행하거나 전송하지 않는다. 최종 명령은 재전사에서 확인한다.
std::optional<Command> repeated_command_candidate(std::string_view text);

// 알려진 모호 전사는 IND_OFF 확인을 위한 재전사 후보만 만든다.
// 원문 자체를 실행하지 않으며 재전사가 IND_ON이면 채택하지 않는다.
std::optional<Command> power_retry_candidate(std::string_view text);

// 알려진 '팬 고' 전사 전체만 원본 모델의 재전사 대상으로 표시한다.
// '고'를 명령으로 교정하지 않으며, 최종 팬 명령·품질·기한은 별도로 검사한다.
bool is_damaged_fan_text(std::string_view text);

// 전송 직전에도 사용할 수 있는 intent/value 계약 검사. 실행 허가는 Core 소관이다.
bool is_valid_command(const Command& command);

}  // namespace hoodseek::voice
