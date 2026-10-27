# 2026-09-20 열린 PR 머지 검증

- 작성자: Cursor Grok 4.6 (머지 담당)
- 작성일: 2026-09-20
- 상태: 검증·실행 완료. P-009 역할 재배치는 이 문서로 결정하지 않음
- 기준 저장소: `yujh5537/HOOD-SEEK`
- 기준 `main`: `5248d9706934bfdf5604625e3310fe4b651dc853`
- 검토 대상: 검증 당시 열린 PR #16, #18~#27, #34
- 선행 검토: [열린 PR 11건 적대적 검토](../inbox/임재범/2026-09-02-open-pr-adversarial-review.md), [2026-09-09 Codex review](2026-09-09%20codex%20review.md)

이 문서는 **지금 형태 그대로 병합해도 되는가**만 답한다. 카메라 실측·LG 회신·역할 재배치의 신규 승인은 여기에 없다.

## 표기 규칙

| 표기 | 뜻 |
|---|---|
| **[실측]** | 이 checkout에서 명령을 실행해 확인. 명령은 「재현 방법」에 있음 |
| **[문서]** | 저장소 문서·PR diff·GitHub API 응답 |
| **[추정]** | 확인하지 못한 해석. 확인 방법을 함께 적음 |

## 이 문서가 답하는 미결 항목

