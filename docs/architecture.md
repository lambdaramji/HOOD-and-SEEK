# HOOD&SEEK 시스템 구조 — 코드 기준 한 장

- 작성자: 임재범
- 작성일: 2026-10-25
- 상태: draft
- 카테고리: architecture
- 기준: main `0dfd04c`. 이 문서는 **지금 코드가 하는 일**을 적는다. 왜 그렇게 정했는지는 [S3~S10 설계서](2026-10-19-s3-s10-design-v0.1.md)와 [결정 대장](ai-integration-decisions.md)을 본다.
- 문서와 코드가 다르면 코드가 맞다고 보고 §7 표에 남긴다. 고치는 일은 그 문서 담당자가 한다. 이 PR에서 직접 고친 것은 README.md(voice 구성·폴더 트리)와 safety_core/README.md(파일 표·ctest 목록)뿐이다.
- 실제 주소·장치 식별값은 적지 않는다.

## 1. 한눈에

```
 Pi 5 (ATLAS Video)                                Pi 4 (ATLAS Headless)                ESP32 ×2
┌──────────────────────────┐                    ┌──────────────────────────────┐
│ flutter_app   (화면·터치) │──ui_cmd / hello──▶│                              │ UART 115200
│               ◀──────────│──── state 2 Hz ────│   SafetyCore (safetycore)    │──FAN n──▶ 팬 컨트롤러
│ vision_node   (카메라)   │──vision ≈3 Hz ───▶│   유일한 판정자               │──IND n / LOCK──▶ 쿡탑 시뮬레이터(LED 판)
│ voice_node    (마이크)   │──voice ──────────▶│   TCP :7420 서버              │◀── ACK ──
└──────────────────────────┘                    │   I2C: MLX90640 · SPS30       │
                                                └──────────────────────────────┘
```

- **판정은 Pi 4 SafetyCore 하나만 한다.** 다른 노드는 관측을 보내거나(vision·voice) 보여 주기만 한다(Flutter). ESP32는 받은 명령을 집행하고, 명령이 끊기면 스스로 안전 상태로 간다.
- 판정 숫자는 `profiles/default.json` 한 곳에만 있다. 코드에 임계값을 새로 쓰지 않는다(CLAUDE.md).

## 2. 노드

| 노드 | 기기 | 언어·진입점 | 하는 일 | 하지 않는 일 | 문서 |
|---|---|---|---|---|---|
| SafetyCore | Pi 4 | C++17, `safety_core/src/main.cpp` | 센서 읽기, 판정(`Policy::tick`), ESP32 구동, state 발행, D-Bus `GetState` | — | [safety_core/README](../safety_core/README.md) |
| Flutter | Pi 5 | Dart, `flutter_app/lib/main.dart` | state 표시, 터치 → `ui_cmd`, 경고음 | 임계값 비교·잠금 해제 판단 | [flutter_app/README](../flutter_app/README.md) |
| vision-node | Pi 5 | C++17, `vision_node/src/main.cpp` | 프레임마다 사람·반려동물 최고 후보와 ROI 겹침 보고 | 재실·고양이 판정, 디바운스 | [vision_node/README](../vision_node/README.md) |
| voice-node | Pi 5 | C++17, `voice_node/src/audio_observer.cpp` | 호출어 「하이엘지」 + 명령 한 문장 → intent. 발행은 기본 끔(`--publish`) | 명령 허가(Core 몫) | [voice_node/README](../voice_node/README.md) |
| 팬 컨트롤러 | ESP32 | Arduino, `firmware/esp32_fan_controller/` | GPIO26 PWM 4단, 0→1단은 300 ms 3단 킥 | 판정. USB 명령 받기 | 폴더 README |
| 쿡탑 시뮬레이터 | ESP32 | Arduino + FastLED, `firmware/esp32_cooktop/` | 16×16 LED 판으로 화구 단수·잠금(파랑)·fail-safe(빨강) 표시. **실제 인덕션이 아니다** | 판정. 물리 입력 | 폴더 README |

