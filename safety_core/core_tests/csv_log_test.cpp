// CSV 판정 로그 시험 (하드웨어 없음, #88): 열 수·목록 형식·빈 값, 같은 초에 다시 떠도 덮어쓰지 않음
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "csv_log.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    ++failures;
    std::printf("FAIL %s\n", what);
  }
}

size_t columns(const std::string& line) {
  size_t n = 1;
  for (char c : line) n += c == ',';
  return n;
}

// 0부터 센 i번째 열
std::string column(const std::string& line, size_t i) {
  size_t b = 0;
  for (; i > 0; --i) b = line.find(',', b) + 1;
  return line.substr(b, line.find(',', b) - b);
}

size_t index_of(const std::string& name) {
  const std::string h = csv_header();
  for (size_t i = 0; i < columns(h); ++i)
    if (column(h, i) == name) return i;
  return static_cast<size_t>(-1);
}

}  // namespace

int main() {
  using namespace hoodseek;
  const size_t n = columns(csv_header());

  // 부팅 직후: 명령·voice·reasons 없음, t_metric·baseline 없음
  Inputs in{};
  Outputs out{};
  out.hazard = Hazard::kFault;
  out.presence = Presence::kUnknown;
  out.lock = Lock::kLockedDanger;
  out.fan_level = out.fan_min = 3;
  out.alert = Alert::kNone;
  std::string row = csv_row({1, 500, 0, 1700000000000, false, 0, 0, 120, 0, in, out, false, false});
  expect(columns(row) == n, "빈 줄도 열 수가 머리줄과 같음");
  expect(column(row, index_of("t_metric")).empty(), "t_metric 없으면 빈 칸");
  expect(column(row, index_of("hazard")) == "fault", "hazard 이름");
  expect(column(row, index_of("voice_intent")).empty(), "voice 없으면 빈 칸");
  expect(column(row, index_of("lock_release_left_s")).empty(), "해제 남은 초 없으면 빈 칸");
  expect(column(row, index_of("pm_saturated")) == "0", "포화 아님");
  expect(row.back() != ',' || column(row, n - 1).empty(), "마지막 열까지 정확히");

  // 명령 두 개 + reasons 두 개 + voice 거부
  in.commands = {{CommandSource::kUi, CommandKind::kFanSet, 2}, {CommandSource::kVoice, CommandKind::kIndOn, 0}};
  in.vision_msgs.resize(2);
  out.hazard = Hazard::kDanger;
  out.t_metric = 31.25;
  out.reasons = {Reason::kSmoke, Reason::kTempDanger};
  out.voice = VoiceResult{CommandKind::kIndOn, std::nullopt, false, RejectReason::kLocked};
  out.lock_release_left_s = 4;
  in.pm_saturated = true;
  row = csv_row({7, 3500, 3, 1700000003000, true, 0, 41, 230, 2, in, out, true, true});
  expect(columns(row) == n, "목록이 있어도 열 수가 같음 (쉼표 없음)");
  expect(column(row, index_of("commands")) == "ui:FAN_SET=2|voice:IND_ON", "명령 목록");
  expect(column(row, index_of("reasons")) == "SMOKE|TEMP_DANGER", "reasons 목록");
  expect(column(row, index_of("t_metric")) == "31.25", "t_metric 값");
  expect(column(row, index_of("wake")) == "cmd", "명령 tick 표시");
  expect(column(row, index_of("cmd_wait_ms")) == "41", "명령 대기 시간");
  expect(column(row, index_of("vision_msgs")) == "2", "vision 메시지 수");
  expect(column(row, index_of("voice_reason")) == "LOCKED", "voice 거부 사유");
  expect(column(row, index_of("voice_value")).empty(), "IND_ON은 단수 없음");
  expect(column(row, index_of("lock_release_left_s")) == "4", "해제 남은 초 (state v2)");
  expect(column(row, index_of("pm_saturated")) == "1", "SPS30 포화");

  // voice IND_SET은 목표 단수를 남긴다 (D-062, state v4)
  out.voice = VoiceResult{CommandKind::kIndSet, 2, true, std::nullopt};
  row = csv_row({8, 4000, 4, 1700000004000, true, 0, 12, 200, 2, in, out, true, true});
  expect(columns(row) == n, "IND_SET도 열 수가 같음");
  expect(column(row, index_of("voice_intent")) == "IND_SET", "voice IND_SET");
  expect(column(row, index_of("voice_value")) == "2", "IND_SET 목표 단수");
  expect(column(row, index_of("voice_reason")).empty(), "수락이면 사유 빈 칸");

  // 같은 초에 두 번 열면 두 번째는 -2 — 앞 파일을 덮어쓰지 않는다
  char dir[] = "/tmp/csv_log_test.XXXXXX";
  expect(mkdtemp(dir) != nullptr, "임시 폴더");
  const std::string sub = std::string(dir) + "/log";
  std::string first, second;
  {
    CsvLog a(sub);
    a.write("x");
    CsvLog b(sub);
    first = a.path(), second = b.path();
  }
  expect(!first.empty() && !second.empty() && first != second, "같은 초 재시작은 새 이름");
  std::ifstream fa(first);
  std::string line;
  std::getline(fa, line);
  expect(line == csv_header(), "첫 줄은 머리줄");
  std::getline(fa, line);
  expect(line == "x", "쓴 줄이 그대로");
  expect(CsvLog("none").path().empty(), "none이면 끔");
  std::remove(first.c_str());
  std::remove(second.c_str());
  ::rmdir(sub.c_str());
  ::rmdir(dir);

  if (failures == 0) std::printf("csv_log_test ok\n");
  return failures == 0 ? 0 : 1;
}
