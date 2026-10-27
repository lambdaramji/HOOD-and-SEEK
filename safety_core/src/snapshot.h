// 스레드 사이 공유 상태 하나 + mutex (설계서 §5.1). 필드마다 쓰는 스레드를 적는다.
// 여기에는 관측값과 명령만 들어온다. 판정은 policy 루프(main.cpp)의 hoodseek::Policy만 한다.
#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

#include "message.h"
#include "policy.h"

// 모든 신선도·타이머는 수신 측 monotonic clock (설계서 §1.2, D-024). 벽시계는 Pi에 NTP가 없어 쓰지 않는다.
inline std::uint64_t mono_ms() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

struct Snapshot {
  std::mutex mu;

  // sensor 스레드 (#47 2단계): 마지막 정상 읽기 값과 그 시각. 0 = 아직 없음
  std::uint64_t thermal_ok_ms = 0;
  double t_max_c = 0, t_median_c = 0;
  std::uint64_t pm_ok_ms = 0;
  std::uint32_t pm_seq = 0;  // 새 SPS30 샘플마다 +1 (#57 ③)
  double pm25 = 0, tsize = 0;
  bool pm_saturated = false;  // pm25가 SPS30 상한(kSpsPm25Max)에서 잘린 샘플 — 이때 tsize는 믿지 않는다 (#65 10/23)

  // tcp 스레드: 수신 시각 기준 (설계서 §2.4, D-024)
  std::uint64_t vision_rx_ms = 0;              // 마지막 유효 vision 메시지
  std::vector<hoodseek::VisionObs> vision_msgs;  // tick 사이 도착 순서 — policy가 꺼내 감 (#57 v2 ①)
  std::deque<std::pair<std::uint64_t, hoodseek::Command>> commands;  // (수신 시각, 명령) 도착 순서
  std::uint32_t rx_dropped = 0;  // 검증 실패·만료로 버린 메시지 수 (F8: 폐기 + 카운터, 상태 불변)

  // uart 스레드 (#56)
  bool fan_ack = false, ind_ack = false;
};
