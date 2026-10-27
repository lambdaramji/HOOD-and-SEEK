# P-009 · Pi4/Pi5 역할 재배치 (Safety Monitor 패턴)

- 상태: **확정** (2026-10-19, [D-011](../ai-integration-decisions.md) — Pi 4 = SafetyCore 판정, Pi 5 = 인지·HMI. 전체 설계는 [S3~S10 설계서](../2026-10-19-s3-s10-design-v0.1.md))
- 작성자: 임재범
- 작성일: 2026-08-30
- 결정권자: 현지(안전 경계 재정의) · 정인(하드웨어 배선)
- 관련: `D-001` 재검토, [의사결정 대장](../ai-integration-decisions.md), [상세 분석](../../inbox/임재범/2026-08-30-safety-monitor-role-reallocation.md), [Pi4 · Pi5 역할, 설계가 맞는가](../2026-08-30-pi4-pi5-role-design-review.md)

## 한 줄 요약

**안전 판단을 Pi5에서 Pi4로 옮기고, 비전 추론을 Pi4에서 Pi5로 옮긴다.**

## 무엇을 결정하는가

지금은 센서가 Pi4에 있는데 판단과 팬 제어는 Pi5에 있다. **랜선이 안전 경로 안에 들어가 있다.** 이걸 뒤집어 Pi4 안에서 닫히는 로컬 안전 루프를 만들지 정한다.

```text
현행:  I2C 센서(Pi4) ──랜선──▶ Pi5 판단 ──USB──▶ ESP32 ──▶ 팬
                       ↑ 랜선이 끊기면 정상 센서값을 쥔 Pi4가 아무것도 못 한다

제안:  I2C 센서(Pi4) ─▶ Pi4 판단 ──USB──▶ ESP32 ──▶ 팬      ← 랜선 없음
       카메라(Pi5) ─▶ YOLO ──랜선──▶ Pi4                    ← 관측만 랜선을 탄다
```

기능 안전 설계의 **안전 감시기(safety monitor) 패턴**이다. 작고 검증 가능한 안전 채널을, 크고 복잡한 기능 채널에서 분리한다.

## 왜 지금인가

`pi4_sensor_node`가 아직 스텁이다. I2C 드라이버도 카메라도 YOLO도 없고, 하드코딩한 5개 샘플을 순환 발행한다. 마지막 `{95.0, 18.0, true}`는 위험 온도(≥90°C) 분기를 관측 노드에서 한 번 내기 위한 것이다.

```cpp
Reading demo_reading(std::size_t sample) {
  constexpr Reading sequence[] = {
      {42.0, 18.0, true},
      {58.0, 42.0, true},
      {72.0, 96.0, true},
      {76.0, 110.0, false},
      {95.0, 18.0, true},
  };
  return sequence[sample % std::size(sequence)];
}
```

`cat-detection`도 계약·DTO·tracker만 있고 발행자가 없다. **지금은 설계 결정이지만, 구현이 들어간 뒤에는 값비싼 재작업이 된다.**

## 두 안 비교

| | A안 · 현행 유지 | B안 · 재배치 (제안) |
|---|---|---|
| 안전 판단 | Pi5 | **Pi4** |
| 비전 추론 | Pi4 | **Pi5** |
| 랜선 끊길 때 팬 제어 | ✗ 정지 (ESP32 3초 fail-safe만 남음) | ✓ 정상 |
| 랜선 끊길 때 온도·PM 판단 | ✗ 불가 | ✓ 정상 |
| 비전 성능 | Pi4 A72 1.5GHz. 합격선 2 FPS | Pi5 A76 2.4GHz. 여유 확보 |
| Pi4 부하 | YOLO + I2C + MQTT 경합. 케이스 팬 필수 | I2C + 판단만. 경합 해소 |
| 안전 판단이 도는 OS | Flutter·GPU·음성과 같은 보드 | Headless. 표면적 최소 |
| 이전 비용 | — | 스텁 상태라 사실상 0 |
| 계약 변경 | 없음 | `schema_version` 상향 필요 (아래) |

