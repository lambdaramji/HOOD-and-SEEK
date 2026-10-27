# Safety Monitor 패턴 적용 — Pi4/Pi5 역할 재배치 제안

- 작성자: 임재범
- 작성일: 2026-08-30
- 상태: draft
- 카테고리: project-management
- 관련 이슈/PR: `P-009`
- 결정 문서: [P-009 Pi4/Pi5 역할 재배치](../../docs/decision/P-009-pi4-pi5-role-reallocation.md)

## 요약

**안전 판단을 Pi4로, 비전 추론을 Pi5로 맞바꾼다.** 현재는 Pi5가 판단과 UI를 함께 맡고 Pi4가 관측을 맡는데, 이를 뒤집는다.

```text
현재:  I2C 센서(Pi4) ──MQTT──▶ Pi5 판단 ──serial──▶ ESP32
                        ↑ 네트워크가 안전 경로 안에 있다

제안:  I2C 센서(Pi4) ─▶ Pi4 판단 ──serial──▶ ESP32     ← 로컬 폐루프
       카메라(Pi5) ─▶ YOLO ──MQTT──▶ Pi4              ← 관측만 네트워크를 탄다
```

기능 안전 설계의 **안전 감시기(safety monitor) 패턴**이다. 작고 검증 가능한 안전 채널을, 크고 복잡한 기능 채널과 분리한다.

**지금이 결정 시점이다.** Pi4 노드가 아직 스텁이라 이전 비용이 사실상 없고, 구현이 들어간 뒤에는 값비싼 재작업이 된다.

## 확인된 사실

### Pi4 노드는 아직 스텁이다

`pi4_sensor_node/src/main.cpp`에 I2C 드라이버도, 카메라도, YOLO도 없다.

```cpp
Reading demo_reading(std::size_t sample) {
  constexpr Reading sequence[] = {
      {42.0, 18.0, true}, {58.0, 42.0, true},
      {72.0, 96.0, true}, {76.0, 110.0, false},
  };
  return sequence[sample % std::size(sequence)];
}
```

하드코딩한 4개 샘플을 순환 발행한다. 발행 topic도 `hoodseek/v1/pi4/snapshot` 하나뿐이라 **`cat-detection`은 계약·DTO·Pi5 tracker만 있고 발행자가 없다.**

### 지금은 네트워크가 안전 경로 안에 있다

센서는 Pi4에 물려 있고 판단과 ESP32 제어는 Pi5에 있다. 폐쇄 LAN이라 Wi-Fi 문제는 없지만, **케이블·스위치·broker 프로세스 중 하나만 죽어도** Pi5는 `offline`을 선언할 뿐이고 정상 센서값을 쥔 Pi4는 아무것도 하지 못한다.

제안 구조에서는 Pi5(화면·AI)가 통째로 죽어도 Pi4의 안전 루프가 그대로 돈다.

### Pi4의 비전 성능이 최대 리스크다

Pi4는 Cortex-A72 1.5GHz, Pi5는 Cortex-A76 2.4GHz다. 둘 다 NPU가 없어 CPU 추론인 것은 같지만 체감 2~3배 차이가 난다.

[검출 모델 통합 제안](../김병직/2026-08-29-vision-model-consolidation.md)의 합격선이 **2 FPS**까지 내려간 이유가 여기 있다. 같은 Pi4가 YOLO 추론 + I2C 센서 3종 폴링 + MQTT 발행을 동시에 하고, 데모 구성도에 케이스 팬이 필수로 적혀 있다.

비전을 Pi5로 옮기면 이 병목과 발열 경합이 함께 풀린다. **다만 실측 전까지 구체적 FPS를 약속하지 않는다.**

### ESP32 3초 fail-safe는 대회 규격이 아니다

저장소 전체에서 3초 fail-safe 언급 25곳을 확인했다. **전부 팀 내부 문서이고, 대회 요구사항이나 외부 규격을 인용한 곳이 없다.** 설계 근거는 [2026-08-29 codex review](../../docs/2026-08-29%20codex%20review.md)의 다음 한 줄뿐이다.

