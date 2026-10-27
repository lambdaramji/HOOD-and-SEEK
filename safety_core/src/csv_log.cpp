#include "csv_log.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <ctime>

namespace {

void add(std::string& s, const char* v) {
  s += v;
  s += ',';
}
void add(std::string& s, const std::string& v) { add(s, v.c_str()); }
void add(std::string& s, std::uint64_t v) { add(s, std::to_string(v)); }
void add(std::string& s, bool v) { add(s, v ? "1" : "0"); }
void add(std::string& s, double v, const char* fmt) {
  char b[32];
  std::snprintf(b, sizeof b, fmt, v);
  add(s, b);
}
void add(std::string& s, const std::optional<double>& v, const char* fmt) {
  if (v) add(s, *v, fmt);
  else add(s, "");
}

}  // namespace

std::string csv_header() {
  return "seq,t_ms,uptime_s,wall_ms,wake,late_ms,cmd_wait_ms,work_us,rx_dropped,"
         "t_max,t_median,t_metric,thermal_valid,pm25,tsize,pm_saturated,pm_seq,pm_baseline,pm_valid,"
         "vision_fresh,vision_msgs,person,cat,presence,commands,"
         "hazard,lock,lock_release_left_s,fan_level,fan_min,fan_requested,fan_ack,induction,allowed_on,ind_ack,"
         "alert,reasons,voice_intent,voice_value,voice_accepted,voice_reason";
}

std::string csv_row(const CsvTick& t) {
  const hoodseek::Inputs& in = t.in;
  const hoodseek::Outputs& o = t.out;
  std::string s;
  s.reserve(256);
  add(s, std::uint64_t{t.seq});
  add(s, t.t_ms);
  add(s, t.uptime_s);
  add(s, t.wall_ms);
  add(s, t.by_command ? "cmd" : "tick");
  add(s, t.late_ms);
  add(s, t.cmd_wait_ms);
  add(s, t.work_us);
  add(s, std::uint64_t{t.rx_dropped});
  // 센서 값은 마지막 정상 읽기 그대로다. 지금 유효한지는 *_valid 열로 본다
  add(s, in.t_max_c, "%.2f");
  add(s, in.t_median_c, "%.2f");
  add(s, o.t_metric, "%.2f");
  add(s, o.thermal_valid);
  add(s, in.pm25, "%.2f");
  add(s, in.tsize, "%.3f");
  add(s, in.pm_saturated);  // 포화(센서 상한)면 연기형 — 과한 연기·포화 지속 판단 재료 (D-054·D-056)
  add(s, std::uint64_t{in.pm_seq});
  add(s, o.pm_baseline, "%.2f");
  add(s, o.pm_valid);
  add(s, o.vision_fresh);
  add(s, std::uint64_t{in.vision_msgs.size()});
  add(s, o.person_seen);
  add(s, o.cat_hit);
  add(s, hoodseek::to_string(o.presence));
  std::string cmds;  // 도착 순서, 예: ui:FAN_SET=2|voice:IND_ON
  for (const hoodseek::Command& c : in.commands) {
    if (!cmds.empty()) cmds += '|';
    cmds += c.source == hoodseek::CommandSource::kVoice ? "voice:" : "ui:";
    cmds += hoodseek::to_string(c.kind);
    if (c.kind == hoodseek::CommandKind::kFanSet || c.kind == hoodseek::CommandKind::kIndSet)
      cmds += '=' + std::to_string(c.value);
  }
  add(s, cmds);
  add(s, hoodseek::to_string(o.hazard));
  add(s, hoodseek::to_string(o.lock));
  if (o.lock_release_left_s) add(s, std::uint64_t(*o.lock_release_left_s));  // state v2 (#116)
  else add(s, "");
  add(s, std::uint64_t(o.fan_level));
  add(s, std::uint64_t(o.fan_min));
  add(s, std::uint64_t(o.fan_requested));
  add(s, t.fan_ack);
  add(s, std::uint64_t(o.induction_level));
  add(s, o.allowed_on);
  add(s, t.ind_ack);
  add(s, hoodseek::to_string(o.alert));
  std::string reasons;
  for (hoodseek::Reason r : o.reasons) {
    if (!reasons.empty()) reasons += '|';
    reasons += hoodseek::to_string(r);
  }
  add(s, reasons);
  if (o.voice) {
    add(s, hoodseek::to_string(o.voice->last_intent));
    if (o.voice->value) add(s, std::uint64_t(*o.voice->value));  // FAN_SET·IND_SET 단수 (D-062)
    else add(s, "");
    add(s, o.voice->accepted);
    add(s, o.voice->reason ? hoodseek::to_string(*o.voice->reason) : "");
  } else {
    s += ",,,,";
  }
  s.pop_back();  // 마지막 쉼표
  return s;
}

CsvLog::CsvLog(const std::string& dir) {
  if (dir == "none") return;
  if (::mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
    std::fprintf(stderr, "[log] %s 만들기 실패 (%s) — CSV 로그 끔\n", dir.c_str(), std::strerror(errno));
    return;
  }
  char stamp[32];
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
  localtime_r(&now, &tm);
  std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
  // 같은 초에 다시 뜬 경우(systemd Restart=always 1 s) 덮어쓰지 않게 O_EXCL로 새 이름을 찾는다
  int fd = -1;
  for (int n = 1; n <= 99 && fd < 0; ++n) {
    path_ = dir + "/" + stamp + (n == 1 ? "" : "-" + std::to_string(n)) + ".csv";
    fd = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0 && errno != EEXIST) break;
  }
  if (fd < 0 || !(f_ = ::fdopen(fd, "w"))) {
    std::fprintf(stderr, "[log] %s 열기 실패 (%s) — CSV 로그 끔\n", path_.c_str(), std::strerror(errno));
    if (fd >= 0) ::close(fd);
    path_.clear();
    return;
  }
  // 한 줄씩 커널로 넘긴다(fsync는 안 함 — 판정 루프를 SD 카드 쓰기에 묶지 않으려고).
  // 전원이 갑자기 끊기면(M11) 커널이 아직 디스크에 안 쓴 마지막 수십 초는 잃을 수 있다
  std::setvbuf(f_, nullptr, _IOLBF, 0);
  std::printf("[log] CSV 로그: %s\n", path_.c_str());
  write(csv_header());
}

CsvLog::~CsvLog() {
  if (f_) std::fclose(f_);
}

void CsvLog::write(const std::string& row) {
  if (!f_) return;
  if (std::fputs(row.c_str(), f_) == EOF || std::fputc('\n', f_) == EOF) {
    std::fprintf(stderr, "[log] %s 쓰기 실패 (%s) — CSV 로그 끔\n", path_.c_str(), std::strerror(errno));
    std::fclose(f_);
    f_ = nullptr;
  }
}
