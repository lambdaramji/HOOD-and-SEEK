#include "message.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <regex>

#include "nlohmann/json.hpp"

using nlohmann::json;
using nlohmann::ordered_json;

namespace {

// 객체의 키가 required를 모두 갖고, required ∪ optional 밖의 키가 없는가 (additionalProperties: false)
bool exact_keys(const json& o, std::initializer_list<const char*> required, std::initializer_list<const char*> optional = {}) {
  if (!o.is_object()) return false;
  for (const char* k : required)
    if (!o.contains(k)) return false;
  for (const auto& [k, _] : o.items()) {
    bool known = false;
    for (const char* r : required) known = known || k == r;
    for (const char* r : optional) known = known || k == r;
    if (!known) return false;
  }
  return true;
}

// JSON Schema "integer"와 같게: 소수부가 0인 실수(7.0)도 정수로 받는다 (PR #75 리뷰 3번)
bool get_int(const json& j, int64_t lo, int64_t hi, int64_t& v) {
  if (j.is_number_unsigned()) {
    const uint64_t u = j.get<uint64_t>();
    if (u > static_cast<uint64_t>(hi)) return false;
    v = static_cast<int64_t>(u);
  } else if (j.is_number_integer()) {
    v = j.get<int64_t>();
  } else if (j.is_number_float()) {
    const double d = j.get<double>();
    if (!std::isfinite(d) || d != std::floor(d) || d < static_cast<double>(lo) || d > static_cast<double>(hi)) return false;
    v = static_cast<int64_t>(d);
  } else {
    return false;
  }
  return v >= lo && v <= hi;
}

bool get_num(const json& j, double lo, double hi, double& v) {
  if (!j.is_number()) return false;
  v = j.get<double>();
  return std::isfinite(v) && v >= lo && v <= hi;
}

// maxLength는 문자(코드 포인트) 수 — UTF-8 연속 바이트(10xxxxxx)를 빼고 센다
size_t utf8_len(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}

// bbox: seen이면 [x_min,y_min,x_max,y_max] 정수이고 0 ≤ x_min < x_max ≤ frame_w (y 동일), 아니면 null
bool check_bbox(const json& b, bool seen, int64_t fw, int64_t fh) {
  if (!seen) return b.is_null();
  if (!b.is_array() || b.size() != 4) return false;
  int64_t v[4];
  for (int i = 0; i < 4; ++i)
    if (!get_int(b[i], 0, 4096, v[i])) return false;
  return v[0] < v[2] && v[2] <= fw && v[1] < v[3] && v[3] <= fh;
}

std::string parse_hello(const json& p, Inbound& out) {
  static const std::regex node_re("^[a-z][a-z0-9-]{0,31}$");
  static const std::regex ver_re("^[0-9]+\\.[0-9]+\\.[0-9]+$");
  if (!exact_keys(p, {"node", "ver"})) return "hello.p keys";
  if (!p["node"].is_string() || !std::regex_match(p["node"].get<std::string>(), node_re)) return "hello.node";
  if (!p["ver"].is_string() || !std::regex_match(p["ver"].get<std::string>(), ver_re)) return "hello.ver";
  (void)out;
  return "";
}

std::string parse_vision(const json& p, Inbound& out) {
  if (!exact_keys(p, {"frame_w", "frame_h", "infer_ms", "person", "cat"})) return "vision.p keys";
  int64_t fw, fh, infer;
  if (!get_int(p["frame_w"], 1, 4096, fw) || !get_int(p["frame_h"], 1, 4096, fh)) return "vision.frame";
  if (!get_int(p["infer_ms"], 0, 60000, infer)) return "vision.infer_ms";
  const json& person = p["person"];
  const json& cat = p["cat"];
  if (!exact_keys(person, {"seen", "score", "bbox"})) return "vision.person keys";
  if (!exact_keys(cat, {"seen", "score", "bbox", "roi_overlap"})) return "vision.cat keys";
  double ps, cs, roi;
  if (!person["seen"].is_boolean() || !get_num(person["score"], 0, 1, ps)) return "vision.person";
  if (!cat["seen"].is_boolean() || !get_num(cat["score"], 0, 1, cs) || !get_num(cat["roi_overlap"], 0, 1, roi)) return "vision.cat";
  const bool pseen = person["seen"].get<bool>(), cseen = cat["seen"].get<bool>();
  if (!check_bbox(person["bbox"], pseen, fw, fh)) return "vision.person.bbox";
  if (!check_bbox(cat["bbox"], cseen, fw, fh)) return "vision.cat.bbox";
  out.vision = hoodseek::VisionObs{pseen, ps, cseen, cs, roi};
  return "";
}

std::string parse_voice(const json& p, int64_t v, Inbound& out) {
  using hoodseek::CommandKind;
  static const std::pair<const char*, CommandKind> kinds[] = {
      {"FAN_SET", CommandKind::kFanSet}, {"FAN_OFF", CommandKind::kFanOff}, {"IND_OFF", CommandKind::kIndOff},
      {"IND_ON", CommandKind::kIndOn},   {"IND_UP", CommandKind::kIndUp},   {"IND_DOWN", CommandKind::kIndDown},
      {"NONE", CommandKind::kNone}, {"IND_SET", CommandKind::kIndSet}};
  if (!exact_keys(p, {"intent", "text", "conf"}, {"value"})) return "voice.p keys";
  if (!p["intent"].is_string()) return "voice.intent";
  const std::string intent = p["intent"].get<std::string>();
  const CommandKind* kind = nullptr;
  for (const auto& [name, k] : kinds)
    if (intent == name) kind = &k;
  if (!kind) return "voice.intent";
  if (*kind == CommandKind::kIndSet && v != 2) return "voice.intent version";
  int64_t value = 0;
  if (*kind == CommandKind::kFanSet || *kind == CommandKind::kIndSet) {
    if (!p.contains("value") || !get_int(p["value"], 1, 3, value)) return "voice.value";
  } else if (p.contains("value")) {
    return "voice.value";
  }
  double conf;
  if (!p["text"].is_string() || utf8_len(p["text"].get<std::string>()) > 200) return "voice.text";
  if (!get_num(p["conf"], 0, 1, conf)) return "voice.conf";
  out.command = hoodseek::Command{hoodseek::CommandSource::kVoice, *kind, static_cast<int>(value)};
  return "";
}

// ui_cmd v 2(D-058)는 USER_LOCK을 더한다. 옛 앱의 v 1 세 명령은 그대로 받고, USER_LOCK은 v 2에서만 받는다.
std::string parse_ui_cmd(const json& p, int64_t v, Inbound& out) {
  using hoodseek::CommandKind;
  if (!exact_keys(p, {"cmd", "value"})) return "ui_cmd.p keys";
  if (!p["cmd"].is_string()) return "ui_cmd.cmd";
  const std::string cmd = p["cmd"].get<std::string>();
  CommandKind kind;
  if (cmd == "FAN_SET") kind = CommandKind::kFanSet;
  else if (cmd == "IND_SET") kind = CommandKind::kIndSet;
  else if (cmd == "ACK_ALERT") kind = CommandKind::kAckAlert;
  else if (cmd == "USER_LOCK" && v == 2) kind = CommandKind::kUserLock;
  else return "ui_cmd.cmd";
  const int64_t max = kind == CommandKind::kAckAlert ? 0 : kind == CommandKind::kUserLock ? 1 : 3;
  int64_t value;
  if (!get_int(p["value"], 0, max, value)) return "ui_cmd.value";
  out.command = hoodseek::Command{hoodseek::CommandSource::kUi, kind, static_cast<int>(value)};
  return "";
}

// 표시용 실수는 소수 둘째 자리까지
double r2(double v) { return std::round(v * 100.0) / 100.0; }

}  // namespace

