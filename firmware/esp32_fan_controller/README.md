# ESP32 fan controller

Pi 4 uart3(`/dev/ttyAMA3`)에서 GPIO UART로 `FAN 0\n`부터 `FAN 3\n`까지의 한 줄 명령만 받습니다 (설계서 §4.2, Q-S6-1). GPIO26 PWM은 IRLZ44N gate로 연결하며 MOSFET 보드와 ESP32 GND를 공통으로 둡니다.

| Pi 4 (물리 핀) | 선 | ESP32 |
|---|---|---|
| 7번 GPIO4 (uart3 TX) | **1 kΩ 직렬** | GPIO16 (Serial2 RX) |
| 29번 GPIO5 (uart3 RX) | **1 kΩ 직렬** | GPIO17 (Serial2 TX) |
| 30번 GND | 직결 | GND |

- 직렬 저항은 한쪽 보드만 꺼졌을 때 켜진 쪽의 TX(쉬는 동안 3.3 V)가 꺼진 쪽 GPIO의 보호 다이오드를 거쳐 전원을 밀어 넣는 것(back-powering)을 막습니다. M11(Pi 4 전원 차단)이 바로 이 상황입니다. 1 kΩ면 흐를 수 있는 전류가 약 3 mA 이하로 제한되고, 115200 bps 신호에는 영향이 없습니다 [추정 — 확인: Pi 4를 끈 상태에서 ESP32 TX 선 전류를 멀티미터로 측정]. 1~4.7 kΩ 범위면 됩니다.
- DFRobot FireBeetle ESP32 V3.0 실크 표기: GPIO16 = `DI/IO16`, GPIO17 = `LRCK/IO17`. 바로 옆 `DO/IO4`와 헷갈리지 않게 합니다.

동작:

- 부팅 직후: 팬 3단, `READY FAILSAFE FAN 3`
- 정상 명령: `ACK FAN n`
- PWM 100 Hz, 듀티 0 / 33 / 67 / 100 %. 25 kHz에서는 1·2단에서 돌다 멈춤 [실측 10/21 #64 L5]
- **킥스타트**: `FAN 0` → `FAN 1`일 때만 0.3 s 동안 3단 듀티로 기동한 뒤 1단 (1단 33 %는 멈춘 상태에서 기동 못 함 [실측 10/22 #65]). 같은 단수 재전송·다른 전이에는 없음. `ACK FAN 1`은 킥 시작 때 바로 나감. 킥 도중 다른 단수·fail-safe가 오면 그 자리에서 그 단수
- 잘못된 명령: 상태 유지 후 `ERR ...`
- 15자를 넘는 줄: `ERR command-too-long` 한 번, 그 줄은 다음 `\n`까지 통째로 버림 (뒷부분을 명령으로 해석하지 않음)
- 3초 동안 정상 명령 없음: 팬 3단 fail-safe, `FAILSAFE timeout FAN 3`
- 하드웨어 워치독 2 s: loop가 멈추면 리셋 → 부팅 fail-safe (D-039). 부팅 때 USB로 `dbg wdt 2s init=0 add=0`(0 = ESP_OK, 실패면 `FAILED`)
- USB 시리얼은 플래시·디버그 출력 전용입니다 (같은 줄 사본 + `dbg …`). **USB로 보낸 명령은 읽지 않습니다.** Pi 5 → USB → ESP32로 명령하던 MQTT 세대 경로(`pi5_gateway`의 `HOOD_SEEK_ESP32_SERIAL`, CLAUDE.md 아키텍처 그림)는 더 이상 쓰지 않으며 #53에서 아카이브됩니다.

한계:

- 회전 감지가 없습니다. 1단으로 도는 중에 팬 전원만 잠깐 끊겼다 돌아오면, 같은 단수에는 킥이 다시 걸리지 않아 멈춘 채로 남을 수 있습니다(ACK는 계속 정상) [추정 — 확인: 1단 회전 중 팬 5 V 선 1 s 분리 후 재연결, #100 리뷰 PM]. 다른 단수로 한 번 바꿨다가 `FAN 0` → `FAN 1`을 보내면 다시 기동합니다.
- 워치독 리셋부터 `applyFanLevel(3)`까지 약 0.4 s 동안 GPIO26이 풀려 팬이 멈춥니다 [실측 2026-10-19: 멈춤 2.02 s → `READY` 2.40 s]. 리셋이 반복되면 팬이 켜졌다 꺼졌다 합니다. 펌웨어로는 막을 수 없습니다. 지금 게이트 저항은 **G–GND 10 kΩ 풀다운**이라(10/23 교체) 리셋 중에는 꺼짐 쪽입니다. 풀업(G–3.3 V 약 10 kΩ)으로 바꾸면 리셋 중에도 게이트가 켜짐 쪽에 머물러 팬이 멈추지 않을 것으로 봅니다(GPIO26은 부팅 strapping 핀이 아님) [추정 — 확인: `wdt_test`로 리셋 중 GPIO26 전압과 팬 회전 관찰].

ESP32 전원은 Pi 4에서 받지 않습니다. Pi 4가 꺼져도(M11) 팬 fail-safe가 살아 있어야 하므로 별도 USB 전원을 씁니다.

```bash
python -m pip install platformio==6.1.19
pio run                          # CI와 같은 빌드
pio run --target upload
pio device monitor
pio run -e wdt_test -t upload    # 워치독 시험 전용 (USB로 'H' 보내면 2 s 뒤 리셋)
```

팬 전원은 ESP32 USB 전원과 분리한 외부 5V를 씁니다. 구동 회로는 **IRLZ44N 로우사이드**입니다: GPIO26 → 220 Ω → G, G–GND 10 kΩ, D → 팬(−), S → GND, 1N4001은 팬 양단(띠가 +5 V 쪽) [실측 10/23 #100: 정지 기동 0→1·0→2 각 5/5, 3단 1분 뒤 MOSFET 차가움, D–GND 0.013 V]. 10/22까지는 2N2222A 임시 회로였습니다.
