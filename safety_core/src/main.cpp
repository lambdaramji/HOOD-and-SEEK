// SafetyCore 진입점 (설계서 §5.1). 스레드: tcp(poll, tcp_server.cpp) · dbus(dbus_service.cpp, 기기 빌드)
// · uart_fan·uart_cook(uart.cpp, #56) · sensor(sensors.cpp, #47) · policy(이 파일의 main 루프).
// policy 루프는 tick마다 CSV 한 줄도 남긴다(csv_log.cpp, #88).
//
// policy 루프만 판정한다: Snapshot을 잠깐 잠가 입력을 복사 → hoodseek::Policy::tick → state 발행.
// 500 ms마다 돌고, voice·ui_cmd가 들어오면 기다리지 않고 바로 한 번 더 돈다(변화 즉시 state).
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "csv_log.h"
#include "message.h"
#include "policy.h"
#include "profile_loader.h"
#include "sensors.h"
#include "snapshot.h"
#include "tcp_server.h"
#include "uart.h"
#ifdef HOODSEEK_WITH_DBUS
#include "dbus_service.h"
#endif

namespace {

constexpr std::uint64_t kTickMs = 500;         // 설계서 §3.1
constexpr std::uint64_t kCommandTtlMs = 5000;  // voice·ui_cmd 유효 기간, 계약 v1 (설계서 §2.4)

std::atomic<bool> g_stop{false};  // lock-free atomic은 시그널 핸들러에서 써도 안전하다
std::atomic<bool> g_reload{false};
void on_stop(int) { g_stop = true; }
void on_hup(int) { g_reload = true; }

// rx가 now보다 뒤면 0 — 음수가 거대한 양수로 바뀌어 방금 온 명령이 만료로 버려지지 않게 (#76 리뷰 ①)
std::uint64_t age_ms(std::uint64_t rx_ms, std::uint64_t now) { return rx_ms > now ? 0 : now - rx_ms; }

// profile.json을 한 번도 못 읽어 Policy가 없을 때의 출력 — 스텁과 같은 fail-safe (#57 답글, 팬 3·인덕션 0)
hoodseek::Outputs failsafe_outputs() {
  hoodseek::Outputs o{};
  o.hazard = hoodseek::Hazard::kFault;
  o.presence = hoodseek::Presence::kUnknown;
  o.lock = hoodseek::Lock::kLockedDanger;
  o.fan_level = 3;
  o.fan_min = 3;
  o.alert = hoodseek::Alert::kNone;
  return o;
}

}  // namespace

int main(int argc, char** argv) {
  uint16_t port = 7420;
  std::string profile_path = "/data/hoodseek/profile.json";
  std::string fan_uart = "/dev/ttyAMA3", cook_uart = "/dev/ttyAMA5";  // 설계서 §4.5 (D-17). "none" = 끔
  std::string i2c_dev = "/dev/i2c-1";                                  // 설계서 §1 D-18. "none" = 끔
  std::string log_dir = "/data/hoodseek/log";                          // 설계서 §5.1 로그 (#88). "none" = 끔
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--port") == 0) port = static_cast<uint16_t>(std::atoi(argv[++i]));
    else if (std::strcmp(argv[i], "--profile") == 0) profile_path = argv[++i];
    else if (std::strcmp(argv[i], "--fan-uart") == 0) fan_uart = argv[++i];
    else if (std::strcmp(argv[i], "--cook-uart") == 0) cook_uart = argv[++i];
    else if (std::strcmp(argv[i], "--i2c") == 0) i2c_dev = argv[++i];
    else if (std::strcmp(argv[i], "--log-dir") == 0) log_dir = argv[++i];
  }

  std::setvbuf(stdout, nullptr, _IOLBF, 0);  // systemd 아래에서도 한 줄씩 바로 로그로
#ifdef HOODSEEK_WITH_DBUS
  // 기기에서는 root만 /dev/i2c-1·/dev/ttyAMA3·5를 연다 (#46 실측). ipk activation(u0_a…)으로 뜬 인스턴스는
  // 장치를 못 쓰면서 :7420을 먼저 잡을 수 있으므로 바로 끝낸다 — root systemd 유닛이 단일 인스턴스가 되게.
  if (geteuid() != 0) {
    std::fprintf(stderr, "[core] root가 아님 (uid %u) — 종료. hoodseek-safety-core.service로 실행 (#46)\n", geteuid());
    return 1;
  }
