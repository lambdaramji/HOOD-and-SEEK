// policy.cpp — SafetyCore 판정 상태기계 (T2 남현지, #57).
// 규칙: 설계서 §3·§6, #57 STEP 3 댓글 동작 규칙 1~10(PM 결정 2026-10-19). tick 처리 순서도 그 댓글 1번과 같다.
// 판정 수치는 전부 Profile에서 온다. 여기 있는 숫자는 계약 v1의 단수 범위(0..3)와 단위 변환뿐이다.
#include "policy.h"

#include <algorithm>
#include <cmath>

namespace hoodseek {
namespace {

// 계약 v1 팬·인덕션 단수 범위. 판정 임계가 아니라 프로토콜 범위라 Profile에 두지 않는다.
constexpr int kLevelMax = 3;

std::uint64_t to_ms(double seconds) { return static_cast<std::uint64_t>(seconds * 1000.0 + 0.5); }

std::uint64_t age_ms(std::uint64_t now, std::uint64_t at) { return now >= at ? now - at : 0; }

bool held_for(const std::optional<std::uint64_t>& since, std::uint64_t now, double seconds) {
  return since && age_ms(now, *since) >= to_ms(seconds);
}

double median(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  const std::size_t n = v.size();
  return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

// alert 우선순위 (#57 규칙 10). 유지 중에는 더 높은 것만 덮어쓴다.
int alert_rank(Alert a) {
  switch (a) {
    case Alert::kDanger: return 6;
    case Alert::kCatLock: return 5;
    case Alert::kWarning: return 4;
    case Alert::kCaution: return 3;
    case Alert::kOfflineRecover: return 2;
    case Alert::kCmdRejected: return 1;
    case Alert::kNone: break;
  }
  return 0;
}

// 정상 조리(low·mid·high)에는 경고음이 없다 (PM 결정 2026-10-26, D-059). CAUTION 클립은 계약에 남지만 내지 않는다.
Alert alert_for(Hazard h) {
  switch (h) {
    case Hazard::kWarning: return Alert::kWarning;
    case Hazard::kDanger:
    case Hazard::kFault: return Alert::kDanger;
    case Hazard::kLow:
    case Hazard::kMid:
    case Hazard::kHigh: break;
  }
  return Alert::kNone;
}

}  // namespace

Policy::Policy(const Profile& profile) : p_(profile) {}

void Policy::set_profile(const Profile& profile) { p_ = profile; }

Outputs Policy::tick(const Inputs& in) {
  const std::uint64_t now = in.now_ms;
  if (!start_ms_) start_ms_ = now;
  const Hazard prev_hazard = hazard_;
  const Lock prev_lock = lock_;
  Alert event = Alert::kNone;
  const auto raise = [&event](Alert a) {
    if (alert_rank(a) > alert_rank(event)) event = a;
  };

  // 1. 센서 유효·fault (#57 ④, F4). 한 번도 못 읽은 센서는 첫 tick부터 P_fault_s 동안 유예한다.
  const std::uint64_t fault_ms = to_ms(p_.P_fault_s);
  const auto valid = [&](std::uint64_t ok_ms) { return ok_ms != 0 && age_ms(now, ok_ms) < fault_ms; };
  const auto faulted = [&](std::uint64_t ok_ms) {
    return !valid(ok_ms) && (ok_ms != 0 || age_ms(now, *start_ms_) >= fault_ms);
  };
  const bool thermal_valid = valid(in.thermal_ok_ms);
  const bool pm_valid = valid(in.pm_ok_ms);
  thermal_seen_ = thermal_seen_ || thermal_valid;
  pm_seen_ = pm_seen_ || pm_valid;
  if (thermal_valid && pm_valid) {
    if (!sensors_ok_since_ms_) sensors_ok_since_ms_ = now;
  } else {
    sensors_ok_since_ms_.reset();
  }
  // fault에서는 두 센서가 P_fault_s 연속 정상이어야 나온다 ("읽기 정상 3 s 후 재판정").
  const bool fault = faulted(in.thermal_ok_ms) || faulted(in.pm_ok_ms) ||
                     (hazard_ == Hazard::kFault && !held_for(sensors_ok_since_ms_, now, p_.P_fault_s));

  // 2. 새 SPS30 샘플마다 baseline과 PM 후보 (#57 ③⑤, D-054). 초기 구간에는 PM 규칙을 쓰지 않는다(#57 STEP 3).
  //    PM_WARN(연기 있음 표시): 작은 입자 ∧ Δ ≥ P_PM_warn가 새 샘플 P_smoke_samples 연속이면 시작 — 증기가 처음 닿는
  //    2행 오탐 방지(#57 STEP 4). 시작한 뒤에는 Δ만 본다. HEM 인센스는 비포화일 때 tsize ≥ 0.49라 입자 크기로 끊으면
  //    연기가 계속 나는데 표시가 꺼졌다 켜진다(#65 10/24, C2 98 %·E1 84 %). hazard는 바꾸지 않는다.
  //    과한 연기: 팬이 3단인데도 포화 샘플이 P_heavy_smoke_samples 연속. 팬 0~1에서는 인센스 1개도 원값 천장(약 33,000)이라
  //    세기를 못 가르고, 팬 3단에서는 1개 최장 10~12 s · 3개 이상 17~31 s로 갈린다(#65 10/24 실측, PR #122).
  //    확정 뒤 Δ < P_PM_smoke가 되면 풀린다.
  //    포화 지속 → 팬 하한 3(D-056): 팬 단수와 상관없이 포화 샘플이 P_smoke_fan_samples 연속. 팬 1·2단에서 인센스 1개는
  //    포화가 17~27 s, 3개 이상은 106 s 넘게 이어진다(#65 10/25 S). 해제도 Δ < P_PM_smoke가 같은 수만큼 연속일 때 —
  //    연기 중에도 Δ < 50이 최장 19 s(28 cm·5개) 나와서 한 샘플로 풀면 팬이 1분 넘게 약해진다(#138 정인 리뷰, #135).
  if (!pm_valid) {
    pm_warn_run_ = 0;
    pm_warn_on_ = false;
    heavy_run_ = 0;
    heavy_ = false;
    sat_run_ = 0;
    smoke_fan_ = false;
    smoke_clear_run_ = 0;
  }
  if (pm_valid && in.pm_seq != last_pm_seq_) {
    last_pm_seq_ = in.pm_seq;
    if (!baseline_) {
      baseline_window_.push_back(in.pm25);
      if (age_ms(now, *start_ms_) >= to_ms(p_.P_baseline_init_s)) {
        baseline_ = median(baseline_window_);
        baseline_window_.clear();
      }
    } else {
      // 포화(센서 상한 초과) 샘플은 tsize를 믿지 않고 연기형으로 본다(#65 10/23, #110).
      const bool small = in.pm_saturated || in.tsize < p_.P_tsize;
      const double delta = in.pm25 - *baseline_;
      if (pm_warn_on_) {
        pm_warn_on_ = delta >= p_.P_PM_warn;
        pm_warn_run_ = 0;
      } else {
        pm_warn_run_ = small && delta >= p_.P_PM_warn ? pm_warn_run_ + 1 : 0;
        pm_warn_on_ = pm_warn_run_ >= p_.P_smoke_samples;
      }
      if (heavy_) {
        heavy_ = delta >= p_.P_PM_smoke;
        if (!heavy_) heavy_run_ = 0;
      } else {
        heavy_run_ = in.pm_saturated && last_fan_level_ >= kLevelMax ? heavy_run_ + 1 : 0;
        heavy_ = heavy_run_ >= p_.P_heavy_smoke_samples;
      }
      if (smoke_fan_) {
        smoke_clear_run_ = delta < p_.P_PM_smoke ? smoke_clear_run_ + 1 : 0;
        smoke_fan_ = smoke_clear_run_ < p_.P_smoke_fan_samples;
        if (!smoke_fan_) sat_run_ = 0;
      } else {
        sat_run_ = in.pm_saturated ? sat_run_ + 1 : 0;
        smoke_fan_ = sat_run_ >= p_.P_smoke_fan_samples;
        smoke_clear_run_ = 0;
      }
      // "정상 상태에서만 천천히 갱신" (§3.1): hazard ≤ mid(중불)이고 Δ < P_PM_warn일 때만. 입자 크기는 보지 않는다 —
      // 큰 입자(증기)라도 따라 올라가면 뒤이은 연기의 Δ가 기준 아래로 떨어져 놓친다 (#79 정인 ②).
      if (delta < p_.P_PM_warn && hazard_ <= Hazard::kMid && last_pm_sample_ms_ && p_.P_baseline_tau_s > 0) {
        const double dt_s = static_cast<double>(age_ms(now, *last_pm_sample_ms_)) / 1000.0;
        *baseline_ += std::min(1.0, dt_s / p_.P_baseline_tau_s) * (in.pm25 - *baseline_);
      }
    }
    last_pm_sample_ms_ = now;
  }
  const bool pm_ready = pm_valid && baseline_.has_value();
  const bool heavy = pm_ready && heavy_;
  const bool pm_warn = pm_ready && pm_warn_on_;

  // 3. presence (§3.4, §2.4). unknown 구간은 absent 시간에 세지 않는다(불변식 ⑤).
  const bool fresh = in.vision_rx_ms != 0 && age_ms(now, in.vision_rx_ms) < to_ms(p_.P_vision_fresh_s);
  if (fresh) {
    for (const VisionObs& v : in.vision_msgs) {
      if (v.person_seen && v.person_score >= p_.P_person_score) last_person_ms_ = now;
    }
  }
  const bool person_seen =
      fresh && last_person_ms_ && age_ms(now, *last_person_ms_) < to_ms(p_.P_vision_fresh_s);
  bool cat_watch = false;
  if (!fresh) {
    presence_ = Presence::kUnknown;
    fresh_since_ms_.reset();
  } else {
    if (!fresh_since_ms_) fresh_since_ms_ = now;
    if (person_seen) {
      presence_ = Presence::kPresent;
    } else {
      std::uint64_t since = *fresh_since_ms_;  // 마지막 사람과 vision이 신선해진 시각 중 늦은 쪽부터 센다
      if (last_person_ms_ && *last_person_ms_ > since) since = *last_person_ms_;
      if (age_ms(now, since) >= to_ms(p_.P_absent_s)) presence_ = Presence::kAbsent;
      // 그 전에는 직전 값 유지: present는 P_absent_s까지 present, 막 신선해졌으면 unknown (M12)
      // 고양이 감시는 같은 시계를 짧게(P_cat_absent_s) 본다 — 자리를 비운 직후 올라온 고양이도 잡고,
      // 부재 보호·켜기 거절·화면 「자리 비움」(P_absent_s)은 그대로 둔다 (D-060).
      cat_watch = age_ms(now, since) >= to_ms(p_.P_cat_absent_s);
    }
  }

  // 4. 고양이 연속 hit — vision 메시지마다, 고양이 감시 중일 때만 센다 (#57 ⑥, 불변식 ④⑤, D-060).
  bool cat_any = false;
  if (!cat_watch) cat_run_ = 0;
  if (fresh) {
    for (const VisionObs& v : in.vision_msgs) {
      const bool hit = v.cat_seen && v.cat_score >= p_.P_cat_score && v.cat_roi_overlap >= p_.P_cat_overlap;
      cat_any = cat_any || hit;
      if (cat_watch) cat_run_ = hit ? cat_run_ + 1 : 0;
    }
  } else {
    cat_run_ = 0;
  }
  // 해제는 사람이 실제로 보이는 동안만 센다. 감시가 presence=absent보다 먼저 시작하므로, 떠난 뒤 P_absent_s 동안
  // present가 남아 있어도 아무도 없이 풀리지 않는다 (D-060).
  if (person_seen && !cat_any) {
    if (!cat_clear_since_ms_) cat_clear_since_ms_ = now;
  } else {
    cat_clear_since_ms_.reset();
  }

  // 5. hazard 전이표 (§3.3) — 위에서부터 첫 일치. 이 if 사다리의 순서가 곧 안전 우선순위다(불변식 ①).
  //    조건을 중간에 끼워 넣으면 우선순위가 바뀌므로 설계서 §3.3과 함께 PR에 근거를 남긴다.
  //    D-054(#65 10/25 확정)·D-059(10/26): 약불 45·중불 50·강불 55 °C는 정상 조리(low·mid·high). 차단(danger)은
  //    절대 고온 또는 「65 °C급 고온 ∧ 과한 연기」뿐이고, 둘 중 하나만이면 warning(팬 3).
  const double t_metric = in.t_max_c - in.t_median_c;
  const bool temp_abs = thermal_valid && in.t_max_c >= p_.P_T_abs_danger;
  const bool temp_danger = thermal_valid && t_metric >= p_.P_T_danger;
  const bool temp_high = thermal_valid && t_metric >= p_.P_T_high;
  const bool temp_mid = thermal_valid && t_metric >= p_.P_T_mid;
  Hazard raw = Hazard::kLow;
  if (fault) {
    raw = Hazard::kFault;
  } else if (temp_abs) {
    raw = Hazard::kDanger;
  } else if (temp_danger && heavy) {  // 화재위험
    raw = Hazard::kDanger;
  } else if (temp_danger || heavy) {  // 65 °C급 고온 단독 · 과한 연기 단독
    raw = Hazard::kWarning;
  } else if (temp_high) {  // 강불
    raw = Hazard::kHigh;
  } else if (temp_mid) {  // 중불
    raw = Hazard::kMid;
  }

  // 6. 히스테리시스 B — 올라갈 때 즉시. 내려갈 때는 아래 조건이 P_release_s(danger면 P_release_danger_s)
  //    연속이면 그동안 본 가장 높은 아래 단계로 직행.
  //    fault 해제는 fault 직전 단계에서 다시 시작해 같은 히스테리시스로 내려온다. 공백 동안 PM 연속 카운트가 비어
  //    해제 tick의 raw는 「후보 없음」일 수 있다 — 연기 중이면 low·팬 0이 한 tick 나옴(#65 Q3 실측, 불변식 ⑥).
  //    직전이 low면 지금처럼 바로 low로 복귀한다(F4, 「읽기 정상 P_fault_s 후 재판정」).
  if (hazard_ == Hazard::kFault && raw != Hazard::kFault) {
    hazard_ = std::max(raw, pre_fault_hazard_);
    release_since_ms_.reset();
  } else if (raw >= hazard_) {
    if (raw == Hazard::kFault && hazard_ != Hazard::kFault) pre_fault_hazard_ = hazard_;
    hazard_ = raw;
    release_since_ms_.reset();
  } else {
    if (!release_since_ms_) {
      release_since_ms_ = now;
      release_target_ = raw;
    } else {
      release_target_ = std::max(release_target_, raw);
    }
    const double wait_s = hazard_ == Hazard::kDanger ? p_.P_release_danger_s : p_.P_release_s;
    if (held_for(release_since_ms_, now, wait_s)) {
      hazard_ = release_target_;
      release_since_ms_.reset();
    }
  }
  if (hazard_ > prev_hazard) raise(alert_for(hazard_));
  if (prev_hazard == Hazard::kFault && hazard_ != Hazard::kFault) raise(Alert::kOfflineRecover);

  // 7. lock (§3.4, D-040 ④). ACK 하나로는 풀리지 않는다 — 래치 ∧ 안전 시간 ∧ 재실, 세 조건 AND.
  if (hazard_ >= Hazard::kDanger) lock_ = Lock::kLockedDanger;
  if (hazard_ > Hazard::kMid) {
    ack_latched_ = false;
    safe_since_ms_.reset();
  }
  if (lock_ == Lock::kLockedDanger && hazard_ <= Hazard::kMid) {
    if (!safe_since_ms_) safe_since_ms_ = now;
    for (const Command& c : in.commands) {
      if (c.kind == CommandKind::kAckAlert) ack_latched_ = true;  // 이 상태가 아니면 ACK는 무시한다
    }
    if (ack_latched_ && presence_ == Presence::kPresent && held_for(safe_since_ms_, now, p_.P_lock_release_s)) {
      lock_ = Lock::kUnlocked;
      ack_latched_ = false;
      safe_since_ms_.reset();
    }
  }
  if (lock_ == Lock::kUnlocked && cat_watch && cat_run_ >= p_.P_cat_hits) {
    lock_ = Lock::kLockedCat;
  } else if (lock_ == Lock::kLockedCat && held_for(cat_clear_since_ms_, now, p_.P_cat_cooldown_s)) {
    lock_ = Lock::kUnlocked;
  }
  if (lock_ == Lock::kLockedCat && prev_lock != Lock::kLockedCat) raise(Alert::kCatLock);

  // 8. 명령 (도착 순서). 켜기·올리기만 allowed_on을 본다 — 끄기·내리기는 항상 집행 (D-029).
  //    재실이면 강불·과열·과한 연기(warning)까지 켜기·올리기를 허용하고, danger·fault는 위험 잠금으로 막힌다 (D-061).
  //    부팅 유예 중 두 센서를 아직 한 번씩 못 읽었으면 hazard를 모르는 것이라 켜기를 HAZARD로 거부한다 (#79 후속).
  const bool sensors_seen = thermal_seen_ && pm_seen_;
  const auto allowed = [&] {
    return lock_ == Lock::kUnlocked && !user_lock_ && hazard_ <= Hazard::kWarning && sensors_seen &&
           presence_ == Presence::kPresent && in.ind_ack;
  };
  const auto reject_reason = [&] {
    if (lock_ != Lock::kUnlocked || user_lock_) return RejectReason::kLocked;
    if (hazard_ > Hazard::kWarning || !sensors_seen) return RejectReason::kHazard;
    if (presence_ != Presence::kPresent) return RejectReason::kAbsent;
    return RejectReason::kIndNoAck;
  };
  for (const Command& c : in.commands) {
    bool accepted = true;
    std::optional<RejectReason> reason;
    int target = induction_level_;
    switch (c.kind) {
      case CommandKind::kFanSet: fan_requested_ = std::clamp(c.value, 0, kLevelMax); break;
      case CommandKind::kFanOff: fan_requested_ = 0; break;
      case CommandKind::kIndOff: target = 0; break;
      case CommandKind::kIndDown: target = std::max(0, induction_level_ - 1); break;
      case CommandKind::kIndSet: target = std::clamp(c.value, 0, kLevelMax); break;
      case CommandKind::kIndOn: target = std::max(induction_level_, 1); break;
      case CommandKind::kIndUp: target = std::min(kLevelMax, induction_level_ + 1); break;
      case CommandKind::kNone:
        accepted = false;
        reason = RejectReason::kNoIntent;
        break;
      case CommandKind::kAckAlert: continue;  // 7에서 처리
      // 사용자 잠금 켜기·끄기(D-058). 잠그는 쪽은 안전 방향이라 조건이 없고, 푸는 것도 이 플래그뿐이다 —
      // 위험·반려동물 잠금(lock_)은 7단계 조건으로만 풀린다(불변식 ⑨). 켜는 명령은 여전히 allowed_on을 거친다.
      case CommandKind::kUserLock: user_lock_ = c.value != 0; continue;
    }
    if (target > induction_level_ && !allowed()) {
      accepted = false;
      reason = reject_reason();
      raise(Alert::kCmdRejected);
    } else {
      induction_level_ = target;
    }
    if (c.source == CommandSource::kVoice) {
      voice_ = VoiceResult{c.kind, (c.kind == CommandKind::kFanSet || c.kind == CommandKind::kIndSet) ? std::optional<int>(c.value) : std::nullopt,
                           accepted, reason};
    }
  }

  // 9. 자동 하향 (§3.5, Q-S5-1 B). Policy는 인덕션을 스스로 올리지 않는다. 재실 중 조리(약·중·강불)는 건드리지 않고,
  //    부재(unknown 포함) ∧ 중불 이상일 때만 1단으로 내린다(D-040 ③ → D-054). 사용자 잠금은 끈 채로 둔다(D-058).
  if (hazard_ >= Hazard::kDanger || lock_ == Lock::kLockedCat || user_lock_) {
    induction_level_ = 0;
  } else if (presence_ != Presence::kPresent && hazard_ >= Hazard::kMid) {
    induction_level_ = std::min(induction_level_, p_.P_ind_max_absent);
  }

  // 10. 팬 하한 (§3.5). danger·fault와 부재(unknown 포함) ∧ 중불 이상의 3단은 불변식이라 Profile 값이 아니다(불변식 ②⑥).
  int fan_min = 0;
  switch (hazard_) {
    case Hazard::kLow: break;
    case Hazard::kMid: fan_min = p_.P_fan_min_mid; break;
    case Hazard::kHigh: fan_min = p_.P_fan_min_high; break;
    case Hazard::kWarning:  // 과열·과한 연기 단독도 후드 최대 (D-059 전에는 P_fan_min_warning = 3)
    case Hazard::kDanger:
    case Hazard::kFault: fan_min = kLevelMax; break;
  }
  if (induction_level_ > 0) fan_min = std::max(fan_min, p_.P_fan_min_cooking);
  if (smoke_fan_) fan_min = kLevelMax;  // 포화 지속 → 후드 최대. 팬 3단에서 과한 연기를 다시 본다 (D-056)
  if (presence_ != Presence::kPresent && hazard_ >= Hazard::kMid) fan_min = kLevelMax;  // 부재 ∧ 중불 이상 (D-054)
  // 부팅 유예 중 두 센서를 아직 한 번씩 못 읽었으면 팬을 내리지 않는다 — ESP32 부팅 fail-safe(3단)가 0단으로
  // 떨어졌다 다시 오르지 않게 (#79 정인 ①). 잠금은 걸지 않는다(유예의 목적).
  if (!sensors_seen) fan_min = kLevelMax;

  // 11. reasons — Reason enum 순서(= 우선순위)대로. 온도·PM은 계열마다 가장 높은 코드 하나만.
  std::vector<Reason> reasons;
  if (hazard_ == Hazard::kFault) reasons.push_back(Reason::kSensorFault);
  if (temp_abs) {
    reasons.push_back(Reason::kTempAbs);
  } else if (temp_danger) {
    reasons.push_back(Reason::kTempDanger);
  }
  if (heavy) reasons.push_back(Reason::kSmoke);  // SMOKE = 과한 연기 (D-054)
  if (temp_high && !temp_abs && !temp_danger) reasons.push_back(Reason::kTempHigh);
  if (pm_warn && !heavy) reasons.push_back(Reason::kPmWarn);  // 연기 있음 — hazard에는 안 씀
  if (temp_mid && !temp_high && !temp_abs) reasons.push_back(Reason::kTempMid);
  if (release_since_ms_) reasons.push_back(Reason::kHold);
  if (presence_ == Presence::kAbsent) reasons.push_back(Reason::kAbsent);
  if (!fresh) reasons.push_back(Reason::kVisionStale);
  if (cat_any) reasons.push_back(Reason::kCatNear);
  if (lock_ == Lock::kLockedDanger && hazard_ <= Hazard::kMid) {
    reasons.push_back(ack_latched_ ? Reason::kUnlockWait : Reason::kAckWait);
  }
  if (!in.fan_ack) reasons.push_back(Reason::kFanNoAck);
  if (!in.ind_ack) reasons.push_back(Reason::kIndNoAck);

  // 12. alert — 이번 tick 이벤트 중 가장 높은 것. P_alert_hold_s 뒤 NONE.
  if (alert_ != Alert::kNone && now >= alert_until_ms_) alert_ = Alert::kNone;
  if (alert_rank(event) > alert_rank(alert_)) {
    alert_ = event;
    alert_until_ms_ = now + to_ms(p_.P_alert_hold_s);
  }

  Outputs out{};
  out.hazard = hazard_;
  out.presence = presence_;
  // 위험·반려동물 잠금이 우선이고, 둘 다 없을 때만 사용자 잠금을 보인다(D-058). 쿡탑 LOCK 1도 이 값으로 나간다.
  out.lock = lock_ != Lock::kUnlocked ? lock_ : user_lock_ ? Lock::kLockedUser : Lock::kUnlocked;
  out.fan_min = fan_min;
  out.fan_requested = fan_requested_;
  out.fan_level = std::clamp(std::max(fan_min, fan_requested_), 0, kLevelMax);
  out.induction_level = induction_level_;
  out.allowed_on = allowed();
  out.thermal_valid = thermal_valid;
  out.pm_valid = pm_valid;
  if (thermal_valid) out.t_metric = t_metric;
  if (pm_valid) out.pm_baseline = baseline_;
  out.vision_fresh = fresh;
  out.person_seen = person_seen;
  out.cat_hit = fresh && cat_any;
  out.reasons = std::move(reasons);
  out.alert = alert_;
  out.voice = voice_;
  // ① 안전 시간의 남은 초 (#116). 7단계와 같은 조건·같은 시계(safe_since_ms_)에서만 계산하고, 해제는 위에서만 한다.
  if (lock_ == Lock::kLockedDanger && hazard_ <= Hazard::kMid && safe_since_ms_) {
    const double elapsed_s = static_cast<double>(age_ms(now, *safe_since_ms_)) / 1000.0;
    out.lock_release_left_s = static_cast<int>(std::ceil(std::max(0.0, p_.P_lock_release_s - elapsed_s)));
  }
  last_fan_level_ = out.fan_level;
  return out;
}

// 계약 문자열 (contracts/README.md). enum 나열 순서와 같아야 한다.
const char* to_string(Hazard v) {
  static const char* const kNames[] = {"low", "mid", "high", "warning", "danger", "fault"};
  return kNames[static_cast<int>(v)];
}
const char* to_string(Presence v) {
  static const char* const kNames[] = {"present", "absent", "unknown"};
  return kNames[static_cast<int>(v)];
}
const char* to_string(Lock v) {
  static const char* const kNames[] = {"unlocked", "locked_cat", "locked_danger", "locked_user"};
  return kNames[static_cast<int>(v)];
}
const char* to_string(Reason v) {
  static const char* const kNames[] = {"SENSOR_FAULT", "TEMP_ABS",    "TEMP_DANGER", "SMOKE",    "TEMP_HIGH",
                                       "PM_WARN",      "TEMP_MID",     "HOLD",        "ABSENT",   "VISION_STALE",
                                       "CAT_NEAR",     "ACK_WAIT",     "UNLOCK_WAIT", "FAN_NOACK", "IND_NOACK"};
  return kNames[static_cast<int>(v)];
}
const char* to_string(Alert v) {
  static const char* const kNames[] = {"NONE", "CAUTION", "WARNING", "DANGER", "CAT_LOCK", "OFFLINE_RECOVER",
                                       "CMD_REJECTED"};
  return kNames[static_cast<int>(v)];
}
const char* to_string(CommandKind v) {
  static const char* const kNames[] = {"FAN_SET", "FAN_OFF", "IND_SET", "IND_ON", "IND_OFF",
                                       "IND_UP",  "IND_DOWN", "ACK_ALERT", "NONE",  "USER_LOCK"};
  return kNames[static_cast<int>(v)];
}
const char* to_string(RejectReason v) {
  static const char* const kNames[] = {"LOCKED", "HAZARD", "ABSENT", "IND_NOACK", "NO_INTENT"};
  return kNames[static_cast<int>(v)];
}

}  // namespace hoodseek