이 문서의 「인덕션 단수」는 쿡탑 시뮬레이터 화구 단수다(불변식 ⑧). 화면은 실제 인덕션이 연결된 앱처럼 「인덕션」이라고 쓰지만(D-047), 저장소 문서에서는 시뮬레이터임을 밝힌다.

## 3. 링크

### 3.1 TCP :7420 (Pi 5 → Pi 4)

계약은 [contracts/README](../contracts/README.md)와 `contracts/*.schema.json`. 아래는 계약 표 밖에서 코드가 정한 값이다.

| 항목 | 값 | 근거 |
|---|---|---|
| 서버 | SafetyCore 하나. 모든 주소에서 받음 | `safety_core/src/tcp_server.cpp` |
| 줄 | UTF-8 JSON 한 줄, 4096바이트 넘으면 서버가 연결을 끊음 | 같음 |
| hello | 접속 5 s 안에 보내야 함. 첫 hello의 `src`로 연결 역할이 정해지고, 두 번째 hello나 hello 전 메시지는 버림. 서버는 hello에 최신 state 한 줄로 답함 | `tcp_server.cpp`, `message.cpp` |
| 연결 수 | 최대 8. 넘으면 바로 닫음 | `tcp_server.cpp` |
| 보낼 줄이 64 KiB 넘게 밀리면 | 그 클라이언트를 닫음 | 같음 |
| 대기열 | 명령 16개(가득 차면 새 것 버림), vision 16개(가득 차면 옛 것 버림) | 같음 |
| seq | 연결마다 따로 검사, 이전 이하면 버림. 클라이언트는 연결마다 hello 0부터 다시 셈. state seq는 프로세스 하나에 하나 | `tcp_server.cpp`, `main.cpp` |
| 명령 신선도 | 받은 지 5 s 넘은 명령은 버림 | `main.cpp` |
| 틀린 메시지 | 버리고 `rx_dropped`만 셈. 판정에 영향 없음 | `tcp_server.cpp` |
| state 주기 | 500 ms tick. voice·ui_cmd가 오면 바로 한 번 더 돌지만 2 Hz 일정은 그대로. 센서·vision 변화는 다음 tick을 기다림 | `main.cpp` |

클라이언트 셋(Flutter·vision·voice)은 모두 1 s 간격으로 다시 붙고 `TCP_USER_TIMEOUT` 2 s를 건다. Flutter는 state가 3 s 없으면, voice는 줄이 3 s 없으면 소켓을 버린다. vision은 state를 읽고 버린다.

### 3.2 UART (Pi 4 → ESP32)

| 항목 | 값 |
|---|---|
| 포트 | 팬 `/dev/ttyAMA3`, 쿡탑 `/dev/ttyAMA5`. ESP32 쪽은 Serial2 GPIO16/17 |
| 설정 | 115200 8N1, 흐름 제어 없음 |
| 보내는 줄 | `FAN n`, `IND n`, `LOCK 0|1` (`STAT`는 진단). tick마다 목표를 넘김 |
| 받는 줄 | `ACK <명령>`, `READY FAILSAFE …`, `FAILSAFE timeout …`, `ERR …` |
| 재전송 | 목표가 바뀌면 바로, 그 뒤 1 s마다(이게 heartbeat). 다른 ACK·READY·FAILSAFE·ERR가 오면 최소 200 ms 간격으로 다시 보냄 |
| ack | 목표마다 **같은 줄의 ACK**가 2 s 안에 있어야 `ack=true` → 아니면 `FAN_NOACK`·`IND_NOACK` |
| 판정 루프가 멈추면 | 2 s 동안 새 목표가 없으면 송신을 멈춘다 → ESP32가 3 s 뒤 fail-safe |
| ESP32 | 켜지면 fail-safe로 시작. 유효 명령이 3 s 없으면 팬 3단 / 쿡탑 0단 + 잠금. 하드웨어 watchdog 2 s. `STAT`은 타이머를 갱신하지 않음. 15자 넘는 줄은 통째로 버림 |

