#include "profile_loader.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include "nlohmann/json.hpp"

namespace {

using hoodseek::Profile;

struct RealKey { const char* name; double Profile::*field; };
struct IntKey { const char* name; int Profile::*field; int lo, hi; };

// policy.h Profile 필드 전부. 새 필드가 생기면 여기에 한 줄 추가한다.
const RealKey kReal[] = {
    {"P_T_abs_danger", &Profile::P_T_abs_danger}, {"P_T_danger", &Profile::P_T_danger},
    {"P_T_high", &Profile::P_T_high},             {"P_T_mid", &Profile::P_T_mid},
    {"P_tsize", &Profile::P_tsize},               {"P_PM_smoke", &Profile::P_PM_smoke},
    {"P_PM_warn", &Profile::P_PM_warn},           {"P_release_s", &Profile::P_release_s},
    {"P_release_danger_s", &Profile::P_release_danger_s}, {"P_fault_s", &Profile::P_fault_s},
    {"P_baseline_init_s", &Profile::P_baseline_init_s},   {"P_baseline_tau_s", &Profile::P_baseline_tau_s},
    {"P_vision_fresh_s", &Profile::P_vision_fresh_s},     {"P_absent_s", &Profile::P_absent_s},
    {"P_cat_absent_s", &Profile::P_cat_absent_s},
    {"P_person_score", &Profile::P_person_score},         {"P_cat_score", &Profile::P_cat_score},
    {"P_cat_overlap", &Profile::P_cat_overlap},           {"P_cat_cooldown_s", &Profile::P_cat_cooldown_s},
    {"P_lock_release_s", &Profile::P_lock_release_s},     {"P_alert_hold_s", &Profile::P_alert_hold_s},
};
const IntKey kInt[] = {
    {"P_smoke_samples", &Profile::P_smoke_samples, 1, 100},
    {"P_heavy_smoke_samples", &Profile::P_heavy_smoke_samples, 1, 600},
    {"P_smoke_fan_samples", &Profile::P_smoke_fan_samples, 1, 600},
    {"P_cat_hits", &Profile::P_cat_hits, 1, 100},
    {"P_fan_min_cooking", &Profile::P_fan_min_cooking, 0, 3},
    {"P_fan_min_mid", &Profile::P_fan_min_mid, 0, 3},
    {"P_fan_min_high", &Profile::P_fan_min_high, 0, 3},
    {"P_ind_max_absent", &Profile::P_ind_max_absent, 0, 3},
};

}  // namespace

bool load_profile(const std::string& path, Profile& out, std::string& err) {
  std::ifstream f(path);
  if (!f) {
    err = "열 수 없음: " + path;
    return false;
  }
  std::stringstream buf;
  buf << f.rdbuf();
  const auto j = nlohmann::json::parse(buf.str(), nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded() || !j.is_object()) {
    err = "JSON 객체가 아님";
    return false;
  }
  Profile p{};
  for (const RealKey& k : kReal) {
    if (!j.contains(k.name) || !j[k.name].is_number()) {
      err = std::string(k.name) + ": 없음 또는 숫자 아님";
      return false;
    }
    const double v = j[k.name].get<double>();
    if (!std::isfinite(v) || v < 0) {
      err = std::string(k.name) + ": 0 이상 유한수가 아님";
      return false;
    }
    p.*k.field = v;
  }
  for (const char* k : {"P_person_score", "P_cat_score", "P_cat_overlap"}) {  // 점수·겹침 비율은 0..1 (#76 리뷰 nit)
    if (j[k].get<double>() > 1.0) {
      err = std::string(k) + ": 0..1이 아님";
      return false;
    }
  }
  for (const IntKey& k : kInt) {
    const double v = j.contains(k.name) && j[k.name].is_number() ? j[k.name].get<double>() : NAN;
    if (!std::isfinite(v) || v != std::floor(v) || v < k.lo || v > k.hi) {
      err = std::string(k.name) + ": " + std::to_string(k.lo) + ".." + std::to_string(k.hi) + " 정수가 아님";
      return false;
    }
    p.*k.field = static_cast<int>(v);
  }
  if (!(p.P_T_mid < p.P_T_high && p.P_T_high < p.P_T_danger)) {
    err = "P_T_mid < P_T_high < P_T_danger 순서가 아님";
    return false;
  }
  // 고양이 감시는 사람 신선 창(P_vision_fresh_s)보다 짧으면 뜻이 없고, 화재 쪽 부재보다 늦으면 안 된다 (D-060)
  if (!(p.P_vision_fresh_s <= p.P_cat_absent_s && p.P_cat_absent_s <= p.P_absent_s)) {
    err = "P_vision_fresh_s ≤ P_cat_absent_s ≤ P_absent_s 순서가 아님";
    return false;
  }
  out = p;
  return true;
}
