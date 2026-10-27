// D-Bus: 버스 이름 소유(서비스 RUNNING 조건) + GetState() → state JSON 한 줄 (설계서 §5.1).
// Stop 콜백(SMClientLib)은 등록하지 않는다 — 등록하면 30 s 유휴 자동 종료가 동작한다 (D-019, LG 가이드 04 §2.7).
// 이름 요청이 실패해도(#46: 재부팅 직후 정책 미반영 등) 안전 루프에는 영향이 없다. 5 s마다 다시 시도한다.
#pragma once

#include <atomic>
#include <functional>
#include <string>

inline constexpr const char* kBusName = "com.atlas.HoodSeekSafetyCore1";  // = serviceinfo.json id

void run_dbus(const std::atomic<bool>& stop, const std::function<std::string()>& get_state);
