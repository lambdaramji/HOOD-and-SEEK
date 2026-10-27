#include "voice_tcp.h"

#include "command_parser.h"
#include "hi_lg_classifier.h"

#include <arpa/inet.h>
#include <fcntl.h>
#ifdef __linux__
#include <netinet/tcp.h>
#endif
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace hoodseek::voice {
namespace {

constexpr std::size_t kMaxLine = 4096;  // 종료 LF 포함. CR도 수신 byte 상한에 센다.

std::uint64_t monotonic_ms() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      VoiceTcpClient::Clock::now().time_since_epoch()).count());
}

const char* intent_name(Intent intent) {
  switch (intent) {
    case Intent::FAN_SET: return "FAN_SET";
    case Intent::FAN_OFF: return "FAN_OFF";
    case Intent::IND_OFF: return "IND_OFF";
    case Intent::IND_ON: return "IND_ON";
    case Intent::IND_SET: return "IND_SET";
    case Intent::IND_UP: return "IND_UP";
    case Intent::IND_DOWN: return "IND_DOWN";
    case Intent::NONE: return "NONE";
  }
  throw std::invalid_argument("invalid voice intent");
}

std::string json_text(std::string_view text) {
  std::string out = "\"";
  for (char ch : text) {
    switch (ch) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += ch;
    }
  }
  return out + '"';
}

std::string envelope(std::string_view type, std::uint32_t seq, std::uint64_t t_ms, int version = 1) {
  return "{\"v\":" + std::to_string(version) + ",\"type\":\"" + std::string(type) +
         "\",\"src\":\"voice\",\"seq\":" + std::to_string(seq) +
         ",\"t_ms\":" + std::to_string(t_ms) + ",\"p\":";
}

}  // namespace

std::optional<std::string> voice_line(std::string_view text, double conf,
                                      std::uint32_t seq, std::uint64_t t_ms,
                                      Activation activation) {
  if (!std::isfinite(conf) || conf < 0 || conf > 1) return std::nullopt;
  const auto normalized = normalize_command_text(text);  // 원문의 UTF-8·200자부터 검사한다.
  if (!normalized) return std::nullopt;
  std::optional<Command> command;
  std::string final_text;
  if (activation == Activation::HiLg) {
    if (conf != 0) return std::nullopt;  // SenseVoice는 ASR conf를 제공하지 않는다.
    const auto recognized = hi_lg::classify(std::string(text));
    if (recognized.intent == "DROP") return std::nullopt;
    for (const auto intent : {Intent::FAN_SET, Intent::FAN_OFF, Intent::IND_OFF,
                              Intent::IND_ON, Intent::IND_UP, Intent::IND_DOWN, Intent::NONE, Intent::IND_SET}) {
      if (recognized.intent == intent_name(intent))
        command = Command{intent, (intent == Intent::FAN_SET || intent == Intent::IND_SET) ? std::optional<int>(recognized.value)
                                                           : std::nullopt};
    }
    final_text = recognized.normalized;
    if (final_text.empty() || !normalize_command_text(final_text)) return std::nullopt;
  } else {
    command = parse_command(*normalized);
    final_text = command && command->intent == Intent::NONE ? std::string(text) : *normalized;
    if (final_text.find("후드") == std::string::npos) return std::nullopt;
  }
  if (!command || !is_valid_command(*command))
    return std::nullopt;
  // 선택한 활성어 규칙에서만 분류한다. 기본 Hood 경로에 HiLg를 자동 적용하지 않는다.
  std::ostringstream score;
  score.imbue(std::locale::classic());
  score << std::setprecision(4) << conf;
  auto line = envelope("voice", seq, t_ms, command->intent == Intent::IND_SET ? 2 : 1) +
              "{\"intent\":\"" + intent_name(command->intent) + '"';
  if (command->value) line += ",\"value\":" + std::to_string(*command->value);
  line += ",\"text\":" + json_text(final_text) + ",\"conf\":" + score.str() + "}}\n";
  if (line.size() > kMaxLine) return std::nullopt;
  return line;
}

VoiceTcpClient::VoiceTcpClient(std::string_view ipv4, std::uint16_t port, Activation activation)
    : port_(port), activation_(activation) {
  in_addr address{};
  const std::string host(ipv4);
  if (host.find('\0') != std::string::npos || inet_pton(AF_INET, host.c_str(), &address) != 1 || port == 0)
    throw std::invalid_argument("numeric IPv4 and nonzero TCP port are required");
  address_ = address.s_addr;
}

VoiceTcpClient::~VoiceTcpClient() {
  if (fd_ >= 0) close(fd_);
}

void VoiceTcpClient::disconnect() {
  if (fd_ >= 0) close(fd_);
  fd_ = -1;
  connected_ = false;
  received_bytes_ = 0;
  next_attempt_ = Clock::now() + std::chrono::seconds(1);
}

void VoiceTcpClient::expire_link() {
  // 추론 중 service()가 멈췄더라도 오래된 연결로 publish하지 않는다.
  // 수신 버퍼의 오래된 줄을 읽어 링크를 다시 신선하게 만들기 전에 검사한다.
  if (connected_ && Clock::now() - last_line_ >= std::chrono::seconds(3)) disconnect();
}