std::string parse_inbound(std::string_view line, Role role, Inbound& out) {
  const json j = json::parse(line, nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded()) return "json";
  if (!exact_keys(j, {"v", "type", "src", "seq", "t_ms", "p"})) return "envelope keys";
  int64_t v, seq, t_ms;
  if (!get_int(j["v"], 1, 2, v)) return "v";
  if (!get_int(j["seq"], 0, 4294967295LL, seq)) return "seq";
  if (!get_int(j["t_ms"], 0, INT64_MAX, t_ms)) return "t_ms";
  if (!j["type"].is_string() || !j["src"].is_string()) return "type/src";
  const std::string type = j["type"].get<std::string>();
  const std::string src = j["src"].get<std::string>();
  const json& p = j["p"];
  if (!p.is_object()) return "p";
  out.seq = static_cast<uint32_t>(seq);

  // 연결의 역할은 hello.src로 고정한다 — hello 전에는 hello만, 이후에는 역할에 맞는 type만 (PR #75 리뷰 3번)
  // v 2는 ui_cmd(D-058)와 voice IND_SET(#182). hello·vision은 v 1.
  if (v != 1 && type != "ui_cmd" && type != "voice") return "v";
  if (type == "hello") {
    if (role != Role::none) return "second hello";
    if (src == "vision") out.hello_role = Role::vision;
    else if (src == "voice") out.hello_role = Role::voice;
    else if (src == "ui") out.hello_role = Role::ui;
    else return "hello.src";
    out.type = Inbound::Type::hello;
    return parse_hello(p, out);
  }
  if (role == Role::none) return "before hello";
  if (type == "vision" && role == Role::vision && src == "vision") {
    out.type = Inbound::Type::vision;
    return parse_vision(p, out);
  }
  if (type == "voice" && role == Role::voice && src == "voice") {
    out.type = Inbound::Type::command;
    return parse_voice(p, v, out);
  }
  if (type == "ui_cmd" && role == Role::ui && src == "ui") {
    out.type = Inbound::Type::command;
    return parse_ui_cmd(p, v, out);
  }
  return "type/src not allowed for role";
}

