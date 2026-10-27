// policy_test.cpp — Policy 판정 시험 (하드웨어 없음, CI safety_core_policy job).
// 입력 시퀀스 → 기대 (hazard, presence, lock, fan, induction). 시간은 now_ms를 500 ms씩 흘려 재현한다.
// 임계 숫자는 profiles/default.json을 실제 로더로 읽은 값만 쓴다. 여기 있는 숫자는 환경값(실내 온도·맑은 공기)과
// 계약 범위(단수 0..3)뿐이다.
// 구성: 불변식 ①~⑧(CLAUDE.md S10 교체본) → D-040 ④ 잠금 해제 → 히스테리시스 B → 설계서 §7 M1~M13 → SPS30 CSV 재생.
// D-054(#65 10/25 확정): 약불·중불·강불은 정상 조리(D-059부터 low·mid·high), 차단은 절대 고온 또는 65 °C급 고온 ∧ 과한 연기.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "policy.h"
#include "profile_loader.h"

using namespace hoodseek;

namespace {

int g_checks = 0;
int g_failed = 0;
const char* g_case = "";
Profile g_profile{};

#define CHECK(cond)                                                                   \
  do {                                                                                \
    ++g_checks;                                                                       \
    if (!(cond)) {                                                                    \
      ++g_failed;                                                                     \
      std::printf("  FAIL %s:%d [%s] %s\n", __FILE__, __LINE__, g_case, #cond);       \
    }                                                                                 \
  } while (0)

constexpr int kLevelMax = 3;          // 계약 v1 단수 범위
constexpr double kAmbientC = 28.0;    // 실내 중앙값 온도 (환경값)
constexpr double kCleanPm25 = 10.0;   // 맑은 공기 PM2.5 (환경값)
constexpr double kCleanTsize = 0.45;  // 맑은 공기 tsize [실측 PR #40 CSV 첫 행 0.43~0.45]

std::uint64_t to_ms(double s) { return static_cast<std::uint64_t>(s * 1000.0 + 0.5); }
int ticks(double s) { return static_cast<int>(s * 2.0 + 0.5); }

bool has(const std::vector<Reason>& r, Reason x) {
  for (Reason v : r) {
    if (v == x) return true;
  }
  return false;
}

int index_of(const std::vector<Reason>& r, Reason x) {
  for (std::size_t i = 0; i < r.size(); ++i) {
    if (r[i] == x) return static_cast<int>(i);
  }
  return -1;
}

// 모든 tick에서 검사하는 불변식: ② fan ≥ fan_min, ⑥ danger·fault면 팬 3·인덕션 0, ③ vision 만료면 사람·고양이 없음.
void check_every_tick(const Outputs& o) {
  CHECK(o.fan_level >= o.fan_min);
  CHECK(o.fan_level >= 0 && o.fan_level <= kLevelMax);
  CHECK(o.induction_level >= 0 && o.induction_level <= kLevelMax);
  if (o.hazard >= Hazard::kDanger) {
    CHECK(o.fan_level == kLevelMax);
    CHECK(o.induction_level == 0);
    CHECK(o.lock == Lock::kLockedDanger);
    CHECK(!o.allowed_on);
  }
  if (!o.vision_fresh) CHECK(!o.person_seen && !o.cat_hit && o.presence == Presence::kUnknown);
  if (o.lock != Lock::kUnlocked) CHECK(!o.allowed_on);
  // D-061: 켜기·올리기 허가는 재실 ∧ warning 이하에서만 (danger·fault·부재는 언제나 막힘)
  if (o.allowed_on) CHECK(o.presence == Presence::kPresent && o.hazard <= Hazard::kWarning);
  if (o.voice && o.voice->accepted) CHECK(!o.voice->reason);
  // #116: 남은 초는 locked_danger ∧ hazard ≤ mid에서만 있고, 그때는 0..P_lock_release_s
  if (o.lock_release_left_s) {
    CHECK(o.lock == Lock::kLockedDanger && o.hazard <= Hazard::kMid);
    CHECK(*o.lock_release_left_s >= 0 && *o.lock_release_left_s <= static_cast<int>(std::ceil(g_profile.P_lock_release_s)));
  } else {
    CHECK(!(o.lock == Lock::kLockedDanger && o.hazard <= Hazard::kMid));
  }
}

// 500 ms tick 시뮬레이터. MLX는 tick마다, SPS30은 1 s마다 새 샘플, vision은 tick마다 vision_per_tick개.
struct Sim {
  explicit Sim(const Profile& profile = g_profile) : p(profile), policy(profile) {}

  Profile p;
  Policy policy;
  std::uint64_t now = 100000;
  bool thermal_ok = true;
  double t_median = kAmbientC;
  double t_max = kAmbientC + 2.0;
  bool pm_ok = true;
  double pm25 = kCleanPm25;
  double tsize = kCleanTsize;
  bool pm_saturated = false;
  bool vision_on = true;
  int vision_per_tick = 1;
  VisionObs obs{true, 1.0, false, 0.0, 0.0};
  bool fan_ack = true;
  bool ind_ack = true;
  std::vector<Command> pending;

  std::uint64_t thermal_ok_ms = 0, pm_ok_ms = 0, vision_rx_ms = 0, last_pm_ms = 0;
  std::uint32_t pm_seq = 0;
  double sampled_pm25 = 0, sampled_tsize = 0;
  bool sampled_saturated = false;
  Outputs out{};

  void metric(double m) { t_max = t_median + m; }
  void person(bool here) {
    obs.person_seen = here;
    obs.person_score = here ? 1.0 : 0.0;
  }
  void cat(bool near) {
    obs.cat_seen = near;
    obs.cat_score = near ? 1.0 : 0.0;
    obs.cat_roi_overlap = near ? 1.0 : 0.0;
  }
  void clean_air() {
    pm25 = kCleanPm25;
    tsize = kCleanTsize;
    pm_saturated = false;
  }
  void saturated_air(double reported_tsize) {  // SPS30 상한에서 잘린 샘플 (#65 10/23: tsize 0.47~1.54로 튐)
    pm25 = 2000.0;
    tsize = reported_tsize;
    pm_saturated = true;
  }
  void smoke_air() {  // 작은 입자 + baseline보다 P_PM_smoke 넘게 높음 (인센스)
    pm25 = kCleanPm25 + p.P_PM_smoke + 20.0;
    tsize = p.P_tsize - 0.05;
    pm_saturated = false;
  }
  void steam_air() {  // 큰 입자 + 높은 농도 (가습기)
    pm25 = kCleanPm25 + p.P_PM_smoke + 100.0;
    tsize = p.P_tsize + 0.2;
    pm_saturated = false;
  }
  // 화재위험 (D-054): 65 °C급 고온 → warning·팬 3단 → 그 상태에서 포화 샘플이 P_heavy_smoke_samples 연속 → danger
  const Outputs& fire() {
    metric(p.P_T_danger + 1.0);
    step();
    saturated_air(1.2);
    return run(p.P_heavy_smoke_samples + 1.5);
  }
  // 열원·연기를 같은 tick에 치움. 다음 step에서 SPS30 새 샘플을 바로 받는다.
  void extinguish() {
    metric(2.0);
    clean_air();
    last_pm_ms = 0;
  }
  void voice(CommandKind k, int v = 0) { pending.push_back({CommandSource::kVoice, k, v}); }
  void ui(CommandKind k, int v = 0) { pending.push_back({CommandSource::kUi, k, v}); }

  const Outputs& step() {
    now += 500;
    Inputs in{};
    in.now_ms = now;
    if (thermal_ok) thermal_ok_ms = now;
    in.thermal_ok_ms = thermal_ok_ms;
    in.t_max_c = t_max;
    in.t_median_c = t_median;
    if (pm_ok && now - last_pm_ms >= 1000) {
      ++pm_seq;
      pm_ok_ms = last_pm_ms = now;
      sampled_pm25 = pm25;
      sampled_tsize = tsize;
      sampled_saturated = pm_saturated;
    }
    in.pm_ok_ms = pm_ok_ms;
    in.pm_seq = pm_seq;
    in.pm25 = sampled_pm25;
    in.tsize = sampled_tsize;
    in.pm_saturated = sampled_saturated;
    if (vision_on) {
      vision_rx_ms = now;
      for (int i = 0; i < vision_per_tick; ++i) in.vision_msgs.push_back(obs);
    }
    in.vision_rx_ms = vision_rx_ms;
    in.fan_ack = fan_ack;
    in.ind_ack = ind_ack;
    in.commands.swap(pending);
    out = policy.tick(in);
    check_every_tick(out);
    return out;
  }
  const Outputs& run(double seconds) {
    for (int i = 0; i < ticks(seconds); ++i) step();
    return out;
  }
  // 부팅 후 PM baseline 초기 구간(P_baseline_init_s)이 끝난 재실·정상 상태
  const Outputs& warm_up() { return run(p.P_baseline_init_s + 2.0); }
  // 재실 → 부재 확정까지
  void leave() {
    person(false);
    run(p.P_absent_s + 1.0);
  }
};

// ───────────────────────── 불변식 ① 분기 순서 = 우선순위 ─────────────────────────
void inv1_priority_ladder() {
  {
    Sim s;  // fault가 절대 고온보다 먼저
    s.warm_up();
    s.pm_ok = false;
    s.t_max = s.p.P_T_abs_danger + 1.0;
    s.run(s.p.P_fault_s + 1.0);
    CHECK(s.out.hazard == Hazard::kFault);
    CHECK(!s.out.reasons.empty() && s.out.reasons.front() == Reason::kSensorFault);
    CHECK(has(s.out.reasons, Reason::kTempAbs));
  }
  {
    Sim s;  // 65 °C급 고온 ∧ 과한 연기 = danger(화재위험), reasons는 고온 → 연기 순 (D-054)
    s.warm_up();
    s.fire();
    CHECK(s.out.hazard == Hazard::kDanger && s.out.lock == Lock::kLockedDanger);
    CHECK(has(s.out.reasons, Reason::kTempDanger) && has(s.out.reasons, Reason::kSmoke));
    CHECK(index_of(s.out.reasons, Reason::kTempDanger) < index_of(s.out.reasons, Reason::kSmoke));
  }
  {
    Sim s;  // 둘 중 하나만이면 warning — 65 °C급 고온 단독
    s.warm_up();
    s.ui(CommandKind::kIndSet, 2);
    s.step();
    s.metric(s.p.P_T_danger + 1.0);
    s.run(30.0);
    CHECK(s.out.hazard == Hazard::kWarning && s.out.lock == Lock::kUnlocked);
    CHECK(s.out.fan_level == kLevelMax && s.out.induction_level == 2);
    CHECK(has(s.out.reasons, Reason::kTempDanger) && !has(s.out.reasons, Reason::kSmoke));
  }
  {
    Sim s;  // 과한 연기 단독 (팬 3단은 사용자 요청)
    s.warm_up();
    s.ui(CommandKind::kFanSet, 3);
    s.step();
    s.saturated_air(1.2);
    s.run(s.p.P_heavy_smoke_samples + 1.5);
    CHECK(s.out.hazard == Hazard::kWarning && s.out.lock == Lock::kUnlocked);
    CHECK(has(s.out.reasons, Reason::kSmoke) && !has(s.out.reasons, Reason::kTempDanger));
  }
  {
    Sim s;  // 강불과 연기 있음(PM_WARN)이 같이 있으면 high — PM_WARN은 hazard를 안 바꾼다. 사유 둘 다
    s.warm_up();
    s.metric(s.p.P_T_high + 1.0);
    s.pm25 = kCleanPm25 + s.p.P_PM_warn + 1.0;
    s.tsize = s.p.P_tsize - 0.05;
    s.run(s.p.P_smoke_samples + 1.0);
    CHECK(s.out.hazard == Hazard::kHigh);
    CHECK((s.out.reasons == std::vector<Reason>{Reason::kTempHigh, Reason::kPmWarn}));
  }
}

// #57 STEP 4: PM 경고도 SPS30 새 샘플 P_smoke_samples 연속일 때만 (증기 첫 2행 오탐 방지)
void pm_warn_needs_consecutive_samples() {
  Sim s;
  s.warm_up();
  for (int n = 0; n < s.p.P_smoke_samples - 1; ++n) {  // 연속 P_smoke_samples − 1개 → 경고 아님
    s.pm25 = kCleanPm25 + s.p.P_PM_warn + 1.0;
    s.tsize = s.p.P_tsize - 0.05;
    s.run(1.0);
    CHECK(!has(s.out.reasons, Reason::kPmWarn) && s.out.hazard == Hazard::kLow);
  }
  s.clean_air();
  s.run(1.0);
  s.pm25 = kCleanPm25 + s.p.P_PM_warn + 1.0;
  s.tsize = s.p.P_tsize - 0.05;
  s.run(s.p.P_smoke_samples * 1.0);
  CHECK(has(s.out.reasons, Reason::kPmWarn) && s.out.hazard == Hazard::kLow);  // 연기 있음 표시만 (D-054)
  // 시작한 뒤에는 Δ만 본다 — 비포화 인센스는 tsize ≥ 0.49가 많다(#65 10/24 C2 98 %)
  s.tsize = s.p.P_tsize + 0.3;
  s.run(5.0);
  CHECK(has(s.out.reasons, Reason::kPmWarn));
  s.clean_air();
  s.run(1.0);
  CHECK(!has(s.out.reasons, Reason::kPmWarn));
}

void inv1_threshold_boundaries() {
  const auto level_at = [](double metric) {
    Sim s;
    s.warm_up();
    // Policy는 max − median으로 metric을 다시 계산한다. (28.0 + 13.4) − 28.0처럼 부동소수점 반올림으로
    // 경계값보다 아주 조금 작아지는 경우가 있어, 「경계값이면 그 단계」를 보는 시험이 반올림에 흔들리지 않게 1e-9를 더한다.
    s.metric(metric + 1e-9);
    return s.step().hazard;
  };
  const Profile& p = g_profile;
  CHECK(level_at(p.P_T_mid - 0.01) == Hazard::kLow);
  CHECK(level_at(p.P_T_mid) == Hazard::kMid);
  CHECK(level_at(p.P_T_high - 0.01) == Hazard::kMid);
  CHECK(level_at(p.P_T_high) == Hazard::kHigh);
  CHECK(level_at(p.P_T_danger - 0.01) == Hazard::kHigh);
  CHECK(level_at(p.P_T_danger) == Hazard::kWarning);  // 65 °C급 고온 단독은 차단이 아님 (D-054)
  {
    Sim s;  // 고온 경계에서 사유가 TEMP_HIGH → TEMP_DANGER로 바뀐다
    s.warm_up();
    s.metric(p.P_T_danger - 0.01 + 1e-9);
    s.step();
    CHECK(has(s.out.reasons, Reason::kTempHigh) && !has(s.out.reasons, Reason::kTempDanger));
    s.metric(p.P_T_danger + 1e-9);
    s.step();
    CHECK(has(s.out.reasons, Reason::kTempDanger) && !has(s.out.reasons, Reason::kTempHigh));
  }
  {
    Sim s;  // T_abs는 max 기준, metric과 무관
    s.warm_up();
    s.t_median = p.P_T_abs_danger - 1.0;
    s.t_max = p.P_T_abs_danger - 0.01;
    CHECK(s.step().hazard == Hazard::kLow);
    s.t_max = p.P_T_abs_danger;
    CHECK(s.step().hazard == Hazard::kDanger);
    CHECK(has(s.out.reasons, Reason::kTempAbs));
  }
  {
    Sim s;  // tsize == P_tsize는 연기형 입자가 아님 (엄격한 <)
    s.warm_up();
    s.pm25 = kCleanPm25 + p.P_PM_smoke + 20.0;
    s.tsize = p.P_tsize;
    s.run(p.P_smoke_samples + 2.0);
    CHECK(s.out.hazard == Hazard::kLow);
    CHECK(!has(s.out.reasons, Reason::kSmoke) && !has(s.out.reasons, Reason::kPmWarn));
  }
}

// #65 10/23 인센스: PM2.5가 SPS30 상한(2000)을 넘는 동안 tsize가 0.47~1.54로 튀어 연기 조건(tsize < P_tsize)이
// 깨졌고, 약 70 s 동안 normal·팬 0이었다. 포화 샘플은 입자 크기와 상관없이 연기 후보로 센다(불변식 ⑥ 쪽).
void saturated_pm_counts_as_smoke() {
  for (const double reported : {0.47, 1.0, 1.54, 3.0}) {
    Sim s;
    s.warm_up();
    s.saturated_air(reported);
    s.run(s.p.P_smoke_samples * 1.0 + 0.5);
    // 연기 있음(PM_WARN). 팬 3단이 아니면 세기를 못 가르므로 hazard는 그대로 (D-054)
    CHECK(has(s.out.reasons, Reason::kPmWarn) && !has(s.out.reasons, Reason::kSmoke));
    CHECK(s.out.hazard == Hazard::kLow && s.out.lock == Lock::kUnlocked);
  }
  {
    Sim s;  // 포화가 아닌 큰 입자는 지금처럼 증기 — 연기·PM 경고 아님 (M7)
    s.warm_up();
    s.steam_air();
    s.run(s.p.P_smoke_samples + 5.0);
    CHECK(!has(s.out.reasons, Reason::kSmoke) && !has(s.out.reasons, Reason::kPmWarn));
    CHECK(s.out.hazard == Hazard::kLow);
  }
  {
    Sim s;  // 포화가 연속 조건 중간에 끊기면(큰 입자 비포화 샘플) 카운트도 끊긴다 — 연속 조건은 그대로
    s.warm_up();
    s.saturated_air(1.2);
    s.run(1.0);
    s.steam_air();
    s.run(1.0);
    s.saturated_air(1.2);
    s.run(1.0);
    CHECK(!has(s.out.reasons, Reason::kPmWarn));
    s.run(s.p.P_smoke_samples * 1.0);
    CHECK(has(s.out.reasons, Reason::kPmWarn) && s.out.hazard == Hazard::kLow);
  }
}

// D-054 과한 연기 = 팬 3단인데도 SPS30 포화가 P_heavy_smoke_samples 연속.
// #65 10/24 실측(PR #122): 팬 0~1에서는 인센스 1개도 원값 천장이라 세기를 못 가르고, 팬 3단에서는 포화 최장 1개 10~12 s,
// 3개 이상 17~31 s로 갈린다.
void heavy_smoke_needs_fan3_and_duration() {
  {
    Sim s;  // 팬이 3단이 아니면 과한 연기로 세지 않는다. 포화가 P_smoke_fan_samples 이어지면 팬을 3단으로 올리고(D-056) 그때부터 센다
    s.warm_up();
    s.ui(CommandKind::kFanSet, 2);
    s.step();
    s.saturated_air(1.2);
    s.run(s.p.P_smoke_fan_samples - 1.0);
    CHECK(!has(s.out.reasons, Reason::kSmoke) && s.out.hazard == Hazard::kLow && s.out.fan_level == 2);
    s.run(1.5);
    CHECK(s.out.fan_level == kLevelMax && s.out.hazard == Hazard::kLow);
    s.run(s.p.P_heavy_smoke_samples - 1.0);
    CHECK(!has(s.out.reasons, Reason::kSmoke));
    s.run(1.5);
    CHECK(has(s.out.reasons, Reason::kSmoke) && s.out.hazard == Hazard::kWarning);
  }
  {
    Sim s;  // 팬 3단: 연속이 한 샘플 모자라면 아님, 끊기면 다시 셈
    s.warm_up();
    s.ui(CommandKind::kFanSet, 3);
    s.step();
    s.saturated_air(1.2);
    s.run(s.p.P_heavy_smoke_samples - 1.0);
    CHECK(!has(s.out.reasons, Reason::kSmoke));
    s.smoke_air();  // 비포화 한 샘플
    s.run(1.0);
    s.saturated_air(1.2);
    s.run(s.p.P_heavy_smoke_samples - 1.0);
    CHECK(!has(s.out.reasons, Reason::kSmoke));
    s.run(1.5);
    CHECK(has(s.out.reasons, Reason::kSmoke) && s.out.hazard == Hazard::kWarning);
    // 확정 뒤에는 비포화라도 Δ ≥ P_PM_smoke면 유지, Δ < P_PM_smoke면 풀림
    s.smoke_air();
    s.run(5.0);
    CHECK(has(s.out.reasons, Reason::kSmoke));
    s.clean_air();
    s.run(1.0);
    CHECK(!has(s.out.reasons, Reason::kSmoke));
    s.run(s.p.P_release_s + 0.5);
    CHECK(s.out.hazard == Hazard::kLow && s.out.lock == Lock::kUnlocked);
  }
}

// D-056 포화 지속 → 팬 하한 3. #65 10/25 S(37 cm·호일): 팬 1·2단 포화 최장 연속이 1개 17~27 s, 3개 이상 106~157 s.
// 포화가 P_smoke_fan_samples 연속이면 후드를 최대로 — hazard·인덕션은 그대로, 끊기면 다시 센다.
// 해제도 Δ < P_PM_smoke가 P_smoke_fan_samples 연속일 때만 — 연기 중 잠깐 Δ < 50(최장 19 s, #135)이면 풀리지 않는다.
void smoke_raises_fan_floor() {
  Sim s;
  s.warm_up();
  s.ui(CommandKind::kIndSet, 1);
  s.step();
  CHECK(s.out.fan_level == s.p.P_fan_min_cooking);
  s.saturated_air(1.2);  // 1개처럼 포화가 중간에 끊기면 다시 센다
  s.run(s.p.P_smoke_fan_samples - 1.0);
  s.smoke_air();
  s.run(1.0);
  s.saturated_air(1.2);
  s.run(s.p.P_smoke_fan_samples - 1.0);
  CHECK(s.out.fan_level == s.p.P_fan_min_cooking && s.out.hazard == Hazard::kLow);
  s.run(1.5);
  CHECK(s.out.fan_min == kLevelMax && s.out.fan_level == kLevelMax);
  CHECK(s.out.hazard == Hazard::kLow && s.out.induction_level == 1 && s.out.lock == Lock::kUnlocked);
  s.smoke_air();  // 확정 뒤에는 비포화라도 Δ ≥ P_PM_smoke면 유지
  s.run(3.0);
  CHECK(s.out.fan_level == kLevelMax);
  s.clean_air();  // Δ < P_PM_smoke가 한 샘플 모자라면 유지, 중간에 연기가 다시 오면 처음부터
  s.run(s.p.P_smoke_fan_samples - 1.0);
  CHECK(s.out.fan_level == kLevelMax);
  s.smoke_air();
  s.run(1.0);
  s.clean_air();
  s.run(s.p.P_smoke_fan_samples - 1.0);
  CHECK(s.out.fan_level == kLevelMax);
  s.run(1.5);
  CHECK(s.out.fan_min == s.p.P_fan_min_cooking && s.out.induction_level == 1 && s.out.hazard == Hazard::kLow);
}

// D-054: 약불·중불·강불은 연기가 어떻게 나든 정상 조리 — 잠그지 않고, 재실 중 인덕션을 내리지 않는다.
void cooking_tiers_never_lock() {
  const Profile& p = g_profile;
  for (const double m : {p.P_T_mid - 1.0, p.P_T_mid + 0.5, p.P_T_high + 0.5}) {
    for (int air = 0; air < 3; ++air) {
      for (const int fan : {0, 3}) {
        Sim s;
        s.warm_up();
        s.ui(CommandKind::kIndSet, 3);
        s.ui(CommandKind::kFanSet, fan);
        s.step();
        s.metric(m);
        if (air == 1) s.smoke_air();
        if (air == 2) s.saturated_air(1.2);
        for (int i = 0; i < ticks(60.0); ++i) {
          s.step();
          CHECK(s.out.hazard <= Hazard::kWarning && s.out.lock == Lock::kUnlocked);
          CHECK(s.out.induction_level == 3);
        }
      }
    }
  }
}

// ───────────────────────── 불변식 ② fan_level = max(fan_min, requested) ─────────────────────────
void inv2_fan_floor() {
  {
    Sim s;
    s.warm_up();
    s.metric(s.p.P_T_high + 1.0);
    s.ui(CommandKind::kFanSet, 0);
    s.step();
    CHECK(s.out.hazard == Hazard::kHigh);
    CHECK(s.out.fan_min == s.p.P_fan_min_high);
    CHECK(s.out.fan_level == s.out.fan_min);
    CHECK(s.out.fan_requested == 0);
  }
  {
    Sim s;
    s.warm_up();
    s.metric(s.p.P_T_danger + 1.0);
    s.voice(CommandKind::kFanSet, 1);
    s.step();
    CHECK(s.out.fan_level == kLevelMax);
    CHECK(s.out.fan_requested == 1);
    CHECK(s.out.voice && s.out.voice->accepted && s.out.voice->value == 1);
  }
}

// ───────────────────────── 불변식 ③ vision 2 s 만료 → unknown, 고양이 후보 소멸 ─────────────────────────
void inv3_vision_expiry() {
  {
    Sim s;
    s.warm_up();
    s.vision_on = false;
    s.run(s.p.P_vision_fresh_s + 0.5);
    CHECK(s.out.presence == Presence::kUnknown);
    CHECK(!s.out.vision_fresh && !s.out.person_seen && !s.out.cat_hit);
    CHECK(has(s.out.reasons, Reason::kVisionStale));
  }
  {
    Sim s;  // 만료 전 고양이 hit는 만료 뒤 이어서 세지 않는다
    s.warm_up();
    s.leave();
    CHECK(s.out.presence == Presence::kAbsent);
    s.cat(true);
    for (int i = 0; i < s.p.P_cat_hits - 1; ++i) s.step();
    CHECK(s.out.lock == Lock::kUnlocked);
    s.vision_on = false;
    s.run(s.p.P_vision_fresh_s + 0.5);
    s.vision_on = true;
    s.run(s.p.P_cat_absent_s - 0.5);
    CHECK(s.out.presence == Presence::kUnknown);  // 다시 신선해졌지만 P_absent_s를 아직 못 셈
    CHECK(s.out.lock == Lock::kUnlocked);         // 고양이 감시(P_cat_absent_s)도 신선해진 뒤부터 센다 (D-060)
  }
}

// ───────────────────────── 불변식 ④ 연속 판정은 Core가 vision 메시지마다 ─────────────────────────
void inv4_core_counts_vision_messages() {
  {
    Sim s;
    s.warm_up();
    s.leave();
    s.cat(true);
    for (int i = 0; i < s.p.P_cat_hits - 1; ++i) s.step();
    s.cat(false);
    s.step();  // 연속이 끊김
    s.cat(true);
    for (int i = 0; i < s.p.P_cat_hits - 1; ++i) s.step();
    CHECK(s.out.lock == Lock::kUnlocked);
    s.step();
    CHECK(s.out.lock == Lock::kLockedCat);
  }
  {
    Sim s;  // tick 사이 2개씩 오면 2개씩 센다
    s.vision_per_tick = 2;
    s.warm_up();
    s.leave();
    s.cat(true);
    const int need = (s.p.P_cat_hits + 1) / 2;
    for (int i = 0; i < need - 1; ++i) s.step();
    CHECK(s.out.lock == Lock::kUnlocked);
    s.step();
    CHECK(s.out.lock == Lock::kLockedCat);
  }
}

// ───────────────────────── 불변식 ⑤ 고양이 단독으로 잠그지 않음 (M9) ─────────────────────────
void inv5_cat_needs_absence_m9() {
  {
    Sim s;  // 재실 중 고양이는 잠그지 않음
    s.warm_up();
    s.cat(true);
    s.run(s.p.P_cat_hits * 2.0);
    CHECK(s.out.lock == Lock::kUnlocked);
    CHECK(s.out.cat_hit && has(s.out.reasons, Reason::kCatNear));
  }
  {
    Sim s;  // 부팅 직후 사람을 못 봤으면 presence는 P_absent_s까지 unknown이지만, 고양이 감시는 vision이 신선해진 뒤
            // P_cat_absent_s부터 센다 — 관측이 있는 빈 주방이다 (D-060)
    s.person(false);
    s.cat(true);
    s.run(s.p.P_cat_absent_s - 0.5);
    CHECK(s.out.presence == Presence::kUnknown);
    CHECK(s.out.lock == Lock::kUnlocked);
    s.run(1.0 + s.p.P_cat_hits * 0.5);
    CHECK(s.out.presence == Presence::kUnknown);
    CHECK(s.out.lock == Lock::kLockedCat);
  }
  {
    Sim s;  // M9: 부재 + 고양이 → locked_cat, 인덕션 0, 켜기 거부. 복귀 후 cooldown 뒤 해제
    s.warm_up();
    s.ui(CommandKind::kIndSet, 1);
    s.step();
    CHECK(s.out.induction_level == 1);
    s.leave();
    s.cat(true);
    for (int i = 0; i < s.p.P_cat_hits; ++i) s.step();
    CHECK(s.out.lock == Lock::kLockedCat);
    CHECK(s.out.induction_level == 0);
    CHECK(s.out.alert == Alert::kCatLock);
    s.voice(CommandKind::kIndOn);
    s.step();
    CHECK(s.out.voice && !s.out.voice->accepted && s.out.voice->reason == RejectReason::kLocked);
    s.cat(false);
    s.person(true);
    s.run(s.p.P_cat_cooldown_s - 0.5);
    CHECK(s.out.lock == Lock::kLockedCat);
    s.run(1.0);
    CHECK(s.out.lock == Lock::kUnlocked);
  }
}

// ───────────────────────── 고양이 감시 (D-060): 화재 쪽 부재(P_absent_s)와 따로, 자리를 비운 직후부터 ─────────────────────────
void cat_watch_d060() {
  const int arm = ticks(g_profile.P_cat_absent_s);  // 마지막 사람 뒤 이 tick에 감시가 시작된다
  {
    Sim s;  // 조리 중 자리를 비우면 P_cat_absent_s 뒤부터 세고, P_cat_hits 연속이면 잠근다 — presence는 아직 present
    s.warm_up();
    s.ui(CommandKind::kIndSet, 2);
    s.step();
    CHECK(s.out.induction_level == 2);
    s.person(false);
    s.cat(true);
    for (int i = 0; i < arm - 1; ++i) s.step();
    CHECK(s.out.lock == Lock::kUnlocked && s.out.cat_hit && s.out.induction_level == 2);
    for (int i = 0; i < s.p.P_cat_hits - 1; ++i) s.step();
    CHECK(s.out.lock == Lock::kUnlocked);
    s.step();
    CHECK(s.out.lock == Lock::kLockedCat && s.out.alert == Alert::kCatLock);
    CHECK(s.out.induction_level == 0 && !s.out.allowed_on);
    // 화재 쪽 부재는 그대로: 부재 보호·화면 「자리 비움」은 P_absent_s 뒤
    CHECK(s.out.presence == Presence::kPresent && !has(s.out.reasons, Reason::kAbsent));
  }
  {
    Sim s;  // 조리 중 사람 인식이 P_cat_absent_s보다 짧게 끊기기를 되풀이하면 고양이가 있어도 잠그지 않는다
    s.warm_up();
    s.cat(true);
    for (int r = 0; r < 10; ++r) {
      s.person(false);
      for (int i = 0; i < arm - 1; ++i) s.step();
      s.person(true);
      s.step();
    }
    CHECK(s.out.lock == Lock::kUnlocked && s.out.presence == Presence::kPresent);
  }
  {
    Sim s;  // 사람이 한 번 보이면 감시가 끝나고 고양이 연속 수도 처음부터 센다
    s.warm_up();
    s.person(false);
    s.cat(true);
    for (int i = 0; i < arm - 1 + s.p.P_cat_hits - 1; ++i) s.step();
    CHECK(s.out.lock == Lock::kUnlocked);
    s.person(true);
    s.step();
    s.person(false);
    for (int i = 0; i < arm - 1 + s.p.P_cat_hits - 1; ++i) s.step();
    CHECK(s.out.lock == Lock::kUnlocked);
    s.step();
    CHECK(s.out.lock == Lock::kLockedCat);
  }
  {
    Sim s;  // 잠긴 뒤 고양이만 사라져도 사람이 다시 보일 때까지 풀리지 않는다 — presence=present가 남아 있어도
    s.warm_up();
    s.person(false);
    s.cat(true);
    s.run(s.p.P_cat_absent_s + s.p.P_cat_hits * 0.5 + 0.5);
    CHECK(s.out.lock == Lock::kLockedCat);
    s.cat(false);
    s.run(s.p.P_cat_cooldown_s + 1.0);
    CHECK(s.out.presence == Presence::kPresent);  // 떠난 지 P_absent_s가 안 됐다
    CHECK(s.out.lock == Lock::kLockedCat);
    s.person(true);
    s.run(s.p.P_cat_cooldown_s - 0.5);
    CHECK(s.out.lock == Lock::kLockedCat);
    s.run(1.0);
    CHECK(s.out.lock == Lock::kUnlocked);
    // 다시 잠그기 (정인 10/26): 사람이 있는 동안 고양이가 와도 그대로, 사람이 떠나면 같은 시간 뒤 다시 잠근다
    s.cat(true);
    s.run(s.p.P_cat_hits * 2.0);
    CHECK(s.out.lock == Lock::kUnlocked);
    s.person(false);
    s.run(s.p.P_cat_absent_s + s.p.P_cat_hits * 0.5 + 0.5);
    CHECK(s.out.lock == Lock::kLockedCat);
  }
}

// ───────────────────────── 불변식 ⑥ 어떤 단일 고장에서도 팬은 올라가고 인덕션은 내려간다 ─────────────────────────
void inv6_single_fault() {
  for (int which = 0; which < 2; ++which) {
    Sim s;  // 열화상 / SPS30 정지
    s.warm_up();
    s.ui(CommandKind::kIndSet, 2);
    s.step();
    CHECK(s.out.induction_level == 2);
    (which == 0 ? s.thermal_ok : s.pm_ok) = false;
    s.run(s.p.P_fault_s - 1.0);
    CHECK(s.out.hazard != Hazard::kFault);
    s.run(2.0);
    CHECK(s.out.hazard == Hazard::kFault);
    CHECK(s.out.fan_level == kLevelMax && s.out.induction_level == 0 && s.out.lock == Lock::kLockedDanger);
  }
  {
    Sim s;  // 부팅 때 센서가 하나도 안 오면 유예 뒤 fault (M11 실패 경로)
    s.thermal_ok = false;
    s.pm_ok = false;
    s.run(s.p.P_fault_s - 0.5);
    CHECK(s.out.hazard != Hazard::kFault);
    CHECK(s.out.fan_level == kLevelMax);  // 유예 중에도 팬을 내리지 않음 — ESP32 부팅 fail-safe 3단 유지 (#79 정인 ①)
    s.voice(CommandKind::kIndOn);         // 유예 중 켜기는 HAZARD로 거부 (#79 후속)
    s.step();
    CHECK(!s.out.allowed_on && s.out.induction_level == 0);
    CHECK(s.out.voice && !s.out.voice->accepted && s.out.voice->reason == RejectReason::kHazard);
    s.run(0.5);
    CHECK(s.out.hazard == Hazard::kFault && s.out.lock == Lock::kLockedDanger);
  }
  {
    Sim s;  // 팬 ESP32 무응답: 사유 표시, 팬 명령을 낮추지 않음 (F5)
    s.warm_up();
    s.metric(s.p.P_T_mid + 1.0);
    s.fan_ack = false;
    s.step();
    CHECK(has(s.out.reasons, Reason::kFanNoAck));
    CHECK(s.out.fan_level >= s.p.P_fan_min_mid);
  }
  {
    Sim s;  // 쿡탑 ESP32 무응답: 켜기 거부, 끄기는 집행 (F6)
    s.warm_up();
    s.ui(CommandKind::kIndSet, 1);
    s.step();
    s.ind_ack = false;
    s.voice(CommandKind::kIndUp);
    s.step();
    CHECK(!s.out.allowed_on);
    CHECK(s.out.voice && s.out.voice->reason == RejectReason::kIndNoAck && s.out.induction_level == 1);
    s.voice(CommandKind::kIndOff);
    s.step();
    CHECK(s.out.voice && s.out.voice->accepted && s.out.induction_level == 0);
  }
  {
    Sim s;  // vision 끊김: 경고 이상이면 부재처럼 팬 3 (F3)
    s.warm_up();
    s.metric(s.p.P_T_high + 1.0);
    s.vision_on = false;
    s.run(s.p.P_vision_fresh_s + 0.5);
    CHECK(s.out.fan_min == kLevelMax);
  }
}

// 불변식 ⑥ — fault 해제는 직전 단계에서 다시 시작한다 (#65 Q3 실측: 연기 중 SPS30 공백이 풀리는 tick에 normal·팬 0)
void inv6_fault_exit_resumes_pre_fault_hazard() {
  {
    Sim s;  // 화재위험 중 공백 → 고농도라 2.5 s마다 샘플 1개만 들어옴: 해제 tick에 과한 연기 카운트가 비어 있음
    s.warm_up();
    s.fire();
    CHECK(s.out.hazard == Hazard::kDanger);
    s.pm_ok = false;
    s.run(s.p.P_fault_s + 1.0);
    CHECK(s.out.hazard == Hazard::kFault);
    bool left_fault = false;
    for (int i = 0; i < ticks(20.0); ++i) {
      s.pm_ok = i % 5 == 0;
      s.step();
      left_fault = left_fault || s.out.hazard != Hazard::kFault;
      // normal·팬 0으로 떨어지지 않는다. 고온이 남아 있으면 danger 뒤 warning으로 내려오지만 잠금·인덕션 0은 유지
      CHECK(s.out.hazard >= Hazard::kWarning && s.out.lock == Lock::kLockedDanger);
      CHECK(s.out.fan_level == kLevelMax && s.out.induction_level == 0);
    }
    CHECK(left_fault);  // 실제로 해제 경로를 지났다
  }
  {
    Sim s;  // 직전이 normal이면 지금처럼 P_fault_s 정상 뒤 바로 normal (F4, #64 기준 그대로)
    s.warm_up();
    s.pm_ok = false;
    s.run(s.p.P_fault_s + 1.0);
    CHECK(s.out.hazard == Hazard::kFault);
    s.pm_ok = true;
    s.run(s.p.P_fault_s - 0.5);
    CHECK(s.out.hazard == Hazard::kFault);
    s.run(1.0);
    CHECK(s.out.hazard == Hazard::kLow && s.out.alert == Alert::kOfflineRecover);
  }
  {
    Sim s;  // 공백 동안 불·연기가 사라졌으면 danger에서 시작해 P_release_danger_s 뒤 내려옴
    s.warm_up();
    s.fire();
    s.pm_ok = false;
    s.run(s.p.P_fault_s + 1.0);
    s.extinguish();
    s.pm_ok = true;
    int t = 0;
    while (s.out.hazard == Hazard::kFault && t < ticks(10.0)) {
      s.step();
      ++t;
    }
    CHECK(s.out.hazard == Hazard::kDanger && s.out.alert == Alert::kOfflineRecover);
    int hold = 0;
    while (s.out.hazard == Hazard::kDanger && hold < ticks(30.0)) {
      s.step();
      ++hold;
    }
    CHECK(s.out.hazard == Hazard::kLow);
    CHECK(hold >= ticks(s.p.P_release_danger_s) - 1 && hold <= ticks(s.p.P_release_danger_s) + 1);
  }
}

// ───────────────────────── 불변식 ⑦ 범위 밖 명령은 상태를 바꾸지 않음 ─────────────────────────
// (JSON·범위·allowlist 검증과 폐기 카운터는 Core 파서 #47 몫. 여기서는 Policy까지 온 명령이 해당 상태가 아닐 때를 본다.)
void inv7_out_of_scope_commands() {
  Sim s;
  s.warm_up();
  const Outputs before = s.out;
  s.ui(CommandKind::kAckAlert);  // 잠금이 없는데 ACK
  s.step();
  CHECK(s.out.lock == before.lock && s.out.alert == Alert::kNone && s.out.reasons == before.reasons);
  s.voice(CommandKind::kNone);  // 지원하지 않는 문장
  s.step();
  CHECK(s.out.voice && !s.out.voice->accepted && s.out.voice->reason == RejectReason::kNoIntent);
  CHECK(s.out.alert == Alert::kNone);
  CHECK(s.out.induction_level == before.induction_level && s.out.fan_requested == before.fan_requested);
}

// ───────────────────────── 불변식 ⑧ 쿡탑은 시뮬레이터 ─────────────────────────
// 표현(UI·보고서 문구)은 Policy 밖이다. Policy 수준에서는 "인덕션은 명령으로만 올라가고 스스로 올리지 않는다"를 본다.
void inv8_induction_rises_only_by_command() {
  Sim s;
  s.warm_up();
  s.ui(CommandKind::kIndSet, 3);
  s.step();
  const auto watch = [&](double seconds) {
    for (int i = 0; i < ticks(seconds); ++i) {
      const bool had_command = !s.pending.empty();
      const int before = s.out.induction_level;
      s.step();
      if (s.out.induction_level > before) CHECK(had_command);
    }
  };
  s.metric(s.p.P_T_high + 1.0);
  watch(3);
  s.metric(2.0);
  watch(s.p.P_release_s + 2);
  s.smoke_air();
  watch(5);
  s.clean_air();
  watch(s.p.P_release_danger_s + 2);
  s.person(false);
  s.cat(true);
  watch(s.p.P_absent_s + 5);
  CHECK(s.out.induction_level == 0);
}

// ───────────────────────── D-040 ④ 위험 잠금 해제 = 해소 ∧ ACK ∧ 재실 ─────────────────────────
void lock_release_needs_all_three() {
  Sim s;
  s.warm_up();
  s.fire();
  CHECK(s.out.lock == Lock::kLockedDanger);
  s.ui(CommandKind::kAckAlert);  // 위험이 남아 있을 때 ACK → 무시
  s.step();
  s.extinguish();
  s.run(s.p.P_release_danger_s + 2.0);
  CHECK(s.out.hazard <= Hazard::kMid);
  CHECK(has(s.out.reasons, Reason::kAckWait) && !has(s.out.reasons, Reason::kUnlockWait));

  s.vision_on = false;  // 재실이 아닐 때 ACK → 래치만, 해제 안 됨
  s.run(s.p.P_vision_fresh_s + 0.5);
  s.ui(CommandKind::kAckAlert);
  s.step();
  CHECK(has(s.out.reasons, Reason::kUnlockWait));
  s.run(s.p.P_lock_release_s + 5.0);
  CHECK(s.out.lock == Lock::kLockedDanger);

  s.metric(s.p.P_T_high + 1.0);  // 위험이 다시 오르면 래치 해제 → 새 ACK 필요
  s.step();
  s.metric(2.0);
  s.run(s.p.P_release_s + 1.0);
  CHECK(s.out.hazard <= Hazard::kMid);
  CHECK(has(s.out.reasons, Reason::kAckWait));

  s.vision_on = true;  // 재실 + ACK + 안전 시간 → 해제
  s.person(true);
  s.ui(CommandKind::kAckAlert);
  s.step();
  CHECK(s.out.presence == Presence::kPresent);
  int waited = 0;
  while (s.out.lock == Lock::kLockedDanger && waited < ticks(s.p.P_lock_release_s + 5.0)) {
    s.step();
    ++waited;
  }
  CHECK(s.out.lock == Lock::kUnlocked);
  CHECK(waited <= ticks(s.p.P_lock_release_s));
}

// ───────────────────────── D-058 사용자 잠금: 잠그면 끄고 켜기 거부, 푸는 건 사용자 잠금뿐 ─────────────────────────
void user_lock_d058() {
  {
    Sim s;  // 조리 중 잠금 → 인덕션 0, 켜기 거부(LOCKED), 후드는 그대로 조작. ACK는 사용자 잠금과 무관
    s.warm_up();
    s.ui(CommandKind::kIndSet, 2);
    s.step();
    CHECK(s.out.induction_level == 2);
    s.ui(CommandKind::kUserLock, 1);
    s.step();
    CHECK(s.out.lock == Lock::kLockedUser);
    CHECK(s.out.induction_level == 0 && !s.out.allowed_on);
    CHECK(!s.out.lock_release_left_s);
    s.ui(CommandKind::kIndSet, 1);
    s.voice(CommandKind::kIndOn);
    s.step();
    CHECK(s.out.induction_level == 0);
    CHECK(s.out.voice && !s.out.voice->accepted && s.out.voice->reason == RejectReason::kLocked);
    CHECK(s.out.alert == Alert::kCmdRejected);
    s.ui(CommandKind::kAckAlert);
    s.ui(CommandKind::kFanSet, 2);
    s.step();
    CHECK(s.out.lock == Lock::kLockedUser);
    CHECK(s.out.fan_requested == 2 && s.out.fan_level == 2);
    s.ui(CommandKind::kUserLock, 0);  // 풀면 다시 켤 수 있다 — 스스로 켜지지는 않는다(불변식 ⑧)
    s.step();
    CHECK(s.out.lock == Lock::kUnlocked && s.out.allowed_on && s.out.induction_level == 0);
    s.ui(CommandKind::kIndSet, 1);
    s.step();
    CHECK(s.out.induction_level == 1);
  }
  {
    Sim s;  // 위험 잠금이 우선. 사용자 해제(USER_LOCK 0)로는 위험 잠금이 안 풀린다(불변식 ⑨)
    s.warm_up();
    s.ui(CommandKind::kUserLock, 1);
    s.step();
    s.fire();
    CHECK(s.out.lock == Lock::kLockedDanger);
    s.ui(CommandKind::kUserLock, 0);
    s.step();
    CHECK(s.out.lock == Lock::kLockedDanger);
    s.extinguish();
    s.run(s.p.P_release_danger_s + s.p.P_lock_release_s + 3.0);  // ACK 없이 안전 시간만 → 그대로 잠김
    CHECK(s.out.lock == Lock::kLockedDanger && has(s.out.reasons, Reason::kAckWait));
    int t = 0;
    while (s.out.lock == Lock::kLockedDanger && t < ticks(30)) {
      if (has(s.out.reasons, Reason::kAckWait)) s.ui(CommandKind::kAckAlert);
      s.step();
      ++t;
    }
    CHECK(s.out.lock == Lock::kUnlocked && s.out.allowed_on);  // 사용자 잠금은 위에서 풀었으니 바로 쓸 수 있다
  }
  {
    Sim s;  // 사용자 잠금을 둔 채 위험 잠금이 풀리면 사용자 잠금이 다시 보인다
    s.warm_up();
    s.ui(CommandKind::kUserLock, 1);
    s.step();
    s.fire();
    s.extinguish();
    int t = 0;
    while (s.out.lock == Lock::kLockedDanger && t < ticks(60)) {
      if (has(s.out.reasons, Reason::kAckWait)) s.ui(CommandKind::kAckAlert);
      s.step();
      ++t;
    }
    CHECK(s.out.lock == Lock::kLockedUser && s.out.induction_level == 0 && !s.out.allowed_on);
  }
  {
    Sim s;  // 반려동물 잠금도 우선(CAT_LOCK 알림). 복귀·cooldown 뒤에는 사용자 잠금으로 돌아간다
    s.warm_up();
    s.ui(CommandKind::kUserLock, 1);
    s.step();
    s.leave();
    s.cat(true);
    for (int i = 0; i < s.p.P_cat_hits; ++i) s.step();
    CHECK(s.out.lock == Lock::kLockedCat && s.out.alert == Alert::kCatLock);
    s.cat(false);
    s.person(true);
    s.run(s.p.P_cat_cooldown_s + 1.0);
    CHECK(s.out.lock == Lock::kLockedUser);
  }
}

// ───────────────────────── #116 state v2: 잠금 해제 남은 초 (표시용, 판정은 7단계) ─────────────────────────
void lock_release_left_seconds() {
  Sim s;
  s.warm_up();
  CHECK(!s.out.lock_release_left_s);  // unlocked → null
  s.fire();
  CHECK(s.out.lock == Lock::kLockedDanger && !s.out.lock_release_left_s);  // 위험 중 → null
  s.extinguish();
  int waited = 0;
  while (s.out.hazard > Hazard::kMid && waited++ < ticks(s.p.P_release_danger_s + 5.0)) s.step();
  CHECK(s.out.hazard <= Hazard::kMid);
  CHECK(s.out.lock_release_left_s && *s.out.lock_release_left_s == static_cast<int>(s.p.P_lock_release_s));  // 첫 tick: 10
  s.run(1.0);
  CHECK(*s.out.lock_release_left_s == static_cast<int>(s.p.P_lock_release_s) - 1);  // 1 s 뒤: 9

  s.metric(s.p.P_T_high + 1.0);  // 중간에 중불(mid)을 넘으면 null로 돌아가고 다시 10부터
  s.step();
  CHECK(s.out.hazard == Hazard::kHigh && !s.out.lock_release_left_s);
  s.metric(2.0);
  waited = 0;
  while (s.out.hazard > Hazard::kMid && waited++ < ticks(s.p.P_release_s + 5.0)) s.step();
  CHECK(*s.out.lock_release_left_s == static_cast<int>(s.p.P_lock_release_s));

  // ACK 없이 시간만 차면 0에서 멈추고 잠금은 남는다(불변식 ⑨)
  waited = 0;
  while (*s.out.lock_release_left_s > 0 && waited++ < ticks(s.p.P_lock_release_s + 5.0)) s.step();
  CHECK(*s.out.lock_release_left_s == 0 && s.out.lock == Lock::kLockedDanger);
  CHECK(waited >= ticks(s.p.P_lock_release_s) - 1 && waited <= ticks(s.p.P_lock_release_s) + 1);
  s.run(2.0);
  CHECK(*s.out.lock_release_left_s == 0 && s.out.lock == Lock::kLockedDanger);

  // 0에서 ACK ∧ 재실 → 같은 tick에 해제, 값은 null
  CHECK(s.out.presence == Presence::kPresent);
  s.ui(CommandKind::kAckAlert);
  s.step();
  CHECK(s.out.lock == Lock::kUnlocked && !s.out.lock_release_left_s);

  s.leave();  // locked_cat → null
  s.cat(true);
  s.run(s.p.P_cat_hits * 0.5 + 1.0);
  CHECK(s.out.lock == Lock::kLockedCat && !s.out.lock_release_left_s);
}

// ───────────────────────── 히스테리시스 B (#57 STEP 3 PM 결정) ─────────────────────────
void hysteresis_direct_release() {
  {
    Sim s;  // warning → normal 직행 (caution을 거치지 않음)
    s.warm_up();
    s.metric(s.p.P_T_high + 1.0);
    s.step();
    s.metric(2.0);
    s.run(s.p.P_release_s - 0.5);
    CHECK(s.out.hazard == Hazard::kHigh && has(s.out.reasons, Reason::kHold));
    s.run(1.0);
    CHECK(s.out.hazard == Hazard::kLow && !has(s.out.reasons, Reason::kHold));
  }
  {
    Sim s;  // 대기 중 mid·low가 섞이면 가장 높은 mid에 착지
    s.warm_up();
    s.metric(s.p.P_T_high + 1.0);
    s.step();
    for (int i = 0; i < ticks(s.p.P_release_s) + 1; ++i) {
      s.metric(i % 2 ? s.p.P_T_mid + 1.0 : 2.0);
      s.step();
    }
    CHECK(s.out.hazard == Hazard::kMid);
  }
  {
    Sim s;  // danger에서는 P_release_danger_s
    s.warm_up();
    s.fire();
    s.extinguish();
    s.run(s.p.P_release_s + 1.0);
    CHECK(s.out.hazard == Hazard::kDanger);
    s.run(s.p.P_release_danger_s - s.p.P_release_s);
    CHECK(s.out.hazard == Hazard::kLow);
  }
}

// ───────────────────────── 설계서 §7 M1~M13 ─────────────────────────
void m1_cooking_start() {
  Sim s;
  s.warm_up();
  s.voice(CommandKind::kIndOn);
  s.step();
  CHECK(s.out.hazard == Hazard::kLow && s.out.presence == Presence::kPresent && s.out.lock == Lock::kUnlocked);
  CHECK(s.out.induction_level == 1 && s.out.fan_level == s.p.P_fan_min_cooking);
}

void m2_caution() {
  Sim s;
  s.warm_up();
  s.metric(s.p.P_T_mid + 0.5);
  s.step();
  CHECK(s.out.hazard == Hazard::kMid && s.out.fan_min == s.p.P_fan_min_mid);
  CHECK(s.out.alert == Alert::kNone);  // 중불은 정상 조리 — 경고음 없음 (D-059)
  s.run(s.p.P_alert_hold_s + 0.5);
  CHECK(s.out.alert == Alert::kNone);
}

// D-054: 강불(warning)은 정상 조리 — 재실 중에는 인덕션을 내리지 않고 팬만 P_fan_min_high. 자리를 비우면 1단·팬 3.
void m3_strong_heat_keeps_induction() {
  Sim s;
  s.warm_up();
  s.ui(CommandKind::kIndSet, 2);
  s.step();
  s.metric(s.p.P_T_high + 0.5);
  s.step();
  CHECK(s.out.hazard == Hazard::kHigh && s.out.alert == Alert::kNone);  // 강불은 정상 조리 — 경고음 없음 (D-059)
  CHECK(s.out.induction_level == 2 && s.out.fan_min == s.p.P_fan_min_high);
  s.leave();
  CHECK(s.out.fan_min == kLevelMax && s.out.induction_level == s.p.P_ind_max_absent);
}

// D-059: 약불·중불·강불은 low·mid·high(정상 조리, 경고음 없음). warning은 과열·과한 연기 단독이고 WARNING·팬 3
void heat_tiers_d059() {
  {
    Sim s;  // 강불(high)에서 과한 연기가 붙으면 warning으로 오르며 WARNING 한 번, 팬 3
    s.warm_up();
    s.ui(CommandKind::kIndSet, 2);
    s.step();
    s.metric(s.p.P_T_high + 0.5);
    s.step();
    CHECK(s.out.hazard == Hazard::kHigh && s.out.alert == Alert::kNone);
    CHECK(s.out.allowed_on);  // 재실이면 강불에서도 켜기·올리기 허용 (D-061, 10/26 「강불에서 막음」을 바꿈)
    s.saturated_air(1.2);
    bool warned = false;
    for (int i = 0; i < ticks(s.p.P_heavy_smoke_samples + 3.0); ++i) {
      s.step();
      warned = warned || s.out.alert == Alert::kWarning;
    }
    CHECK(s.out.hazard == Hazard::kWarning && has(s.out.reasons, Reason::kSmoke));
    CHECK(warned && s.out.fan_min == kLevelMax);
    CHECK(s.out.induction_level == 2);  // 재실 중에는 warning에서도 인덕션을 건드리지 않는다 (D-054)
    s.ui(CommandKind::kIndSet, 3);      // 재실이면 과한 연기 warning에서도 올리기 집행 (D-061)
    s.step();
    CHECK(s.out.allowed_on && s.out.induction_level == 3);
  }
  {
    Sim s;  // 65 °C급 단독(과열)은 바로 warning·WARNING
    s.warm_up();
    s.metric(s.p.P_T_danger + 1.0);
    s.step();
    CHECK(s.out.hazard == Hazard::kWarning && s.out.alert == Alert::kWarning);
    CHECK(has(s.out.reasons, Reason::kTempDanger) && !has(s.out.reasons, Reason::kTempHigh));
  }
  {
    Sim s;  // 강불 열이 남아 있으면 위험 잠금이 안 풀린다 — 해제는 중불(mid) 이하에서만 (불변식 ⑨)
    s.warm_up();
    s.fire();
    CHECK(s.out.lock == Lock::kLockedDanger);
    s.clean_air();
    s.last_pm_ms = 0;
    s.metric(s.p.P_T_high + 0.5);
    int t = 0;
    while (t < ticks(s.p.P_release_danger_s + s.p.P_lock_release_s + 10.0)) {
      s.ui(CommandKind::kAckAlert);  // 사용자가 계속 「위험 확인」을 눌러도
      s.step();
      ++t;
    }
    CHECK(s.out.hazard == Hazard::kHigh && s.out.lock == Lock::kLockedDanger);
    CHECK(!has(s.out.reasons, Reason::kAckWait) && !s.out.lock_release_left_s);
  }
}

void m4_voice_fan() {
  Sim s;
  s.warm_up();
  for (int v = 3; v >= 1; --v) {
    s.voice(CommandKind::kFanSet, v);
    s.step();
    CHECK(s.out.fan_requested == v && s.out.fan_level == v);
  }
  s.metric(s.p.P_T_mid + 1.0);
  s.voice(CommandKind::kFanOff);
  s.step();
  CHECK(s.out.fan_requested == 0 && s.out.fan_level == s.p.P_fan_min_mid);  // 하한에 눌림 (requested ≠ level)
}

void voice_absolute_induction() {
  Sim s;
  s.warm_up();
  for (int level : {1, 3, 2}) {
    s.voice(CommandKind::kIndSet, level);
    s.step();
    CHECK(s.out.induction_level == level && s.out.voice && s.out.voice->accepted);
    CHECK(s.out.voice->value == level);
  }
  s.person(false);
  s.run(s.p.P_absent_s + 1);
  s.voice(CommandKind::kIndSet, 3);
  s.step();
  CHECK(s.out.voice && !s.out.voice->accepted && s.out.voice->reason == RejectReason::kAbsent);
  s.voice(CommandKind::kIndSet, 1);
  s.step();
  CHECK(s.out.induction_level == 1 && s.out.voice->accepted);
  s.person(true);
  s.step();
  s.ind_ack = false;
  s.voice(CommandKind::kIndSet, 3);
  s.step();
  CHECK(!s.out.voice->accepted && s.out.voice->reason == RejectReason::kIndNoAck);
  s.ind_ack = true;
  s.ui(CommandKind::kUserLock, 1);
  s.step();
  s.voice(CommandKind::kIndSet, 2);
  s.step();
  CHECK(s.out.induction_level == 0 && !s.out.voice->accepted && s.out.voice->reason == RejectReason::kLocked);
  Sim hot;
  hot.warm_up();
  hot.metric(hot.p.P_T_high + 1);
  hot.voice(CommandKind::kIndSet, 3);
  hot.step();
  CHECK(hot.out.voice && hot.out.voice->accepted && hot.out.induction_level == 3);
}

void m5_voice_induction() {
  Sim s;
  s.warm_up();
  s.voice(CommandKind::kIndOn);
  s.step();
  s.voice(CommandKind::kIndUp);
  s.step();
  CHECK(s.out.induction_level == 2);
  s.metric(s.p.P_T_high + 1.0);
  s.step();
  s.voice(CommandKind::kIndUp);  // 재실이면 강불에서도 올리기 집행 (D-061)
  s.step();
  CHECK(s.out.voice && s.out.voice->accepted && s.out.induction_level == 3);
  s.voice(CommandKind::kIndDown);  // 내리기는 항상. 강불은 자동으로 내리지 않으므로 3 → 2 (D-054)
  s.step();
  CHECK(s.out.voice && s.out.voice->accepted && s.out.induction_level == 2);
}

// D-061: 재실이면 warning(과열·과한 연기 단독)에서도 켜기·올리기를 집행한다. 부재·danger는 그대로 막는다.
void warning_raise_d061() {
  {
    Sim s;  // 정인 E2E 1회차 M3: 과열 중 사람을 놓쳐 1단 → 돌아오면 다시 올릴 수 있어야 한다
    s.warm_up();
    s.ui(CommandKind::kIndSet, 2);
    s.step();
    s.metric(s.p.P_T_danger + 1.0);
    s.step();
    CHECK(s.out.hazard == Hazard::kWarning && s.out.allowed_on);
    s.voice(CommandKind::kIndUp);
    s.step();
    CHECK(s.out.voice && s.out.voice->accepted && s.out.induction_level == 3);
    s.leave();  // 부재면 1단으로 내리고 올리기는 ABSENT로 거부 (P_ind_max_absent)
    CHECK(!s.out.allowed_on && s.out.induction_level == s.p.P_ind_max_absent);
    s.voice(CommandKind::kIndUp);
    s.step();
    CHECK(s.out.voice && !s.out.voice->accepted && s.out.voice->reason == RejectReason::kAbsent);
    CHECK(s.out.induction_level == s.p.P_ind_max_absent);
    s.person(true);  // 돌아오면 warning이 남아 있어도 다시 올릴 수 있다
    s.run(1.0);
    CHECK(s.out.presence == Presence::kPresent && s.out.hazard == Hazard::kWarning && s.out.allowed_on);
    s.ui(CommandKind::kIndSet, 3);
    s.step();
    CHECK(s.out.induction_level == 3);
  }
  {
    Sim s;  // danger(65 °C급 ∧ 과한 연기)는 위험 잠금 — 켜기는 LOCKED로 거부, 인덕션 0
    s.warm_up();
    s.fire();
    CHECK(s.out.hazard == Hazard::kDanger && !s.out.allowed_on);
    s.voice(CommandKind::kIndOn);
    s.step();
    CHECK(s.out.voice && !s.out.voice->accepted && s.out.voice->reason == RejectReason::kLocked);
    CHECK(s.out.induction_level == 0);
  }
}

// D-054 화재위험: 65 °C급 고온(warning·팬 3) 위에 과한 연기가 이어지면 danger — 인덕션 0·잠금·팬 3
void m6_fire_locks() {
  Sim s;
  s.warm_up();
  s.ui(CommandKind::kIndSet, 2);
  s.step();
  s.metric(s.p.P_T_danger + 1.0);
  s.step();
  CHECK(s.out.hazard == Hazard::kWarning && s.out.induction_level == 2 && s.out.fan_level == kLevelMax);
  s.saturated_air(1.2);
  int t = 0;
  while (s.out.hazard != Hazard::kDanger && t < ticks(60)) {
    s.step();
    ++t;
  }
  CHECK(s.out.hazard == Hazard::kDanger && has(s.out.reasons, Reason::kSmoke) && has(s.out.reasons, Reason::kTempDanger));
  CHECK(t <= ticks(s.p.P_heavy_smoke_samples + 1.0));  // SPS30 1 Hz 기준 포화 샘플 P_heavy_smoke_samples개
  CHECK(s.out.induction_level == 0 && s.out.lock == Lock::kLockedDanger && s.out.fan_level == kLevelMax);
  s.voice(CommandKind::kIndOn);
  s.step();
  CHECK(s.out.voice && s.out.voice->reason == RejectReason::kLocked);
}

void m7_steam_is_not_smoke() {
  Sim s;
  s.warm_up();
  s.steam_air();
  for (int i = 0; i < ticks(30); ++i) {
    s.step();
    CHECK(s.out.hazard <= Hazard::kMid);
    CHECK(!has(s.out.reasons, Reason::kSmoke));
  }
}

// #79 정인 ②: 증기가 길게 이어져도 baseline이 따라 올라가 뒤이은 연기를 놓치면 안 된다
void m7_steam_then_smoke() {
  Sim s;
  s.warm_up();
  s.steam_air();
  s.run(120.0);
  CHECK(s.out.hazard <= Hazard::kMid);
  s.smoke_air();
  int t = 0;
  while (!has(s.out.reasons, Reason::kPmWarn) && t < ticks(30)) {
    s.step();
    ++t;
  }
  CHECK(has(s.out.reasons, Reason::kPmWarn));  // baseline이 증기를 따라가지 않아 뒤이은 연기가 보인다
  CHECK(t <= ticks(s.p.P_smoke_samples + 1.0));
}

// D-054: 부재 ∧ 중불 이상 → 팬 3·인덕션 1단 (10/22 ③을 「경고」에서 「중불 이상」으로)
void m8_unattended_warning() {
  Sim s;
  s.warm_up();
  s.ui(CommandKind::kIndSet, 3);
  s.step();
  s.metric(s.p.P_T_mid + 0.5);
  s.person(false);
  s.run(s.p.P_absent_s - 1.0);
  CHECK(s.out.presence == Presence::kPresent && s.out.fan_min == s.p.P_fan_min_mid && s.out.induction_level == 3);
  s.run(1.5);
  CHECK(s.out.presence == Presence::kAbsent && s.out.fan_min == kLevelMax);
  CHECK(s.out.induction_level == s.p.P_ind_max_absent && has(s.out.reasons, Reason::kAbsent));
}

void m10_recover_and_unlock() {
  Sim s;
  s.warm_up();
  s.fire();
  CHECK(s.out.lock == Lock::kLockedDanger);
  s.extinguish();
  int t = 0;
  while (s.out.lock != Lock::kUnlocked && t < ticks(60)) {
    if (has(s.out.reasons, Reason::kAckWait)) s.ui(CommandKind::kAckAlert);  // 화면 "위험 확인"
    s.step();
    ++t;
  }
  CHECK(s.out.lock == Lock::kUnlocked && s.out.hazard == Hazard::kLow);
  // 불·연기를 치운 뒤 hazard 해제(P_release_danger_s) + 잠금 안전 시간(P_lock_release_s), SPS30 1 s 여유
  CHECK(t >= ticks(s.p.P_release_danger_s + s.p.P_lock_release_s));
  CHECK(t <= ticks(s.p.P_release_danger_s + s.p.P_lock_release_s + 1.5));
}

void m11_reboot() {
  {
    Sim s;  // 재부팅 직후 센서가 유예 안에 올라오면 unlocked로 시작
    s.run(s.p.P_fault_s + 1.0);
    CHECK(s.out.hazard == Hazard::kLow && s.out.lock == Lock::kUnlocked);
  }
  {
    Sim s;  // 한 센서만 먼저 올라오면 다른 센서를 한 번 읽을 때까지 켜기 거부, 읽은 뒤 바로 허용 (#79 후속)
    s.pm_ok = false;
    s.run(s.p.P_fault_s - 1.0);
    CHECK(s.out.presence == Presence::kPresent && !s.out.allowed_on && s.out.fan_min == kLevelMax);
    s.ui(CommandKind::kIndSet, 1);
    s.step();
    CHECK(s.out.induction_level == 0 && s.out.alert == Alert::kCmdRejected);
    s.pm_ok = true;
    s.step();
    CHECK(s.out.hazard == Hazard::kLow && s.out.lock == Lock::kUnlocked && s.out.allowed_on);
    s.ui(CommandKind::kIndSet, 1);
    s.step();
    CHECK(s.out.induction_level == 1);
  }
  {
    Sim s;  // 늦게 올라오면 fault → 복구 alert, 잠금은 ACK·재실 전까지 유지
    s.thermal_ok = false;
    s.run(s.p.P_fault_s + 1.0);
    CHECK(s.out.hazard == Hazard::kFault);
    s.thermal_ok = true;
    s.run(s.p.P_fault_s - 0.5);
    CHECK(s.out.hazard == Hazard::kFault);
    s.run(1.0);
    CHECK(s.out.hazard == Hazard::kLow && s.out.alert == Alert::kOfflineRecover);
    CHECK(s.out.lock == Lock::kLockedDanger && has(s.out.reasons, Reason::kAckWait));
  }
}

void m12_short_absence() {
  Sim s;
  s.warm_up();
  const Outputs before = s.out;
  s.person(false);
  for (int i = 0; i < ticks(s.p.P_absent_s - 10.0); ++i) {
    s.step();
    CHECK(s.out.presence == Presence::kPresent && s.out.alert == Alert::kNone);
    CHECK(s.out.fan_level == before.fan_level && !has(s.out.reasons, Reason::kAbsent));
  }
  s.person(true);
  s.step();
  CHECK(s.out.presence == Presence::kPresent);
}

void m13_lan_loss() {
  Sim s;
  s.warm_up();
  s.vision_on = false;
  s.metric(s.p.P_T_mid + 0.5);
  s.run(3.0);
  CHECK(s.out.presence == Presence::kUnknown && s.out.hazard == Hazard::kMid);  // 열 판정은 계속
  CHECK(s.out.fan_min == kLevelMax && has(s.out.reasons, Reason::kVisionStale));  // unknown은 부재처럼 (D-054 중불 이상)
  s.vision_on = true;
  s.step();
  CHECK(s.out.presence == Presence::kPresent && s.out.fan_min == s.p.P_fan_min_mid);
}

// ───────────────────────── SPS30 CSV 재생 (#57 ③, [실측 PR #40]) ─────────────────────────
struct Sample {
  double sec, pm25, tsize;
};

std::vector<Sample> load_csv(const std::string& name) {
  std::ifstream f(std::string(HOODSEEK_TEST_DATA) + "/" + name);
  std::vector<Sample> rows;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#' || line[0] == 's') continue;  // 출처 주석·헤더
    Sample s{};
    char comma = 0;
    std::istringstream in(line);
    in >> s.sec >> comma >> s.pm25 >> comma >> s.tsize;
    rows.push_back(s);
  }
  return rows;
}

struct Replay {
  double first_smoke_s = -1, first_pm_warn_s = -1, first_fault_s = -1;
  Hazard max_hazard = Hazard::kLow;
};

// 샘플은 CSV 시각 그대로(간격도 그대로) 넣고, tick은 500 ms. 열화상·vision은 정상·재실로 둔다.
Replay replay(const std::string& name) {
  const std::vector<Sample> rows = load_csv(name);
  Replay r;
  CHECK(!rows.empty());
  if (rows.empty()) return r;
  Policy policy(g_profile);
  const std::uint64_t t0 = 100000;
  std::size_t next = 0;
  std::uint32_t seq = 0;
  std::uint64_t pm_ok_ms = 0;
  double pm25 = 0, tsize = 0;
  for (int k = 0; k <= ticks(rows.back().sec + 15.0); ++k) {
    const double t = k * 0.5;
    while (next < rows.size() && rows[next].sec <= t) {
      pm25 = rows[next].pm25;
      tsize = rows[next].tsize;
      pm_ok_ms = t0 + to_ms(rows[next].sec);
      ++seq;
      ++next;
    }
    Inputs in{};
    in.now_ms = t0 + to_ms(t);
    in.thermal_ok_ms = in.now_ms;
    in.t_max_c = kAmbientC + 2.0;
    in.t_median_c = kAmbientC;
    in.pm_ok_ms = pm_ok_ms;
    in.pm_seq = seq;
    in.pm25 = pm25;
    in.tsize = tsize;
    in.vision_rx_ms = in.now_ms;
    in.vision_msgs.push_back({true, 1.0, false, 0.0, 0.0});
    in.fan_ack = in.ind_ack = true;
    const Outputs o = policy.tick(in);
    check_every_tick(o);
    if (o.hazard > r.max_hazard) r.max_hazard = o.hazard;
    if (r.first_smoke_s < 0 && has(o.reasons, Reason::kSmoke)) r.first_smoke_s = t;
    if (r.first_pm_warn_s < 0 && has(o.reasons, Reason::kPmWarn)) r.first_pm_warn_s = t;
    if (r.first_fault_s < 0 && o.hazard == Hazard::kFault) r.first_fault_s = t;
  }
  std::printf("    %-15s max_hazard=%-7s first SMOKE=%6.1fs  first PM_WARN=%6.1fs  first fault=%6.1fs\n", name.c_str(),
              to_string(r.max_hazard), r.first_smoke_s, r.first_pm_warn_s, r.first_fault_s);
  return r;
}

void csv_replay_steam_vs_smoke() {
  // 증기 3종: 연기(SMOKE)로 확정되면 안 된다 (M7, #57 ③).
  // box_mist_1·mist_2는 처음 닿는 2행(tsize < P_tsize)이 P_smoke_samples 연속이 안 돼 PM 경고도 없어야 한다 (#57 STEP 4).
  // mist.csv는 67~69 s 세 샘플이 Δ 49.5/43.0/30.4·tsize 0.474~0.480으로 크기로는 연기와 구분되지 않는다 →
  // P_PM_warn 초기값(30, [추정])으로는 경고가 난다. 수치 문제라 G3(#65)에서 정하고 여기서는 출력만 한다.
  // 고농도 구간 샘플 공백으로 생기는 fault는 D4(#64 실측)로 따로 확인한다.
  for (const char* name : {"box_mist_1.csv", "mist_2.csv"}) {
    const Replay r = replay(name);
    CHECK(r.first_smoke_s < 0);
    CHECK(r.first_pm_warn_s < 0);
  }
  CHECK(replay("mist.csv").first_smoke_s < 0);
  // 인센스: 고농도 구간 샘플 공백(83.7 s → 118.6 s) 전에 연기 있음(PM_WARN)이 떠야 한다. 과한 연기(SMOKE)는
  // 팬 3단·포화 연속이 조건이라 이 재생(열·팬 정상, 포화 표시 없음)에서는 나오지 않는다 (D-054).
  const Replay r = replay("box_incense.csv");
  CHECK(r.first_pm_warn_s > 0 && r.first_pm_warn_s < 83.7);
}

struct Case {
  const char* name;
  void (*fn)();
};

}  // namespace

