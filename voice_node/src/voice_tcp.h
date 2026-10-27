#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <string>
#include <string_view>

namespace hoodseek::voice {

enum class Activation { Hood, HiLg };

// 원문 검증 뒤 지원 명령의 제한된 정규화문을 전송한다. 품질 판정은 호출자 소관이다.
// HiLg의 conf=0은 ASR 점수 미제공을 뜻한다. 자모 점수를 conf로 사용하지 않는다.
std::optional<std::string> voice_line(std::string_view text, double conf,
                                      std::uint32_t seq, std::uint64_t t_ms,
                                      Activation activation = Activation::Hood);

enum class PublishResult { Sent, Invalid, QualityRejected, Expired, Duplicate, Disconnected, IoError };

class VoiceTcpClient {
 public:
  using Clock = std::chrono::steady_clock;

  // 숫자 IPv4만 받는다. DNS나 새 네트워크 의존성 없이 Pi4 주소를 명시한다.
  VoiceTcpClient(std::string_view ipv4, std::uint16_t port,
                 Activation activation = Activation::Hood);
  ~VoiceTcpClient();
  VoiceTcpClient(const VoiceTcpClient&) = delete;
  VoiceTcpClient& operator=(const VoiceTcpClient&) = delete;

  // 단일 호출 스레드에서 주기적으로 호출. 연결·1초 backoff·수신 버퍼 비우기를 담당한다.
  // state는 안전 판단에 쓰지 않고 프레이밍만 확인한다. UI 피드백은 Flutter 소관이다.
  void service();
  // std::launch::async로 시작한 작업을 기다리는 동안 같은 스레드에서 TCP를 처리한다.
  // 작업은 이 client에 접근하지 않으며 결과·예외를 돌려준다. hello/state만 처리하고 voice는 발행하지 않는다.
  int wait_with_service(std::future<int>& task);
  bool connected() const { return connected_; }
  std::uint64_t received_lines() const { return received_lines_; }

  // id는 프로세스 내에서 단조 증가하는 발화 번호다. 실패/생략한 id도 소비한다.
  // deadline은 발화 종료 기준으로 호출자가 정하며, 송신 직전에 새로 연장하지 않는다.
  // Sent는 socket 쓰기 완료일 뿐 Core 수락·집행 ACK가 아니다. 큐·재전송은 없다.
  PublishResult publish(std::uint64_t id, std::string_view text, double conf,
                        bool quality_passed, Clock::time_point deadline);

 private:
  void disconnect();
  void expire_link();
  bool send_line(std::string_view line, Clock::time_point deadline);
  std::uint32_t address_;
  std::uint16_t port_;
  Activation activation_;
  int fd_ = -1;
  bool connected_ = false;
  Clock::time_point next_attempt_{};
  Clock::time_point connect_deadline_{};
  Clock::time_point last_line_{};
  std::uint64_t next_seq_ = 0;
  std::uint64_t last_id_ = 0;
  std::size_t received_bytes_ = 0;
  std::uint64_t received_lines_ = 0;
};

}  // namespace hoodseek::voice
