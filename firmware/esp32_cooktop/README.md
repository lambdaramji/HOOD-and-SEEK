# ESP32 cooktop (LED 화구 시뮬레이터)

Pi 4 uart5(`/dev/ttyAMA5`)에서 GPIO UART로 `IND n`·`LOCK x`·`STAT` 한 줄 명령만 받아 WS2812B 16×16 판에 화력·잠금을 그립니다 (설계서 §4.3·§4.4, #60). 쿡탑은 **시뮬레이터**이고 물리 입력이 없습니다(Q-S7-1, #55). 화력은 SafetyCore의 `IND n`으로만 바뀌고, 이 보드는 판정하지 않습니다.

## 배선

| Pi 4 (물리 핀) | 선 | ESP32 (DevKit 실크) |
|---|---|---|
| 32번 GPIO12 (uart5 TX) | **1 kΩ 직렬** | `G16` (Serial2 RX) |
| 33번 GPIO13 (uart5 RX) | **1 kΩ 직렬** | `G17` (Serial2 TX) |
| GND (34·39번 등) | 직결 | `GND` |

| LED 판 | 선 | 연결 |
|---|---|---|
| **3핀 암 커넥터**(입력) 녹색 `DIN` | **220~330 Ω 직렬** | ESP32 `G18` |
| 보조 전원선 빨강·검정 | 직결 | 별도 5 V 전원(D-036), **GND는 ESP32·Pi와 공통** |

- 직렬 1 kΩ는 팬 보드와 같은 이유(한쪽만 꺼졌을 때 역류 방지)입니다 — [팬 README](../esp32_fan_controller/README.md).
- 숫 커넥터(출력 `DOUT`)에 데이터를 넣으면 아무것도 켜지지 않습니다 [실측 10/20].
- `RX`·`TX`(`RX0`·`TX0`, GPIO3·1)는 USB 업로드 핀이라 쓰지 않습니다. `G16`·`G17`(보드에 따라 `RX2`·`TX2`).
- 판 배선 [실측 10/20 `z` 시험]: 앞면 왼쪽 아래가 0번, 1번 줄 좌→우, 2번 줄 우→좌인 지그재그(`kSerpentine = true`). 무늬가 원형이라 판 방향은 결과에 영향이 없습니다.

## 프로토콜 (115200 8N1, 한 줄 15자 이하)

| 방향 | 줄 | 의미 |
|---|---|---|
| Pi→ESP | `IND n` (0..3) | 화력. 그 밖의 값(`IND 4` 등)은 `ERR invalid-command`, 상태 그대로 |
| Pi→ESP | `LOCK 0` / `LOCK 1` | 잠금 표시 끄기·켜기 (표시만 — 판정은 Pi 4) |
| Pi→ESP | `STAT` | `STAT ind=n lock=x fs=0|1 age=ms`. fail-safe 타이머를 살리지 않음 |
| ESP→Pi | `ACK IND n` / `ACK LOCK x` | 적용 완료 (SafetyCore #56은 보낸 줄과 같은 ACK만 확인으로 셈) |
| ESP→Pi | `READY FAILSAFE IND 0 LOCK 1` | 부팅 |
| ESP→Pi | `FAILSAFE timeout IND 0 LOCK 1` | 3 s 동안 정상 `IND`·`LOCK` 없음 → OFF + 잠금 (D-21) |
| ESP→Pi | `ERR invalid-command` / `ERR command-too-long` | 버림. 15자 넘는 줄은 다음 `\n`까지 통째로 버림 |

- 하드웨어 워치독 2 s (D-039). 부팅 때 USB로 `dbg wdt 2s init=0 add=0`.
- USB 시리얼은 플래시·디버그 전용입니다(같은 줄 사본 + `dbg …`). **USB로 보낸 `IND`·`LOCK`은 읽지 않습니다.** 디버그용 `BRI n`(0~255, 전체 밝기)만 받고, 재부팅하면 기본값으로 돌아갑니다.

## 표현

| 상태 | 판 |
|---|---|
| `IND 0` | 소등 |
| `IND 1` / `2` / `3` | 코일 링 3개(반지름 2.75 · 4.8 · 7.2, 바깥은 판 가장자리 LED까지). **켜진 링 수 = 단수**, 색은 한 빨강, 밝기 약 60 % · 중 80 % · 강 100 %. 안쪽 링은 대각선 칸까지 채워 원으로, 안쪽 링 안 8칸과 바로 밖에는 약 2/255 잔열 점, 정중앙은 끔(10/26 현장에서 맞춤. PM 지시서(2026-10-20 「가상 인덕션 화구 LED 표시」)의 가우시안 링을 코일 모양으로 바꿈). 단계 전환 350 ms 크로스페이드, 강불은 링 위 최대 6 % 일렁임 |
| `LOCK 1` | 반지름 7.0 **파란 원형 링** 1 Hz 점멸 (화면 위에 덧씀) |
| FAILSAFE | 중앙 소등 + 반지름 7.0 **빨간 원형 링** 1 Hz 점멸 |

설계서 §4.4 [제안]과 다른 점: `IND 0`의 「흐린 흰색 링」 대신 PM 지시서대로 소등, 잠금·fail-safe의 「외곽 링」은 사각 테두리가 아니라 화구와 같은 원형 링(10/20 정정인 실물 확인 후).

## 전원·밝기

- `kMaxMilliamps = 1500`: FastLED가 전체 밝기를 줄여 이 전류를 넘지 않게 합니다(PM 지시서·10/04 문서).
- `kDefaultBrightness = 76`(약 30 %, D-036 상한). PM 지시서의 160(약 63 %)은 D-036보다 높아 쓰지 않았습니다. 아크릴 없이 보면 30 %도 눈부십니다 [실측 10/20] — 아크릴을 덮고 USB `BRI n`으로 맞춘 뒤 PM과 정합니다.
- PM 지시서 추정 전류(밝기 255 기준): 약 0.8 A · 중 1.4 A · 강 3.5 A. 실측은 #48.

## 확인 [실측 10/20, Pi 4 + 이 보드 + LED 판, 시각은 Pi 4 시계(NTP 없음) 기준]

- 부팅: `READY FAILSAFE IND 0 LOCK 1`, `dbg wdt 2s init=0 add=0`
- SafetyCore(#56·#47 빌드)와 연결 → `[uart:cook] ack=true` (첫 명령은 부팅 잡음에 붙어 `ERR` → 즉시 재전송으로 회복)
- 약·중·강 10 s씩 연속 송신: `ACK IND n`·`ACK LOCK 0` 각 10/10, 이후 `FAILSAFE timeout IND 0 LOCK 1`. 원이 매끈하고(지그재그 설정 맞음) 색이 빨강·주황·노랑(색 순서 GRB 맞음), 전환이 부드러움 (육안)
- `IND 2` + `LOCK 1`: 파란 점멸이 화면 위에 덧씌워짐 (육안)
- 워치독 (`pio run -e wdt_test`): USB `H` → `dbg hang` 2.08 s → `dbg reset_reason=6`(task watchdog)·`READY FAILSAFE IND 0 LOCK 1` 4.39 s (멈춤 후 2.31 s 리셋)
- F1: 중불·팬 2단 송신 중 Pi 4 어댑터 분리 → 쿡탑 빨간 표시 + 소등, 팬 최고 속도 (육안). 재연결 후 SafetyCore 기동 → 두 보드 2 s 안 `ack=true`

```bash
pio run                          # CI와 같은 빌드
pio run --target upload
pio device monitor               # USB 디버그. BRI 30 같은 밝기 조정
pio run -e wdt_test -t upload    # 워치독 시험 전용 (USB로 'H' 보내면 2 s 뒤 리셋)
```