#endif
  std::signal(SIGPIPE, SIG_IGN);  // 끊긴 클라이언트에 send해도 죽지 않게
  std::signal(SIGINT, on_stop);
  std::signal(SIGTERM, on_stop);
  std::signal(SIGHUP, on_hup);

  hoodseek::Profile profile{};
  std::optional<hoodseek::Policy> policy;
  {
    std::string err;
    if (load_profile(profile_path, profile, err)) {
      policy.emplace(profile);
      std::printf("[core] profile 로드: %s\n", profile_path.c_str());
    } else {
      std::fprintf(stderr, "[core] profile 로드 실패 (%s) — fail-safe 출력 유지, SIGHUP으로 재시도\n", err.c_str());
    }
  }

  Snapshot snap;
  std::mutex wake_mu;
  std::condition_variable wake_cv;
  bool command_pending = false;

  TcpServer tcp(port, snap, [&] {
    {
      std::lock_guard<std::mutex> lock(wake_mu);
      command_pending = true;
    }
    wake_cv.notify_one();
  });
  // ponytail: 포트를 못 잡으면 종료한다. 같은 포트를 쥔 SafetyCore가 이미 있다는 뜻이라 단일 인스턴스 잠금 역할을 한다
  // (두 개가 동시에 UART를 쓰면 안 됨). systemd Restart=always가 1 s 뒤 다시 시도한다.
  if (!tcp.listen_ok()) return 1;
  CsvLog csv(log_dir);  // 포트를 잡은 뒤에 — 재시작이 반복돼도 빈 파일이 쌓이지 않게

  std::mutex state_mu;  // D-Bus GetState용 최신 state
  std::string state_latest;

  std::atomic<bool> stop{false};
  std::atomic<bool> tcp_dead{false};
  std::thread tcp_thread([&] {
    if (!tcp.run(stop)) {  // poll 오류 등으로 tcp 스레드가 끝나면 프로세스를 끝낸다 → systemd가 재기동 (§6 F2, #76 리뷰 ②)
      tcp_dead = true;
      wake_cv.notify_one();
    }
  });
  // ESP32 링크: ack가 바뀌면 Snapshot에 쓴다 → 다음 tick에 fan.ack·FAN_NOACK (§4.1, F5·F6)
  UartLink fan_link("fan", fan_uart, [&](bool ok) {
    std::lock_guard<std::mutex> lock(snap.mu);
    snap.fan_ack = ok;
  });
  UartLink cook_link("cook", cook_uart, [&](bool ok) {
    std::lock_guard<std::mutex> lock(snap.mu);
    snap.ind_ack = ok;
  });
  std::vector<std::thread> io_threads;
  if (fan_uart != "none") io_threads.emplace_back([&] { fan_link.run(stop); });
  if (cook_uart != "none") io_threads.emplace_back([&] { cook_link.run(stop); });
  // sensor 스레드: 정상 읽기만 Snapshot에 쓴다. 끊기면 3 s 뒤 Policy가 fault (F4)
  SensorThread sensors(snap, i2c_dev);
  if (i2c_dev != "none") io_threads.emplace_back([&] { sensors.run(stop); });

#ifdef HOODSEEK_WITH_DBUS
  std::thread dbus_thread([&] {
    run_dbus(stop, [&] {
      std::lock_guard<std::mutex> lock(state_mu);
      return state_latest;
    });
  });