int main() {
  std::string err;
  if (!load_profile(HOODSEEK_DEFAULT_PROFILE, g_profile, err)) {
    std::printf("profiles/default.json 로드 실패: %s\n", err.c_str());
    return 2;
  }
  const Case cases[] = {
      {"inv1_priority_ladder", inv1_priority_ladder},
      {"inv1_threshold_boundaries", inv1_threshold_boundaries},
      {"pm_warn_needs_consecutive_samples", pm_warn_needs_consecutive_samples},
      {"inv2_fan_floor", inv2_fan_floor},
      {"inv3_vision_expiry", inv3_vision_expiry},
      {"inv4_core_counts_vision_messages", inv4_core_counts_vision_messages},
      {"inv5_cat_needs_absence_m9", inv5_cat_needs_absence_m9},
      {"cat_watch_d060", cat_watch_d060},
      {"inv6_single_fault", inv6_single_fault},
      {"inv6_fault_exit_resumes_pre_fault_hazard", inv6_fault_exit_resumes_pre_fault_hazard},
      {"inv7_out_of_scope_commands", inv7_out_of_scope_commands},
      {"inv8_induction_rises_only_by_command", inv8_induction_rises_only_by_command},
      {"lock_release_needs_all_three", lock_release_needs_all_three},
      {"lock_release_left_seconds", lock_release_left_seconds},
      {"user_lock_d058", user_lock_d058},
      {"hysteresis_direct_release", hysteresis_direct_release},
      {"m1_cooking_start", m1_cooking_start},
      {"m2_caution", m2_caution},
      {"m3_strong_heat_keeps_induction", m3_strong_heat_keeps_induction},
      {"heat_tiers_d059", heat_tiers_d059},
      {"m4_voice_fan", m4_voice_fan},
      {"m5_voice_induction", m5_voice_induction},
      {"voice_absolute_induction", voice_absolute_induction},
      {"warning_raise_d061", warning_raise_d061},
      {"m6_fire_locks", m6_fire_locks},
      {"m7_steam_is_not_smoke", m7_steam_is_not_smoke},
      {"m7_steam_then_smoke", m7_steam_then_smoke},
      {"m8_unattended_warning", m8_unattended_warning},
      {"m10_recover_and_unlock", m10_recover_and_unlock},
      {"m11_reboot", m11_reboot},
      {"m12_short_absence", m12_short_absence},
      {"m13_lan_loss", m13_lan_loss},
      {"saturated_pm_counts_as_smoke", saturated_pm_counts_as_smoke},
      {"heavy_smoke_needs_fan3_and_duration", heavy_smoke_needs_fan3_and_duration},
      {"smoke_raises_fan_floor", smoke_raises_fan_floor},
      {"cooking_tiers_never_lock", cooking_tiers_never_lock},
      {"csv_replay_steam_vs_smoke", csv_replay_steam_vs_smoke},
  };
  for (const Case& c : cases) {
    g_case = c.name;
    const int failed_before = g_failed;
    c.fn();
    std::printf("%s %s\n", g_failed == failed_before ? "ok  " : "FAIL", c.name);
  }
  std::printf("%d cases, %d checks, %d failed\n", static_cast<int>(sizeof(cases) / sizeof(cases[0])), g_checks,
              g_failed);
  return g_failed == 0 ? 0 : 1;
}
