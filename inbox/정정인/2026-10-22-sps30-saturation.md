# SPS30 고농도 공백 — 포화 처리·재시작 조건·폐기 로그 (#65 PM 팔로업 2번)

- 작성자: 정정인
- 작성일: 2026-10-22
- 상태: **코드 변경 + 확인 계획.** Pi 4에는 10/22 원격 배포함(main `fbf7526` + 이 변경, sha256 `e2055ea2…`). 가설 확인은 10/23 저녁(아래 「확인 방법」)
- 카테고리: safety-core-sensor
- 조사 대상: `safety_core/src/sensors.cpp` SPS30 읽기 — #65 Q3 인센스 중 PM2.5 값 공백(최대 60.5 / 195.5 / 186.5 s) → `fault`

---

## 표기 규칙

| 표기 | 뜻 |
|---|---|
| **[실측]** | 기기·로그에서 직접 확인 (명령·대상·날짜) |
| **[문서]** | 저장소 코드·문서 인용 (경로·행) |
| **[추정]** | 확인 못 한 해석 — 확인 방법을 함께 적음 |

## 이 문서가 답하는 미결 항목

#65 PM 팔로업(2026-10-22) 2번 「SPS30 공백 — 원인이 우리 코드일 가능성」과 수정 제안 ①②③. #99 리뷰(PM)의 `tsize` 참고.

## 요약

| 항목 | 내용 |
|---|---|
| 가설 [추정] | 진한 연기에서 PM2.5가 2000을 넘으면 Core가 값을 버리고(→ 3 s 뒤 `fault`), 정상값이 3 s 없으니 측정 시작을 다시 보내 공백이 더 길어짐 (PM 분석) |
| 근거 [문서] | main `safety_core/src/sensors.cpp` 214~215행: `pm25 <= 2000`이 아니면 버림. 197~200행: 정상값이 `kSpsRestartMs`(3 s) 없으면 `sps_start()` |
| 근거 [실측] | #65 Q3 세 회차 최대 PM2.5 1903.43 / 1930.66 / 1886.28 — 상한 2000 바로 아래에서 끊김 ([g3 원자료](2026-10-22-g3-remeasure-data.md)) |
| 지금 로그로는 판단 불가 [실측] | 10/22 state 로그의 `[sensor]` 줄은 9줄로 시작·준비 메시지뿐. 지금 코드는 재시작이 성공하면 아무것도 찍지 않고 버린 값도 찍지 않음 (`grep "\[sensor\]" g3_1021_core_state.log`) |
| 이 PR의 변경 | ① 2000 초과는 **포화**로 보고 2000을 정상 읽기로 받음 ② 재시작은 **새 샘플(data-ready + CRC 정상)이 3 s 없을 때만** ③ 포화·버림·재시작을 원값·누적 횟수와 함께 1줄씩 로그 |

## 변경 내용 [문서]

| 위치 | 전 | 후 |
|---|---|---|
| `sensors.h` `classify_sps30()` | (없음 — 조건이 `sensors.cpp` 안에 있음) | 정상 / 포화 / 버림 3가지. NaN·음수·무한대·`tsize` 0~10 밖은 버림. **포화여도 `tsize`가 없으면 버림**(연기·증기를 못 가르므로) |
| `sensors.cpp` 값 처리 | 2000 초과 → 버림 → `pm_ok_ms` 그대로 → 3 s 뒤 `fault` | 2000 초과 → `pm25 = 2000`, `tsize`는 원값으로 Snapshot에 씀 → Policy가 연기·PM 경고 규칙으로 판정 |
| `sensors.cpp` 재시작 기준 | 마지막 **정상값** 시각 (`sps_last_ok_ms`) | 마지막 **새 샘플** 시각 (`sps_last_ready_ms`, 값 범위와 무관) |
| 로그 | 첫 시작·시작 실패만 | `[sensor] SPS30 포화 pm2.5=… → 2000 tsize=… (누적 n)` / `[sensor] SPS30 값 버림 pm2.5=… tsize=… (누적 n)` / `[sensor] SPS30 새 샘플 3000 ms 없음 → 측정 다시 시작 (누적 n)` |
| `core_tests/sensor_test.cpp` | — | `classify_sps30()` 10가지 경우 |

- **안전 쪽 확인**: 포화 + 작은 입자(`tsize < P_tsize`)면 Δ가 `P_PM_smoke`를 넘으므로 새 샘플 3개 뒤 연기 `danger`. 포화 + 큰 입자면 연기·PM 경고 후보가 아니어서 `normal` — 수증기를 연기로 보지 않는 설계(M7)와 같음. 전에는 둘 다 `fault`(팬 3)였음. 큰 입자 포화를 `fault`로 둘지는 아래 「남은 문제」
- **F4는 그대로**: 센서를 뽑으면 새 샘플이 없어 `pm_ok_ms`가 멈춤 → 3 s 뒤 `fault`, 3 s마다 측정 시작 재시도(전원 재인가 대비)
- 계약: `state.schema.json`의 `pm25`에는 상한이 없어 2000을 보내도 계약 변경 없음
- `inbox/정정인/sps30_bundle.sh`의 `sps30_test`(351행)도 같은 2000 상한이지만 시험 도구라 바꾸지 않음

