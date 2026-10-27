#include "link.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>

namespace hoodseek {
namespace {
constexpr int kConnectWaitMs = 300, kSendWaitMs = 200;
constexpr int64_t kBackoffMs = 1000;  // design §2.1: client retries every 1 s, forever
}  // namespace

CoreLink::CoreLink(const std::string& address) {
  const auto colon = address.rfind(':');
  in_addr host{};
  if (colon == std::string::npos || inet_pton(AF_INET, address.substr(0, colon).c_str(), &host) != 1)
    throw std::runtime_error("--core must be IPv4:port, e.g. 192.0.2.10:7420");
  size_t used = 0;
  const auto digits = address.substr(colon + 1);
  const int port = std::stoi(digits, &used);
  if (used != digits.size() || port < 1 || port > 65535) throw std::runtime_error("--core port must be 1..65535");
  ipv4_ = host.s_addr;
  port_ = htons(static_cast<uint16_t>(port));
}

CoreLink::~CoreLink() { close_socket(); }

void CoreLink::close_socket() {
  if (fd_ >= 0) close(fd_);
  fd_ = -1;
  next_attempt_ms_ = now_ms_ + kBackoffMs;
}

bool CoreLink::connect_once() {
  next_attempt_ms_ = now_ms_ + kBackoffMs;
  int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) return false;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = port_;
  address.sin_addr.s_addr = ipv4_;
  if (connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof address) < 0) {
    // Non-blocking connect: an unreachable Pi 4 must not hold the inference loop for the kernel's SYN timeout.
    pollfd wait{fd, POLLOUT, 0};
    int error = 0;
    socklen_t length = sizeof error;
    if (errno != EINPROGRESS || poll(&wait, 1, kConnectWaitMs) != 1 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error != 0) {
      close(fd);
      return false;
    }
  }
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);  // ≈300 B lines; do not wait for Nagle
  // A pulled LAN cable (M13, F3) would otherwise let send() queue into the kernel for minutes and deliver the
  // gap as a late burst. Unacknowledged data for 2 s fails the socket, so the 1 s reconnect path drops the gap.
  unsigned int unacked_ms = 2000;
  setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &unacked_ms, sizeof unacked_ms);
  fd_ = fd;
  seq_ = 0;
  return send_line(encode_line(envelope("hello", seq_, now_ms_, {{"node", "vision-node"}, {"ver", "0.1.0"}})));
}

bool CoreLink::send_line(const std::string& line) {
  size_t sent = 0;
  while (sent < line.size()) {
    const auto n = send(fd_, line.data() + sent, line.size() - sent, MSG_NOSIGNAL);
    if (n > 0) { sent += static_cast<size_t>(n); continue; }
    if (n < 0 && errno == EINTR) continue;
    pollfd wait{fd_, POLLOUT, 0};
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && poll(&wait, 1, kSendWaitMs) == 1) continue;
    // A half-written line would corrupt framing; drop the connection and let the server re-base on the next one.
    close_socket();
    return false;
  }
  return true;
}

void CoreLink::drain() {
  // ponytail: state lines are read and discarded; #68 (Cooking/Idle) is the first consumer.
  char buffer[4096];
  for (;;) {
    const auto n = recv(fd_, buffer, sizeof buffer, MSG_DONTWAIT);
    if (n > 0) continue;
    if (n < 0 && errno == EINTR) continue;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    close_socket();  // 0 = server closed the connection; anything else = broken link
    return;
  }
}

void CoreLink::publish(const Json& payload, int64_t now_ms) {
  now_ms_ = now_ms;
  if (fd_ >= 0) drain();
  if (fd_ < 0 && (now_ms_ < next_attempt_ms_ || !connect_once())) return;
  if (seq_ == UINT32_MAX) { close_socket(); return; }  // 32-bit seq; a fresh connection starts over
  send_line(encode_line(envelope("vision", ++seq_, now_ms_, payload)));
}
}  // namespace hoodseek
