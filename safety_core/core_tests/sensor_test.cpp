// SPS30 샘플 분류 시험 (하드웨어 없음, #65 PM 팔로업 2번): 정상 / 포화(상한으로 받음) / 버림(F4)
#include <cmath>
#include <cstdio>

#include "sensors.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    ++failures;
    std::printf("FAIL %s\n", what);
  }
}

}  // namespace

int main() {
  expect(classify_sps30(11.19f, 0.45f) == SpsSample::kOk, "맑은 공기");
  expect(classify_sps30(kSpsPm25Max, 0.44f) == SpsSample::kOk, "상한과 같으면 정상");
  expect(classify_sps30(1930.66f, 0.46f) == SpsSample::kOk, "#65 Q3 최대값은 정상");
  expect(classify_sps30(2345.6f, 0.44f) == SpsSample::kSaturated, "상한 초과 = 포화");
  expect(classify_sps30(2345.6f, NAN) == SpsSample::kInvalid, "포화여도 tsize가 없으면 버림");
  expect(classify_sps30(2345.6f, 10.5f) == SpsSample::kInvalid, "포화여도 tsize 범위 밖이면 버림");
  expect(classify_sps30(NAN, 0.45f) == SpsSample::kInvalid, "pm2.5 NaN");
  expect(classify_sps30(INFINITY, 0.45f) == SpsSample::kInvalid, "pm2.5 무한대");
  expect(classify_sps30(-0.5f, 0.45f) == SpsSample::kInvalid, "pm2.5 음수");
  expect(classify_sps30(12.0f, -0.1f) == SpsSample::kInvalid, "tsize 음수");
  if (failures == 0) std::printf("sensor_test ok\n");
  return failures == 0 ? 0 : 1;
}
