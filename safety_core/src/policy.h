// policy.h — SafetyCore 판정 모듈 인터페이스 (T2 남현지, #57·#58)
//
// Policy는 I/O·스레드·mutex·시계 읽기가 없는 결정적 상태기계다.
//   - 입력은 Inputs 하나, 출력은 Outputs 하나. 시간은 Inputs::now_ms로만 흐른다.
//   - 같은 Profile과 같은 Inputs 시퀀스를 주면 항상 같은 Outputs 시퀀스가 나온다 → PC 단위 테스트.
//   - 센서·UART·TCP 스레드(정정인 #47)가 Snapshot을 채우고, policy 스레드가 500 ms마다 tick()을 부른다.
// 판정 수치는 전부 Profile(profiles/default.json)에서 온다. 이 파일과 policy.cpp에 임계 숫자를 쓰지 않는다.
// 근거: 설계서 §3(전이표)·§5.1(인터페이스)·§6(fail-safe), 계약 v1 contracts/state.schema.json, #57 합의 6건.

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace hoodseek {

// 세 축 (설계서 §3.2, state.p). 나열 순서가 심각도 순서라 비교 연산(>=)에 쓴다.
// low·mid·high는 정상 조리의 약불(대기 포함)·중불·강불, warning부터 주의가 필요한 상태다 (state v 3, D-059).
enum class Hazard { kLow, kMid, kHigh, kWarning, kDanger, kFault };
enum class Presence { kPresent, kAbsent, kUnknown };
// kLockedUser는 출력 전용(state v 3, D-058): 내부 lock_은 위험·반려동물만 갖고, 사용자 잠금은 user_lock_ 플래그다.
enum class Lock { kUnlocked, kLockedCat, kLockedDanger, kLockedUser };

// state.p.reasons 15종 (contracts/README.md 코드표). 나열 순서 = 출력 순서.
enum class Reason {
  kSensorFault, kTempAbs, kTempDanger, kSmoke, kTempHigh, kPmWarn, kTempMid,
  kHold, kAbsent, kVisionStale, kCatNear, kAckWait, kUnlockWait, kFanNoAck, kIndNoAck,
};

// state.p.alert 7종.
enum class Alert { kNone, kCaution, kWarning, kDanger, kCatLock, kOfflineRecover, kCmdRejected };

// voice intent 7종 + ui_cmd 4종. 파서(#47)가 allowlist·범위·신선도(5 s)를 통과한 것만 넣는다.
// kUserLock은 ui_cmd v 2에서만 온다(D-058). 나열 순서 = to_string 순서라 끝에 붙인다.
enum class CommandKind { kFanSet, kFanOff, kIndSet, kIndOn, kIndOff, kIndUp, kIndDown, kAckAlert, kNone, kUserLock };
enum class CommandSource { kVoice, kUi };

// state.p.voice.reason 5종.
enum class RejectReason { kLocked, kHazard, kAbsent, kIndNoAck, kNoIntent };

// vision 메시지 1개의 관측값 (계약 vision.p). 판정 임계는 Policy가 적용한다.
struct VisionObs {
  bool person_seen;
  double person_score;
  bool cat_seen;
  double cat_score;
  double cat_roi_overlap;
};

struct Command {
  CommandSource source;
  CommandKind kind;
  int value;  // kFanSet: voice 1..3 / ui 0..3, kIndSet: voice 1..3 / ui 0..3, kUserLock: 1 잠금 · 0 해제, 그 외 0
};

// 판정 파라미터. 필드 이름 = profiles/default.json 키 이름.
// 키·값은 남현지(#58), JSON 로더·검증·SIGHUP 재로드는 정정인(#47).
// danger·fault·부재 경고의 팬 3단과 인덕션 0단은 안전 불변식이라 여기 두지 않는다.
struct Profile {
  // hazard (§3.3)
  double P_T_abs_danger;      // T_max ≥ → danger (°C)
  double P_T_danger;          // T_metric(max − median) ≥ → 65 °C급 고온: 단독이면 warning, 과한 연기와 함께면 danger (D-054)
  double P_T_high;            // ≥ → high = 강불 55 °C (D-054, D-059)
  double P_T_mid;             // ≥ → mid = 중불 50 °C. 그 아래는 low = 약불 45 °C·대기 (D-054, D-059)
  double P_tsize;             // tsize < 이 값 = 연기형 입자 (µm). 비포화 연기 표시(PM_WARN)를 시작할 때만 본다
  double P_PM_smoke;          // 과한 연기가 확정된 뒤 PM_delta가 이 값 아래로 떨어지면 해제 (µg/m³, D-054)
  double P_PM_warn;           // PM_delta ≥ ∧ 연기형 입자 새 샘플 연속 P_smoke_samples → PM_WARN 표시. hazard는 안 바꿈 (D-054)
  int P_smoke_samples;        // PM_WARN 시작을 SPS30 새 샘플 기준 연속 이 횟수로 확정 (#57 ③·STEP 4)
  int P_heavy_smoke_samples;  // 팬 3단인 동안 SPS30 포화 샘플이 연속 이 횟수 → 과한 연기 (D-054, #65 10/24 실측)
  int P_smoke_fan_samples;    // 포화 지속: 팬 단수와 상관없이 포화 샘플이 연속 이 횟수 → 팬 하한 3, 해제도 Δ < P_PM_smoke 이 횟수 연속. hazard는 안 바꿈 (D-056)
  double P_release_s;         // 아래 단계 조건이 이 시간 연속이면 그 단계로 직행 (히스테리시스 B)
  double P_release_danger_s;  // danger에서 내려올 때
  double P_fault_s;           // 마지막 정상 읽기 후 이 시간 → fault, 다시 이 시간 정상 → 복귀 (#57 ④, F4)
  double P_baseline_init_s;   // 부팅 후 PM baseline 이동 중앙값 구간 (§3.1)
  double P_baseline_tau_s;    // 이후 hazard ≤ mid일 때 baseline 갱신 시간 상수 (#57 ⑤)
  // presence·lock (§3.4)
  double P_vision_fresh_s;    // 마지막 vision 수신 후 이 시간 → presence=unknown (§2.4)
  double P_absent_s;          // vision이 신선한 동안 사람이 이 시간 안 보이면 absent (부재 보호·켜기 거절·화면 「자리 비움」)
  double P_cat_absent_s;      // 사람이 이 시간 안 보이면 고양이 감시 시작 — P_absent_s와 따로 센다 (D-060)
  double P_person_score;      // person_seen = seen ∧ score ≥ 이 값 (병직 #52 ②)
  double P_cat_score;
  double P_cat_overlap;
  int P_cat_hits;             // 고양이 감시 중 cat_hit가 vision 메시지 기준 연속 이 횟수 → locked_cat
  double P_cat_cooldown_s;    // 사람이 보이는 동안 연속 ∧ cat_hit 없음 → locked_cat 해제 (§3.6, D-060)
  double P_lock_release_s;    // locked_danger 해제: hazard ≤ mid 연속 이 시간 ∧ ACK ∧ present (D-040 ④)
  // 집행 (§3.5)
  int P_fan_min_cooking;      // normal인데 induction > 0
  int P_fan_min_mid;          // 중불(mid)
  int P_fan_min_high;         // 강불(high). 과열·과한 연기(warning)·위험은 불변식이라 3 고정
  int P_ind_max_absent;       // 부재(unknown 포함) ∧ hazard ≥ mid이면 induction ≤ 이 값 (D-040 ③ → D-054)
  double P_alert_hold_s;      // alert를 NONE으로 되돌리기 전 유지 시간
};

// Core가 tick마다 Snapshot에서 복사한다. *_ms는 Core monotonic 시계(ms), 0 = 아직 없음.
struct Inputs {
  std::uint64_t now_ms;

  std::uint64_t thermal_ok_ms;  // MLX 마지막 정상 프레임 (#57 ④)
  double t_max_c;
  double t_median_c;

  std::uint64_t pm_ok_ms;  // SPS30 마지막 정상 샘플
  std::uint32_t pm_seq;    // SPS30 새 샘플마다 +1 (#57 ③)
  double pm25;
  double tsize;
  bool pm_saturated = false;  // pm25가 센서 상한에서 잘림 → tsize를 믿지 않고 연기형으로 본다 (#65 10/23)

  std::uint64_t vision_rx_ms;         // 마지막 vision 메시지 수신 시각 (§2.4)
  std::vector<VisionObs> vision_msgs;  // 지난 tick 이후 받은 vision 메시지, 도착 순서 (≈3 Hz > tick 2 Hz, #57 정인 ①)

  bool fan_ack;  // 팬 ESP32 ACK가 2 s 안에 옴 (#56, F5)
  bool ind_ack;  // 쿡탑 ESP32 ACK가 2 s 안에 옴 (#56, F6)

  std::vector<Command> commands;  // 지난 tick 이후 받은 명령, 도착 순서
};

struct VoiceResult {
  CommandKind last_intent;
  std::optional<int> value;  // kFanSet이면 1..3
  bool accepted;
  std::optional<RejectReason> reason;  // accepted면 nullopt
};

// Core가 state.p와 UART 명령(FAN n / IND n / LOCK x)으로 옮긴다.
struct Outputs {
  Hazard hazard;
  Presence presence;
  Lock lock;
  int fan_level;  // 0..3, 항상 fan_min 이상
  int fan_min;
  int fan_requested;  // 마지막 사용자·음성 요청 (기본 0)
  int induction_level;
  bool allowed_on;
  bool thermal_valid;
  bool pm_valid;
  std::optional<double> t_metric;     // thermal_valid일 때만
  std::optional<double> pm_baseline;  // 초기 구간이거나 PM 무효면 nullopt
  bool vision_fresh;  // state.vision.fresh (#57 정인 ②)
  bool person_seen;   // state.vision.person = 최근 P_vision_fresh_s 안 사람 (score 임계 적용)
  bool cat_hit;       // state.vision.cat = 이번 tick cat_hit
  std::vector<Reason> reasons;
  Alert alert;
  std::optional<VoiceResult> voice;  // 부팅 후 voice 명령이 없으면 nullopt
  // 위험 잠금 해제 조건 ①(hazard ≤ mid P_lock_release_s 연속)의 남은 초. locked_danger ∧ hazard ≤ mid일 때만
  // ceil(남은 s), 0이면 ① 충족. 그 밖은 nullopt. 표시용이며 해제 판정은 tick 7단계뿐이다 (#116, state v2).
  std::optional<int> lock_release_left_s;
};

class Policy {
 public:
  explicit Policy(const Profile& profile);

  // now_ms는 직전 호출보다 작으면 안 된다.
  Outputs tick(const Inputs& in);

  // SIGHUP 재로드 반영. 타이머·카운터·잠금은 유지한다. tick()과 같은 스레드에서 부른다.
  void set_profile(const Profile& profile);

 private:
  Profile p_;

  std::optional<std::uint64_t> start_ms_;  // 첫 tick 시각 (부팅 직후 P_fault_s 유예 기준)

  Hazard hazard_ = Hazard::kLow;
  std::optional<std::uint64_t> release_since_ms_;  // 아래 단계 조건이 시작된 시각
  Hazard release_target_ = Hazard::kLow;        // 그동안 본 가장 높은 아래 단계 (보수적 직행 목표)
  Hazard pre_fault_hazard_ = Hazard::kLow;      // fault에 들어가기 직전 단계 — 해제 후 여기서 다시 내려온다 (#65 Q3)
  std::optional<std::uint64_t> sensors_ok_since_ms_;
  bool thermal_seen_ = false;  // 부팅 뒤 한 번이라도 정상 읽기가 있었나 (유예 중 팬 하한, #79)
  bool pm_seen_ = false;

  std::uint32_t last_pm_seq_ = 0;
  std::optional<std::uint64_t> last_pm_sample_ms_;
  int pm_warn_run_ = 0;
  bool pm_warn_on_ = false;  // PM_WARN 확정 뒤에는 Δ만 본다 — 비포화 인센스 tsize ≥ 0.49로 끊기지 않게 (#65 10/24, D-054)
  int heavy_run_ = 0;        // 팬 3단 중 포화 샘플 연속 수
  bool heavy_ = false;       // 과한 연기 확정. Δ < P_PM_smoke가 되면 풀린다
  int last_fan_level_ = 0;   // 직전 tick의 fan_level — 「팬 3단에서도」 판정에 쓴다
  int sat_run_ = 0;          // 포화 샘플 연속 수 (팬 단수 무관)
  bool smoke_fan_ = false;   // 포화 지속 → 팬 하한 3 (D-056)
  int smoke_clear_run_ = 0;  // 포화 지속 확정 뒤 Δ < P_PM_smoke 샘플 연속 수 — P_smoke_fan_samples가 되면 풀린다
  std::vector<double> baseline_window_;
  std::optional<double> baseline_;

  Presence presence_ = Presence::kUnknown;
  std::optional<std::uint64_t> last_person_ms_;
  std::optional<std::uint64_t> fresh_since_ms_;  // vision이 신선해진 시각 (unknown 구간은 absent에 안 셈)

  int cat_run_ = 0;
  std::optional<std::uint64_t> cat_clear_since_ms_;

  Lock lock_ = Lock::kUnlocked;  // 위험·반려동물 잠금만. 사용자 잠금은 아래 플래그(출력에서만 합친다)
  // 사용자 잠금(D-058). USER_LOCK 1·0으로만 바뀌고 lock_의 진입·해제(불변식 ⑨)에 관여하지 않는다.
  // ponytail: 메모리에만 있어 Core를 다시 켜면 풀린다. 유지가 필요해지면 /data에 남긴다.
  bool user_lock_ = false;
  bool ack_latched_ = false;
  std::optional<std::uint64_t> safe_since_ms_;  // locked_danger 중 hazard ≤ mid가 시작된 시각

  int fan_requested_ = 0;
  int induction_level_ = 0;

  Alert alert_ = Alert::kNone;
  std::uint64_t alert_until_ms_ = 0;
  std::optional<VoiceResult> voice_;
};

// 계약 문자열 (contracts/README.md). Core 직렬화가 그대로 쓴다.
const char* to_string(Hazard value);
const char* to_string(Presence value);
const char* to_string(Lock value);
const char* to_string(Reason value);
const char* to_string(Alert value);
const char* to_string(CommandKind value);  // voice.last_intent (kAckAlert는 voice에 나오지 않음)
const char* to_string(RejectReason value);

}  // namespace hoodseek
