# HOOD&SEEK 기여 규칙 (10분 버전)

처음 합류했으면 이 문서만 읽고 시작합니다. 긴 설명은 끝의 링크에 있습니다.

## 0. 설정 (최초 1회)

```bash
git clone https://github.com/lambdaramji/HOOD-SEEK.git && cd HOOD-SEEK
git config user.name "본인 이름"
git config user.email "GitHub에 등록된 이메일"     # 기여도 귀속에 필요
```

빌드 환경(atlas-dev Docker)은 마스터 PC를 기준으로 운영합니다. 팀원의 실제 빌드 작업은 마스터 PC에서 수행합니다(원격 SSH 가능).

## 1. 작업 흐름

```text
이슈 만들기 → 브랜치 → 커밋 → PR (Closes #n) → 요청 리뷰어 전원 승인 + CI → 작성자가 병합 → 보드 자동 Done
```

1. **이슈부터.** 없는 작업은 하지 않습니다. New issue → 템플릿 4종 중 하나 (작업 / 결함 / 실측 / 결정). 이슈는 하루 안에 끝나는 크기로.
2. **브랜치** `<종류>/<이름>-<이슈번호>-<topic>` — 예: `feat/jeongin-12-uart3-overlay`, `fix/jaebeom-31-tcp-reconnect`, `docs/hyunji-40-threshold-record`
3. **커밋** `종류(영역): 한국어 결과 문장` — 예: `feat(safety_core): uart3 송신과 ACK 타임아웃 추가`. 종류: `feat` `fix` `docs` `test` `chore` `refactor`. 영역: `safety_core` `vision` `voice` `flutter` `esp32` `contracts` `profiles` `deploy` `ci`.
4. **PR** 본문 첫 줄에 `Closes #이슈번호`. 템플릿 체크 항목만 채우면 됩니다. 제목은 커밋 규칙과 같게.
5. **리뷰·병합** CODEOWNERS가 자동 지정합니다. **요청한 리뷰어 전원이 Approve**하고 CI가 초록이면 **작성자가 병합합니다.** 남의 PR은 병합하지 않습니다. 작성자가 병합할 수 없으면 PR 댓글로 위임받은 뒤 병합하고, 아래 경고 댓글에 사유를 답니다 (2026-10-22 PM 결정).
   - `all_reviewers_approved` 검사가 아직 요청 중인 리뷰어나 마지막 결정이 승인이 아닌 리뷰어(코멘트만 남긴 경우 포함)가 있으면 실패합니다. 새 push로 승인이 무효화되면 다시 승인받아야 합니다.
   - 작성자가 아닌 사람이 병합하면 `Merge watch`가 PR에 경고 댓글을 남깁니다.
   - 라벨 `safety-invariant`·`contract-change`가 붙으면 **PM(@lambdaramji) Approve가 추가로 필요**합니다.
6. **병합 방식** 혼자 만든 PR은 *Squash and merge*, 여러 명 커밋이 섞였으면 *Merge commit* (공동 작성자 보존). 병합 후 브랜치 삭제.

`main`에 직접 푸시하지 않습니다 — Ruleset이 막습니다(PR 필수, 승인 1, Code Owners, 스레드 해결, `system_ci_result`·`flutter_result`·`all_reviewers_approved` 통과, force-push·삭제 금지, bypass 없음). `v*` 태그는 삭제·갱신이 막혀 있습니다. `git push --force`, `git reset --hard`는 팀 합의 없이 쓰지 않습니다. PR에서 **Enable auto-merge**를 누르면 승인·CI가 끝나는 순간 자동 병합되고 브랜치는 자동 삭제됩니다.

## 2. 병합 전 로컬에서 돌릴 것

CI와 **같은 명령**입니다. 바꾼 영역 것만 돌리면 됩니다.

| 바꾼 곳 | 명령 |
|---|---|
| `safety_core/` | `cmake -S safety_core/tests -B safety_core/tests/build && cmake --build safety_core/tests/build && ctest --test-dir safety_core/tests/build --output-on-failure` |
| `flutter_app/` | `cd flutter_app && flutter pub get && dart format lib test && flutter analyze && flutter test && flutter build web` |
| `contracts/` | 스키마·예제 짝 맞추기. CI가 `jsonschema`로 예제 검증 |
| `profiles/` | `python -m json.tool profiles/default.json` |
| `firmware/<보드>/` | `cd firmware/<보드> && pio run` |
| `voice_node/` | `cmake -S voice_node -B voice_node/build -DCMAKE_BUILD_TYPE=Release && cmake --build voice_node/build && ctest --test-dir voice_node/build --output-on-failure` — 모델 없는 파서/VAD/TCP는 System CI. ASR/PulseAudio ARM 빌드·실기 결과는 PR 본문에 별도 기록 |
| `vision_node/` | atlas-dev Docker 크로스 빌드. CI 없음 → PR 본문 "검증"에 명령·결과 기록 |
| `docs/` `inbox/` | 실질 검사 없음(discover만 돌고 전부 skip → 초록). 출처 표기 [실측]/[문서]/[추정] 확인 |

## 3. 절대 혼자 바꾸지 않는 것

| 대상 | 절차 |
|---|---|
| `contracts/*.schema.json` (10/20 18:00 동결) | 결정 이슈 → PM 승인 → 스키마 + 예제 + `v` 증가 + **송신·수신 양쪽 파서를 한 PR에** |
| `profiles/default.json` 수치 | 실측 이슈에 출력 원문 → 그 번호를 PR에 |
| CLAUDE.md 안전 불변식 | 근거 + 테스트 수정을 같은 PR에. 완화 방향은 불가 |
| 남의 담당 문서를 `deprecated`로 | 담당자 확인 후 |
| `docs/archive/**` `inbox/archive/**` `REF/**` | ARCHIVE_POLICY.md. 에이전트·검색 제외 경로 |

## 4. 문서 쓸 때

- `docs/`·`inbox/`의 모든 사실에 **[실측] / [문서] / [추정]** 표기. 추정에는 확인 방법을 같이 적고, 못 적으면 넣지 않습니다.
- 수치·오류 메시지는 요약하지 말고 **출력 그대로**.
- 이전 문서를 뒤집으면 "무엇을 어떻게 고쳐야 하는지" 표를 남깁니다.
- LG 기밀 자료는 원문을 옮기지 않고 문서명·절 번호와 결론만.
- 팀 승인이 필요한 선택은 `docs/decision/TEMPLATE.md`를 복사해 `P-0xx-topic.md`로. 짧으면 결정 이슈로 끝냅니다.

## 5. 보드·마일스톤

- 이슈·PR을 열면 Projects 보드에 자동으로 들어갑니다. **직접 옮길 것은 In Progress와 Blocked뿐**입니다.
- 마일스톤 G1~G8은 날짜 게이트입니다. 이슈를 만들 때 반드시 하나 붙입니다. 자세한 운영은 [docs/github-projects-guide.md](docs/github-projects-guide.md).

## 6. 더 읽기

- Git이 처음이면 → [docs/git-start-guide.md](docs/git-start-guide.md)
- 기여 귀속·포트폴리오·공동 작성자 → [docs/contribution-guide.md](docs/contribution-guide.md)
- AI 에이전트로 작업할 때 → [AGENTS.md](AGENTS.md), [CLAUDE.md](CLAUDE.md)
- 설계·계약·판정 규칙 → `docs/2026-10-19-s3-s10-design-v0.1.md`, `contracts/README.md`
