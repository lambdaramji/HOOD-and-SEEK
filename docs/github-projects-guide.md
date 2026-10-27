# GitHub Projects·마일스톤 운영 가이드

- 작성: 2026-10-19 / 갱신: 2026-10-19 15:30 (실제 적용 결과·출처 표기·스크립트 수정 반영) / 적용 대상: 10/19~10/28 개발 기간과 11/5 시연까지
- 한 줄 요약: **이슈가 작업의 단위, 마일스톤이 날짜, Projects 보드가 현황판**이다. 메신저로 정한 것도 보드에 없으면 없는 것으로 친다.

## 1. 세 가지가 각각 맡는 것

| 도구 | 답하는 질문 | 누가 만지나 |
|---|---|---|
| **Issue** | 무엇을 하고, 끝났는지 어떻게 아는가 | 작업자가 열고, PR `Closes #n`으로 닫힘 |
| **Milestone (G1~G8)** | 언제까지인가. 게이트를 넘겼는가 | PM만 만들고, 작업자는 이슈에 붙이기만 |
| **Projects 보드** | 지금 누가 무엇을 하고 어디서 막혔나 | 전원. 하루 한 번 자기 카드 Status 갱신 |

마일스톤 번호는 **G**(게이트)를 쓴다. 데모 시나리오 M1~M13과 섞이지 않게 하기 위해서다.

