// 계약 v1 수신 검증·state 직렬화 시험 (하드웨어 없음).
// 통과 줄은 contracts/examples, 거부 줄은 contracts/examples/invalid(PR #75)와 같은 경우를 한 줄로 옮겼다.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "message.h"
#include "profile_loader.h"
#include "nlohmann/json.hpp"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    ++failures;
    std::printf("FAIL %s\n", what);
  }
}

void accept(const char* line, Role role, const char* what) {
  Inbound m{};
  const std::string why = parse_inbound(line, role, m);
  if (!why.empty()) std::printf("  (사유: %s)\n", why.c_str());
  expect(why.empty(), what);
}

void reject(const char* line, Role role, const char* what) {
  Inbound m{};
  expect(!parse_inbound(line, role, m).empty(), what);
}

const char* kHello = R"({"v":1,"type":"hello","src":"vision","seq":0,"t_ms":0,"p":{"node":"vision-node","ver":"0.1.0"}})";
const char* kVision = R"({"v":1,"type":"vision","src":"vision","seq":881,"t_ms":120331,"p":{"frame_w":640,"frame_h":480,"infer_ms":313,"person":{"seen":true,"score":0.81,"bbox":[120,40,300,480]},"cat":{"seen":false,"score":0.0,"bbox":null,"roi_overlap":0.0}}})";
const char* kVoice = R"({"v":1,"type":"voice","src":"voice","seq":12,"t_ms":99120,"p":{"intent":"FAN_SET","value":3,"text":"후드 강풍","conf":0.72}})";
const char* kUi = R"({"v":1,"type":"ui_cmd","src":"ui","seq":5,"t_ms":5000,"p":{"cmd":"FAN_SET","value":2}})";

}  // namespace