근거: `safety_core/src/uart.{h,cpp}`, `firmware/*/src/main.cpp`.

### 3.3 센서 (Pi 4 I2C `/dev/i2c-1`)

- MLX90640(0x33): 서브페이지 2 Hz → 프레임 1 Hz. 768 화소 중 700 이상이 정상일 때만 최대·중앙값을 낸다. 5번 연속 오류면 2 s마다 다시 초기화.
- SPS30(0x69): 1 Hz, CRC 검사. PM2.5 > 2000은 잘라서 포화로 표시. 3 s 새 샘플이 없으면 측정을 다시 시작시킨다.
- 정상 읽기만 기록한다. 끊기면 `P_fault_s`(3 s) 뒤 fault.

근거: `safety_core/src/sensors.cpp`.

### 3.4 D-Bus (Pi 4)

`com.atlas.HoodSeekSafetyCore1`, 메서드 `GetState() → s`(최신 state 한 줄). 이름을 못 잡아도 5 s마다 다시 시도하고 판정 루프는 계속 돈다. root만 소유·호출(정책 파일 `safety_core/files/dbus/`).

## 4. SafetyCore 안

스레드: `tcp`(poll 하나), `uart_fan`, `uart_cook`, `sensor`, `dbus`(기기 빌드만), `policy`(메인 루프). 공유 상태는 뮤텍스 하나로 지키는 `Snapshot`이고 시간은 단조 시계만 쓴다 (`snapshot.h`).

`Policy::tick`은 입출력·스레드·시계가 없는 순수 함수다. 단계는 `policy.cpp` 주석 번호 그대로:

| 단계 | 하는 일 |
|---|---|
| 1 | 센서 유효·fault. 한 번도 못 읽은 센서는 부팅 유예 |
| 2 | SPS30 baseline(처음 60 s 중앙값 → 정상일 때만 천천히), `PM_WARN` 후보, 과한 연기(팬 3단 중 포화 연속) |
| 3 | presence. vision 만료면 unknown, unknown 시간은 부재로 안 셈 |
| 4 | 고양이 연속 hit — 고양이 감시 중(사람이 `P_cat_absent_s` 5.0 s 안 보임)일 때만, 화재 쪽 부재와 따로 (D-060·D-065) |
| 5 | hazard 전이표 — 위에서 첫 일치: fault > 절대 고온 > (65 °C급 ∧ 과한 연기)=danger > 둘 중 하나=warning > 강불=high > 중불=mid > low(약불·대기) (D-054, D-059) |
| 6 | 히스테리시스: 오를 땐 즉시, 내릴 땐 `P_release_s`(danger면 `P_release_danger_s`) 유지 뒤 |
| 7 | 잠금: danger·fault → `locked_danger`. 해제 = ACK 래치 ∧ ≤mid 10 s ∧ 재실. `locked_cat` = 고양이 감시 ∧ 고양이 hit 연속, 해제 = 사람이 보이며 5 s 고양이 없음 (D-060) |
| 8 | 명령(도착 순서). 켜기·올리기만 `allowed_on` 검사, 끄기·내리기는 늘 집행. 거부면 `CMD_REJECTED` |
| 9 | 인덕션 자동 하향: danger·fault·`locked_cat` → 0, 재실 아님 ∧ ≥mid → 1단까지 |
| 10 | 팬 하한: hazard별 값, 조리 중 하한, 재실 아님 ∧ ≥mid → 3, 센서를 아직 못 읽었으면 3 |
| 11 | reasons (enum 순서, 온도·PM 계열은 하나씩) |
| 12 | alert (이번 tick의 가장 높은 이벤트, `P_alert_hold_s` 뒤 NONE) |

profile: 부팅 때 읽고, 키 하나라도 없거나 범위가 틀리면 전체 거부 → **fail-safe 출력**(fault·unknown·`locked_danger`·팬 3·인덕션 0, `reasons` 빈 배열). `SIGHUP`이면 다시 읽고, 실패하면 이전 profile을 유지한다. `tick`이 예외를 던지면 그 tick은 fail-safe 출력이다.

