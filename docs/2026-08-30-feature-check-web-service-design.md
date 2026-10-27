# 기능점검 웹 서비스 기초 설계 (제안)

- 작성자: 임재범 (Claude Code 세션에서 작성)
- 작성일: 2026-08-30
- 상태: 제안(draft) — 팀 합의 전
- 카테고리: tooling / integration-test
- 조사 대상: Pi4 → Pi5 → Flutter/ESP32 체인의 기능을 한 화면에서 한 번에 점검하는 웹 서비스

## 표기 규칙

[CLAUDE.md](../CLAUDE.md)의 [실측]/[문서]/[추정] 규칙을 따릅니다. 이 문서는 설계 제안이므로 대부분이 제안이며, 기존 코드·문서를 근거로 인용할 때만 [문서]를 붙입니다.

## 이 문서가 답하는 미결 항목

- 직접 연결되는 `P-00x` 항목은 없습니다. 다만 [docs/ai-integration-decisions.md](ai-integration-decisions.md)의 「하드웨어 도착 후 검증 순서」를 사람이 손으로 반복하는 대신 화면 한 장으로 확인하게 만드는 것이 목적이므로, P-003~P-005 결정 후 실측값 검증에 재사용할 수 있습니다.
- 이 설계 자체가 새 파일·새 실행 파일을 추가하므로 **합의 필요** 항목입니다. 아래 「합의 필요 사항」 절 참조.

## 요약

- **무엇을**: broker 연결, Pi4 발행(snapshot·cat detection), Pi5 상태 발행과 안전 불변식을 항목별 PASS / FAIL / WAIT로 보여 주는 단일 웹 페이지.
- **어떻게**: 새 언어·프레임워크 없이 기존 Dart 스택을 재사용합니다. `pi5_gateway`에 점검 전용 실행 파일을 하나 추가하고, `hood_seek_protocol`의 DTO 검증을 그대로 호출하며, `dart:io`의 `HttpServer`로 페이지와 JSON을 서빙합니다. **새 의존성 0개**가 목표입니다.
- **경계**: 점검 서비스는 **관측 전용(read-only)** 입니다. 어떤 topic에도 발행하지 않고, 안전 판단·팬 제어 경로에 끼어들지 않습니다. Pi4와 같은 "관측 노드" 위치에 섭니다.
- 합성 payload를 발행해 fail-safe를 자극하는 능동 점검(active probe)은 2단계로 분리하고, 팀 합의 전에는 구현하지 않습니다.

## 배경: 지금 점검이 어떻게 이루어지는가

| 확인 대상 | 현재 방법 | 근거 |
|---|---|---|
| Pi4 발행 | `mosquitto_sub`로 topic을 눈으로 확인 | [문서] [contracts/README.md](../contracts/README.md) topic 표 |
| Pi5 판단 | gateway stdout 로그(`level=... fan=...`)와 Dart 테스트 | [문서] `pi5_gateway/lib/runtime.dart` `_applyState` |
| Flutter 표시 | HMI 화면을 직접 보기 | [문서] CLAUDE.md 아키텍처 표 |
| 스키마 위반 | gateway stderr의 `Rejected <topic>: ...` 로그 | [문서] `pi5_gateway/lib/runtime.dart` `_handleMessages` |

각 확인이 서로 다른 터미널·화면에 흩어져 있어, 데모 리허설이나 하드웨어 도착 후 검증 때 "지금 어디가 죽어 있는지"를 한 번에 볼 수 없습니다. 이것이 이 서비스가 지금 필요한 이유입니다.

## 설계 원칙 (Ponytail 사다리 적용 결과)

1. **재사용**: payload 검증은 `SensorSnapshot.fromJson` / `CatDetectionEvent.fromJson` / `KitchenState.fromJson`을 그대로 호출합니다. `KitchenState.fromJson`은 `fan_level ≥ minimum_fan_level` 같은 불변식을 이미 검사합니다. [문서] `packages/hood_seek_protocol/lib/hood_seek_protocol.dart`
2. **기존 의존성만**: MQTT는 `pi5_gateway`가 이미 쓰는 `mqtt_client`, HTTP 서버는 Dart 표준 `dart:io` `HttpServer`. shelf 등 웹 프레임워크를 추가하지 않습니다.
3. **판단 중복 구현 금지의 해석**: 이 서비스는 상태를 *결정*하지 않고 *교차 검증*만 합니다. 최근 snapshot에 대해 `const SafetyPolicy().evaluate(...)`(protocol의 동일 함수)를 호출해 Pi5가 발행한 state와 비교하는 것은 판단의 중복 구현이 아니라 단일 진실 공급원의 재사용입니다. 점검 결과는 표시 전용이며 어떤 제어에도 쓰이지 않습니다.
4. **관측 전용**: 발행 권한이 없어야 합니다. Mosquitto ACL로 점검용 계정에 subscribe만 허용하는 것을 함께 제안합니다(아래 배치 절).