| 마일스톤 | 마감 (KST) | 통과 조건 |
|---|---|---|
| G1 환경·계약 동결 | 10/20 18:00 | atlas-dev 2대, IP·SSH, uart·I2C 실측, 계약 v1 동결, whisper 빌드 판정, Q-S4~S10 결정 |
| G2 1차 통합 | 10/22 | Pi4↔Pi5↔ESP32×2 전 링크, fail-safe F1·F3·F4 실증 |
| G3 임계값 확정 | 10/23 | 손난로·인센스·가습기 재실측 → `profiles/default.json` |
| G4 Must 시나리오 완료 | 10/24 | M1·M2·M3·M6·M8·M10·M12·M13 리허설 통과 (설계서 §7). M7(가습기)은 10/26에 뺌, 10/26 판정 8/8 통과(#67, D-057) |
| G5 Should 통합 | 10/25 | M9 → M11 → 음성(M4·M5) 순 |
| G6 기능 동결·E2E | 10/26 18:00 | E2E 3회 연속, ipk 시도 |
| G7 코드 프리즈·제출 | 10/27 22:00 / 10/28 13:00 | 태그, README, 삭제 목록 처리, 제출 |
| G8 라이브 시연 | 11/5 | 안정화, Could 보강, 발표 |

G4 미달이면 G5의 Should 이슈를 전부 `G8`로 미루거나 닫는다 (D-29 폴백 트리거). 미실시는 미달이 아니다 — Must를 돌려 실패했을 때 적용(#67 합의 10/25, D-033).

## 2. 보드 필드

프로젝트: https://github.com/users/lambdaramji/projects/1 (비공개. 팀원은 Manage access에서 Write로 추가해야 보인다 — 저장소 권한과 별개. 개인 소유 프로젝트의 collaborator는 수락 단계 없이 추가 즉시 권한이 생긴다)

| 필드 | 값 | 채우는 기준 |
|---|---|---|
| Status | Backlog · Todo · In Progress · In Review · Blocked · Done | 작업자가 직접. PR 열면 In Progress(자동), 병합되면 Done(자동) |
| 트랙 | T1~T7 | 이슈 라벨 `track:`과 같게 |
| 우선순위 | Must · Should · Could | 라벨과 같게 |
| 시나리오 | M1~M13 · 해당 없음 | 이 작업이 없으면 **가장 먼저** 안 되는 장면 하나. 전 시나리오 공통(계약·저장소 구조)은 해당 없음. M12·M13은 2026-10-19 설계서 v0.2에서 추가 — 보드 옵션에 수동 추가 필요 |
| 실기기 | 필요 · 불필요 | 검증에 Pi·ESP32·센서가 필요하면 "필요". 코딩은 집, 확인만 사무실인 것도 "필요" |
| 목표일 | 날짜 | 마일스톤 마감보다 앞당긴 개인 목표. 비어 있으면 로드맵에 안 보인다 |
| 근거 | 텍스트 | 비워도 됨. 이슈 본문에 설계서 절 번호가 있다 |

## 3. 뷰 4개 (적용 완료)

| 뷰 | Layout | 설정 | 용도 |
|---|---|---|---|
| 보드 | Board | Column = Status, Sort = 우선순위, 필터 `-status:Done` | 아침 스탠드업 |
| 트랙별 | Table | Group by 트랙, Sort 우선순위, 필터 `-status:Done` | 자기 카드 필드 채우기 |
| 로드맵 | Roadmap | Start date 없음·Target date = 목표일, Markers = Milestone, Group by 트랙, Zoom Month(최소 단위) | PM이 게이트 대비 확인 |
| 실기기 대기 | Table | 필터 `실기기:필요 -status:Done`, Group by Milestone | 사무실 가기 전 |

Roadmap은 같은 날짜 필드를 Start·Target 양쪽에 못 건다. Zoom은 Month·Quarter·Year뿐이며 Month가 일 단위 칸을 보여 준다.

## 4. 자동화 (적용 완료, Workflows 9개 On)

| Workflow | 설정 |
|---|---|
| Auto-add to project | 저장소 HOOD-SEEK, 필터 `is:issue,pr is:open` (쉼표는 값 사이에만. `is:issue,is:pr`은 오류) |
| Item added to project | Status → Todo |
| Item reopened | Status → Todo |
| Pull request linked to issue | Status → In Progress (GitHub 기본) |
| Pull request merged | Status → Done |
| Item closed | Status → Done |
| Auto-close issue | 카드를 Done으로 옮기면 이슈 닫힘 (GitHub 기본) |
| Auto-add sub-issues | GitHub 기본 |
| Auto-archive items | `is:closed updated:<@today-2w` (상대 날짜는 `@today` 기준) |

Off: Code changes requested, Code review approved (In Review는 사람이 유지).

## 4b. 브랜치·태그 규칙 (Ruleset, 적용 완료)

| Ruleset | 상태 | 내용 |
|---|---|---|
| main 보호 | Active | 삭제·force-push 금지. PR 필수: 승인 1, Code Owners 리뷰, 새 push 시 승인 무효, 스레드 전부 해결, 필수 check `system_ci_result`·`flutter_result`. **bypass 없음(PM 포함)** |
| 제출 태그 보호 | Active | `v*` 태그 삭제·갱신 금지 |

브랜치 이름 패턴 규칙은 만들지 않는다. [문서] GitHub Docs "About rulesets": 커밋 메타데이터·브랜치/태그 이름 형식 제한 규칙은 Enterprise 플랜 조직용. Pro에서 불가하다고 직접 적힌 문장은 없으나 [실측] 2026-10-19 `POST /repos/…/rulesets` (branch_name_pattern) exit=1로 생성 실패. `scripts/github/setup-rulesets.sh`에서 해당 단계를 삭제했다. 브랜치 형식 `<종류>/<이름>-<이슈번호>-<topic>`은 CONTRIBUTING.md 약속으로만 지킨다.

저장소 설정: 병합 후 브랜치 자동 삭제 On, auto-merge 허용 On. 병합 방식 3종 모두 허용.

CODEOWNERS는 모든 경로에 소유자 2명 이상. PR 작성자 본인 승인은 세지 않아서, 혼자 소유한 경로에 본인이 PR을 올리면 아무도 승인할 수 없기 때문이다. `contract-change`·`safety-invariant` PR에 PM 승인을 요구하는 것은 **Ruleset으로 강제되지 않는 팀 약속**이다(라벨 기반 필수 리뷰어는 Enterprise 기능).

왜 CI를 매번 돌리나: 필수 check가 걸린 워크플로를 `paths:`로 건너뛰면 check가 "Expected"로 남아 병합이 막힌다. 두 워크플로는 매번 돌되 관련 없는 job은 `if`로 skip하고(skip = 성공), 집계 job 하나를 필수 check로 건다. 문서만 바꾼 PR은 discover 10초 + 집계만 돌고 초록이 된다.

## 5. 하루 루틴

| 때 | 누가 | 할 것 |
|---|---|---|
| 09:00 | 전원 | 보드 뷰에서 자기 카드 확인. 오늘 할 카드 In Progress로 |
| 작업 시작 | 작업자 | 이슈 없으면 템플릿으로 먼저 만든다(딱 맞는 이슈가 없으면 "실측" 또는 "작업" 템플릿으로 하나 만들고 `Closes`). 브랜치 `feat/<이름>-<이슈번호>-<topic>` |
| 막힘 | 작업자 | Status → Blocked, 본문에 `Blocked by #n` 또는 라벨 `needs-decision` |
| PR 열기 | 작업자 | 첫 줄 `Closes #n`. **Enable auto-merge** 눌러 두면 승인·CI 끝나는 순간 자동 병합·브랜치 삭제 |
| 리뷰 | 리뷰어 | PR 템플릿 체크 확인 후 Approve. `safety-invariant`·`contract-change`는 PM 추가 |
| 23:00 | PM | 로드맵 뷰로 다음 게이트 대비 미완 Must 확인 |

## 6. 이슈를 쪼개는 기준

- 하루(14시간) 안에 끝나지 않으면 쪼갠다. 큰 이슈는 Sub-issue로 묶는다.
- "검증 방법"에 쓸 명령이 없으면 아직 조사다 → 실측 템플릿.
- 같은 이슈에 2명 Assign은 통합 리허설뿐.

## 7. 제출 후

G7 통과 시 `v1.0.0-submission` 태그. 이후 작업은 G8에 달고 보드는 그대로. 시연 끝나면 보드 Close, README "현재 상태" 갱신.

## 8. 2026-10-19 적용 기록

- PR #42(템플릿 29파일)·#43(#41 재반영) 병합. 라벨 30, 마일스톤 G1~G8, 이슈 #44~#72 29건(전부 assignee·마일스톤·Todo).
- 이슈 일괄 생성은 20초 간격에서 1회 원인 미상 실패, **60초 간격에서는 18건 연속 성공**(8+10). [추정] 422 콘텐츠 생성 남용 감지 — 응답 원문은 당시 스크립트 결함으로 유실. 확인 방법: 아래 수정 적용 후 재발 시 stderr의 응답 원문. 403·429는 한 번도 없었다.
- 스크립트 수정 (이 가이드와 같은 PR에서 적용): ① `setup-rulesets.sh` 브랜치 이름 규칙 단계 삭제(실패 시 뒤의 병합 설정이 안 돌았음) ② `_pace.sh` 기본 `PACE_SECONDS=60`, 모든 실패의 응답 원문을 stderr에 출력, `too quickly|HTTP 422` 감지 시 재시도 없이 즉시 중단 ③ `seed-issues.sh`의 `url=$(…)`를 `if ! url=$(…)` 패턴으로 바꿔 실패 행과 원문이 남게 ④ `setup-project.sh` 끝의 안내 문구를 실제 문법으로(`is:issue,pr is:open`, `updated:<@today-2w`, Item reopened 추가).
- 병합 완료 브랜치 4개 정리 완료 (13:26, 로컬 `-d` → 원격 삭제 순. squash 병합이라 순서가 바뀌면 `-D` 필요).
- #54 Q-S5-1: **B 확정**(고양이 잠금 진입 시 인덕션 즉시 0단), 이슈 댓글로 기록 후 닫음. 대장 `docs/ai-integration-decisions.md` D-041 행.
- 시나리오 M12(짧은 이탈 네거티브)·M13(LAN 분리 fail-safe) 추가(설계서 v0.2 §7, D-040) → 보드 "시나리오" 필드 옵션과 #67 본문에 반영 필요(웹에서).
