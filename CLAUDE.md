# CLAUDE.md

이 파일은 [Claude Code](https://claude.com/claude-code)가 이 저장소에서 작업할 때 참고하는 안내입니다.

## 작업 규칙

**코드를 쓰기 전에 [AGENTS.md](AGENTS.md)를 먼저 읽습니다.** 이 저장소는 Ponytail `full` 원칙(의사결정 사다리, 구현 규칙, 줄이면 안 되는 것)을 적용하며, 그 내용은 여기서 반복하지 않습니다. 아래는 AGENTS.md에 없는 실행·구조 정보입니다.

문서와 커밋 메시지, PR 본문은 한국어로 씁니다. 코드 식별자와 메시지 필드 이름은 영어입니다.

## 아카이브 필수 규칙

사용자가 아카이브 확인을 별도로 요청하지 않는 한 `docs/archive/**`와 `inbox/archive/**`를 목록 조회, 검색, 열람, 요약, 인용하거나 작업 콘텍스트로 사용하지 않습니다. 활성 문서의 링크도 아카이브 경로라면 자동으로 따라가지 않습니다.

- 파일 검색과 의미 검색에서 두 경로를 제외합니다.
- 현재 설계와 의사결정은 활성 위치의 코드와 문서만 기준으로 판단합니다.
- 과거 결정 복원, 감사·회고 또는 특정 아카이브 파일 확인을 사용자가 명시적으로 요청한 경우에만 요청 범위 안에서 읽습니다.
- 문서를 사용 완료·대체한 뒤 아카이브할 때는 필요한 결론을 활성 문서에 먼저 반영하고 링크를 갱신합니다.

세부 이동 기준과 예외는 [ARCHIVE_POLICY.md](ARCHIVE_POLICY.md)를 따릅니다.

## 반입 원문 제외 규칙

사용자가 특정 REF 파일 확인을 별도로 요청하지 않는 한 `REF/**`를 목록 조회, glob, `rg` 검색, 열람, 요약, 인용하거나 작업 콘텍스트로 사용하지 않습니다. `REF/`는 아카이브가 아니라 Git 밖 원문 반입 경계입니다. 세부 기준은 [ARCHIVE_POLICY.md](ARCHIVE_POLICY.md)의 「반입 원문」 절과 [P-010](docs/decision/P-010-lg-source-custody.md)을 따릅니다.

## 문서화 규칙

`docs/`와 `inbox/`에 조사·시험 결과를 쓸 때는 **각 항목에 출처를 붙입니다.** 여러 사람과 세션이 이어서 작업하므로, 확인한 사실과 아직 추정인 해석이 섞이면 다음 사람이 추정을 전제로 작업하게 됩니다.

| 표기 | 뜻 | 함께 적을 것 |
|---|---|---|
| **[실측]** | 기기·코드에서 직접 실행해 확인 | 실행한 명령, 대상, 날짜 |
| **[문서]** | 개발 문서·설정 파일·소스 인용 | 파일 경로 또는 문서명과 절 |
| **[추정]** | 아직 확인하지 못한 해석·가설 | **확인 방법** |

- 확인 방법을 쓸 수 없는 추정은 문서에 넣지 않습니다.
- 다른 문서의 [추정]을 인용할 때는 추정임을 함께 적습니다. 확정 사실로 승격시키지 않습니다.
- 수치와 오류 메시지는 요약하지 않고 출력 그대로 옮깁니다.
- 조사·시험 문서에는 같은 결과를 다시 확인할 수 있는 명령을 절로 남깁니다.
- **이전 문서를 뒤집는 내용을 쓸 때는 무엇을 어떻게 고쳐야 하는지 표로 남깁니다.** 새 사실만 적고 옛 문서를 그대로 두면 두 문서가 동시에 인용되어 혼선이 생깁니다.
- 기밀 표기가 있는 자료는 **원문을 옮기지 않습니다.** 문서명과 절 번호로만 인용하고 우리가 도출한 결론만 적습니다. 저장소가 비공개여도 같습니다.
- 팀 합의가 없는 내용을 「결정 사항」에 넣지 않습니다. 제안이면 `(제안)`, 이전 합의를 바꾸는 것이면 **`합의 필요`**로 따로 뺍니다.
- 남의 담당 문서를 상의 없이 `deprecated`로 바꾸지 않습니다. 새 문서에 사유를 적고 담당자 확인을 받습니다.

조사·시험 문서의 권장 구성입니다.

```text
머리말 (작성자·작성일·상태·카테고리·조사 대상)
표기 규칙
이 문서가 답하는 미결 항목   ← 해당하는 P-00x가 있으면 여기서 연결
요약                        ← 결론마다 근거 링크
확인된 사실 [실측]/[문서]
시험 기록
남은 문제 [추정]과 확인 방법
이전 문서 수정 필요 사항
재현 방법
미확인 사항 (항목 / 확인 방법 / 담당)
근거 목록
```

## 검증 명령

CI와 **똑같은 명령**을 로컬에서 먼저 돌립니다. 바꾼 영역에 해당하는 것만 돌면 됩니다. 표 형태는 [CONTRIBUTING.md](CONTRIBUTING.md) §2에 있습니다.

```bash
# flutter_app/ 변경 → Flutter CI
cd flutter_app
flutter pub get
dart format lib test && git diff --exit-code -- lib test   # CI가 이 diff로 실패시킵니다
flutter analyze
flutter test
flutter build web

# contracts/ 변경 → System CI contracts job (jsonschema==4.23.0)
python contracts/validate.py

# safety_core/ 또는 profiles/ 변경 → System CI safety_core_policy job
cmake -S safety_core/tests -B safety_core/tests/build
cmake --build safety_core/tests/build
ctest --test-dir safety_core/tests/build --output-on-failure

# safety_core/ 변경 → 로컬 확인 (CI job 없음, message_test: main contracts/examples를 Core 파서로 시험)
cmake -S safety_core -B safety_core/build -DHOODSEEK_WITH_DBUS=OFF
cmake --build safety_core/build
ctest --test-dir safety_core/build

# vision_node/ 변경 → 로컬 확인 (CI job 없음, 호스트에 OpenCV 4·nlohmann_json 필요)
cmake -S vision_node -B vision_node/build
cmake --build vision_node/build
ctest --test-dir vision_node/build

# voice_node/ 변경 → System CI voice_node job (ASR 모델·마이크 없이 파서/VAD/TCP)
cmake -S voice_node -B voice_node/build -DCMAKE_BUILD_TYPE=Release
cmake --build voice_node/build
ctest --test-dir voice_node/build --output-on-failure

# profiles/ 변경
python -m json.tool profiles/default.json >/dev/null

# firmware/ 변경
cd firmware/<보드> && pio run
```

System CI는 `discover`가 바뀐 경로를 보고 관련 job만 돌립니다. `docs/`와 `inbox/`만 바꾸면 나머지 job은 skip되고 필수 check는 초록입니다. 대상 경로는 `safety_core/**`, `profiles/**`, `contracts/**`, `firmware/**`, `flutter_app/**`, `voice_node/**`입니다. `voice_node/`의 모델 없는 파서/VAD/TCP는 CI에서 실행합니다. ASR/PulseAudio와 `vision_node/`의 기기 의존 빌드는 atlas-dev에서 확인하고 PR의 검증에 기록합니다.

하드웨어 없이 확인할 수 있는 것: Flutter demo feed(`flutter run -d chrome`), 계약 예제 검증, SafetyCore 판정 테스트(`safety_core/tests`)와 메시지 시험(`message_test`), vision-node 호스트 시험, ESP32 빌드.

## 아키텍처

```text
Pi 4 (C++ SafetyCore · 안전 평면) ◀──TCP :7420 line-JSON──▶ Pi 5 (vision-node · voice-node · Flutter)
  MLX90640 / SPS30 (I2C)                                       V4L2+ECO.AI · ReSpeaker+SenseVoice · 15.8" 1080p
  │ UART uart3 ──▶ 팬 ESP32 (GPIO26 PWM, 3 s fail-safe → 팬 3단)
  └ UART uart5 ──▶ 쿡탑 ESP32 (WS2812B 16×16, 3 s fail-safe → OFF+잠금 표시)
```

| 경로 | 역할 |
|---|---|
| `safety_core/` | **유일한 판정.** 센서 읽기, `Policy::tick`, UART 집행, TCP state 발행. `src/policy.*`는 T2 |
| `profiles/default.json` | 판정 파라미터. 코드에 숫자를 박지 않는다 |
| `contracts/` | TCP 메시지 5종 JSON Schema + examples. 계약 표는 [contracts/README.md](contracts/README.md) |
| `vision_node/` `voice_node/` | 관측·의도 **보고만.** 누적값·디바운스 없음 |
| `flutter_app/` | state 표시 + ui_cmd 전송. 임계값 비교를 앱에서 하지 않는다 |
| `firmware/` | ESP32 2종 집행 + 독립 fail-safe |
| `deploy/` | systemd 폴백 유닛·env 예시 |
| `inbox/` | 팀원별 원문·작업 메모 (규칙은 [inbox/README.md](inbox/README.md)) |
| `docs/` | 설계·정책·의사결정 문서. 단일 기준은 [S3~S10 설계서](docs/2026-10-19-s3-s10-design-v0.1.md) |

폴더마다 README가 있고, 코드 기준 구조 한 장은 [docs/architecture.md](docs/architecture.md)입니다. 판정 로직은 `safety_core/src/policy.*`에서 한 번만 고칩니다. Flutter나 관측 노드에 판단을 중복 구현하지 않습니다.

## 안전 불변식

아래는 편의나 코드량을 이유로 완화하지 않습니다. 바꾸려면 PR에 근거를 적고 테스트를 함께 고칩니다. 코드·시험 주석의 「불변식 ①~⑨」는 이 목록의 번호입니다.

1. **`Policy::tick`의 hazard 전이표 순서(설계서 §3.3)가 곧 안전 우선순위입니다.** fault → T_abs → (65 °C급 고온 ∧ 과한 연기) → 65 °C급 고온 또는 과한 연기(warning) → T_metric high(강불) → T_metric mid(중불) → low(약불·대기). 연기만으로는 차단하지 않습니다(D-054, #65 2026-10-25). low·mid·high는 정상 조리 단계입니다(D-059). 조건을 중간에 끼우면 PR에 근거를 적습니다.
2. **팬은 `fan_min` 아래로 내려갈 수 없습니다.** `fan_level = max(fan_min, requested)`. 사용자·음성 요청도 같습니다.
3. **vision 2 s 만료 → presence=unknown, cat 후보 소멸.** Flutter는 state 3 s 만료 → "연결 끊김". ESP32는 명령 3 s 끊김 → 팬 3단 / 쿡탑 OFF+잠금. `voice`·`ui_cmd`는 수신 시각 기준 5 s가 지나면 폐기하고 집행하지 않습니다.
4. **Pi 5 관측 노드의 누적값은 받지 않습니다.** vision 메시지에 `consecutive_hits` 같은 필드를 두지 않고 SafetyCore만 연속 판정합니다.
5. **고양이 검출 하나로 잠그지 않습니다.** 사람이 `P_cat_absent_s` 동안 보이지 않아야(고양이 감시) `locked_cat` 후보이고, 사람이 보이는 동안에는 잠그지 않습니다. 고양이 감시는 화재 쪽 부재(`presence=absent`, `P_absent_s`)와 따로 셉니다(D-060).
6. **어떤 단일 고장에서도 팬은 올라가고 인덕션은 내려갑니다.** 반대 방향 경로가 생기면 설계 오류입니다.
7. TCP·UART·JSON 경계에서 범위·타입·seq·allowlist를 검증합니다. 검증 실패는 폐기 + 카운터, **상태를 바꾸지 않습니다.**
8. 쿡탑은 **시뮬레이터**입니다. 보고서·발표 자료·저장소 문서에서는 이를 밝히고 실제 인덕션을 제어한 것처럼 쓰지 않습니다. 사용자 디스플레이(심사 화면)만 실제 LG 후드·인덕션이 연결된 ThinQ 앱처럼 표현합니다(PM 결정 2026-10-24, PR #113 · D-047).
9. `locked_danger` 해제는 **hazard ≤ mid(중불) `P_lock_release_s` 연속 ∧ `ACK_ALERT` ∧ presence=present** 세 조건 AND입니다(강불 열이 남아 있으면 풀리지 않음). `ACK_ALERT` 하나로 잠금을 푸는 코드 경로가 생기면 설계 오류입니다.

## 주의할 점

- **기기 IP·SSH 계정 같은 환경값을 저장소에 넣지 않습니다.** Flutter는 `--dart-define`(예: `CORE_HOST`)으로 주입합니다.
- `dart format`은 CI가 `git diff --exit-code`로 검사합니다. 커밋 전에 반드시 돌립니다.
- `flutter analyze`는 info 수준 경고도 실패로 칩니다.
- 계약(`contracts/*.schema.json`)을 바꾸면 예제와 송신·수신 파서를 함께 고치고 envelope `v`를 올립니다. 동결(10/20) 후에는 라벨 `contract-change`와 PM 승인이 필요합니다([contracts/README.md](contracts/README.md) 「변경 규칙」).
- `main`에 직접 푸시하지 않습니다. 브랜치에서 작업하고 PR과 리뷰를 거칩니다. PR 본문은 `.github/pull_request_template.md`를 채웁니다.
- `inbox/` 문서는 `YYYY-MM-DD-topic.md` 형식이고 머리말에 작성자·작성일·상태·카테고리를 적습니다. 내용이 활성 기준 문서에 완전히 반영되면 삭제하지 않고 `inbox/archive/`로 이동하며, 별도 요청이 없으면 에이전트가 읽지 않습니다.
- 미결 기술 결정은 [docs/ai-integration-decisions.md](docs/ai-integration-decisions.md)의 `P-00x` 항목으로 관리합니다. 관련 제안을 쓸 때는 새 결론을 세우기 전에 해당 항목을 먼저 확인합니다.