bool VoiceTcpClient::send_line(std::string_view line, Clock::time_point deadline) {
  // 쓰기 대기로 오디오 수집을 오래 막지 않는다. 부분 전송도 실패 후 재전송하지 않는다.
  deadline = std::min(deadline, Clock::now() + std::chrono::milliseconds(250));
  std::size_t sent = 0;
  while (sent < line.size() && Clock::now() < deadline) {
#ifdef MSG_NOSIGNAL
    constexpr int flags = MSG_NOSIGNAL;
#else
    constexpr int flags = 0;
#endif
    const auto n = send(fd_, line.data() + sent, line.size() - sent, flags);
    if (n > 0) {
      sent += static_cast<std::size_t>(n);
    } else if (n < 0 && errno == EINTR) {
      continue;
    } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      pollfd pfd{fd_, POLLOUT, 0};
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
      if (remaining <= 0) break;
      const auto rc = poll(&pfd, 1, static_cast<int>(remaining));
      if (rc < 0 && errno == EINTR) continue;
      if (rc <= 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) break;
    } else {
      break;
    }
  }
  if (sent == line.size()) return true;
  disconnect();
  return false;
}

void VoiceTcpClient::service() {
  expire_link();
  auto now = Clock::now();
  if (fd_ < 0) {
    if (now < next_attempt_) return;
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) { disconnect(); return; }
    if (fcntl(fd_, F_SETFL, O_NONBLOCK) < 0 || fcntl(fd_, F_SETFD, FD_CLOEXEC) < 0) {
      disconnect(); return;
    }
#ifdef SO_NOSIGPIPE
    const int no_sigpipe = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe)) < 0) {
      disconnect(); return;
    }
#endif
#ifdef __linux__
    // vision-node와 동일: FIN 없는 단절 때 커널에 남은 명령의 장기 재전송을 제한한다.
    const unsigned int unacked_ms = 2000;
    if (setsockopt(fd_, IPPROTO_TCP, TCP_USER_TIMEOUT, &unacked_ms, sizeof(unacked_ms)) < 0) {
      disconnect(); return;
    }
#endif
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = address_;
    remote.sin_port = htons(port_);
    const auto rc = connect(fd_, reinterpret_cast<sockaddr*>(&remote), sizeof(remote));
    if (rc < 0 && errno != EINPROGRESS && errno != EINTR) { disconnect(); return; }
    connect_deadline_ = now + std::chrono::seconds(1);
    return;  // 완료 확인은 다음 tick. 추론 뒤 늦게 확인하는 경로도 동일하게 처리한다.
  }
  if (!connected_) {
    pollfd pfd{fd_, POLLOUT, 0};
    const auto rc = poll(&pfd, 1, 0);
    if (rc < 0 && errno == EINTR) return;
    if (rc < 0 || (rc > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))) {
      disconnect(); return;
    }
    if (rc == 0) {
      if (Clock::now() >= connect_deadline_) disconnect();
      return;
    }
    int error = 0;
    socklen_t size = sizeof(error);
    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error != 0) {
      disconnect(); return;
    }
    const auto hello = envelope("hello", 0, monotonic_ms()) +
                       "{\"node\":\"voice-node\",\"ver\":\"0.1.0\"}}\n";
    if (!send_line(hello, Clock::now() + std::chrono::milliseconds(250))) return;
    next_seq_ = 1;
    last_line_ = Clock::now();
    connected_ = true;
  }
  // 본문을 해석·축적하지 않는다. state 표시는 Flutter 경로가 담당한다.
  // 한 번에 16 KiB까지만 읽어 연속 수신 중에도 호출자가 진행할 수 있게 한다.
  char buffer[4096];
  for (int batch = 0; batch < 4; ++batch) {
    const auto n = recv(fd_, buffer, sizeof(buffer), 0);
    if (n == 0) { disconnect(); return; }
    if (n < 0) {
      if (errno == EINTR) continue;
      if (errno != EAGAIN && errno != EWOULDBLOCK) disconnect();
      return;
    }
    for (std::ptrdiff_t i = 0; i < n; ++i) {
      if (++received_bytes_ > kMaxLine) { disconnect(); return; }
      if (buffer[i] == '\n') {
        ++received_lines_;
        received_bytes_ = 0;
        last_line_ = Clock::now();
      }
    }
  }
}

int VoiceTcpClient::wait_with_service(std::future<int>& task) {
  if (!task.valid()) throw std::future_error(std::future_errc::no_state);
  if (task.wait_for(std::chrono::milliseconds(0)) == std::future_status::deferred)
    throw std::invalid_argument("inference task must use std::launch::async");
  do {
    service();
  } while (task.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready);
  service();
  return task.get();
}

PublishResult VoiceTcpClient::publish(std::uint64_t id, std::string_view text, double conf,
                                      bool quality_passed, Clock::time_point deadline) {
  if (id <= last_id_) return PublishResult::Duplicate;
  last_id_ = id;  // 끊김·품질 탈락·만료로 생략한 발화도 나중에 재전송하지 않는다.
  if (!quality_passed) return PublishResult::QualityRejected;
  if (Clock::now() >= deadline) return PublishResult::Expired;
  const auto line = voice_line(text, conf, static_cast<std::uint32_t>(next_seq_), monotonic_ms(), activation_);
  if (!line) return PublishResult::Invalid;
  if (Clock::now() >= deadline) return PublishResult::Expired;
  expire_link();
  if (!connected_) return PublishResult::Disconnected;
  if (next_seq_ > std::numeric_limits<std::uint32_t>::max()) {
    disconnect(); return PublishResult::Disconnected;  // 같은 연결 안에서 seq를 되감지 않는다.
  }
  if (!send_line(*line, deadline)) return PublishResult::IoError;
  ++next_seq_;
  return PublishResult::Sent;
}

}  // namespace hoodseek::voice