- 2026-09-09 정리의 실행 순서(#28 먼저 병합, #18~#27 통합)가 오늘 `main`에서도 유효한가
- 오늘 새로 열린 #34를 지금 병합해도 되는가
- #16·#18~#27을 개별 병합해도 되는가

## 요약

**지금 형태 그대로 병합해도 되는 열린 PR은 검증 당시 #34 하나였다.** `#34`는 2026-09-20T12:57:23Z에 `main`으로 병합됐다 (`7ae88de`). 나머지는 병합하지 않고, 살릴 문장만 통합 재작성한다. [실측]

### 실행 결과 [실측 2026-09-20]

| PR | 결과 |
|---|---|
| #34 | MERGED |
| #36 | MERGED (`f4b3e0e`) — 시나리오 카탈로그. 이 검증의 대상은 아님 |
| #37 | MERGED (`ef160e4`) — #18~#27 유효 문장 통합 재작성 |
| #16, #18~#27, #35 | CLOSED, 병합하지 않음 |

`gh pr list --state open` 결과 0건. P-009는 여전히 **제안**. 원문 Git 반입은 여전히 P-010 보류.

| PR | 판정 | 이유 (한 줄) |
|---|---|---|
| **#34** | **병합 가능** | 결정 칸을 건드리지 않고 선행 확인 표에 실측만 채움 |
| **#16** | **Drop** | LG 원문 변환본 43파일 +10,660줄을 Git에 넣음 |
| **#18** | **통합 재작성** | 상태만 `논의 중`으로 바꾸고, 정의 없는 `ARC-001`을 인용 |
| **#19** | **통합 재작성** | 승인 뒤 착수와 승인 전 실측이 순환 |
| **#20** | **통합 재작성** | 코드 경로를 「완료된 사실」로 적어 물리 결선과 혼동 |
| **#21** | **통합 재작성** | 상태 어휘에 없는 값. #34와 텍스트 충돌 |
| **#22** | **통합 재작성** | x86 컨테이너 미실행을 「실패」로 기록. 재실행 명령이 명제를 못 증명 |
| **#23** | **통합 재작성** | 텍스트 충돌은 없으나 출처 경로가 없고, 9/2 이후 실측이 미반영 |
| **#24** | **Drop (대리 승인)** | 남현지·정정인·김병직 결정 칸을 대신 채움 |
| **#25** | **통합 재작성** | `REF/` 제외 방향은 맞음. 아카이브 절에 잘못 끼움. #16은 못 막음 |
| **#26** | **통합 재작성** | 원문 미저장 원칙은 맞음. VisionService를 후보로 남긴 결론은 LG 회신과 어긋남 |
| **#27** | **Drop (사실 오류)** | 「remote 없음 / PR #16을 못 찾음」은 실행 환경 제약이지 저장소 사실이 아님 |

P-009 문서 상태는 `main`에서 여전히 **`제안`**이다. 이 검증이 `논의 중`·`보류`·`확정`으로 올리지 않는다. [실측]

## 확인된 사실

### 기준면 [실측]

| 항목 | 값 |
|---|---|
| `origin/main` | `5248d9706934bfdf5604625e3310fe4b651dc853` (`Merge pull request #33`) |
| 열린 PR | 검증 당시 12건. 코드 변경 0건 |
| CI | 12건 모두 `flutter.yml`·`system.yml` 경로 트리거 밖. 사람 리뷰가 유일한 게이트 |
| 사람 리뷰·댓글 (검증 직전) | 12건 모두 0 |
| 이미 병합된 선행 작업 | #28, #29, #31, #32, #33 |

2026-09-09 문서의 「#28을 먼저 병합한다」는 실행 순서는 **이미 끝났다.** 그 문서를 따라 개별 순차 병합하면 안 된다.

### 각 PR × `main` [실측]

12건 모두 `git merge-tree origin/main prN`에서 텍스트 충돌이 없다. GitHub의 초록색은 「서로는 충돌한다」를 보여 주지 않는다.

### 텍스트 충돌 매트릭스 [실측]

`git merge-tree --write-tree --messages prA prB`에서 `CONFLICT`가 난 쌍만 적는다.

| A × B | 충돌 파일 |
|---|---|
| #18 × #21 | `docs/decision/P-009-pi4-pi5-role-reallocation.md` |
| #18 × #24 | `P-009-...md`, `docs/decision/README.md` |
| #18 × #27 | `docs/decision/README.md` |
| #19 × #21 | `docs/ai-integration-decisions.md` |
| #19 × #24 | `docs/ai-integration-decisions.md` |
| #19 × #27 | `docs/ai-integration-decisions.md` |
| #21 × #22 | `P-009-...md` |
| #21 × #24 | `docs/ai-integration-decisions.md`, `P-009-...md` |
| #21 × #27 | `docs/ai-integration-decisions.md` |
| **#21 × #34** | **`P-009-...md`** ← 2026-09-20에 추가된 쌍 |
| #24 × #27 | `docs/ai-integration-decisions.md`, `docs/decision/README.md` |
| #25 × #27 | `.gitignore` |

2026-09-02 매트릭스와 같다. 새로 생긴 충돌은 **#21 × #34**뿐이다.

### 조용한 겹침 [실측]

텍스트 충돌은 없지만 같은 파일을 다른 hunk에서 고친다. 순서대로 넣으면 한 문서에 전제가 다른 절이 나란히 남는다.

| 쌍 | 파일 |
|---|---|
| #18 × #19, #18 × #22, #18 × #34 | `P-009-...md` |
| #19 × #22, #19 × #34 | `P-009-...md` |
| #22 × #24, #22 × #34 | `P-009-...md` |
| #24 × #34 | `P-009-...md` |

### P-009 상태 한 줄 [실측]

`docs/decision/README.md` 어휘는 `제안` · `논의 중` · `확정` · `보류` 넷이다.

| 위치 | 상태 |
|---|---|
| `main` | `제안` |
| #18 | `논의 중` |
| #21 | `제안 — 선행 검증 대기` (**어휘에 없음**) |
| #24 | `보류` |
| #19, #22, #34 | `제안` (헤더 미변경) |

### #16 [실측]/[문서]

- 파일 43개, 추가 10,660줄, 전부 `docs/markdown/`
- README: 「원본 PDF 86개를 마크다운으로 변환·색인화했습니다」
- 변환본에서 `대외비|기밀|confidential|internal use only|사내한|proprietary` 매칭 **0건**. 원문 PDF 표기는 이 저장소에서 확인 불가
- 활성 문서 `inbox/김병직/2026-08-30-device-survey-and-vision-test.md`는 「배포받은 개발 문서 (기밀 표기 있음 — 원문은 저장소에 올리지 않고 문서명만 인용)」이라고 적음
- `CLAUDE.md`: 기밀 표기가 있는 자료는 원문을 옮기지 않음. `inbox/README.md` 5항도 동일
- `#25`·`#27`의 `REF/` ignore는 `docs/markdown/`을 막지 않음

### #27 「현재 확인 결과」 [실측]

P-010이 적는 값과 이 checkout의 값:

| P-010 서술 | 이 checkout |
|---|---|
| remote 설정 없음 | `origin` = `https://github.com/yujh5537/HOOD-SEEK` |
| 브랜치 `work` 하나 | `main` + `refs/pull/*/head` |
| PR #16을 찾지 못함 | open, head `yujh5537-patch-1`, 파일 43 |
| 추적된 LG 원문 없음 | #16이 그 변환본을 추가하는 중 |
| 결정 기록자 `Codex` | 결정 문서 규칙을 위반. 사람 입력이 아님 |

### #24 결정 기록 [문서]

남현지·정정인·김병직 세 행을 2026-09-02 의견·결론으로 채운다. `docs/decision/README.md`는 「팀원은 그 표에 직접 의견과 날짜를 채운다」고 한다. 대응하는 사람 서명 기록이 활성 문서에 없다.

#24가 승인 근거로 쓰는 3.7~3.8 FPS는 `VisionService1`+GPU 경로다. LG 2026-09-03 회신으로 그 경로는 RPi5 공식 미지원이다. [문서] `inbox/임재범/2026-09-03-lg-answer-review.md`

### #22 재실행 명령 [문서]

결정 문서에 넣은 재실행 절차:

```bash
dart compile aot-snapshot pi5_gateway/bin/hood_seek_gateway.dart -o /tmp/hood-seek-safety.aot
timeout 5s dart run pi5_gateway/bin/hood_seek_gateway.dart
```

`aot-snapshot`은 자립 실행 파일이 아니다. `dart run`은 Dart SDK가 있는 환경이다. P-009가 묻는 「Yocto 이미지 재빌드 없이 바이너리만 갱신할 수 있는가」를 이 명령으로 답할 수 없다.

원문 로그의 `uname`은 `x86_64`이고 `dart: command not found`다. 대상 보드 실패가 아니라 **실행 환경 부재**다.

### #20 「완료된 사실」 [문서]

`docs/final-demo-architecture.md`에 「Pi5 gateway, Pi5의 Mosquitto broker, Pi5와 ESP32 사이의 USB serial 연결은 **완료된 사실**」을 넣는다. `docs/development-progress.md`는 「실제 장비 통합 **미완료**」다. 같은 배치 #23은 안전 gateway ↔ ESP32를 **미확인**으로 둔다.

### #34 [실측]/[문서]

- 파일 1개, +3/−1. 상태 헤더·결정 칸·결정 기록 표를 변경하지 않음
- 선행 확인 「카메라를 Pi5에 물릴 수 있는 폼팩터인지」만 채움
- 근거 링크 두 개는 이미 `main`에 있음 (`inbox/김병직/2026-09-20-vision-node-*.md`)
- 상대 경로 `../../inbox/김병직/...`는 `docs/decision/` 기준으로 유효
- 3.2 FPS는 `jaebeom/HOOD-SEEK` 실험 코드, TFLite **CPU/XNNPACK**. 본문이 통합 성능이 아니라고 명시
- #21과만 텍스트 충돌. #18·#19·#22·#24와는 같은 파일의 다른 hunk — 그 PR들을 이후에 병합하면 #34 근거가 섞이거나 덮일 수 있음

## 시험 기록

| # | 명령 | 결과 |
|---:|---|---|
| 1 | `git rev-parse origin/main` | `5248d9706934bfdf5604625e3310fe4b651dc853` |
| 2 | `git fetch origin refs/pull/{16,18-27,34}/head` 후 각 `merge-tree origin/main prN` | 12건 모두 충돌 없음 |
| 3 | 쌍별 `merge-tree prA prB \| grep CONFLICT` | 위 매트릭스. #21×#34 추가 확인 |
| 4 | 같은 파일·CONFLICT 0인 쌍 목록 | 위 조용한 겹침 |
| 5 | `git grep -c -iE '대외비\|기밀\|…' pr16 -- docs/markdown` | 매칭 0 |
| 6 | `gh pr view 16 --json state,headRefName,changedFiles` | `OPEN`, `yujh5537-patch-1`, 43 |
| 7 | `git remote -v` | `origin` 존재 |
| 8 | 각 PR `gh pr view --json comments,reviews` | 검증 직전 모두 0 |
| 9 | `.github/workflows/*.yml` `on.pull_request.paths` | 열린 PR 변경 경로와 교집합 없음 |

## 남은 문제 [추정]과 확인 방법

- **[추정]** #16 원본 PDF 86개에 배포 제한 표기가 있는지는 변환본 0매칭으로 부정할 수 없다. **확인 방법:** 원본 PDF 표지·머리말·꼬리말을 남현지가 확인한다. 이 저장소 변환본을 근거로 쓰지 않는다.
- **[추정]** #23의 배선·측정 계획 자체는 유용할 수 있다. **확인 방법:** 출처를 `inbox/김병직/2026-08-30-device-survey-and-vision-test.md` 절 링크로 고친 뒤, 오늘자 V4L2 실측(#32·#33)과 모순이 없는지 한 번 더 대조한다. 고치기 전에는 병합하지 않는다.

## 이전 문서 수정 필요 사항

| 문서 | 지금 상태 | 이 검증이 고치는 것 |
|---|---|---|
| [2026-09-09 Codex review](2026-09-09%20codex%20review.md) | 기준 `main`=`100a6e1`, 실행 1단계가 「#28 병합」 | 머리말에 **실행 순서는 이 문서가 대체한다**고 표시. 판정 표(#16 Drop, #18~#27 통합, 대리 승인 금지)는 유지 |
| [적대적 검토](../inbox/임재범/2026-09-02-open-pr-adversarial-review.md) | 기준 `main`=`100a6e1`, 대상에 #34 없음 | 충돌 매트릭스에 #21×#34를 추가해야 함. 본 문서가 그 갱신본이다 |
| `docs/decision/P-009-…md` | `2026-08-29 codex review.md` 상대 링크 | `main`에 해당 파일이 활성 `docs/`에 없음. **이번 열린 PR의 신규 결함이 아님.** 별도 링크 수정 |

## 재현 방법

```bash
git fetch origin main
git rev-parse origin/main   # 5248d9706934bfdf5604625e3310fe4b651dc853

for n in 16 18 19 20 21 22 23 24 25 26 27 34; do
  git fetch origin "refs/pull/$n/head:pr$n"
done

for n in 16 18 19 20 21 22 23 24 25 26 27 34; do
  git merge-tree --write-tree --messages origin/main "pr$n" | grep -i CONFLICT \
    && echo "PR#$n conflicts with main"
done

for a in 16 18 19 20 21 22 23 24 25 26 27 34; do
  for b in 16 18 19 20 21 22 23 24 25 26 27 34; do
    [ "$a" -ge "$b" ] && continue
    git merge-tree --write-tree --messages "pr$a" "pr$b" 2>/dev/null | grep -q CONFLICT \
      && echo "#$a x #$b"
  done
done
```

## 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| LG 원본 PDF 배포 제한 표기 | 원본 표지·머리말·꼬리말. 변환본 부재를 근거로 쓰지 않음 | 남현지 |
| #16 변환본 Git 반입 여부 | PR은 닫힘. 원문 보관 정책은 P-010 결정 기록 | 남현지 |
| Dart AOT·ESP32 Pi4 재배선 | 실제 Pi4에서 실행. #22 로그는 증거가 아님 | 정정인 |
| Fresh/Grace/Unknown-safe 승인 | P-009 결정 기록 표에 본인이 기입 | 남현지 |

## 근거 목록

- 충돌·clean merge·파일 목록: 위 「재현 방법」 [실측]
- PR 본문·diff: `git diff origin/main...prN`, `gh pr view` [문서]
- 저장소 규칙: `CLAUDE.md`, `AGENTS.md`, `docs/decision/README.md`, `inbox/README.md`, `.github/pull_request_template.md` [문서]
- 기밀 원문 존재 진술: `inbox/김병직/2026-08-30-device-survey-and-vision-test.md` [문서]
- VisionService 미지원: `inbox/임재범/2026-09-03-lg-answer-review.md` [문서]
- #34 근거: `inbox/김병직/2026-09-20-vision-node-pi5-runtime-gap.md`, `inbox/김병직/2026-09-20-vision-node-v4l2-fix-and-live-test.md` [문서]
