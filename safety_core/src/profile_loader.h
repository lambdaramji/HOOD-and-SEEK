// /data/hoodseek/profile.json → hoodseek::Profile (키·값은 #58 남현지, 로더·검증·SIGHUP은 #47 정정인 — #57 합의 2번).
#pragma once

#include <string>

#include "policy.h"

// 키 누락·타입 오류·비유한수·음수·범위 밖·P_T_mid < P_T_high < P_T_danger 위반이 하나라도 있으면
// false + err(첫 사유)를 돌려주고 out은 건드리지 않는다. 호출자는 이전 값을 그대로 쓴다.
// 형식: 키 = Profile 필드 이름인 평평한 JSON 객체. 모르는 키는 무시한다.
bool load_profile(const std::string& path, hoodseek::Profile& out, std::string& err);