## 확인 방법 (10/23 사무실 #64, 정인)

1. Pi 4는 이 변경으로 이미 돌고 있음(10/22 원격 배포). 병합되면 main으로 다시 빌드·배포하고 #64 L0(sha256·기동 로그)를 다시 함.
2. **20:45 #65 Q4 가습기 ×3** — 큰 입자. PM2.5가 2000을 넘으면 `포화` 줄이 나와야 하고 hazard는 ≤ caution(아래 「남은 문제」 3행).
3. **인센스 1회 (PM에 제안, 21:15쯤 Q4 뒤)** — #65 Q3와 같은 배치(냄비 자리 2분 → 환기 5분). 원래 가설은 이것으로 확인함. 안 되면 10/24 G4 M6 인센스 때.
4. 판단 (인센스):

| 로그 결과 | 해석 |
|---|---|
| `SPS30 포화` 줄이 나오고 그동안 `pm.valid=true` 유지(공백 없음) | **가설 맞음 + 수정 확인** |
| `포화` 없이 `값 버림`(NaN 등)이 나옴 | 상한이 아니라 다른 값 문제 — 원값으로 다시 판단 |
| 포화·버림 없이 `측정 다시 시작`만 나옴 | 센서가 data-ready를 안 줌(센서 쪽 원인) — I2C 오류 로그 추가 필요 |

```bash
# Pi 4에서 (시각은 Pi 4 시계, NTP 없음)
journalctl -u hoodseek-safety-core --since "-15min" -o short-unix | grep "\[sensor\] SPS30"
journalctl -u hoodseek-safety-core --since "-15min" -o short-unix | grep -c '"pm":{"valid":false'
```

## 남은 문제 [추정]과 확인 방법

| 항목 | 확인 방법 |
|---|---|
| 위 가설 자체 | 「확인 방법」의 인센스 1회 |
| **#65 Q3 2회차 공백 직후 `tsize` 0.55~0.58** (1·3회차 감쇠 구간 0.42~0.45). `6191.2 s`에 PM2.5 1229.46·`tsize` 0.58이라 PM이 높아도 연기·PM 경고 후보에서 빠짐 (#99 PM 리뷰). 원인 후보: ① SPS30 측정 재시작 직후 값 ② 연기가 박스에 오래 머물러 입자가 커짐 | 이 PR 뒤 인센스 재측정에서 **공백 없는 감쇠 구간의 `tsize` 추이**를 봄. 재시작 없이도 0.5 넘게 오르면 ②. ②라면 STEP 8에서 `P_tsize`(0.49)를 다시 볼 근거(`docs/hardware-facts.md` 1,000 초과 구간: 연기 0.41~0.44, 수증기 0.99~1.51) |
| 큰 입자 포화(`tsize ≥ P_tsize`, PM2.5 > 2000)를 `normal`로 두는 것이 맞는지 | 물컵 김(Q4 대체)·가습기 Q4에서 PM2.5가 2000을 넘는지 확인. 넘는데 `normal`이 위험하다고 보면 PM·T2 판단(`policy.cpp` 쪽 변경) |

## 이전 문서 수정 필요 사항

| 문서 | 고칠 내용 | 상태 |
|---|---|---|
| [2026-10-22-g3-remeasure-data.md](2026-10-22-g3-remeasure-data.md) 「남은 문제」 SPS30 행 | 확인 결과(맞음·틀림)와 이 문서 링크 | ☐ 10/23 확인 뒤 (정인) |
| [docs/hardware-facts.md](../../docs/hardware-facts.md) 「SPS30 PM2.5가 1,000을 넘은 직후에만 기록이 끊김」 행 | 가설이 맞으면 원인을 「Core 상한 2000 폐기 + 3 s 재시작」으로. 측정 범위 1000 초과는 사실로 남김 | ☐ 확인 뒤 (재범, #99 리뷰) |

## 재현 방법

```bash
# 호스트 (Mac): 시험 3개 + 판정 시험
cmake -S safety_core -B safety_core/build -DHOODSEEK_WITH_DBUS=OFF && cmake --build safety_core/build && ctest --test-dir safety_core/build
cmake -S safety_core/tests -B safety_core/tests/build && cmake --build safety_core/tests/build && ctest --test-dir safety_core/tests/build
# SPS30 읽기 코드는 Linux에서만 컴파일됨(#if __linux__) → Linux에서 컴파일 확인
g++ -std=c++17 -Wall -Wextra -Werror -Isafety_core/src -Isafety_core/third_party -Isafety_core/third_party/mlx90640 -c safety_core/src/sensors.cpp -o /tmp/sensors.o
```

## 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| 가설 | 인센스 1회 (위) — 10/23 저녁(PM 확인 대기), 안 되면 10/24 M6 | 정인 |
| 큰 입자 포화의 판정 | 10/23 Q4 가습기 ×3 | 정인 |
| 2회차 `tsize` 원인 | 인센스 1회의 공백 없는 감쇠 구간 `tsize` | 정인 |