std::string build_state_line(const StateView& s) {
  using hoodseek::to_string;
  const hoodseek::Outputs& o = s.out;
  ordered_json reasons = ordered_json::array();
  for (hoodseek::Reason r : o.reasons) reasons.push_back(to_string(r));

  // valid=false면 그 센서 값은 전부 null (D-042 ⑥, 스키마 else가 강제)
  ordered_json thermal = {{"valid", o.thermal_valid}, {"max_c", nullptr}, {"median_c", nullptr}, {"metric", nullptr}};
  if (o.thermal_valid) {
    thermal["max_c"] = r2(s.t_max);
    thermal["median_c"] = r2(s.t_median);
    thermal["metric"] = r2(o.t_metric.value_or(s.t_max - s.t_median));
  }
  ordered_json pm = {{"valid", o.pm_valid}, {"pm25", nullptr}, {"tsize", nullptr}, {"baseline", nullptr}};
  if (o.pm_valid) {
    pm["pm25"] = r2(s.pm25);
    pm["tsize"] = r2(s.tsize);
    if (o.pm_baseline) pm["baseline"] = r2(*o.pm_baseline);
  }
  ordered_json voice = nullptr;
  if (o.voice) {
    voice = {{"last_intent", to_string(o.voice->last_intent)},
             {"value", nullptr},
             {"accepted", o.voice->accepted},
             {"reason", nullptr}};
    if (o.voice->value) voice["value"] = *o.voice->value;
    if (o.voice->reason) voice["reason"] = to_string(*o.voice->reason);
  }
  const hoodseek::Profile& pf = s.profile;
  const ordered_json p = {
      {"hazard", to_string(o.hazard)},
      {"presence", to_string(o.presence)},
      {"lock", to_string(o.lock)},
      {"lock_release_left_s", o.lock_release_left_s ? ordered_json(*o.lock_release_left_s) : ordered_json(nullptr)},
      {"fan", {{"level", o.fan_level}, {"min", o.fan_min}, {"requested", o.fan_requested}, {"ack", s.fan_ack}}},
      {"induction", {{"level", o.induction_level}, {"allowed_on", o.allowed_on}, {"ack", s.ind_ack}}},
      {"sensors", {{"thermal", thermal}, {"pm", pm}}},
      {"vision", {{"fresh", o.vision_fresh}, {"person", o.vision_fresh && o.person_seen}, {"cat", o.vision_fresh && o.cat_hit}}},
      {"voice", voice},
      {"reasons", reasons},
      {"alert", to_string(o.alert)},
      {"thresholds",
       {{"t_mid", pf.P_T_mid}, {"t_high", pf.P_T_high}, {"t_danger", pf.P_T_danger}, {"pm_warn", pf.P_PM_warn},
        {"pm_smoke", pf.P_PM_smoke}, {"lock_release_s", pf.P_lock_release_s}}},
      {"uptime_s", s.uptime_s}};
  // state v 4(#182): voice IND_SET·value. 앱의 v4 수신을 먼저 배포한다.
  const ordered_json msg = {{"v", 4}, {"type", "state"}, {"src", "core"}, {"seq", s.seq}, {"t_ms", s.t_ms}, {"p", p}};
  return msg.dump();
}