## 구성 (제안)

```text
Pi4 ──MQTT──▶ Mosquitto(Pi5) ◀──subscribe only── feature_check (Pi5, Dart)
                    ▲                                   │ dart:io HttpServer :8080
                    └── Pi5 gateway (기존, 변경 없음)      ▼
                                                   브라우저 (팀원 노트북 / HMI)
```

새 파일은 세 개입니다.

| 경로 | 역할 |
|---|---|
| `pi5_gateway/bin/feature_check.dart` | 진입점. 환경변수에서 broker 접속 정보를 읽고 서버 기동 |
| `pi5_gateway/lib/feature_check.dart` | topic 구독 → 검증 → 점검 항목 상태 갱신 → `GET /api/checks` JSON |
| `pi5_gateway/lib/feature_check_page.dart` (또는 lib 내 상수 문자열) | 단일 HTML 페이지. 5초 간격으로 `/api/checks`를 폴링해 표를 다시 그림 |

`pi5_gateway` 패키지 안에 두는 이유: 필요한 의존성(`mqtt_client`, `hood_seek_protocol`)과 freshness 검증 함수(`validateSensorFreshness` 등)가 이미 그 패키지에 있고, 새 패키지를 만들면 pubspec·CI 경로가 하나 더 늘기 때문입니다. gateway 본체(`runtime.dart`)는 수정하지 않습니다.

## 점검 항목 (1단계, 수동 개입 없는 passive 점검)

각 항목은 `PASS` / `FAIL` / `WAIT`(아직 데이터 없음) 세 상태와 근거 메시지(원문 그대로), 마지막 갱신 시각을 가집니다.

| # | 항목 | 판정 기준 | 근거 |
|---|---|---|---|
| C1 | broker 연결 | 점검 서비스 자신의 MQTT 연결이 살아 있음 | — |
| C2 | Pi4 snapshot 수신 | 마지막 유효 snapshot이 3초 이내 (gateway의 `sensorTimeout`과 동일 기준) | [문서] `runtime.dart` `sensorTimeout = 3s` |
| C3 | snapshot 유효성 | `SensorSnapshot.fromJson` + `validateSensorFreshness`(maxAge 5 s, skew 2 s) 통과. 최근 60초 거부 건수와 마지막 거부 사유 표시 | [문서] CLAUDE.md 안전 불변식 |
| C4 | cat detection 유효성 | `CatDetectionEvent.fromJson` + `validateDetectionFreshness` 통과 (수신이 없으면 WAIT — 고양이가 없으면 정상) | 동일 |
| C5 | Pi5 state 발행 | retained `hoodseek/v1/pi5/state`가 존재하고 `KitchenState.fromJson` 통과 | [문서] contracts/README.md (retain 예) |
| C6 | 상태 신선도 | state의 `sensor.observed_at`이 최근 snapshot과 정합 (state가 옛 snapshot에 머물러 있지 않음) | — |
| C7 | 정책 교차 검증 | 최근 유효 snapshot을 `SafetyPolicy().evaluate`에 넣은 결과의 `level`·`minimum_fan_level`·`induction_locked`가 Pi5 state와 일치. 단, pet access·수동 팬 요청 등 점검 서비스가 모르는 입력이 있으므로 **불일치는 FAIL이 아니라 REVIEW(사람 확인 필요)로 표시** | [문서] `SafetyPolicy.evaluate` |
| C8 | offline 전환 관찰 | snapshot이 3초 넘게 끊긴 동안 state `level == offline`·`fan_level == 3`·`induction_locked == true`인지. 끊김이 발생한 적 없으면 WAIT | [문서] CLAUDE.md 안전 불변식 |

