# S10 레이아웃 전환 체크리스트

- 작성: 2026-10-19 / 상태: 제안 (PM 승인 2026-10-19 "추천대로") / 담당: 임재범(저장소 구조), 정정인(safety_core 골격), 남현지(문서 승인)
- 이슈: "[T7] 저장소 S10 레이아웃 전환" (G1) / 근거: 설계서 §8.1, `docs/ai-integration-decisions.md` "이전 문서 수정 필요 사항"
- 원칙: **한 PR**로 옮긴다. 코드를 고치지 않고 위치만 바꾼다. 아카이브는 삭제가 아니라 `git mv` (이력 보존, ARCHIVE_POLICY).

## 1. 왜 지금

D-019(SafetyCore C++)·D-020(TCP, MQTT 폐기)이 확정돼 `packages/hood_seek_protocol`(Dart 정책), `pi5_gateway`(MQTT 판단), `pi4_sensor_node`(MQTT 발행)는 **설계와 반대 방향의 코드**가 됐다. 활성 위치에 두면 에이전트와 신규 합류자가 그것을 기준으로 작업한다. 단, 이 코드의 **판정 규칙·테스트 케이스·DTO 검증 로직은 SafetyCore 이식 재료**이므로 아카이브로 보존한다.

## 2. 이동표