**재배치 후 최종 FPS는 아직 실측 전이다.** Pi5로 옮기면 병목이 풀린다고 보지만, `pi5_gateway`·계약과 통합된 수치는 기기에서 다시 측정한다.

> **[실측 2026-09-20] 부분 갱신.** Pi5에서 HD-3000 카메라(V4L2 직접 캡처) + ECO.AI TFLite(XNNPACK, CPU 백엔드)로 YOLOX 실추론을 실행해 **평균 처리 313.7ms(약 3.2 FPS)**를 확인했다. 사람이 카메라 앞에 서자 실제로 `person_detected`가 `true`로 전환되고 디바운스 로직(연속 프레임 조건, 오검출 한 프레임에 흔들리지 않음)도 실기에서 의도대로 동작했다. **다만 이건 이 절이 가정한 "비전이 Pi5로 완전히 재배치된 뒤"의 최종 성능이 아니라, 별도 저장소(jaebeom/HOOD-SEEK)의 실험 코드로 얻은 값**이며, 대회 실제와 같은 모델·해상도인지는 확인했으나(모델 해시 일치) `pi5_gateway`·`packages/hood_seek_protocol`과의 통합 성능은 아니다. 8/30의 3.7~3.8 FPS는 `VisionService1` 경로 참고값이며 RPi5 공식 미지원이므로 이 안의 승인 근거로 쓰지 않는다. 근거: [jaebeom PR #1 실기 검증](../../inbox/김병직/2026-09-20-vision-node-pi5-runtime-gap.md), [V4L2 수정과 라이브 검증](../../inbox/김병직/2026-09-20-vision-node-v4l2-fix-and-live-test.md).

## 하드웨어 세팅

### 물리적으로 바꾸는 것은 USB 두 개뿐이다

| 부품 | 현행 | 제안 |
|---|---|---|
| **HD-3000 USB 카메라** | Pi4 | **Pi5** |
| **ESP32 USB** | Pi5 | **Pi4** |
| MLX90640 (I2C) | Pi4 | 그대로 |
| SPS30 (I2C) | Pi4 | 그대로 |
| 디스플레이(7" → 2026-10-23 15.8인치 smartmon 158TF3 터치, D-025) | Pi5 | 그대로 |
| 유선 LAN | 양쪽 | 그대로 |

I2C 센서는 조리면 온도와 PM을 재므로 **판단하는 보드 옆에 그대로 둔다.**

### ★ mosquitto를 Pi4로 옮기지 않으면 제안 전체가 무의미해진다

Pi4 안에서도 프로세스가 둘이다. 센서를 읽는 C++ 노드와 판단하는 Dart 노드가 MQTT로 대화한다. **broker가 Pi5에 남아 있으면 이 대화가 랜선을 두 번 왕복한다.**

```text
잘못된 구성:  센서(Pi4) ──랜선──▶ broker(Pi5) ──랜선──▶ 판단(Pi4) ─▶ ESP32
              랜선이 빠지면 Pi4 안의 두 프로세스가 서로 대화하지 못한다

올바른 구성:  센서(Pi4) ─▶ broker(localhost) ─▶ 판단(Pi4) ─▶ ESP32   ← 랜선 0회
              비전(Pi5) ──랜선──▶ broker(Pi4)                        ← 관측만
              Flutter(Pi5) ◀─랜선── broker(Pi4)                      ← 표시만
```

배선만 바꾸고 broker를 두면 **지금보다 나빠진다.** Pi4 부하는 걱정하지 않아도 된다. mosquitto는 가볍고, Pi4는 YOLO를 내려놓으므로 순감이다.

### 보드별 구성

**Pi 4 (ATLAS Headless)**

| 프로세스 | 역할 | 비고 |
|---|---|---|
| `mosquitto` | broker | **Pi5에서 이설** |
| `hood-seek-pi4` (C++) | MLX90640·SPS30 I2C 읽기 | 비전 부분 제거 |
| `hood-seek-safety` (Dart) | `SafetyPolicy` 판단 + ESP32 serial | **기존 `pi5_gateway` 이설** |

`deploy/systemd/hood-seek-pi5-gateway.service`의 `SupplementaryGroups=dialout`(시리얼 권한)이 Pi4 유닛으로 따라가야 한다.

**Pi 5 (ATLAS Video)**

| 프로세스 | 역할 |
|---|---|
| 비전 노드 | HD-3000 → YOLO → `person`·`cat` 관측 발행 |
| Flutter | 화면 표시, 팬 요청 |
| 음성 | 명령 텍스트 발행 |

### 판단 흐름 (조리면 95°C, 사용자 부재)

```text
1. MLX90640 ─I2C─▶ hood-seek-pi4          : 95.0°C
2. hood-seek-pi4 ─localhost─▶ snapshot 발행
3. 비전 노드(Pi5) ─랜선─▶ person_present: false
4. hood-seek-safety(Pi4) → SafetyPolicy.evaluate
     95 >= dangerTemperatureC(90) → danger, 팬 3단, 인덕션 잠금
5. hood-seek-safety ─USB serial─▶ ESP32 : "FAN 3\n"
6. ESP32 ─GPIO26 PWM─▶ IRLZ44N ─▶ 팬 3단
7. 상태 발행 → Flutter 화면 표시
```

**4~6이 전부 Pi4 한 보드 안에서 일어난다.** 3번(비전)이 끊겨도 1·2·4·5·6은 계속 돈다.

## 계약 파급효과

역할을 바꾸면 계약이 함께 바뀐다.

| 계약 | 현행 | 재배치 후 | 조치 |
|---|---|---|---|
| `sensor-snapshot.person_present` | Pi4가 YOLO로 판정해 snapshot에 실음 | **비전이 Pi5로 가므로 분리 필요** | 필드 분리 + `schema_version` 2 |
| `cat-detection.source` | `const: "pi4"` | `"pi5"` | 계약 수정 + Dart DTO 동기화 |
| `sensor-snapshot.source` | `const: "pi4"` | 유지 | 변경 없음 |
| **MQTT topic 이름** | 발행 보드 기준 (`pi4/`, `pi5/`) | `pi4/cat/detection`을 Pi5가, `pi5/state`를 Pi4가 발행하게 되어 **이름과 실제가 어긋남** | 이름 규칙 재검토 (결정 항목 9) |

`person_present`는 YOLO 산출물인데 [`sensor-snapshot.schema.json`](../archive/mqtt-generation/contracts/sensor-snapshot.schema.json)의 필수 필드다. 비전이 Pi5로 가면 **이 필드만 다른 보드에서 오게 되므로** 계약을 쪼개야 한다.

topic 이름은 지금 발행 보드를 기준으로 붙어 있다. 5개 중 2개가 재배치 후 실제와 어긋난다.

| topic | 현재 발행자 | 재배치 후 발행자 |
|---|---|---|
| `hoodseek/v1/pi4/snapshot` | Pi4 | Pi4 (그대로) |
| `hoodseek/v1/pi4/cat/detection` | Pi4 | **Pi5** ← 어긋남 |
| `hoodseek/v1/pi5/state` | Pi5 | **Pi4** ← 어긋남 |
| `hoodseek/v1/pi5/fan/request` | Flutter(Pi5) | 그대로 |
| `hoodseek/v1/pi5/voice/text` | Pi5 | 그대로 |

`person_present`를 분리하면 새 topic도 하나 필요하다. **이름을 고칠 거면 `schema_version` 상향과 같은 시점에 한 번에 처리한다.** 파괴적 변경을 두 번 하지 않기 위해서다.

`source` allowlist 검증은 PR #8(2026-08-30 병합)로 `SensorSnapshot`·`FanCommand`에 들어 있다. 재배치 확정 후 `cat-detection` 쪽을 함께 갱신하면 된다.

## 미해결 리스크

### ★ 선행 조건: ATLAS Headless에서 Dart AOT가 도는가

이게 안 되면 **제안 자체를 재고해야 한다.** `SafetyPolicy`를 C++로 포팅하는 선택지는 배제한다 — 단일 진실 공급원이 둘로 갈라져 [CLAUDE.md](../../CLAUDE.md) 규칙 위반이다.

독립 배포를 확인하려면 빌드 호스트에서 Linux ARM64 실행 파일을 만들고 Pi4에서 그 파일만 실행한다. `aot-snapshot`+`dart run`은 SDK가 있는 환경이라 이 명제를 증명하지 못한다.

```bash
dart compile exe \
  --target-os=linux \
  --target-arch=arm64 \
  pi5_gateway/bin/hood_seek_gateway.dart \
  -o hood-seek-safety-arm64
```

2026-09-02에 x86_64 컨테이너에서 시도한 기록은 **실패가 아니라 판정 불가/미실행**이다. 원문 로그와 재실행 절차는 [Pi4 안전 권한 시험](../../inbox/정정인/2026-09-02-pi4-safety-authority-test.md)에 둔다.

### ★ 선행 조건: 두 센서는 함께 읽혀야 한다

Pi4에 판단을 붙이기 전에 MLX90640과 SPS30을 **동시에 polling**하는 최소 기준선을 남긴다. 각 센서가 단독으로 한 번 읽히는 것만으로는 I2C 버스·CPU 부하·샘플 지연을 확인할 수 없다. 결과에는 실행 명령, 두 센서의 성공/실패 수, 관측 기간, timestamp 간격과 오류 로그를 포함한다. 이 확인은 임계값 보정이나 실제 장치 제어 승인을 뜻하지 않는다.

### 비전이 끊겼을 때 `person_present`를 어떻게 볼 것인가

재배치 후에는 센서가 로컬이라 **끊기는 것은 비전**이다. 그런데 `person_present`가 `SafetyPolicy.evaluate`의 세 분기에 입력으로 들어간다 (`hood_seek_protocol.dart:517`, `:519`, `:541`).

| 방식 | 결과 | 위험 |
|---|---|---|
| 부재로 간주 | 안전 쪽으로 기움 | 비전이 잠깐 끊길 때마다 **오경보** |
| 재실로 간주 | 오경보 없음 | 부재 판정이 필요한 순간 **침묵**. 안전 원칙 위반 |

온도·PM 판단은 비전과 무관하게 계속 동작하므로 이 부분은 오히려 지금보다 견고해진다.

### (제안) Fresh / Grace / Unknown-safe

비전 재실 관측을 센서 snapshot과 분리할 때의 초안이다. **결정권자 승인이 아니다.** 항목 5의 결정 칸을 채우지 않는다. 이 정책은 `SafetyPolicy.evaluate`의 offline → 위험 온도 → 위험 PM2.5 우선순위를 약화하지 않는다.

| 상태 | 기준 | attendance-dependent escalation |
|---|---|---|
| **Fresh** | 유효 관측 수신 후 3초 이내 | 최신 `person_present`로 부재+고온·부재+반려동물 분기를 평가 |
| **Grace** | Fresh 만료 뒤 최대 2초 (수신 후 3초 초과~5초 이하) | **새 escalation 금지.** 마지막 제한만 유지 |
| **Unknown-safe** | 관측 없음·무효·5초 초과 | 누락을 `person_present: false`로 바꾸지 않음. 누락만으로 새 부재 escalation을 만들지 않음 |

Unknown을 재실 또는 부재로 위장하지 않는다. 온도·PM 독립 위험 분기와 팬 요구치는 모든 비전 상태에서 계속 평가한다.

### 액추에이터 두 개가 서로 다른 보드로 갈라진다

`final-demo-architecture.md`와 `project-context.md`(둘 다 2026-10-19 아카이브)에 인덕션 잠금이 **ATLAS Video Profile D-Bus**로 적혀 있다. Video Profile은 Pi5다.

| 액추에이터 | 위치 | 랜선 의존 |
|---|---|---|
| 후드 팬 (ESP32) | Pi4 | 없음 |
| **인덕션 잠금 (D-Bus)** | **Pi5** | **있음** |

지금은 가상 상태라 문제가 되지 않지만 실물 연결 단계에서 정해야 한다. 다만 **현행보다는 낫다** — 지금은 팬과 잠금이 둘 다 랜선에 의존하는데, 재배치 후에는 최소한 팬이 보장된다.

## 참고: ESP32 3초 fail-safe는 대회 규격이 아니다

저장소에서 3초 fail-safe 언급 25곳을 전수 확인했다. **전부 팀 내부 문서이고 대회 요구사항을 인용한 곳이 없다.** 설계 근거는 [2026-08-29 codex review](../2026-08-29%20codex%20review.md)의 다음 한 줄뿐이다.

> Pi5 watchdog과 ESP32 timeout을 겹쳐 두어 Pi4 또는 Pi5 한 노드 장애 시에도 팬 3단 fail-safe가 남습니다.

팀이 정한 값이므로 조정할 수 있다. 다만 **두 계층을 겹친다는 설계 의도는 유지한다.**

> **별건이지만 남긴다.** 저장소에 대회 요구사항 문서 자체가 없다. 지금 「대회가 원하는 것」이라 말하는 내용이 전부 구두 전달이라 이번 논의도 같은 이유로 혼선이 있었다. 별도로 정리할 필요가 있다.

---

## 구현 단계 (승인 전 검증 / 승인 후 이설)

아래는 작업 분해다. **승인 전 검증은 1번 결정 전에 해도 된다.** 승인 후 이설은 1번이 승인된 뒤에만 시작한다. 이 표가 역할 재배치를 승인하지 않는다.

| 단계 | ID | 작업 | 주 역할 | 착수 조건 |
|---|---|---|---|---|
| 승인 전 검증 | ARC-001 | Pi4에서 Dart Linux ARM64 독립 실행·무이미지 배포 | 정인 | Headless 접근. 수용: `dart compile exe --target-os=linux --target-arch=arm64` 산출물이 Pi4에서 기동 |
| 승인 전 검증 | ARC-003 | ESP32를 Pi4 USB에 **시험 연결**·3초 fail-safe | 정인 | 현행 Pi5 연결을 제거하지 않음. 수용: `FAN 0`~`FAN 3` allowlist와 3초 단절 |
| 승인 전 검증 | ARC-S1 | MLX90640·SPS30 동시 polling baseline | 정인 | 명령·기간·성공/실패·timestamp 간격·오류 로그 |
| 승인 전 검증 | ARC-V1 | 권장 비전 경로 재측정 (`VisionService` 제외) | 수현·병직 | ECO.AI 직접 추론. 3.7~3.8 FPS는 미지원 경로 참고값 |
| 승인 후 이설 | ARC-002 | mosquitto를 Pi4 localhost로 이설 | 정인 | 1번 승인 + ARC-001 Go |
| 승인 후 이설 | ARC-004 | 안전 gateway를 Pi4로 이설 | 재범·정인 | ARC-001·003 Go. 정책을 C++로 복제하지 않음 |
| 승인 후 이설 | ARC-005 | 비전 노드를 Pi5 관측 발행으로 이설 | 수현·병직 | 1번 승인. 카메라 폼팩터는 2026-09-20 실측됨 |
| 승인 후 이설 | ARC-006 | MQTT/JSON 계약 v2와 소비자 동시 전환 | 재범·수현·정인 | ARC-004~005 |
| 최종 승인 | ARC-007 | LAN·비전·USB 단선 주입·복구·롤백 | 현지·정인 | ARC-004~006. 인덕션은 D-Bus 확정 전까지 가상 |

AOT 실행 또는 무이미지 갱신이 실패하면 C++에 정책을 복제하지 않고 즉시 No-Go로 기록한다. 현행 Pi5 안전 경계를 유지한다.

---

# 결정할 항목

각 항목의 「결정」 칸을 채운다. 기본안은 작성자 제안이며 확정이 아니다.

| # | 항목 | 선택지 | 기본안 | 결정권자 | 결정 |
|---|---|---|---|---|---|
| 1 | **역할 재배치 여부** | 현행 유지 / 재배치 | 재배치 | 현지 | |
| 2 | mosquitto 위치 | Pi5 유지 / **Pi4 이설** | Pi4 이설 | 정인 | |
| 3 | ESP32 USB 연결 | Pi5 / Pi4 | Pi4 | 정인 | |
| 4 | 카메라 USB 연결 | Pi4 / Pi5 | Pi5 | 정인·병직 | |
| 5 | 비전 두절 시 `person_present` | 부재 간주 / 재실 간주 | 부재 간주 + `vision_offline` 상태 표시 | 현지·수현 | |
| 6 | 인덕션 잠금 액추에이터 | ESP32 릴레이(Pi4) / ATLAS D-Bus(Pi5) / 데모까지 가상 | 데모까지 가상 유지 | 현지·정인 | |
| 7 | ESP32 3초 timeout | 유지 / 조정 | 유지 | 정인 | |
| 8 | 계약 개정 시점 | 즉시 / 재배치 확정 후 | 재배치 확정 후 | 재범·정인 | |
| 9 | MQTT topic 이름 규칙 | 발행 보드 기준 유지 / 의미 기준으로 개명 | 의미 기준 개명, 8번과 동시 처리 | 재범·정인 | |

**1번이 부결되면 2~8은 논의하지 않는다.** 1번은 아래 선행 확인이 끝나야 판단할 수 있다.

## 선행 확인 (회의 전)

| 항목 | 담당 | 결과 |
|---|---|---|
| ATLAS Headless에서 Dart AOT 실행 가능 여부 **및 배포 방법** (Yocto 이미지 재빌드 없이 바이너리만 갱신할 수 있는가) | 정인 | **[실측 2026-09-02]** 판정 불가/미실행. x86_64 컨테이너에 Dart·Pi 장치 없음. → [시험 기록](../../inbox/정정인/2026-09-02-pi4-safety-authority-test.md) |
| ESP32 USB를 Pi4로 재배선 가능 여부 | 정인 | |
| MLX90640·SPS30 동시 polling baseline (명령·기간·성공/실패·timestamp 간격·오류 로그) | 정인 | |
| 카메라를 Pi5에 물릴 수 있는 폼팩터인지 | 병직 | **[실측 2026-09-20]** 가능. HD-3000은 USB UVC라 Pi4·Pi5 어느 쪽에 꽂아도 재부팅 없이 즉시 인식됨(`uvcvideo` 드라이버). Pi5에서 실제로 물려 라이브 추론까지 확인했다. 다만 시험 중 Pi5의 한 USB 포트를 담당하는 xHCI 컨트롤러가 죽는 현상(`HC died`)이 있었고 다른 포트로 옮겨 복구함 — 최종 설치에서 어느 포트를 쓸지 확인 필요. 근거: [jaebeom PR #1 실기 검증](../../inbox/김병직/2026-09-20-vision-node-pi5-runtime-gap.md) |

네 확인 결과와 **현지의 safety policy 승인**이 모두 기록되기 전에는 역할 재배치를 구현으로 병합하지 않는다. 이 문서는 보드·broker·USB 역할의 합의를 정리할 뿐이며, firmware·센서 드라이버·정책 코드를 승인하거나 변경하지 않는다. 결정 기록 표는 각 팀원이 직접 채운다.

## 결정 기록

| 팀원 | 역할 | 검토일 | 의견 | 결론 |
|---|---|---|---|---|
| 남현지 | 팀장 · 안전 경계 승인 | | | |
| 정정인 | 임베디드 · 배선 | | | |
| 이수현 | 비전 · 판정 로직 | | | |
| 김병직 | 하드웨어 · 시연 | | | |
| 임재범 | 작성자 | 2026-08-30 | 재배치 찬성. 선행 확인 결과에 따름 | |

확정되면 이 문서의 상태를 `확정`으로 바꾸고, [의사결정 대장](../ai-integration-decisions.md)의 `P-009`를 `D-00x`로 이관한다. **파일명은 바꾸지 않는다.**
