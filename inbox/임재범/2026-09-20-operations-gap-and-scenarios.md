# 작전 장교 현황 · 빈곳 · 시나리오 착수

- 작성자: 임재범 (작전 장교)
- 작성일: 2026-09-20
- 상태: draft
- 카테고리: project-management
- 관련 이슈/PR: 작성 당시 열린 PR #16·#18~#27·#34. 코드 반영은 #36
- 2026-09-20 이후: 해당 PR은 모두 닫힘. #34·#36·#37 병합, #16·#18~#27·#35 종료. 열린 PR 0건. 아래 열린 PR 표는 작성 당시 스냅샷이다.
- 조사 대상: 작성 당시 열린 PR, 활성 문서, `packages/`·`flutter_app/`·`pi4_sensor_node/` 구현 공백

## 표기 규칙

| 표기 | 뜻 |
|---|---|
| **[실측]** | 이 checkout에서 명령을 실행해 확인 |
| **[문서]** | 저장소 문서·PR 목록 인용 |
| **[추정]** | 아직 확인하지 못한 해석. 확인 방법을 함께 적음 |

## 이 문서가 답하는 미결 항목

- [확인·결정 목록](../김병직/2026-08-30-open-questions-and-decisions.md) **F-4**(오탐 시연 시나리오)와 [경고 정책](../../docs/archive/warning-policy.md) 검증 항목(경계값·재실/부재·timeout)이 코드에 고정돼 있지 않다는 공백
- [재범에게](../../docs/archive/to-재범.md) 완료 조건: 반려동물 위험, 사용자 재실, offline이 화면에서 혼동되지 않아야 함
- 열린 문서 PR 11건의 병합은 **이 작전의 범위가 아님** — 같은 시각에 머지 전문가가 그 축을 담당한다

## 요약

**문서 PR을 더 쌓는 대신, 하드웨어·P-009 합의 없이도 닫을 수 있는 시연 공백을 코드로 고정한다.** 우선순위 사다리는 이미 `SafetyPolicy.evaluate`에 있지만 검사와 HMI가 정상·화재 위험만 다루고, 오탐 억제·연결 끊김·반려동물 잠금이 같은 화면에서 구분되지 않았다. [실측]

이 작전의 구현 범위는 다음으로 한정한다.

1. 정책 분기 순서·경계값·오탐 억제를 실행 가능한 시나리오 카탈로그와 테스트로 고정
2. Flutter 데모 HMI에서 그 시나리오를 바로 재생하고, 반려동물 / 화재 / offline이 서로 다른 문구로 보이게 함
3. Pi4 데모 순환에 위험 온도(≥90°C) 샘플을 넣어 화재 경로가 관측 노드에서도 한 번은 나오게 함

P-009 보드 재배치, LG 원문 반입, `person_present` 3값화, 기능점검 웹 서비스는 합의 또는 다른 담당 축이 막혀 있어 이번 구현에서 빼 둔다.

## 확인된 사실

### 열린 PR [실측]

`gh pr list --state open` (2026-09-20):

| PR | 제목 | 비고 |
|---|---|---|
| #34 | P-009에 Pi5 카메라 실기 검증 근거 추가 | 오늘 열림. 문서만 |
| #27 | P-010 LG source-custody | 2026-09-02 Codex 배치 |
| #26 | LG 원문 검토 기록 | 같은 배치 |
| #25 | REF/ 제외 | 같은 배치 |
| #24 | P-009 보류·비전 정책 | 같은 배치 |
| #23 | 배선·SPS30 측정 계획 | 같은 배치 |
| #22 | Pi4 안전 권한 시험 블록 | 같은 배치 |
| #21 | P-009 선행 검증 | 같은 배치 |
| #20 | 현재 구현 / P-009 목표 분리 | 같은 배치 |
| #19 | ARC-001..007 백로그 | 같은 배치 |
| #18 | P-009 상태 `논의 중` | 같은 배치 |
| #16 | LG 제공 문서 정리 | 원문 변환본. [문서] 적대적 검토에서 **Drop** |

