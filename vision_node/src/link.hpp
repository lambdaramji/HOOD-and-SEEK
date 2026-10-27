#pragma once
#include <cstdint>
#include <string>
#include "vision.hpp"

namespace hoodseek {
// TCP line-JSON client to SafetyCore :7420 (design §2.1, contracts/README.md v1).
// Driven from the inference loop and never waits more than ~0.3 s per call, so the stall watchdog stays quiet.
class CoreLink {
 public:
  explicit CoreLink(const std::string& address);  // "a.b.c.d:port"
  ~CoreLink();
  CoreLink(const CoreLink&) = delete;
  CoreLink& operator=(const CoreLink&) = delete;
  // Sends one vision message. Reconnects at most once per second; each connection starts with hello (seq 0) and
  // restarts vision seq at 1 (seq is per connection). While disconnected the payload is dropped, never queued.
  void publish(const Json& payload, int64_t now_ms);
 private:
  bool connect_once();
  bool send_line(const std::string& line);
  void drain();
  void close_socket();
  uint32_t ipv4_ = 0;
  uint16_t port_ = 0;
  int fd_ = -1;
  uint32_t seq_ = 0;
  int64_t now_ms_ = 0, next_attempt_ms_ = 0;
};
}  // namespace hoodseek