#endif

  const std::uint64_t start = mono_ms();
  std::uint64_t next_tick = start;
  std::uint32_t seq = 0;
  std::string last_printed;

  while (!g_stop && !tcp_dead) {
    bool by_command;
    {
      std::unique_lock<std::mutex> lock(wake_mu);
      const std::uint64_t now = mono_ms();
      const auto wait = std::chrono::milliseconds(next_tick > now ? next_tick - now : 0);
      wake_cv.wait_for(lock, wait, [&] { return command_pending; });
      by_command = command_pending;
      command_pending = false;
    }
    const auto woke = std::chrono::steady_clock::now();
    const std::uint64_t woke_ms = mono_ms();
    const std::uint64_t late_ms = !by_command && woke_ms > next_tick ? woke_ms - next_tick : 0;
    if (g_reload.exchange(false)) {  // set_profile()은 tick과 같은 스레드에서 (#57 확인 2)
      hoodseek::Profile next{};
      std::string err;
      if (load_profile(profile_path, next, err)) {
        profile = next;
        if (policy) policy->set_profile(profile);
        else policy.emplace(profile);
        std::printf("[core] SIGHUP: profile 재로드\n");
      } else {
        std::fprintf(stderr, "[core] SIGHUP: profile 재로드 실패 (%s) — 이전 값 유지\n", err.c_str());
      }
    }
    hoodseek::Inputs in{};
    std::uint64_t now, cmd_wait_ms = 0;
    std::uint32_t rx_dropped;
    bool fan_ack, ind_ack;
    {
      std::lock_guard<std::mutex> lock(snap.mu);
      // now는 lock 안에서 읽는다: tcp 스레드가 lock 안에서 넣은 rx_ms는 모두 now 이하가 된다 (#76 리뷰 ① underflow)
      now = mono_ms();
      in.now_ms = now;
      in.thermal_ok_ms = snap.thermal_ok_ms;
      in.t_max_c = snap.t_max_c;
      in.t_median_c = snap.t_median_c;
      in.pm_ok_ms = snap.pm_ok_ms;
      in.pm_seq = snap.pm_seq;
      in.pm25 = snap.pm25;
      in.tsize = snap.tsize;
      in.pm_saturated = snap.pm_saturated;
      in.vision_rx_ms = snap.vision_rx_ms;
      in.vision_msgs.swap(snap.vision_msgs);  // 넘기고 비움 (#57 v2 ①)
      in.fan_ack = fan_ack = snap.fan_ack;
      in.ind_ack = ind_ack = snap.ind_ack;
      for (const auto& [rx_ms, cmd] : snap.commands) {  // 도착 순서, 5 s 지난 명령은 집행하지 않음 (#57 확인 1)
        if (age_ms(rx_ms, now) <= kCommandTtlMs) {
          in.commands.push_back(cmd);
          cmd_wait_ms = std::max(cmd_wait_ms, age_ms(rx_ms, now));
        } else {
          ++snap.rx_dropped;
        }
      }
      snap.commands.clear();
      rx_dropped = snap.rx_dropped;
    }
    if (now >= next_tick) next_tick = now + kTickMs;  // 명령으로 일찍 깬 tick은 2 Hz 주기를 밀지 않는다

    // tick이 예외를 던지면 프로세스를 죽이지 않고 이번 tick은 fail-safe 출력 (불변식 ⑥, #76 리뷰 ②).
    // 죽어서 재시작하면 locked_danger·타이머가 사라지고 부팅 유예부터 다시 시작하기 때문이다.
    hoodseek::Outputs out = failsafe_outputs();
    if (policy) {
      try {
        out = policy->tick(in);
      } catch (const std::exception& e) {
        std::fprintf(stderr, "[core] Policy::tick 예외 — 이번 tick fail-safe 출력: %s\n", e.what());
      } catch (...) {
        std::fprintf(stderr, "[core] Policy::tick 예외 — 이번 tick fail-safe 출력\n");
      }
    }

    // 집행: 매 tick 목표를 넘긴다 (바뀌면 링크가 즉시 송신, 같으면 1 s 재전송만). 잠금이면 쿡탑에 잠금 표시 (§4.3)
    fan_link.set_target({"FAN " + std::to_string(out.fan_level)});
    cook_link.set_target({"IND " + std::to_string(out.induction_level),
                          out.lock == hoodseek::Lock::kUnlocked ? "LOCK 0" : "LOCK 1"});

    StateView view{++seq, now, (now - start) / 1000, out, profile, fan_ack, ind_ack,
                   in.t_max_c, in.t_median_c, in.pm25, in.tsize};
    const std::string line = build_state_line(view);
    tcp.publish(line);
    {
      std::lock_guard<std::mutex> lock(state_mu);
      state_latest = line;
    }
    const auto work_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - woke);
    const auto wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    csv.write(csv_row({view.seq, now, view.uptime_s, static_cast<std::uint64_t>(wall_ms.count()), by_command, late_ms,
                       cmd_wait_ms, static_cast<std::uint64_t>(work_us.count()), rx_dropped, in, out, fan_ack, ind_ack}));

    // 콘솔: 판정 내용이 바뀔 때만 한 줄 (seq·시각·uptime을 0으로 둔 사본으로 비교)
    StateView same = view;
    same.seq = 0, same.t_ms = 0, same.uptime_s = 0;
    const std::string key = build_state_line(same);
    if (key != last_printed) {
      std::printf("[state] %s\n", line.c_str());
      last_printed = key;
    }
  }

  if (tcp_dead) std::fprintf(stderr, "[core] tcp 스레드 종료 — 프로세스 종료, systemd가 재기동 (§6 F2)\n");
  else std::printf("[core] 종료 신호 — 정지\n");
  stop = true;
  tcp_thread.join();
  for (std::thread& t : io_threads) t.join();
#ifdef HOODSEEK_WITH_DBUS
  dbus_thread.join();
#endif
  return tcp_dead ? 1 : 0;
}