| 현재 | 이동 후 | 비고 |
|---|---|---|
| `packages/hood_seek_protocol/` | `docs/archive/mqtt-generation/packages/hood_seek_protocol/` | #53에서 먼저 이동(`safety_core/`가 아직 없음). 판정 규칙·테스트 이식 목록은 #57 댓글, 이식할 때는 아카이브 위치에서 읽음 (PM 답변 2026-10-19, #53) |
| `pi5_gateway/` | `docs/archive/mqtt-generation/pi5_gateway/` | serial bridge 코드는 `safety_core/src/uart.cpp` 참고 재료 |
| `pi4_sensor_node/` | `docs/archive/mqtt-generation/pi4_sensor_node/` | MLX/SPS30 읽기 코드는 `safety_core/src/sensor_*.cpp`로 이식 (기존 busybox 로거도 함께 확인) |
| `deploy/mosquitto/` | `docs/archive/mqtt-generation/deploy/mosquitto/` | |
| `deploy/systemd/hood-seek-pi5-gateway.service`, `hood-seek-pi4.service` | `docs/archive/mqtt-generation/deploy/systemd/` | 새 유닛(Q-S10-1 폴백용)은 각 트랙 PR에서 신설. SafetyCore 유닛 위치는 #46 결정을 따름 (PM 답변 2026-10-19, #53) |
| `contracts/*.schema.json` (3종, MQTT) | `docs/archive/mqtt-generation/contracts/` | 새 `contracts/` = hello/vision/voice/ui_cmd/state 5종 + `examples/` |
| `docs/final-demo-architecture.md`, `docs/warning-policy.md`, `docs/development-progress.md` | `docs/archive/` | 결론이 설계서에 있는지 먼저 대조. 없는 결론은 설계서에 추가 후 이동 |
| `inbox/category/*.md` (8종) | `inbox/archive/category/` | 설계서가 단일 기준. 각 문서 머리말에 "대체: 설계서 §x" 한 줄 적고 이동 |
| `inbox/정정인/2026-10-04-single-display-led-cooktop.md` §2 구성도(쿡탑 ← MQTT, 팬 ← Pi 5 USB) | 머리말에 "연결 방식은 설계서 v0.2 §4로 대체" 표시(PR #73에서 처리됨) | 문서 자체는 활성 유지(LED 판·전원·박스 결정의 출처) |
| `docs/to-*.md` (5종) | 내용 확인 후 `docs/archive/` 또는 이슈로 전환 | 전달 사항은 이슈가 더 추적하기 쉽다 |
| `flutter_app/lib/data/*mqtt*` | 삭제 → `tcp_feed.dart` | T6 이슈 범위. 전환 PR에서는 건드리지 않음 |

신설 디렉터리는 **빈 폴더를 만들지 않는다.** `safety_core/`는 T1 골격 이슈가, `vision_node/`·`voice_node/`·`firmware/esp32_cooktop/`·`profiles/`는 각 트랙 첫 PR이 만든다.

## 3. 같은 PR에서 고칠 문서

`docs/ai-integration-decisions.md` "이전 문서 수정 필요 사항" 표 그대로. 아래는 CLAUDE.md 두 절의 교체 초안이다.

### CLAUDE.md "아키텍처" 절 교체 초안

```text
Pi 4 (C++ SafetyCore · 안전 평면) ◀──TCP :7420 line-JSON──▶ Pi 5 (vision-node · voice-node · Flutter)
  MLX90640 / SPS30 (I2C)                                       V4L2+ECO.AI · PulseAudio+whisper · 7인치
  │ UART uart3 ──▶ 팬 ESP32 (GPIO26 PWM, 3 s fail-safe → 팬 3단)
  └ UART uart5 ──▶ 쿡탑 ESP32 (WS2812B 16×16, 3 s fail-safe → OFF+잠금 표시)
```

| 경로 | 역할 |
|---|---|
| `safety_core/` | **유일한 판정.** 센서 읽기, `Policy::tick`, UART 집행, TCP state 발행. `src/policy.*`는 T2 |
| `profiles/default.json` | 판정 파라미터. 코드에 숫자를 박지 않는다 |
| `contracts/` | TCP 메시지 5종 JSON Schema + examples. 계약 표는 contracts/README.md |
| `vision_node/` `voice_node/` | 관측·의도 **보고만.** 누적값·디바운스 없음 |
| `flutter_app/` | state 표시 + ui_cmd 전송. 임계값 비교를 앱에서 하지 않는다 |
| `firmware/` | ESP32 2종 집행 + 독립 fail-safe |
| `deploy/` | systemd 유닛(폴백), config.txt 조각, hosts.example.json |

### CLAUDE.md "안전 불변식" 절 교체 초안

- **`Policy::tick`의 hazard 전이표 순서(설계서 §3.3)가 곧 안전 우선순위다.** fault → T_abs → T_metric danger → smoke_confirmed → warning → caution → normal. 조건을 중간에 끼우면 PR에 근거.
- **팬은 `fan_min` 아래로 내려갈 수 없다.** `fan_level = max(fan_min, requested)`. 사용자·음성 요청도 같다.
- **vision 2 s 만료 → presence=unknown, cat 후보 소멸.** Flutter는 state 3 s 만료 → "연결 끊김". ESP32는 명령 3 s 끊김 → 팬 3단 / 쿡탑 OFF+잠금.
- **Pi 5 관측 노드의 누적값은 받지 않는다.** vision 메시지에 `consecutive_hits` 같은 필드를 두지 않고 SafetyCore만 연속 판정한다.
- **고양이 검출 하나로 잠그지 않는다.** `presence=absent`가 선행돼야 `locked_cat` 후보.
- **어떤 단일 고장에서도 팬은 올라가고 인덕션은 내려간다.** 반대 방향 경로가 생기면 설계 오류.
- TCP·UART·JSON 경계에서 범위·타입·seq·allowlist 검증. 검증 실패는 폐기 + 카운터, **상태를 바꾸지 않는다.**
- 쿡탑은 **시뮬레이터**다. UI 문구·보고서에서 실제 인덕션 제어처럼 표현하지 않는다.

### CLAUDE.md "검증 명령" 절

CONTRIBUTING.md §2 표를 그대로 옮긴다. 트리거 경로: `safety_core/**` `profiles/**` `contracts/**` `firmware/**` `flutter_app/**`.

### AGENTS.md "HOOD&SEEK 경계" 절 교체 초안 (계약 v1 #52 기준)

현재 L57~L61은 MQTT 세대 문구다. 특히 L60 "Flutter는 … 팬 요청만 보냅니다"는 계약 v1의 `ui_cmd` 3종(`contracts/README.md`)과 다르다. L57~L61을 아래로 바꾸고 L62~L63은 유지한다.

```text
- Pi 4 SafetyCore(C++)가 유일한 판정입니다. 센서 읽기, Policy::tick, UART 집행, TCP state 발행을 담당합니다.
- Pi 5 vision-node·voice-node는 관측·의도를 TCP로 보고만 하고 누적값·디바운스를 두지 않습니다.
- ESP32 2대는 명령을 집행하고 UART 명령이 3초 끊기면 팬 3단 / 쿡탑 OFF+잠금 표시 fail-safe를 적용합니다.
- Flutter는 SafetyCore가 확정한 state를 표시하고 ui_cmd 3종(FAN_SET 0..3, IND_SET 0..3, ACK_ALERT)만 보냅니다. ACK_ALERT는 위험 잠금 해제의 필요조건 하나일 뿐 잠금을 직접 풀지 못하며, 안전 기준 변경은 허용하지 않습니다.
- TCP·UART·JSON 경계에서는 범위, 타입, seq, 신선도와 allowlist를 검증합니다.
```

## 4. PR 완료 기준

- [ ] `git mv`만 사용. `git log --follow`로 이력 추적되는지 확인
- [ ] System CI `discover` job 출력에서 `dart=[]`, `pi4_sensor_node=false`, `esp32=["esp32_fan_controller"]`
- [ ] 활성 문서(README·CLAUDE·AGENTS·contracts/README)에 `packages/`·`pi5_gateway`·`MQTT`·`Mosquitto` 문자열이 남지 않음: `rg -l "pi5_gateway|hood_seek_protocol|Mosquitto" --glob '!docs/archive/**' --glob '!inbox/archive/**'`
- [ ] `docs/archive/README.md`(없으면 신설)에 이동일·사유·대체 문서 기록
- [ ] P-009 문서 상태 `확정`
- [ ] CODEOWNERS "MQTT 세대" 블록 삭제
