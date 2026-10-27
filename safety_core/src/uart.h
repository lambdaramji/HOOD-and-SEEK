// ESP32 UART 링크 하나 (설계서 §4.1, #56). 팬(uart3)·쿡탑(uart5)이 같은 클래스를 쓴다.
//
// policy 루프가 tick마다 set_target()으로 목표 명령 줄을 넘기면, 링크 스레드가
//   - 목표가 바뀌면 즉시 송신 (R6 판정 → PWM ≤ 500 ms, D-040 ⑥)
//   - 1 s마다 현재 목표를 재전송 (heartbeat 겸용 — 별도 HB 명령 없음)
//   - 「ACK <명령>」이 목표와 같을 때만 확인으로 센다. 단수가 다르면(비트 오류 등) 즉시 다시 보낸다 (#78 리뷰 3)
//   - 목표마다 마지막 일치 ACK가 2 s 안에 있으면 ack=true, 아니면 false → FAN_NOACK·IND_NOACK (F5·F6)
// 판정은 하지 않는다. 장치를 못 열면 2 s마다 다시 연다(그동안 ack=false).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class UartLink {
 public:
  struct Timing {
    std::uint64_t resend_ms = 1000;      // 설계서 §4.1 재전송 주기
    std::uint64_t ack_timeout_ms = 2000;  // 설계서 §4.1 Pi 4 측 ACK
    std::uint64_t stale_ms = 2000;        // 이 시간 set_target이 없으면 송신을 멈춘다 (아래 설명)
    std::uint64_t reopen_ms = 2000;
    std::uint64_t min_resend_gap_ms = 200;  // ACK 불일치·ERR·READY에 다시 보낼 때 최소 간격 (잡음 폭주 방지)
  };

  // name: 로그 이름("fan"·"cook"), path: /dev/ttyAMA3 등. on_ack: ack 상태가 바뀔 때만 호출 (링크 스레드에서)
  UartLink(std::string name, std::string path, std::function<void(bool)> on_ack, Timing timing);
  UartLink(std::string name, std::string path, std::function<void(bool)> on_ack)
      : UartLink(std::move(name), std::move(path), std::move(on_ack), Timing{}) {}
  ~UartLink();
  UartLink(const UartLink&) = delete;
  UartLink& operator=(const UartLink&) = delete;

  // policy 스레드: 이번 tick의 목표 명령들 (예: {"FAN 3"}, {"IND 0", "LOCK 1"}). 같은 값이어도 매 tick 부른다 —
  // 부르지 않으면 stale_ms 뒤 송신이 멈춰 ESP32가 3 s fail-safe로 간다. policy 루프가 멈췄는데 이 스레드만 살아
  // 옛 명령을 계속 보내면 ESP32 fail-safe(F2)가 영영 안 걸리기 때문이다.
  void set_target(std::vector<std::string> lines);

  void run(const std::atomic<bool>& stop);  // 링크 스레드 본체

 private:
  struct Target {
    std::string line;            // "FAN 3"
    std::string kind;            // "FAN" — ACK를 목표와 짝짓는 열쇠
    std::uint64_t acked_ms = 0;  // 이 kind의 마지막 일치 ACK (목표가 바뀌어도 유지 → 단수 바뀔 때 ack가 깜빡이지 않음)
  };

  bool open_port();
  void close_port();
  void send_all(std::uint64_t now);
  void handle_line(const std::string& line, std::uint64_t now);
  void update_ack(std::uint64_t now);

  const std::string name_, path_;
  const std::function<void(bool)> on_ack_;
  const Timing t_;
  int fd_ = -1;

  std::mutex mu_;  // targets_·target_ms_·dirty_ (policy 스레드와 공유)
  std::vector<Target> targets_;
  std::uint64_t target_ms_ = 0;  // 마지막 set_target 시각
  bool dirty_ = false;           // 목표가 바뀜 → 즉시 송신

  // 링크 스레드만
  std::string rx_;
  bool rx_overflow_ = false;
  std::uint64_t last_send_ms_ = 0, last_open_try_ms_ = 0;
  bool resend_soon_ = false;
  int ack_state_ = -1;  // -1 = 아직 알리지 않음
  bool stale_logged_ = false;
  std::uint64_t last_log_ms_ = 0;
  unsigned suppressed_ = 0;
};
