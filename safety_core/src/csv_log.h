// 판정 CSV 로그 (#88, 설계서 §5.1 「로그」): tick마다 입력·상태·출력 한 줄.
// journal의 [state]는 판정이 바뀔 때만 남지만, 이 파일은 매 tick 남아서 그래프·지연 분석 재료가 된다.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "policy.h"

struct CsvTick {
  std::uint32_t seq;
  std::uint64_t t_ms, uptime_s;  // t_ms = monotonic (state.t_ms와 같은 값)
  std::uint64_t wall_ms;         // Pi 벽시계 — NTP가 없어 틀리다. journal 시각과 맞출 때만 쓴다
  bool by_command;               // voice·ui_cmd가 와서 일찍 깬 tick
  std::uint64_t late_ms;         // 정기 tick이 예정 시각보다 늦게 깬 정도 (명령 tick은 0)
  std::uint64_t cmd_wait_ms;     // 이번 tick 명령 중 가장 오래 기다린 것의 수신 → tick 시간
  std::uint64_t work_us;         // 깬 뒤 입력 복사·판정·집행 목표·state 발행까지 걸린 시간
  std::uint32_t rx_dropped;      // 누적 폐기 수 (F8)
  const hoodseek::Inputs& in;
  const hoodseek::Outputs& out;
  bool fan_ack, ind_ack;
};

std::string csv_header();
std::string csv_row(const CsvTick& t);  // 개행 없음. 쉼표가 든 값은 없다(목록은 '|'로 잇는다)

// 파일 쓰기. 실패하면 stderr에 한 번 알리고 그 뒤로는 쓰지 않는다 — 판정·집행에는 영향 없음.
// ponytail: Core 시작마다 새 파일 하나, 회전·삭제 없음. 하루 약 30~35 MB(2 Hz × 약 180~200 B)라 /data 여유 5.4 GB로
// 시연 기간(~11/5)을 넘긴다. 오래 켜 둘 일이 생기면 크기 회전을 더한다.
class CsvLog {
 public:
  // dir = "none"이면 끔. 파일 이름은 Pi 벽시계 기준 YYYYMMDD-HHMMSS.csv (같은 이름이 있으면 -2, -3 …)
  explicit CsvLog(const std::string& dir);
  ~CsvLog();
  void write(const std::string& row);
  const std::string& path() const { return path_; }

 private:
  std::FILE* f_ = nullptr;
  std::string path_;
};
