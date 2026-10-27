// TCP 계약 v1 경계: 수신 한 줄 검증(hello/vision/voice/ui_cmd)과 state 직렬화.
// 형식 규칙은 contracts/*.schema.json과 같고, 스키마가 못 거는 의미 규칙(역할, bbox가 프레임 안)도 여기서 본다.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "policy.h"

enum class Role { none, vision, voice, ui };

struct Inbound {
  enum class Type { hello, vision, command } type;
  std::uint32_t seq;
  Role hello_role;            // type == hello
  hoodseek::VisionObs vision; // type == vision
  hoodseek::Command command;  // type == command (voice·ui_cmd)
};

// line: 개행을 뺀 한 줄. role: 이 연결이 hello로 정한 역할(hello 전이면 Role::none).
// 통과하면 ""를 돌려주고 out을 채운다. 실패하면 폐기 사유(로그용)를 돌려준다.
std::string parse_inbound(std::string_view line, Role role, Inbound& out);

struct StateView {
  std::uint32_t seq;
  std::uint64_t t_ms, uptime_s;
  const hoodseek::Outputs& out;
  const hoodseek::Profile& profile;  // thresholds = 표시용 사본 (D-042 ②)
  bool fan_ack, ind_ack;
  double t_max, t_median, pm25, tsize;  // out.*_valid일 때만 쓴다
};

// 계약 v1 state 한 줄(개행 없음).
std::string build_state_line(const StateView& s);