> Pi5 watchdog과 ESP32 timeout을 겹쳐 두어 Pi4 또는 Pi5 한 노드 장애 시에도 팬 3단 fail-safe가 남습니다.

즉 팀이 정한 값이므로 이번 재배치에 맞춰 조정할 수 있다. 다만 **두 계층을 겹쳐 둔다는 설계 의도 자체는 유지해야 한다.**

참고로 저장소에 **대회 요구사항 문서가 아예 없다.** 지금 「대회가 원하는 것」이라 말하는 내용이 전부 구두 전달이라, 이번 논의도 같은 이유로 혼선이 있었다.

## 결정 사항 (제안)

**1. Pi 4 (ATLAS Headless) — 안전 감시기**

I2C 센서 폴링 + Dart `SafetyPolicy` + ESP32 USB serial 제어. 네트워크 없이 닫히는 로컬 안전 루프.

**2. Pi 5 (ATLAS Video) — AI·UI 게이트웨이**

Flutter UI + 터치 + 음성 + 카메라 YOLO 추론. 관측 결과를 MQTT로 Pi4에 단방향 전달.

**3. 신뢰 경계는 유지하되 방향만 바꾼다**

[CLAUDE.md](../../CLAUDE.md)의 불변식 취지는 「비전 노드가 안전 판단을 우회하지 못하게 한다」이다. 재배치 후에도 비전(복잡·비결정론)이 판단자(단순·결정론)에게 재검증받는 구조는 그대로다. `PetAccessTracker`가 Pi4로 옮겨갈 뿐이다.

오히려 **「복잡·AI」와 「단순·안전」이 보드 두 대에 1:1로 떨어져** 더 깔끔해진다.

## 계약 파급효과

릴레이된 검토안에 빠져 있던 부분이다. **역할을 바꾸면 계약이 함께 바뀐다.**

| 계약 | 현재 | 재배치 후 | 조치 |
|---|---|---|---|
| `sensor-snapshot.person_present` | Pi4가 YOLO로 판정해 snapshot에 실어 보냄 | **비전이 Pi5로 가므로 snapshot에서 빠져야 함** | 필드 분리 + `schema_version` 2로 상향 |
| `cat-detection.source` | `const: "pi4"` | `"pi5"` | 계약 수정 + Dart DTO 동기화 |
| `sensor-snapshot.source` | `const: "pi4"` | 유지 (센서는 Pi4에 남음) | 변경 없음 |

`person_present`가 [`sensor-snapshot.schema.json`](../../docs/archive/mqtt-generation/contracts/sensor-snapshot.schema.json)의 필수 필드인데 YOLO 산출물이다. 비전이 Pi5로 가면 **이 필드만 다른 보드에서 오게 되므로 계약을 쪼개야 한다.**

또한 현재 열려 있는 PR #8이 `SensorSnapshot`·`FanCommand`에 `source` allowlist 검증을 막 추가했다. 재배치가 확정되면 `cat-detection` 쪽 allowlist도 함께 갱신해야 한다. **PR #8을 막을 이유는 아니고, 후속으로 처리하면 된다.**

## 비전 두절 시 동작을 새로 정해야 한다

이것도 검토안에 없던 항목이고, **가장 중요한 미결 사항이다.**

현재 `offline` 판정은 「Pi4 snapshot이 3초 끊김」 = 센서 두절이다. 재배치 후에는 **센서가 로컬이라 항상 살아 있고, 끊기는 것은 비전**이다. 성격이 다르다.

문제는 `person_present`가 `SafetyPolicy.evaluate`의 세 분기에 입력으로 들어간다는 점이다 (`hood_seek_protocol.dart:495`, `:497`, `:519`).

- 부재 + 고온 → 위험
- 부재 + 경고 온도 → 경고
- 부재 + 반려동물 접근 → 위험

비전이 끊기면 이 세 분기의 입력이 사라진다. 선택지는 둘이다.