## 5. 고장 때

| 고장 | 일어나는 일 |
|---|---|
| 센서 읽기 실패 3 s | fault → 팬 3·인덕션 0·`locked_danger`·`SENSOR_FAULT` |
| vision 2 s 끊김 | presence unknown, `VISION_STALE`. ≥mid이면 팬 3·인덕션 1단까지 |
| vision 프로세스 멈춤 | `--stall-ms`(10 s) 뒤 종료 코드 3 → systemd가 1 s 뒤 재시작 |
| voice 없음 | 명령이 안 올 뿐. 판정 영향 없음 |
| Pi 4 전원 끊김 | ESP32가 3 s 뒤 fail-safe(팬 3, 쿡탑 0 + 잠금 빨강). Flutter는 3 s 뒤 「연결 끊김」. **복구는 수동 기동 1회** |
| SafetyCore 멈춤·죽음 | UART 송신이 2 s 뒤 멈춤 → ESP32 3 s 뒤 fail-safe. systemd `Restart=always` |
| Pi 4↔Pi 5 LAN 끊김 | 클라이언트 2~3 s 뒤 끊고 1 s마다 재접속. Core는 vision 만료로 unknown 처리, 열·연기 판정은 계속 |
| ESP32 ACK 없음 2 s | `FAN_NOACK`·`IND_NOACK`. 인덕션 ACK가 없으면 켜기를 거부. 재전송은 계속 |
| ESP32 펌웨어 멈춤 | watchdog 2 s 리셋 → fail-safe로 다시 시작 |
| profile 오류 | 부팅 때: fail-safe 출력. `SIGHUP` 때: 이전 profile 유지 |
| 틀린 TCP 메시지 | 버리고 셈만. 판정 영향 없음 |

## 6. 기기 파일과 기동

| 기기 | 경로 | 무엇 |
|---|---|---|
| Pi 4 | `/data/share/usr/atlas/services/com.atlas.HoodSeekSafetyCore1/safetycore` | SafetyCore (arc ipk 설치 위치) |
| Pi 4 | `/data/hoodseek/profile.json` | 판정 값 (저장소 `profiles/default.json` 사본) |
| Pi 4·Pi 5 | `/data/share/usr/lib/systemd/system/hoodseek-*.service` | systemd 유닛 (`/`가 읽기 전용이라 여기) |
| Pi 5 | `/data/hoodseek/bin/hoodseek_vision`, `vision-node.env`, `config/roi.json` | vision |
| Pi 5 | `/data/ecoai/assets/models/object_detection/yolox.tflite` | vision 모델 |
| Pi 5 | `/data/hoodseek/bin/hoodseek-voice-observer`, `voice-node.env` | voice |
| Pi 5 | `/data/edid-1920x1080.bin` | 화면 1080p EDID |