C7의 REVIEW 구분은 점검 도구가 gateway보다 아는 것이 적다는 사실을 화면에 그대로 드러내기 위한 것입니다. 점검 도구의 FAIL이 gateway 판단보다 우선한다는 인상을 주면 안 됩니다.

## API와 화면 (제안)

- `GET /` — 단일 HTML. JS 수십 줄(폴링 + 표 렌더). 외부 CDN·프레임워크 없음.
- `GET /api/checks` — 예시:

```json
{
  "generated_at": "2026-08-30T05:00:00Z",
  "checks": [
    {
      "id": "C2",
      "name": "Pi4 snapshot 수신",
      "status": "FAIL",
      "detail": "last snapshot 7.2s ago (threshold 3s)",
      "updated_at": "2026-08-30T05:00:00Z"
    }
  ]
}
```

- 오류 메시지는 요약하지 않고 `FormatException.message` 원문을 `detail`에 그대로 둡니다(문서화 규칙과 동일한 태도).

## 배치와 보안 (제안)

- Pi5에서 systemd 서비스로 실행. 접속 정보는 gateway와 같은 방식으로 환경 파일 주입 — **저장소에 계정·비밀번호를 넣지 않는 규칙 그대로**. [문서] CLAUDE.md 주의할 점
- Mosquitto에 점검 전용 계정을 만들고 ACL로 `hoodseek/v1/#` **read만** 허용합니다. 코드 실수로도 발행이 불가능하도록 broker 쪽에서 막는 것이 핵심입니다.
- HTTP는 폐쇄형 유선 LAN 내부 전용, 인증 없음(1단계). LAN 밖 노출이 필요해지면 그때 다시 결정합니다.

## 단계 계획

| 단계 | 내용 | 조건 |
|---|---|---|
| 1 | 위 passive 점검 C1~C8 + 웹 페이지 | 이 문서 합의 후 바로 |
| 2 | active probe: 합성 snapshot·fan request를 발행해 offline 전환, 최소 풍량 하한 등을 자동 시험 | **합의 필요.** 실장비 연결 시 오동작 위험이 있어 점검 모드 플래그와 실행 조건(데모 환경 한정 여부)을 팀이 먼저 결정 |
| 3 | ESP32 serial fail-safe 점검 | 방법 미정 (아래 미확인 사항) |

## 합의 필요 사항

1. 이 서비스를 만드는 것 자체 (새 실행 파일 3개, 새 의존성 0개).
2. 위치를 `pi5_gateway/` 안으로 하는 것 vs 별도 패키지. 본 문서는 전자를 제안.
3. 점검 전용 MQTT 계정과 read-only ACL 추가 (`deploy/mosquitto/` 수정 수반).
4. 2단계 active probe의 허용 범위.

## 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| ESP32 fail-safe를 웹에서 관측할 방법 [추정: gateway가 serial 송신 결과를 별도 topic으로 발행하게 확장해야 할 수 있음] | ESP32 실물 도착 후 serial 회신 여부 확인, gateway 확장 필요성 검토 | 미정 |
| Whisper adapter(`voice/text`) 점검 방법 [추정: 발행 주기가 없어 passive로는 WAIT만 가능] | 음성 파이프라인 확정(P-00x 외 항목) 후 재검토 | 미정 |
| HMI(Flutter)가 실제로 state를 렌더하는지의 자동 확인 [추정: 웹 서비스 범위 밖, 사람 확인으로 남길 가능성 높음] | 팀 논의 | 미정 |

## 재현 방법 (1단계 구현 후 예정)

```bash
cd pi5_gateway
dart pub get
dart run bin/feature_check.dart   # MQTT_HOST 등 환경변수 필요
# 브라우저에서 http://<pi5>:8080
```

## 근거 목록

- [문서] [contracts/README.md](../contracts/README.md) — topic 표, retain, QoS
- [문서] `packages/hood_seek_protocol/lib/hood_seek_protocol.dart` — DTO 검증, `SafetyPolicy.evaluate`
- [문서] `pi5_gateway/lib/runtime.dart` — `sensorTimeout`, freshness 검증, `Rejected` 로그
- [문서] [CLAUDE.md](../CLAUDE.md) — 안전 불변식, 비밀정보 주입 규칙
- [문서] [AGENTS.md](../AGENTS.md) — 의사결정 사다리, 관측/판단 경계