이 11건(#16, #18~#27)의 충돌·병합 순서는 [적대적 검토](2026-09-02-open-pr-adversarial-review.md)와 [2026-09-09 재검토](../../docs/2026-09-09%20codex%20review.md)가 이미 정했다. **코드 경로(`packages/`·`flutter_app/`·`pi4_sensor_node/`)를 건드리는 열린 PR은 없다.** [실측]

`main`은 `5248d97` (PR #33 병합, vision_node V4L2 실기 기록). [실측]

### 구현 공백 [실측]

| 공백 | 근거 | 이번 작전 |
|---|---|---|
| `SafetyPolicy` 테스트가 정상·부재 고온·반려동물·offline·PM 위험·팬 하한만 검사. 주의/경고 분기, 경계값 직전·동일, 우선순위 충돌, 복구가 없음 | `packages/hood_seek_protocol/test/hood_seek_protocol_test.dart` | **구현** |
| 음성 파서가 강풍·인덕션 두 문장만 검사 | 같은 파일 | **구현** |
| Flutter 테스트가 정상·팬 변경·화재 위험만 검사. 반려동물/offline 혼동 금지 미검증 | `flutter_app/test/dashboard_screen_test.dart`, [to-재범.md](../../docs/archive/to-재범.md) | **구현** |
| 위험 카드 아이콘이 반려동물 잠금에도 화재 아이콘 | `dashboard_screen.dart` `SafetyLevel.danger => Icons.local_fire_department_outlined` | **구현** |
| `pet_access_confirmed`를 화면에 명시하지 않음 | 같은 파일. 근거는 `reason` 한 줄뿐 | **구현** |
| 데모 feed가 정적 정상 상태만 보여 오탐·fail-safe 리허설이 안 됨 | `DemoKitchenFeed`, F-4 | **구현** |
| Pi4 데모 순환에 온도 ≥90°C가 없어 화재 분기가 관측 노드에서 안 나옴 | `pi4_sensor_node/src/main.cpp` `demo_reading` | **구현** |
| 기능점검 웹 서비스 | [설계](../../docs/2026-08-30-feature-check-web-service-design.md)가 **합의 필요** | **보류** |
| `person_present` 3값 + heartbeat | F-1, 계약 `schema_version` 상향 | **보류** |
| P-009 역할 재배치 | 상태 `제안`, 결정권자 미서명 | **보류** |
| 고양이 detection 발행자 | 계약·tracker만 있고 Pi4/Pi5 발행 코드 없음. 비전은 `jaebeom/HOOD-SEEK` 쪽 | **보류** — 발행 없이 정책 입력(`petAccessConfirmed`)으로 시연 |

## 시나리오 표 (이번 구현의 작전 계획)

카탈로그 ID는 `packages/hood_seek_protocol/lib/safety_scenarios.dart`와 같다. 임계값은 데모 기본값이며 제품 기준이 아니다.

| ID | 시연 이름 | 입력 | 기대 | 왜 필요한가 |
|---|---|---|---|---|
| `normal` | 정상 | 42°C, PM 18, 재실 | 정상, 팬 하한 1, 잠금 아님 | 기준 화면 |
| `caution-temp` | 주의 | 55°C | 주의, 팬 하한 2 | 경계값 동일 |
| `warning-pm` | 경고 | PM 80 | 경고, 팬 3 | 온도가 아닌 PM 경로 |
| `unattended-danger` | 부재 고온 | 70°C, 부재 | 위험, 잠금, 부재 고온 근거 | 화재가 아닌 부재 경로 |
| `fire` | 화재 | 90°C | 위험, 잠금, 온도 근거 | 열원 차단 시연 |
| `pet-absent` | 반려동물 | 부재 + 접근 확정 | 위험, 잠금, 반려동물 근거 | D-004의 잠금 성립 |
| `pet-present` | 오탐 억제 | 재실 + 접근 확정 | **정상, 잠금 아님** | F-4. 고양이만으로 안 잠근다 |
| `offline` | 연결 끊김 | sensorOnline=false | 연결 끊김, 팬 3, 잠금 | heartbeat fail-safe |
| `fire-over-pet` | 화재 우선 | 95°C, 부재, 접근 확정 | 근거는 온도, 플래그는 유지 | 우선순위 |
| `voice-floor` | 팬 하한 | PM 90, 팬 요청 0 | 경고인데 팬은 3 | 음성·수동이 하한을 못 뚫음 |

카탈로그 밖 검사는 테스트에만 둔다. 화면을 늘리지 않기 위해서다.

| 검사 | 기대 |
|---|---|
| offline vs 화재·PM·반려동물·부재 고온 동시 | 결과는 항상 offline |
| 90°C vs PM 200 + 반려동물 | 근거는 온도 |
| PM 150 vs 부재 70°C + 반려동물 | 근거는 PM |
| 부재 70°C vs 반려동물 | 근거는 부재 고온 |
| 70°C + PM 80 | 근거는 온도 경고 |
| 부재 55°C | 경고(부재+열원), 잠금 아님 |
| 55°C + PM 35 | 주의, 근거는 온도 |
| 54.9 / 55, 69.9 / 70, 89.9 / 90, PM 34.9 / 35 / 79.9 / 80 / 149.9 / 150 | 선언한 쪽에 남음 |
| offline 뒤 신선한 snapshot | 정상으로 복구 |

## 이번에 하지 않는 것

| 항목 | 이유 |
|---|---|
| #16·#18~#27 병합 | 머지 전문가 축. 충돌 매트릭스가 이미 있다 |
| P-009 상태 문구 변경 | 결정 기록 표를 에이전트가 대신 채우지 않는다 |
| 기능점검 웹 서비스 | 설계가 합의 전이고 새 실행 파일이 필요 |
| `person_present` 3값 | 계약 파괴적 변경. F-1은 통보이나 소비자 전환이 같이 움직여야 함 |
| 실제 카메라·YOLO·I2C | 이 환경에 기기가 없고, 비전 코드는 별도 저장소에 있다 |
| 음성 마이크 UI | 파서와 하한은 카탈로그로 고정. `speech_to_text` 추가는 시연 환경 합의 후 |

## 이전 문서 수정 필요 사항

| 문서 | 지금 상태 | 이 작전이 고치는 것 |
|---|---|---|
| [warning-policy.md](../../docs/archive/warning-policy.md) | 검증 항목이 목록만 | 실행 가능한 시나리오 절과 테스트 경로 |
| [to-재범.md](../../docs/archive/to-재범.md) | UI 후속 1·2가 미완 | 반려동물 칩·시나리오 구분 완료로 갱신 |
| [flutter-ui.md](../archive/category/flutter-ui.md) | 합의 공란 | 시연 시나리오 칩과 구분 문구 |
| [warning-ux.md](../archive/category/warning-ux.md) | 합의 공란 | 오탐 억제·fail-safe 시연 경로 |

## 재현 방법

```bash
cd packages/hood_seek_protocol && dart pub get && dart test
cd flutter_app && flutter pub get && flutter test
cmake -S pi4_sensor_node -B pi4_sensor_node/build && cmake --build pi4_sensor_node/build
ctest --test-dir pi4_sensor_node/build --output-on-failure
# 데모 HMI에서 시나리오 칩 확인
cd flutter_app && flutter run -d chrome
```

## 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| 남현지가 Fresh/Grace/Unknown-safe를 승인했는가 | P-009 결정 기록 표에 본인 기입 | 남현지 |
| #16 원문 반입 권한 | 원본 PDF 배포 제한 표기 | 남현지 |
| 비전 발행자를 이 저장소 `pi4_sensor_node`에 넣을지, `jaebeom/HOOD-SEEK`를 쓸지 | 팀 합의 | 재범·병직 |
| 손난로로 90°C 화재 시나리오를 실측할 수 있는가 | C-2 | 병직 |

## 근거

- 열린 PR: `gh pr list --state open` [실측]
- `main`: `git log -1 --oneline` = `5248d97` [실측]
- 정책 분기: [`hood_seek_protocol.dart`](../../docs/archive/mqtt-generation/packages/hood_seek_protocol/lib/hood_seek_protocol.dart) `SafetyPolicy.evaluate` [문서]
- 기존 테스트 범위: 위 「구현 공백」 표의 파일 [실측]
- F-4: [확인·결정 목록](../김병직/2026-08-30-open-questions-and-decisions.md) [문서]
- D-004: [의사결정 대장](../../docs/ai-integration-decisions.md) [문서]