- **부팅 뒤 자동 기동이 없다.** `/data`가 늦게 마운트되어 유닛 링크가 무시되고, ipk 서비스는 재부팅 뒤 D-Bus 권한으로 거부된다(#46 결정 (4)). 부팅 뒤 사람이 `systemctl start hoodseek-safety-core`(Pi 4), vision·voice 유닛과 앱(Pi 5)을 띄운다.
- Pi 5 부팅 뒤 순서: EDID 확인·적용 → window-manager 재시작 → Wi-Fi 절전 끄기 → 노드·앱 ([deploy/README](../deploy/README.md)).
- SafetyCore는 root가 아니면 바로 끝난다.

## 7. 문서와 코드가 다른 곳

이 PR은 아래 문서를 고치지 않는다. 담당자가 확인하고 고친다. (제안)

| # | 문서 | 문서 내용 | 코드 | 담당 |
|---|---|---|---|---|
| 1 | README.md 40행, CLAUDE.md 112행, 설계서 §1·§5.3 | voice = whisper.cpp + 에너지 VAD, 호출어 「후드」 | 배포 유닛은 SenseVoice(sherpa-onnx) + Silero VAD, 호출어 「하이엘지」(#107) | PM·이수현 → **고침(10/25 PM 문서 정리)** |
| 2 | 설계서 §1·§5.1 | SafetyCore `logger` 스레드가 `/data/hoodseek/log/*.csv` 기록 | logger 스레드는 없음. 판정이 바뀔 때 stdout 한 줄 + `policy` 루프가 tick마다 CSV 한 줄(#201, 10/27) | PM·정인 → **고침(10/25 PM 문서 정리, 10/27 CSV 반영)** |
| 3 | 설계서 §5.1 | 파일 `sensor_mlx.cpp`·`sensor_sps30.cpp` | `sensors.cpp` 하나 + `message`·`profile_loader`·`dbus_service`·`snapshot.h` | PM → **고침(10/25 PM 문서 정리)** |
| 4 | 설계서 §8.2 | SafetyCore 바이너리는 `/data/hoodseek` | ipk 설치 위치 `/data/share/usr/atlas/services/...` | PM·정인 → **고침(10/25 PM 문서 정리)** |
| 5 | 설계서 §8.3 | `deploy/hosts.example.json`, `/data/hoodseek/hosts.json` | 둘 다 없음. 주소는 env 파일·dart-define | PM → **고침(10/25 PM 문서 정리)** |
| 6 | 설계서 §5.2 | vision이 state를 구독해 조리/대기 모드 전환, 모델 YOLOX-Nano/Tiny, MJPEG/YUYV | state는 읽고 버림, 모델은 YOLOX-S 미세조정 v3e, YUYV만 | PM·김병직 → **고침(10/25 PM 문서 정리)** |
| 7 | contracts/README 13행, 설계서 §2.1 | state는 「2 Hz + 변화 즉시」 | 명령이 올 때만 바로 돈다. 센서·vision 변화는 다음 500 ms tick | 정인 |
| 8 | (없음) | — | profile 실패 fail-safe state는 `reasons`가 빈 배열(`SENSOR_FAULT` 없음). 화면은 「온도 센서」 고장으로 보인다 | 정인·임재범 |
| 9 | 설계서 §4.1 | UART 재전송 규칙 일부만 | 일치 ACK만 인정, 불일치·READY·ERR면 200 ms 뒤 재전송, 판정 루프 2 s 멈춤 → 송신 중단 | 정인 |
| 10 | contracts/README §2.1 | — | 서버 한계(연결 8, 64 KiB, 대기열 16), 클라이언트 `TCP_USER_TIMEOUT` 2 s | 정인 |
| 11 | 설계서 §3.5 팬 하한 식 | 센서 미확인 때 조건 없음(§6 F1에만) | 두 센서를 한 번씩 읽기 전에는 팬 3 | PM → **고침(10/25 PM 문서 정리)** |
| 12 | CLAUDE.md 129행 | `voice_node/`·`firmware/esp32_cooktop/`은 「첫 PR에서 생김」 | 둘 다 있음 (README.md는 이 PR에서 고침) | 임재범·PM → **고침(10/25 PM 문서 정리)** |
| 13 | CLAUDE.md 133행 | 불변식 「①~⑧」 | 목록은 ⑨까지 | 임재범·PM → **고침(10/25 PM 문서 정리)** |
| 14 | `firmware/esp32_fan_controller/README.md` 24행 | CLAUDE.md 그림의 MQTT 경로 | CLAUDE.md에 없음 | 정인 |
| 15 | `.github/workflows/system.yml`, `flutter.yml` | `dart`·`pi4_sensor_node` 잡, `packages/hood_seek_protocol/` 경로 | 그 폴더들 없음(늘 skip) | .github 소유자 |
| 16 | `.github/workflows/system.yml` | — | vision_node 잡 없음, safety_core는 `policy_test`만(message·uart·sensor 시험은 CI 밖) | 정인 |
| 17 | `flutter_app/lib/data/tcp_feed.dart`, 화면, pubspec | — | 버전 문자열이 hello `0.3.0`, 화면 `0.4`, pubspec `0.4.0` | 임재범 |
