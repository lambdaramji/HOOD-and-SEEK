// sensor 스레드 (설계서 §5.1, #47 2단계): /dev/i2c-1 하나로 MLX90640(0x33)과 SPS30(0x69)을 번갈아 읽는다.
// 정상 읽기만 Snapshot에 쓴다(thermal_ok_ms·pm_ok_ms·pm_seq). 실패·범위 밖 값은 쓰지 않는다(PM2.5 상한 초과만 포화로 받음) →
// 3 s 동안 정상 읽기가 없으면 Policy가 fault로 판정한다(F4). 이 스레드는 판정하지 않는다.
#pragma once

#include <atomic>
#include <cmath>
#include <string>

#include "snapshot.h"

// SPS30 PM2.5 상한. 넘으면 버리지 않고 「포화」로 이 값을 정상 읽기로 받는다 — 진한 연기에서 값을 버리면 공백 → fault로
// 보여 「연기 위험」이 「센서 고장」이 된다 [추정, #65 PM 팔로업 2번: Q3 최대 1886~1931이 상한 바로 아래]
constexpr float kSpsPm25Max = 2000.0f;

enum class SpsSample { kOk, kSaturated, kInvalid };

// CRC가 맞은 SPS30 샘플 하나의 처리. NaN·음수·tsize 범위 밖은 버린다(F4). 포화여도 tsize가 없으면 연기·증기를 못 가르므로 버린다.
inline SpsSample classify_sps30(float pm25, float tsize) {
  if (!std::isfinite(pm25) || pm25 < 0 || !std::isfinite(tsize) || tsize < 0 || tsize > 10) return SpsSample::kInvalid;
  return pm25 > kSpsPm25Max ? SpsSample::kSaturated : SpsSample::kOk;
}

class SensorThread {
 public:
  SensorThread(Snapshot& snap, std::string i2c_path) : snap_(snap), path_(std::move(i2c_path)) {}
  void run(const std::atomic<bool>& stop);

 private:
  Snapshot& snap_;
  const std::string path_;
};