int main() {
  // 통과
  accept(kHello, Role::none, "hello");
  accept(kVision, Role::vision, "vision");
  accept(kVoice, Role::voice, "voice FAN_SET 3");
  accept(kUi, Role::ui, "ui_cmd FAN_SET 2");
  accept(R"({"v":1,"type":"ui_cmd","src":"ui","seq":7.0,"t_ms":9100,"p":{"cmd":"ACK_ALERT","value":0}})", Role::ui, "seq 7.0 = 정수 (스키마와 같게)");
  accept(R"({"v":1,"type":"voice","src":"voice","seq":13,"t_ms":1,"p":{"intent":"NONE","text":"","conf":0.1}})", Role::voice, "voice NONE");

  // 역할 규칙 (PR #75 리뷰 3번)
  reject(kVision, Role::none, "hello 전 vision");
  reject(kHello, Role::vision, "두 번째 hello");
  reject(kVision, Role::ui, "ui 연결에서 vision");
  reject(kUi, Role::voice, "voice 연결에서 ui_cmd");
  reject(R"({"v":1,"type":"hello","src":"core","seq":0,"t_ms":0,"p":{"node":"x","ver":"0.1.0"}})", Role::none, "hello src core");

  // envelope
  reject("not json", Role::none, "JSON 아님");
  reject(R"({"v":2,"type":"hello","src":"vision","seq":0,"t_ms":0,"p":{"node":"vision-node","ver":"0.1.0"}})", Role::none, "v 2");
  reject(R"({"v":1,"type":"ui_cmd","src":"ui","seq":-1,"t_ms":5000,"p":{"cmd":"FAN_SET","value":2}})", Role::ui, "seq 음수");
  reject(R"({"v":1,"type":"ui_cmd","src":"ui","seq":4294967296,"t_ms":5000,"p":{"cmd":"FAN_SET","value":2}})", Role::ui, "seq 32-bit 초과");
  reject(R"({"v":1,"type":"ui_cmd","src":"ui","seq":5,"t_ms":5000,"debug":1,"p":{"cmd":"FAN_SET","value":2}})", Role::ui, "추가 필드");

  // ui_cmd
  reject(R"({"v":1,"type":"ui_cmd","src":"ui","seq":7,"t_ms":9100,"p":{"cmd":"UNLOCK","value":0}})", Role::ui, "잠금 해제 명령 없음");
  reject(R"({"v":1,"type":"ui_cmd","src":"ui","seq":7,"t_ms":9100,"p":{"cmd":"ACK_ALERT","value":1}})", Role::ui, "ACK_ALERT value 1");
  reject(R"({"v":1,"type":"ui_cmd","src":"ui","seq":5,"t_ms":5000,"p":{"cmd":"FAN_SET","value":4}})", Role::ui, "FAN_SET 4");
  // ui_cmd v 2 (D-058): USER_LOCK 0·1은 v 2에서만. 옛 세 명령은 v 1·v 2 둘 다
  accept(R"({"v":2,"type":"ui_cmd","src":"ui","seq":8,"t_ms":9200,"p":{"cmd":"USER_LOCK","value":1}})", Role::ui, "ui_cmd v2 USER_LOCK 1");
  accept(R"({"v":2,"type":"ui_cmd","src":"ui","seq":9,"t_ms":9300,"p":{"cmd":"FAN_SET","value":2}})", Role::ui, "ui_cmd v2 FAN_SET");
  reject(R"({"v":1,"type":"ui_cmd","src":"ui","seq":8,"t_ms":9200,"p":{"cmd":"USER_LOCK","value":1}})", Role::ui, "USER_LOCK는 v 1에 없음");
  reject(R"({"v":2,"type":"ui_cmd","src":"ui","seq":8,"t_ms":9200,"p":{"cmd":"USER_LOCK","value":2}})", Role::ui, "USER_LOCK value 2");
  accept(R"({"v":2,"type":"voice","src":"voice","seq":12,"t_ms":1,"p":{"intent":"FAN_OFF","text":"후드 꺼","conf":0.72}})", Role::voice, "voice v2 keeps existing intents");
  for (int level = 1; level <= 3; ++level) {
    auto command = nlohmann::json::parse(kVoice);
    command["v"] = 2;
    command["p"]["intent"] = "IND_SET";
    command["p"]["value"] = level;
    Inbound parsed{};
    expect(parse_inbound(command.dump(), Role::voice, parsed).empty() &&
           parsed.command.kind == hoodseek::CommandKind::kIndSet && parsed.command.value == level,
           "voice v2 IND_SET target preserved");
    command["v"] = 1;
    reject(command.dump().c_str(), Role::voice, "voice v1 cannot add IND_SET");
  }

  // voice
  reject(R"({"v":1,"type":"voice","src":"voice","seq":12,"t_ms":1,"p":{"intent":"FAN_SET","text":"후드 강풍","conf":0.72}})", Role::voice, "FAN_SET value 없음");
  reject(R"({"v":1,"type":"voice","src":"voice","seq":13,"t_ms":1,"p":{"intent":"IND_OFF","value":1,"text":"후드 인덕션 꺼","conf":0.66}})", Role::voice, "IND_OFF에 value");
  reject(R"({"v":1,"type":"voice","src":"voice","seq":13,"t_ms":1,"p":{"intent":"FAN_UP","text":"x","conf":0.66}})", Role::voice, "intent allowlist 밖");

  // vision — bbox가 프레임 안인지는 스키마가 못 보므로 여기서 (contracts/README.md)
  reject(R"({"v":1,"type":"vision","src":"vision","seq":1,"t_ms":1,"p":{"frame_w":640,"frame_h":480,"infer_ms":313,"person":{"seen":true,"score":0.81,"bbox":[300,40,120,480]},"cat":{"seen":false,"score":0.0,"bbox":null,"roi_overlap":0.0}}})", Role::vision, "bbox x_min > x_max");
  reject(R"({"v":1,"type":"vision","src":"vision","seq":1,"t_ms":1,"p":{"frame_w":640,"frame_h":480,"infer_ms":313,"person":{"seen":true,"score":0.81,"bbox":[120,40,700,480]},"cat":{"seen":false,"score":0.0,"bbox":null,"roi_overlap":0.0}}})", Role::vision, "bbox 프레임 밖");
  reject(R"({"v":1,"type":"vision","src":"vision","seq":1,"t_ms":1,"p":{"frame_w":640,"frame_h":480,"infer_ms":313,"person":{"seen":false,"score":0.0,"bbox":null},"cat":{"seen":true,"score":0.9,"bbox":[1,2,3,4],"roi_overlap":0.5,"consecutive_hits":5}}})", Role::vision, "consecutive_hits 금지");
  reject(R"({"v":1,"type":"vision","src":"vision","seq":1,"t_ms":1,"p":{"frame_w":640,"frame_h":480,"infer_ms":313,"person":{"seen":true,"score":1.2,"bbox":[120,40,300,480]},"cat":{"seen":false,"score":0.0,"bbox":null,"roi_overlap":0.0}}})", Role::vision, "score > 1");
  reject(R"({"v":1,"type":"vision","src":"vision","seq":1,"t_ms":1,"p":{"frame_w":640,"frame_h":480,"infer_ms":313,"person":{"seen":false,"score":0.0,"bbox":null},"cat":{"seen":true,"score":0.9,"bbox":null,"roi_overlap":0.5}}})", Role::vision, "seen인데 bbox null");

  // state 직렬화: 스텁 출력(fault)이 계약 v1 모양을 갖는지
  using namespace hoodseek;
  Outputs out{};
  out.hazard = Hazard::kFault;
  out.presence = Presence::kUnknown;
  out.lock = Lock::kLockedDanger;
  out.fan_level = 3;
  out.fan_min = 3;
  out.pm_valid = true;
  out.reasons = {Reason::kSensorFault, Reason::kUnlockWait};
  out.alert = Alert::kNone;
  out.voice = VoiceResult{CommandKind::kFanSet, 3, true, std::nullopt};
  Profile prof{};
  prof.P_T_mid = 16, prof.P_T_high = 22, prof.P_T_danger = 30, prof.P_PM_warn = 30, prof.P_PM_smoke = 50;
  prof.P_lock_release_s = 10;
  out.vision_fresh = false;
  out.person_seen = true;  // fresh=false면 state에서는 false여야 함
  out.cat_hit = true;
  const StateView view{7, 1000, 1, out, prof, false, false, 0, 0, 9.04, 0.711};
  const auto j = nlohmann::json::parse(build_state_line(view));
  expect(j["v"] == 4 && j["type"] == "state" && j["src"] == "core" && j["seq"] == 7, "state envelope v4 (#182)");
  expect(j["p"]["lock_release_left_s"].is_null(), "남은 초 없으면 null (fault)");
  expect(j["p"]["hazard"] == "fault" && j["p"]["fan"]["level"] == 3 && j["p"]["induction"]["level"] == 0, "state 스텁 값");
  expect(j["p"]["reasons"] == nlohmann::json::array({"SENSOR_FAULT", "UNLOCK_WAIT"}), "reasons 문자열");
  expect(j["p"]["sensors"]["thermal"]["max_c"].is_null() && j["p"]["sensors"]["thermal"]["valid"] == false, "thermal 무효면 null");
  expect(j["p"]["sensors"]["pm"]["pm25"] == 9.04 && j["p"]["sensors"]["pm"]["tsize"] == 0.71, "pm 소수 둘째 자리");
  expect(j["p"]["sensors"]["pm"]["baseline"].is_null(), "baseline 없으면 null");
  expect(j["p"]["vision"]["person"] == false && j["p"]["vision"]["cat"] == false, "vision fresh=false면 person·cat false");
  expect(j["p"]["voice"]["last_intent"] == "FAN_SET" && j["p"]["voice"]["value"] == 3 && j["p"]["voice"]["reason"].is_null(), "voice 결과");
  expect(j["p"]["thresholds"]["t_high"] == 22 && j["p"]["thresholds"]["pm_smoke"] == 50, "thresholds = Profile 사본");
  expect(j["p"]["thresholds"]["lock_release_s"] == 10, "thresholds.lock_release_s = P_lock_release_s 사본");
  Outputs counting = out;  // locked_danger ∧ caution에서 7초 남음 → 정수로 나간다 (contracts/examples/state.lock-release.json)
  counting.hazard = Hazard::kMid;
  counting.lock = Lock::kLockedDanger;
  counting.lock_release_left_s = 7;
  const auto jc = nlohmann::json::parse(build_state_line(StateView{8, 1500, 1, counting, prof, false, false, 0, 0, 9.04, 0.711}));
  expect(jc["p"]["lock_release_left_s"] == 7, "남은 초 7 → 정수");
  Outputs user = out;  // 사용자 잠금은 lock 문자열 locked_user (D-058)
  user.hazard = Hazard::kLow;
  user.lock = Lock::kLockedUser;
  const auto ju = nlohmann::json::parse(build_state_line(StateView{9, 2000, 1, user, prof, false, false, 0, 0, 9.04, 0.711}));
  expect(ju["p"]["lock"] == "locked_user" && ju["p"]["lock_release_left_s"].is_null(), "사용자 잠금 → locked_user, 남은 초 null");

  // profile 로더: 전부 맞으면 통과, 하나라도 틀리면 전체 거부 (#57 답글)
  const std::string good = R"({"P_T_abs_danger":90,"P_T_danger":30,"P_T_high":22,"P_T_mid":16,"P_tsize":0.49,"P_PM_smoke":50,"P_PM_warn":30,"P_smoke_samples":3,"P_heavy_smoke_samples":15,"P_smoke_fan_samples":60,"P_release_s":5,"P_release_danger_s":10,"P_fault_s":3,"P_baseline_init_s":60,"P_baseline_tau_s":300,"P_vision_fresh_s":2,"P_absent_s":30,"P_cat_absent_s":3,"P_person_score":0.35,"P_cat_score":0.35,"P_cat_overlap":0.25,"P_cat_hits":5,"P_cat_cooldown_s":5,"P_lock_release_s":10,"P_fan_min_cooking":1,"P_fan_min_mid":1,"P_fan_min_high":2,"P_ind_max_absent":1,"P_alert_hold_s":1,"_comment":"모르는 키는 무시"})";
  const auto try_load = [](const std::string& body, Profile& p) {
    const std::string path = "message_test_profile.json";
    std::ofstream(path) << body;
    std::string err;
    const bool ok = load_profile(path, p, err);
    if (!ok) std::printf("  (로더 사유: %s)\n", err.c_str());
    std::remove(path.c_str());
    return ok;
  };
  Profile loaded{};
  expect(try_load(good, loaded) && loaded.P_T_high == 22 && loaded.P_cat_hits == 5, "profile 정상");
  const auto edit = [&](const std::string& from, const std::string& to) {
    std::string b = good;
    b.replace(b.find(from), from.size(), to);
    return b;
  };
  Profile untouched{};
  untouched.P_T_high = 99;
  expect(!try_load(edit(R"("P_T_high":22,)", ""), untouched) && untouched.P_T_high == 99, "키 누락 → 거부, 값 그대로");
  expect(!try_load(edit(R"("P_T_high":22)", R"("P_T_high":35)"), untouched), "mid < high < danger 순서 위반 → 거부");
  expect(!try_load(edit(R"("P_cat_hits":5)", R"("P_cat_hits":0)"), untouched), "횟수 0 → 거부");
  expect(!try_load(edit(R"("P_fan_min_high":2)", R"("P_fan_min_high":2.5)"), untouched), "정수 키에 2.5 → 거부");
  expect(!try_load(edit(R"("P_cat_score":0.35)", R"("P_cat_score":35)"), untouched), "점수 키 > 1 → 거부");
  expect(!try_load(edit(R"("P_cat_absent_s":3)", R"("P_cat_absent_s":31)"), untouched), "고양이 감시 > 부재 → 거부");
  expect(!try_load(edit(R"("P_cat_absent_s":3)", R"("P_cat_absent_s":1)"), untouched), "고양이 감시 < 신선 창 → 거부");

  // 계약 v1 예제 전수 (main contracts/, PR #75 동결): 수신 4종은 examples/ 통과, examples/invalid/ 거부.
  // state는 Core가 보내는 쪽이라 여기서 보지 않는다. MQTT 세대 예제(이름이 4종이 아님)도 건너뛴다.
  {
    namespace fs = std::filesystem;
    const auto role_of = [](const std::string& name, Role& role) {
      const std::string type = name.substr(0, name.find('.'));
      if (type == "hello") role = Role::none;
      else if (type == "vision") role = Role::vision;
      else if (type == "voice") role = Role::voice;
      else if (type == "ui_cmd") role = Role::ui;
      else return false;
      return true;
    };
    int checked = 0;
    for (const bool valid : {true, false}) {
      const fs::path dir = fs::path(CONTRACTS_DIR) / (valid ? "examples" : "examples/invalid");
      for (const auto& entry : fs::directory_iterator(dir)) {
        Role role;
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".json" || !role_of(name, role)) continue;
        std::ifstream f(entry.path());
        std::stringstream buf;
        buf << f.rdbuf();
        const std::string line = nlohmann::json::parse(buf.str()).dump();  // 여러 줄 예제 → 한 줄 메시지
        Inbound m{};
        const std::string why = parse_inbound(line, role, m);
        const std::string what = std::string(valid ? "예제 통과: " : "invalid 거부: ") + name + (why.empty() ? "" : " (" + why + ")");
        expect(valid == why.empty(), what.c_str());
        if (!valid) std::printf("  invalid %-34s → %s\n", name.c_str(), why.c_str());
        ++checked;
      }
    }
    std::printf("contracts 예제 %d개 확인\n", checked);
    expect(checked == 34, "contracts 수신 예제 34개(IND_SET 통과1·거부5 추가)");
  }

  std::printf(failures ? "message_test: %d FAIL\n" : "message_test: all passed\n", failures);
  return failures ? 1 : 0;
}
