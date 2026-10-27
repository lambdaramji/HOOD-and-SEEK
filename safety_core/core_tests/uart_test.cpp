// UART 링크 시험 (#56, 하드웨어 없음). 가상 터미널(pty) 한 쌍의 한쪽을 UartLink가 열고, 다른 쪽에서 ESP32를 흉내 낸다.
// 시간 값은 실제(1 s·2 s)보다 줄여서 몇 초 안에 끝난다. 규칙 자체는 같다.
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#include <atomic>
#include <mutex>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "snapshot.h"
#include "uart.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

// ESP32 쪽: 받은 줄을 모아 둔다
struct FakeEsp {
  int fd;
  std::string buf;
  std::vector<std::pair<std::uint64_t, std::string>> lines;  // (받은 시각, 줄)

  void pump(std::uint64_t ms) {
    const std::uint64_t end = mono_ms() + ms;
    while (mono_ms() < end) {
      pollfd p{fd, POLLIN, 0};
      if (::poll(&p, 1, 5) > 0) {
        char c[128];
        const ssize_t n = ::read(fd, c, sizeof c);
        for (ssize_t i = 0; i < n; ++i) {
          if (c[i] == '\n') {
            lines.emplace_back(mono_ms(), buf);
            buf.clear();
          } else {
            buf += c[i];
          }
        }
      }
    }
  }
  void say(const std::string& line) {
    const std::string s = line + "\r\n";  // 펌웨어 println과 같은 \r\n
    (void)::write(fd, s.data(), s.size());
  }
  int count(const std::string& line, std::uint64_t since = 0) const {
    int n = 0;
    for (const auto& [t, l] : lines) n += (t >= since && l == line);
    return n;
  }
};

}  // namespace

int main() {
  int master = -1, slave = -1;
  char name[128];
  if (openpty(&master, &slave, name, nullptr, nullptr) != 0) {
    std::printf("openpty 실패\n");
    return 2;
  }
  ::close(slave);  // UartLink가 이름으로 다시 연다
  fcntl(master, F_SETFL, O_NONBLOCK);

  std::atomic<bool> ack{false};
  UartLink::Timing tm;
  tm.resend_ms = 200;
  tm.ack_timeout_ms = 400;
  tm.stale_ms = 400;
  tm.reopen_ms = 100;
  tm.min_resend_gap_ms = 50;
  UartLink link("test", name, [&](bool ok) { ack = ok; }, tm);
  std::atomic<bool> stop{false};
  std::thread th([&] { link.run(stop); });
  FakeEsp esp{master, {}, {}};

  // policy 루프 흉내: target을 계속 갱신하는 스레드 (멈추면 stale 시험)
  std::atomic<bool> feeding{true};
  std::string target = "FAN 3";
  std::mutex tmu;
  std::thread feeder([&] {
    while (feeding) {
      {
        std::lock_guard<std::mutex> lock(tmu);
        link.set_target({target});
      }
      usleep(50 * 1000);
    }
  });

  // 1. 즉시 송신 + 재전송
  esp.pump(500);
  expect(esp.count("FAN 3") >= 2, "목표 즉시 송신 후 resend 주기로 재전송 (500 ms에 2회 이상)");
  expect(!ack, "ACK 전에는 ack=false");

  // 2. 일치 ACK → true
  esp.say("ACK FAN 3");
  esp.pump(100);
  expect(ack, "「ACK FAN 3」 → ack=true");

  // 3. 단수 변경: 즉시 송신, 직전 확인이 남아 있어 ack가 바로 꺼지지 않음
  const std::uint64_t t0 = mono_ms();
  {
    std::lock_guard<std::mutex> lock(tmu);
    target = "FAN 1";
  }
  esp.pump(120);
  bool first_fast = false;
  for (const auto& [t, l] : esp.lines) first_fast = first_fast || (l == "FAN 1" && t - t0 <= 120);
  expect(first_fast, "목표가 바뀌면 120 ms 안에 송신 (R6 500 ms 이내)");
  expect(ack, "단수 변경 직후에도 ack 유지 (직전 확인이 ack_timeout 안)");

  // 4. 불일치 ACK(비트 오류 흉내 — #78 리뷰 3): 확인으로 세지 않고 즉시 재전송
  esp.pump(250);  // 직전 확인이 만료되게 기다림 → 이제 FAN 1 확인이 필요
  const std::uint64_t t1 = mono_ms();
  esp.say("ACK FAN 3");
  esp.pump(120);
  expect(esp.count("FAN 1", t1) >= 1, "「ACK FAN 3」(보낸 건 FAN 1) → 재전송");
  esp.pump(200);
  expect(!ack, "불일치 ACK만 오면 ack_timeout 뒤 ack=false");
  esp.say("ACK FAN 1");
  esp.pump(80);
  expect(ack, "일치 ACK가 오면 다시 ack=true");

  // 5. ESP32 재부팅·fail-safe 줄 → 즉시 재전송
  const std::uint64_t t2 = mono_ms();
  esp.say("READY FAILSAFE FAN 3");
  esp.pump(100);
  expect(esp.count("FAN 1", t2) >= 1, "「READY …」 → 즉시 재전송");

  // 6. 잡음 줄(64자 초과)은 버리고, 그 뒤 정상 ACK는 받음
  esp.say(std::string(100, 'X') + "ACK FAN 1");
  esp.pump(60);
  esp.say("ACK FAN 1");
  esp.pump(60);
  expect(ack, "넘친 잡음 줄 뒤 정상 ACK 처리");

  // 7. policy 멈춤(set_target 없음) → 송신 중단 → ESP32가 fail-safe로 갈 수 있음 (F2)
  feeding = false;
  feeder.join();
  esp.pump(tm.stale_ms + 100);
  const std::uint64_t t3 = mono_ms();
  esp.pump(500);
  expect(esp.count("FAN 1", t3) == 0, "set_target이 stale_ms 넘게 없으면 재전송을 멈춤");

  // 8. 쿡탑: 두 명령 모두 확인돼야 ack=true
  stop = true;
  th.join();
  {
    std::atomic<bool> ack2{false};
    UartLink cook("cook", name, [&](bool ok) { ack2 = ok; }, tm);
    std::atomic<bool> stop2{false};
    std::thread th2([&] { cook.run(stop2); });
    FakeEsp esp2{master, {}, {}};
    for (int i = 0; i < 6; ++i) {
      cook.set_target({"IND 0", "LOCK 1"});
      esp2.pump(50);
    }
    expect(esp2.count("IND 0") >= 1 && esp2.count("LOCK 1") >= 1, "쿡탑: IND·LOCK 두 줄 송신");
    esp2.say("ACK IND 0");
    for (int i = 0; i < 2; ++i) {
      cook.set_target({"IND 0", "LOCK 1"});
      esp2.pump(50);
    }
    expect(!ack2, "쿡탑: IND만 확인되면 ack=false");
    esp2.say("ACK LOCK 1");
    for (int i = 0; i < 2; ++i) {
      cook.set_target({"IND 0", "LOCK 1"});
      esp2.pump(50);
    }
    expect(ack2, "쿡탑: IND·LOCK 둘 다 확인 → ack=true");
    stop2 = true;
    th2.join();
  }

  ::close(master);
  std::printf(failures ? "uart_test: %d FAIL\n" : "uart_test: all passed\n", failures);
  return failures ? 1 : 0;
}