| 방식 | 결과 | 위험 |
|---|---|---|
| 부재로 간주 (보수적) | 안전 쪽으로 기움 | 비전이 잠깐 끊길 때마다 **오경보**가 뜬다 |
| 재실로 간주 | 오경보 없음 | **부재 판정이 필요한 순간에 침묵**한다. 안전 원칙 위반 |

기본값은 보수적(부재 간주)이 맞다고 본다. 다만 오경보 빈도를 실측해야 하고, 별도의 `vision_offline` 상태를 두어 사용자에게 「비전 연결 끊김」을 구분해 보여줄지도 함께 정해야 한다.

**온도·PM2.5 기반 판단은 비전과 무관하게 계속 동작한다.** 센서가 로컬이므로 이 부분은 오히려 지금보다 견고해진다.

## 대회 관점

AI-native OS 부문 심사에서 단순 기능 구현보다 신뢰성 설계가 높게 평가될 가능성이 있다. 발표에서 다음과 같이 설명할 수 있다.

> UI와 AI 비전 같은 무겁고 비결정론적인 작업은 Pi5(Video 프로파일)에 격리하고, 센서 수집과 액추에이터 제어는 가볍고 검증 가능한 Pi4(Headless)에 격리해 **네트워크가 끊겨도 안전 루프가 유지되도록** 설계했습니다.

ATLAS 프로파일 조합(Headless + Video)은 그대로라 [ATLAS 플랫폼 기준](../김병직/2026-08-29-atlas-platform-baseline.md)의 대회 프로파일 요구와 충돌하지 않는다.

## 미확인 사항

| 우선순위 | 항목 | 확인 방법 |
|---|---|---|
| **P0** | **ATLAS Headless(Yocto)에서 Dart AOT 바이너리가 실행되는가** | `pi5_gateway`를 arm64로 AOT 빌드해 Pi4에 올려보는 1~2시간 스파이크. **`SafetyPolicy`를 C++로 포팅하는 선택지는 배제한다** — 단일 진실 공급원이 둘로 갈라져 CLAUDE.md 규칙 위반이다 |
| P1 | ESP32 USB serial을 Pi4로 재배선 가능한가 | 물리 배선 확인. 옮기지 않으면 판단 결과가 다시 MQTT를 타서 이 제안의 핵심 이점이 사라진다 |
| P1 | 카메라를 Pi5에 물릴 수 있는 폼팩터인가 | 후드 장착 위치와 USB 케이블 길이 확인 |
| P1 | 비전 두절 시 `person_present` 기본값 | 위 「비전 두절 시 동작」 절. 회의 결정 필요 |
| P2 | MQTT broker 위치 | Pi5에 둬도 되지만, **broker가 죽어도 Pi4 안전 루프는 살아 있어야 한다** |
| P2 | ESP32 3초 timeout 값을 재배치에 맞춰 조정할지 | 두 계층을 겹치는 설계 의도는 유지 |

## 근거

- `pi4_sensor_node/src/main.cpp` — `demo_reading()` 스텁 상태와 발행 topic 확인
- `packages/hood_seek_protocol/lib/hood_seek_protocol.dart:495`, `:497`, `:519` — `personPresent`를 쓰는 분기
- [`contracts/sensor-snapshot.schema.json`](../../docs/archive/mqtt-generation/contracts/sensor-snapshot.schema.json), [`contracts/cat-detection.schema.json`](../../docs/archive/mqtt-generation/contracts/cat-detection.schema.json)
- [ATLAS 플랫폼 기준](../김병직/2026-08-29-atlas-platform-baseline.md) — 프로파일 구성과 Pi5 이미지 문의
- [사람·반려동물 검출 모델 통합 제안](../김병직/2026-08-29-vision-model-consolidation.md) — 2 FPS 합격선과 근거
- [Atlas AI 통합 의사결정 대장](../../docs/ai-integration-decisions.md) — `P-009`
- [AGENTS.md](../../AGENTS.md) 「HOOD&SEEK 경계」, [CLAUDE.md](../../CLAUDE.md) 「안전 불변식」 — 재정의 대상
