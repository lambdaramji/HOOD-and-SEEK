#include "uart.h"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "snapshot.h"  // mono_ms

namespace {

constexpr std::size_t kMaxRxLine = 64;  // ESP32 응답은 30자 안쪽. 넘으면 잡음으로 보고 다음 \n까지 버린다
constexpr int kPollMs = 20;            // set_target 즉시 송신 지연 상한 (R6 500 ms 대비 충분히 작게)

std::string kind_of(const std::string& line) { return line.substr(0, line.find(' ')); }

}  // namespace

UartLink::UartLink(std::string name, std::string path, std::function<void(bool)> on_ack, Timing timing)
    : name_(std::move(name)), path_(std::move(path)), on_ack_(std::move(on_ack)), t_(timing) {}

UartLink::~UartLink() { close_port(); }

void UartLink::set_target(std::vector<std::string> lines) {
  std::lock_guard<std::mutex> lock(mu_);
  target_ms_ = mono_ms();
  bool changed = lines.size() != targets_.size();
  for (std::size_t i = 0; !changed && i < lines.size(); ++i) changed = lines[i] != targets_[i].line;
  if (!changed) return;
  std::vector<Target> next;
  for (auto& l : lines) {
    Target t{l, kind_of(l), 0};
    for (const Target& old : targets_) {
      if (old.kind == t.kind) t.acked_ms = old.acked_ms;  // 같은 장치 kind의 마지막 확인은 이어 간다
    }
    next.push_back(std::move(t));
  }
  targets_ = std::move(next);
  dirty_ = true;
}

bool UartLink::open_port() {
  fd_ = ::open(path_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) return false;
  termios tio{};
  if (tcgetattr(fd_, &tio) != 0) {
    close_port();
    return false;
  }
  cfmakeraw(&tio);  // 8N1, 에코·줄 편집 없음 (설계서 §4.1 115200 8N1)
  cfsetispeed(&tio, B115200);
  cfsetospeed(&tio, B115200);
  tio.c_cflag |= CLOCAL | CREAD;
  tio.c_cflag &= ~CSTOPB;
  tio.c_cflag &= ~CRTSCTS;
  if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
    close_port();
    return false;
  }
  tcflush(fd_, TCIOFLUSH);  // 열기 전에 쌓인 잡음은 버린다
  rx_.clear();
  rx_overflow_ = false;
  return true;
}

void UartLink::close_port() {
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
}

void UartLink::send_all(std::uint64_t now) {
  std::string out;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (const Target& t : targets_) out += t.line + "\n";
    dirty_ = false;
  }
  last_send_ms_ = now;
  resend_soon_ = false;
  if (out.empty()) return;
  // 한 번에 몇 바이트라 커널 버퍼에 바로 들어간다. 비차단 write가 일부만 쓰면 줄이 잘릴 수 있는데(예: "IND 0\nLO"),
  // ESP32가 잘린 줄을 ERR로 버리고 → 그 ERR를 보고 즉시 재전송하므로 다음 송신에서 회복한다 (#82 재범 nit).
  if (::write(fd_, out.data(), out.size()) < 0 && errno != EAGAIN) {
    std::fprintf(stderr, "[uart:%s] write 실패: %s — 다시 연다\n", name_.c_str(), std::strerror(errno));
    close_port();
  }
}

void UartLink::handle_line(const std::string& line, std::uint64_t now) {
  if (line.rfind("ACK ", 0) == 0) {
    const std::string acked = line.substr(4);
    const std::string kind = kind_of(acked);
    std::lock_guard<std::mutex> lock(mu_);
    for (Target& t : targets_) {
      if (t.kind != kind) continue;
      if (acked == t.line) {
        t.acked_ms = now;
      } else {
        // 보낸 단수와 다른 ACK = ESP32가 다른 값을 집행 중 (비트 오류 등, #78 리뷰 3). 확인으로 세지 않고 다시 보낸다
        std::fprintf(stderr, "[uart:%s] ACK 불일치: 보냄 '%s', 받음 '%s' — 재전송\n", name_.c_str(), t.line.c_str(),
                     line.c_str());
        resend_soon_ = true;
      }
    }
    return;
  }
  // READY(부팅)·FAILSAFE(3 s 무명령)·ERR(폐기) — 목표를 바로 다시 보내 정상 집행으로 되돌린다
  if (line.rfind("READY", 0) == 0 || line.rfind("FAILSAFE", 0) == 0 || line.rfind("ERR", 0) == 0) {
    // GND가 빠지면 ERR가 줄마다 쏟아진다(10/20 실측) → 1 s에 한 줄만 출력하고 나머지는 개수만
    if (now - last_log_ms_ >= 1000) {
      std::printf("[uart:%s] %s%s\n", name_.c_str(), line.c_str(),
                  suppressed_ ? (" (+" + std::to_string(suppressed_) + "줄 생략)").c_str() : "");
      last_log_ms_ = now;
      suppressed_ = 0;
    } else {
      ++suppressed_;
    }
    resend_soon_ = true;
  }
}

