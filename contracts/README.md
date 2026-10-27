# TCP 계약 (Pi 4 SafetyCore ↔ Pi 5 노드)

- 상태: **2026-10-20 18:00 동결 대상** (#52). 동결 후 변경 규칙은 맨 아래 「변경 규칙」
- 기준: 설계서 [`docs/2026-10-19-s3-s10-design-v0.1.md`](../docs/2026-10-19-s3-s10-design-v0.1.md) §2(전송·envelope·payload), §3.2(세 축 값), §3.5(명령 허가), §6(F5·F6·F8). 스키마와 설계서가 다르면 같은 PR에서 둘 다 고칩니다.

## 전송 규칙 (설계서 §2.1)

| 규칙 | 값 |
|---|---|
| 연결 | TCP `:7420`, SafetyCore가 서버 |
| 프레이밍 | UTF-8 JSON **한 줄 = 한 메시지**, `\n` 종료, `\r` 무시. 최대 4096 byte — 넘으면 서버가 그 연결을 끊음 |
| 접속 직후 | 클라이언트가 `hello`를 먼저 보냄. 5 s 안에 없으면 서버가 끊음. 서버는 `state` 1회로 즉시 응답 |
| `state` | 2 Hz 주기 + 변화 즉시. 모든 클라이언트에 **같은 payload** (seq는 아래 규칙) |
| seq | 송신자가 **연결마다** 단조 증가(32-bit). 수신자는 같은 연결 안에서 역행·중복 seq를 폐기하고, 새 연결에서는 기준을 다시 잡음. Core의 `state` seq는 프로세스 전체 카운터 하나라서 연결 안에서 **건너뛸 수 있음** — 수신자는 건너뜀을 손실로 보지 않음 |
| 연결 역할 | 첫 `hello`의 `src`로 고정. `vision` 연결은 `vision`만, `voice` 연결은 `voice`만, `ui` 연결은 `ui_cmd`만 받고 그 밖의 type은 폐기 + 카운터. `state`는 모든 연결이 받음 |
| `hello` 순서 | `hello` 전에 온 메시지와 같은 연결의 두 번째 `hello`는 폐기 (5 s 타이머는 그대로) |
| 동시 연결 | 최대 8개. 넘으면 새 연결을 바로 끊음 (재접속 때 반쯤 열린 이전 연결 대비) |
| 정수 필드 | `7.0`처럼 소수부가 0인 실수도 정수로 받음 — JSON Schema와 같은 기준이라 「스키마 통과 = Core 형식 통과」 |
| 신선도 | **수신 시각** 기준(아래 유효 기간). `t_ms`는 송신자 monotonic ms로 로그 상관용이며 판정에 쓰지 않음 |
| 검증 실패 | 메시지 폐기 + 카운터 증가, **상태를 바꾸지 않음** (F8) |

## 메시지 5종

| type | 방향 | `src` | 스키마 | 유효 기간 (만료 시) |
|---|---|---|---|---|
| `hello` | 클라이언트 → Core, 접속 직후 1회 | `vision` `voice` `ui` | [hello.schema.json](hello.schema.json) | — |
| `vision` | vision-node → Core, 추론마다 ≈3 Hz | `vision` | [vision.schema.json](vision.schema.json) | 2 s (presence=`unknown`, 고양이 후보 소멸) |
| `voice` | voice-node → Core, 인식 완료마다 | `voice` | [voice.schema.json](voice.schema.json) | 5 s (폐기, 집행 안 함) |
| `ui_cmd` | Flutter → Core, 사용자 터치 | `ui` | [ui_cmd.schema.json](ui_cmd.schema.json) | 5 s (폐기, 집행 안 함) |
| `state` | Core → 모든 클라이언트 | `core` | [state.schema.json](state.schema.json) | Flutter 쪽 3 s ("연결 끊김" 화면) |

공통 envelope: `{"v":<버전>,"type":"<type>","src":"<src>","seq":<0..4294967295>,"t_ms":<0 이상>,"p":{...}}` — 6개 필드 모두 필수, 그 밖의 필드는 금지(`additionalProperties: false`). 예제는 [examples/](examples/).

`v`는 메시지 종류마다 따로 정합니다: **`state` 4(#182), `ui_cmd` 2(`USER_LOCK`만), `voice` 1·2(`IND_SET`은 2), `hello`·`vision` 1** (#116 2026-10-24, D-058 2026-10-26). Core는 `ui_cmd`를 `v` 1·2로 받고(USER_LOCK은 `v` 2에서만) `voice`는 `v` 1·2로 받되 새 `IND_SET`은 `v` 2에서만 받고, `hello`·`vision`은 `v` 1로만 받습니다. Flutter는 옛 세 명령을 `v` 1로, USER_LOCK만 `v` 2로 보내고, state `v` 1·2·3·4를 모두 받습니다(`v` 1이면 남은 초 없음).

## allowlist

| 필드 | 허용 값 |
|---|---|
| `voice.p.intent` | `FAN_SET`(value 1·2·3 = 약·중·강) `FAN_OFF` `IND_OFF` `IND_ON` `IND_UP` `IND_DOWN` `NONE` 및 `v` 2의 `IND_SET`(value 1..3). `value`는 `FAN_SET`·`IND_SET`에만 있고 다른 intent에 있으면 거부 |
| `ui_cmd.p.cmd` | `FAN_SET`(value 0..3) `IND_SET`(value 0..3) `ACK_ALERT`(value 0) `USER_LOCK`(value 1 잠금 · 0 해제, `v` 2만). **위험·반려동물 잠금을 직접 푸는 명령은 없음** — `ACK_ALERT`는 위험 잠금 해제의 필요조건 하나일 뿐이고, `USER_LOCK` 0은 사용자 잠금만 끈다 (설계서 §3.4, D-058) |
| `state.p.hazard` | `low`(약불·대기) `mid`(중불) `high`(강불) — 정상 조리, `warning`(과열·과한 연기 단독) `danger` `fault` (v 3, D-059. 옛 `normal`·`caution`은 없앰) |
| `state.p.presence` | `present` `absent` `unknown` |
| `state.p.lock` | `unlocked` `locked_cat` `locked_danger` `locked_user`(사용자 잠금, v 3). 여러 잠금이 겹치면 위험 > 반려동물 > 사용자 순으로 하나만 |
| `state.p.alert` | 아래 alert 표 7종 |

`voice.p.intent=NONE` 및 지원 intent 전송 규칙 (voice-node, #62):

> **확정 규칙:** [PM 결정 2026-10-24, 표 1~5](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5637198979)과 [계약 담당 정정인 확인 2026-10-25](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5641375459)에 따른다. 스키마·intent·필드를 추가하지 않으며 voice envelope는 v1을 유지한다. 실제 발행·시연 화자 범위는 별도 [PM 조건부 승인](https://github.com/lambdaramji/HOOD-SEEK/issues/62#issuecomment-6393359736)의 현장 부정 시험과 고정 자리 조건을 따른다. 방향 필드·새 intent 계약은 별도 합의 대상이다.

- **Whisper/후드 모드:** 정상 ASR의 최종 문장에 활성어 “후드”가 있지만 단일 지원 명령이 아니면 발화당 최대 1회 NONE을 보낸다. [닫힌 별칭](../voice_node/README.md#알려진-전사-오인식-정규화--2026-10-22)은 전체가 단일 지원 명령일 때만 적용한다. 최초 전사는 로그에 보존한다.
- **SenseVoice/하이엘지 모드:** 후드 활성어 규칙과 분리한다. 하이엘지 별칭·근접 발음 확인 후 명령 전체의 대상/동작을 검사한다. 활성어가 없거나 텍스트가 유효하지 않으면 DROP(미전송), 부정·허가/상태 질문·인용·복합·미지원 문장이면 NONE, 단일 지원 명령이면 기존 intent를 보낸다. “후드” 단어는 추가 필수 조건이 아니다. 예: “하이엘지 인덕션 꺼줘” → IND_OFF. 원문·최종 문장·역할별 유사도를 로그에 보존한다. 상세 목록/문턱은 [voice README](../voice_node/README.md#sensevoice-하이엘지-실행과-선택적-송신)를 따른다.
- **끝 물음표 규칙:** 하이엘지가 확인된 단일 지원 명령은 끝의 `?`·`？` 유무로 거절하지 않는다. “하이엘지 인덕션 꺼?” → IND_OFF, “하이엘지 인덕션 켜?” → IND_ON. 의미상 허가 질문 “켜도 돼?”·부정 “끄지 마?”·상태/인용/복합 문장은 계속 NONE이며 호출어 없는 발화는 보내지 않는다. 실행 최종 text에서는 끝 물음표를 제거하고 최초 전사는 로그에 보존한다. 이 처리는 SenseVoice/하이엘지 모드에 적용하며 Whisper/후드 모드의 기존 정책은 바꾸지 않는다.
- 무음·빈 전사·ASR 실패·품질 탈락·송신 기한 초과·끊긴 연결은 보내지 않는다. SenseVoice는 ASR 신뢰도 점수를 제공하지 않으므로 Whisper 품질 문턱을 적용하지 않는다. 대신 호출어·전체 문법/유사도·입력 검증·기한 검사를 거친다. 끝 물음표 수용은 ASR의 억양 구별 개선을 뜻하지 않으며 의미상 부정/허가/상태 발화의 오인식은 계속 별도 검증 대상이다.
- NONE에도 text·conf를 넣고 value는 뺀다. text는 최종 인식 문장(200 Unicode 문자 이하), 최초 전사는 노드 로그에 보존한다. **SenseVoice는 ASR 점수를 제공하지 않으므로 `conf=0`을 보낸다. 여기서 0은 낮은 확신이 아니라 점수 미제공 표기다.** 자모 유사도나 ASR 정답 확률이 아니다. Whisper는 기존 ASR 진단 점수를 사용한다. Core는 conf를 판정에 쓰지 않고 0..1 형식만 검사한다.
- 지원 intent의 허용·거부와 팬 안전 하한은 Core가 정한다. 방향 필드/새 intent는 이 변경에 없다. 풍량 없는 “후드/팬 켜”는 FAN_SET(1)로 보낸다. 이미 높은 단수의 requested가 1로 바뀔 수 있으며 실제 팬은 Core가 안전 하한을 적용한다. 대상 없는 “하이엘지 꺼”는 NONE이다. 목표 단수+방향 계약은 별도 합의 대상이다.

## 코드표

### `state.p.reasons` — 현재 상태를 만든 조건 (우선순위 순으로 나열)

UI 문구는 앱 `reasonLabel`(`flutter_app/lib/screens/labels.dart`)과 같습니다(#130, PM 10/25). 문서 칸이라 스키마·`v`와 무관합니다.

| 코드 | 뜻 | 근거 | UI 문구 |
|---|---|---|---|
| `SENSOR_FAULT` | 열화상 또는 PM 센서 읽기 실패 3 s → hazard=fault | §3.3 1, F4 | 센서 고장 |
| `TEMP_ABS` | `T_max ≥ P_T_abs_danger` | §3.3 2 | 절대 고온 |
| `TEMP_DANGER` | `T_metric ≥ P_T_danger` (65 °C급 고온). 단독이면 hazard warning, `SMOKE`와 함께면 danger (D-054) | §3.3 3·4 | 과열 |
| `SMOKE` | 과한 연기: 팬 3단인 동안 SPS30 포화가 `P_heavy_smoke_samples` 연속, 확정 뒤 `PM_delta < P_PM_smoke`면 해제. 단독이면 warning, `TEMP_DANGER`와 함께면 danger (D-054) | §3.3 3·4 | 연기 많음 |
| `TEMP_HIGH` | `T_metric ≥ P_T_high` (강불, hazard high) | §3.3 5 | 강불 |
| `PM_WARN` | 연기 있음: `PM_delta ≥ P_PM_warn ∧ (tsize < P_tsize ∨ 포화)`가 새 샘플 `P_smoke_samples` 연속이면 시작, 그 뒤 Δ만 봄. **hazard를 바꾸지 않음** (D-054) | §3.3 (표시) | 연기 있음 |
| `TEMP_MID` | `T_metric ≥ P_T_mid` (중불, hazard mid) | §3.3 6 | 중불 |
| `HOLD` | 조건은 내려갔지만 해제 히스테리시스 대기 중 (5 s / danger에서 10 s) | §3.3 해제 | 해제 대기 중 |
| `ABSENT` | presence=absent (`P_absent_s` 30 s) | §3.4 | 자리 비움 |
| `VISION_STALE` | vision 2 s 만료 → presence=unknown | §2.4, F3·F7 | 카메라 정보 없음 |
| `CAT_NEAR` | 이번 tick `cat_hit` (연속 판정 중이거나 `locked_cat`의 원인) | §3.4 | 반려동물 접근 |
| `ACK_WAIT` | `locked_danger`, hazard ≤ mid, `ACK_ALERT` **미수신**. Flutter는 이 코드가 있을 때만 "위험 확인" 버튼을 보임 | §3.4, D-040 ④ | 위험 해소 · 확인 필요 |
| `UNLOCK_WAIT` | `locked_danger`, `ACK_ALERT` **받음**, 나머지 해제 조건(hazard ≤ mid 10 s 연속·재실) 대기. 재실이 막고 있으면 `ABSENT`·`VISION_STALE`이 함께 나옴 | §3.4, D-040 ④ | 확인됨 · 해제 대기 |
| `FAN_NOACK` | 팬 ESP32 ACK 2 s 없음 | F5 | 후드 응답 없음 |
| `IND_NOACK` | 쿡탑 ESP32 ACK 2 s 없음 → `allowed_on=false` | F6 | 인덕션 응답 없음 |

### `state.p.voice.reason` — 음성 명령이 거부된 이유 (`accepted=true`면 `null`)

| 코드 | 뜻 | 근거 | UI 문구 (제안) |
|---|---|---|---|
| `LOCKED` | `lock ≠ unlocked`(사용자 잠금 포함)라 켜기·올리기 거부 | §3.5 | 잠금 중 |
| `HAZARD` | hazard > warning이거나 부팅 뒤 센서 확인 전이라 켜기·올리기 거부. 화재위험·고장은 위험 잠금이 먼저라 보통 `LOCKED`로 나옴 (D-061 전에는 hazard > mid) | §3.5 | 지금은 더 올릴 수 없어요 |
| `ABSENT` | presence ≠ present라 켜기·올리기 거부 | §3.5 | 자리 비움 |
| `IND_NOACK` | 쿡탑 무응답이라 켜기 거부 | F6 | 인덕션 응답 없음 |
| `NO_INTENT` | intent=`NONE` (지원하지 않는 문장) | §2.3 | 지원하지 않는 명령 |

팬 요청이 하한(`fan.min`)에 눌린 경우는 거부가 아닙니다 — `accepted=true`, `state.p.fan.requested`로 보여 줍니다(§3.5).

### `state.p.alert` — 클립 ID

Flutter는 값이 `NONE`이 아닌 다른 값으로 **바뀔 때만** `assets/audio/<alert>.wav`를 재생합니다(설계서 §2.3·§5.4). 어느 전이에서 어떤 값을 내고 얼마나 유지하는지는 Policy(#57)가 정합니다.

| 값 | 뜻 |
|---|---|
| `NONE` | 새 경고 없음 |
| `CAUTION` | (D-059부터 내지 않음 — 중불은 정상 조리. 클립 ID는 계약에 남김) |
| `WARNING` | 경고 진입 — 과열(`TEMP_DANGER` 단독)·과한 연기(`SMOKE` 단독). 강불(high)·중불(mid)에는 없음 (D-059) |
| `DANGER` | 위험 진입(차단) |
| `CAT_LOCK` | 고양이 잠금 |
| `OFFLINE_RECOVER` | 고장·끊김에서 복구 |
| `CMD_REJECTED` | 명령 거부 |

## 필드 정의 (스키마 주석 보충)

- `vision.p.person` / `cat`: 클래스별 최고 score 1개. `bbox` = `[x_min, y_min, x_max, y_max]` 픽셀, `seen=true`면 배열·`false`면 `null`. `roi_overlap` = bbox∩ROI 면적 / bbox 면적(§5.2, P-003). **`consecutive_hits` 같은 누적값은 금지** — SafetyCore만 연속 판정합니다.
- `state.p.vision`: `person` = 최근 2 s 안 `person.seen`, `cat` = 이번 tick `cat_hit`. `fresh=false`면 둘 다 `false`.
- `state.p.sensors`: `valid=false`면 **그 센서의 값은 전부 `null`** (스키마가 강제, `pm.baseline` 포함). `pm.baseline`은 부팅 후 60 s 동안에도 `null`. MLX 값이 NaN이거나 −40~300 °C 밖이면 Core는 `valid=false`로 보냄.
- `state.p.thresholds`: `profiles/default.json`의 **표시용 사본**(그래프 임계선). Flutter는 이 값으로 비교·판단하지 않습니다. PM 선은 `baseline + pm_warn` / `baseline + pm_smoke`. 온도 선은 `t_mid`(중불)·`t_high`(강불)·`t_danger`(65 °C급) — v 3에서 `t_caution`·`t_warn`을 바꿈(D-059). `lock_release_s`는 `P_lock_release_s` 사본으로, 아래 남은 초 진행 원의 전체 길이입니다.
- `state.p.lock_release_left_s` (v 2): 위험 잠금 해제 조건 ①(hazard ≤ mid가 `P_lock_release_s` 동안 이어짐)의 남은 초. `lock=locked_danger` ∧ hazard ≤ mid이면 `max(0, ceil(P_lock_release_s − 경과 s))`, 그 외 `null`(스키마가 강제). `0`이면 ① 충족. Core는 `safe_since_ms_`(`policy.cpp` 7단계)에서만 계산하고, 해제 판정 경로(불변식 ⑨)는 바꾸지 않습니다. Flutter는 숫자를 옮겨 그리기만 하고 시간을 세지 않습니다.
- `state.p.voice`: 부팅 후 voice 명령이 없었으면 `null`. `value`는 `last_intent=FAN_SET`·`IND_SET`이면 목표 1..3, 그 외 `null` — UI "마지막 음성 명령"에 단수를 보여 주기 위함.

## 스키마가 검사하는 것과 안 하는 것

| 스키마 (형식) — `validate.py`, CI | SafetyCore 파서·Policy (의미) — #47·#57 |
|---|---|
| 필수 필드, 타입, enum, 범위, 추가 필드 금지 | 4096 byte, `hello` 5 s, seq 역행·중복, 수신 시각 신선도 |
| `seen` ↔ `bbox`, `FAN_SET` ↔ `value`, `ACK_ALERT` value 0 | bbox가 프레임 안인지 (`0 ≤ x_min < x_max ≤ frame_w`, y 동일) |
| `fresh=false` → person·cat `false`, `accepted=true` → reason `null` | `allowed_on` 허가, `fan_min` 하한, 잠금 해제 3조건(`ACK_ALERT` 단독 해제 불가) |
| `valid=false` → 센서 값 `null`, `last_intent ∈ {FAN_SET, IND_SET}` ↔ `voice.value` | 필드 간 비교: `fan.level ≥ fan.min`, hazard ∈ {danger, fault} → `induction.level = 0` (`policy_test`에서 검사) |
| — | 연결 역할(`hello.src`), `hello` 순서, 동시 연결 8개 |

## 검증 (CI와 같은 명령)

```bash
python3 -m venv ~/.venvs/hoodseek-contracts                       # 처음 한 번
~/.venvs/hoodseek-contracts/bin/pip install jsonschema==4.23.0    # CI와 같은 버전
~/.venvs/hoodseek-contracts/bin/python contracts/validate.py      # 저장소 루트에서
```

`examples/<name>*.json`은 통과해야 하고, `examples/invalid/<name>.<case>.json`은 실패해야 합니다. CI `system.yml`의 contracts job이 같은 파일을 실행합니다.

## 변경 규칙

- 동결(10/20 18:00) 후 변경은 PR + envelope `v` 증가 + **송신·수신 양쪽 파서 동반**, 라벨 `contract-change`, PM 승인.
- v1 최초 반입(#52)은 스키마·예제만 넣습니다. 파서는 #47(Core), #51·#63(Flutter), #61(vision), #62(voice)에서 구현합니다.
- state v 2 (#116, PM 결정 2026-10-24): `p.lock_release_left_s`·`p.thresholds.lock_release_s` 추가. state만 올리고 입력 4종은 v 1 그대로 둡니다. state를 읽는 쪽은 Flutter뿐입니다. **배포 순서: Pi 5 Flutter(v 1·v 2 수신) 먼저, 그다음 Pi 4 Core.** 옛 Flutter(v 1만 수신)가 켜진 채 Core를 먼저 올리면 state를 전부 버려 화면이 「연결 끊김」이 됩니다(#119 정인 리뷰). vision-node는 state 줄을 읽고 버리고(`vision_node/src/link.cpp` `drain`), voice-node는 해석하지 않습니다.
- state v 3 · ui_cmd v 2 (D-058, PM 결정 2026-10-26): 사용자 잠금. `ui_cmd`에 `USER_LOCK`(1·0, `v` 2만), state `lock`에 `locked_user`. 나머지 필드는 그대로. **배포 순서: Pi 5 Flutter(state v 1~3 수신, 옛 명령은 v 1로 송신) 먼저, 그다음 Pi 4 Core.** 새 앱 + 옛 Core 사이에는 터치가 그대로 되고 「인덕션 잠그기」만 Core가 거부(폐기)한다. 옛 앱 + 새 Core면 state v 3을 버려 「연결 끊김」이 되므로 이 순서를 지킨다.
- state v 3 추가(D-059, PM 결정 2026-10-26, #160과 같은 v 3 — 배포 전 확정): hazard를 `low`·`mid`·`high`(정상 조리)·`warning`·`danger`·`fault`로 나눔, 사유 `TEMP_HIGH`·`TEMP_MID`, 임계 사본 `t_mid`·`t_high`. 프로파일 키도 `P_T_mid`·`P_T_high`·`P_fan_min_mid`·`P_fan_min_high`·`P_ind_max_absent`로 바꿈 — **새 Core를 올리기 전에 Pi 4 `profile.json` 키 이름을 옮겨야 함**(deploy/README 「현장 보정」 0단계). 앱은 v 1·2의 옛 이름을 새 이름으로 읽으므로 앱 먼저 올리는 순서는 그대로.


## 인덕션 목표 단수 확장 — #182

voice v2·state v4 및 새 음원 문구는 #186의 PM 승인(2026-10-26)으로 확정했다. 운영 발행은 별도 현장 확인 뒤에 허용한다. #107에서 확정한 기존 일곱 intent는 그대로 v1로 보낸다.

- 하이엘지와 이어 말한 「인덕션 1·2·3단(계)으로 해줘」·「약불·중불·강불(로 해줘)」는 각각 `IND_SET(1·2·3)`이다. 숫자 표현은 설정 동사를 요구한다. 「일단 켜」, 대상 없는 단수, 목표+방향, 부정·허가/상태 질문·인용·복합은 지원하지 않는다.
- `voice` v2에만 `IND_SET`을 추가한다. `value`는 정수 1..3 필수. 0단은 새로 추측하지 않고 기존 `IND_OFF`를 사용한다. hello는 v1이다. Core는 기존 voice v1과 기존 intent의 v2도 받는다.
- `state` v4에서 마지막 음성 결과의 `last_intent=IND_SET`·`value`를 전달한다. Flutter는 v1·2·3·4를 받되 v3 이하의 IND_SET은 거부한다. 기존 `ui_cmd`·vision·hello 계약은 바꾸지 않는다.
- 적용 시점의 Core 단수와 목표를 비교하는 기존 `kIndSet`을 재사용한다. 목표가 현재보다 높으면 기존 `allowed_on`(잠금·위험·재실·ACK)을 검사한다. 내림/동일 단수 처리와 안전 자동 하향, 팬 하한, 5초 만료는 기존 정책대로다. 쿡탑은 ESP32 시뮬레이터다.
- 결과 음성은 `accepted`에 더해 목표=최종 `induction.level`·ACK를 확인한다. 다른 최종 값이나 만료는 무음이다. 별도 결과 음원을 늘리지 않고 기존 RESULT_IND_1·2·3을 재사용한다.
- 배포 순서: v4 수신 Flutter → voice v2 수신/state v4 Core → voice-node 후보. 이전 앱(v3까지만 수신)에 Core를 먼저 올리면 state를 버리므로 순서를 지킨다. 운영 재배포는 리뷰와 고정 구성의 현장 부정 시험 후다.