void UartLink::update_ack(std::uint64_t now) {
  bool ok;
  {
    std::lock_guard<std::mutex> lock(mu_);
    ok = !targets_.empty() && fd_ >= 0;
    for (const Target& t : targets_) {
      ok = ok && t.acked_ms != 0 && now - t.acked_ms < t_.ack_timeout_ms;
    }
  }
  if (static_cast<int>(ok) != ack_state_) {
    ack_state_ = ok;
    std::printf("[uart:%s] ack=%s\n", name_.c_str(), ok ? "true" : "false");
    if (on_ack_) on_ack_(ok);
  }
}

void UartLink::run(const std::atomic<bool>& stop) {
  bool open_logged = false;
  while (!stop) {
    const std::uint64_t now = mono_ms();
    if (fd_ < 0) {
      if (now - last_open_try_ms_ >= t_.reopen_ms || last_open_try_ms_ == 0) {
        last_open_try_ms_ = now;
        if (open_port()) {
          std::printf("[uart:%s] %s 열림\n", name_.c_str(), path_.c_str());
          open_logged = false;
          resend_soon_ = true;
        } else if (!open_logged) {  // 같은 실패는 한 번만 출력
          std::fprintf(stderr, "[uart:%s] %s 열기 실패: %s — %llu ms마다 재시도\n", name_.c_str(), path_.c_str(),
                       std::strerror(errno), static_cast<unsigned long long>(t_.reopen_ms));
          open_logged = true;
        }
      }
      update_ack(now);
      usleep(kPollMs * 1000);
      continue;
    }

    pollfd p{fd_, POLLIN, 0};
    const int r = ::poll(&p, 1, kPollMs);
    const std::uint64_t t = mono_ms();
    if (r < 0 && errno != EINTR) {
      std::fprintf(stderr, "[uart:%s] poll 실패: %s — 다시 연다\n", name_.c_str(), std::strerror(errno));
      close_port();
      continue;
    }
    if (r > 0 && (p.revents & (POLLERR | POLLHUP | POLLNVAL))) {
      std::fprintf(stderr, "[uart:%s] 장치 오류(revents=%d) — 다시 연다\n", name_.c_str(), p.revents);
      close_port();
      continue;
    }
    if (r > 0 && (p.revents & POLLIN)) {
      char buf[256];
      const ssize_t n = ::read(fd_, buf, sizeof buf);
      for (ssize_t i = 0; i < n; ++i) {
        const char c = buf[i];
        if (c == '\r') continue;
        if (c == '\n') {
          if (!rx_overflow_) handle_line(rx_, t);
          rx_.clear();
          rx_overflow_ = false;
        } else if (!rx_overflow_) {
          if (rx_.size() >= kMaxRxLine) {
            rx_overflow_ = true;  // 넘친 줄은 끝까지 버린다 (펌웨어 #78 리뷰 1과 같은 규칙)
            rx_.clear();
          } else {
            rx_ += c;
          }
        }
      }
    }

    bool dirty, stale, ever_targeted;
    {
      std::lock_guard<std::mutex> lock(mu_);  // target_ms_는 policy 스레드가 쓴다 → 읽기는 모두 이 잠금 안에서 (#82 재범 nit)
      dirty = dirty_;
      ever_targeted = target_ms_ != 0;
      // target_ms_는 다른 스레드가 t 다음에 찍었을 수 있다 → 음수가 거대한 양수가 되지 않게 (#76 리뷰 ①과 같은 이유)
      stale = target_ms_ == 0 || (t > target_ms_ && t - target_ms_ >= t_.stale_ms);
    }
    if (stale) {
      // policy 루프가 멈춤 → 송신 중단 → ESP32가 3 s 뒤 스스로 fail-safe (F2). 판정 없는 heartbeat는 보내지 않는다
      if (!stale_logged_ && ever_targeted) {
        std::fprintf(stderr, "[uart:%s] 목표가 %llu ms 넘게 갱신되지 않음 — 송신 중단(ESP32 fail-safe로)\n",
                     name_.c_str(), static_cast<unsigned long long>(t_.stale_ms));
        stale_logged_ = true;
      }
    } else {
      stale_logged_ = false;
      if (dirty || t - last_send_ms_ >= t_.resend_ms ||
          (resend_soon_ && t - last_send_ms_ >= t_.min_resend_gap_ms)) {
        send_all(t);
      }
    }
    update_ack(t);
  }
}
