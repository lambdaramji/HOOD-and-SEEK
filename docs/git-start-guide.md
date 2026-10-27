# HOOD&SEEK Git 시작 가이드

이 문서는 Git을 처음 사용하는 팀원이 저장소를 망가뜨리지 않고 자신의 작업을 브랜치와 Pull Request로 제출하는 데 필요한 최소 지식을 설명합니다.

기여 기록, 공동 작성자와 포트폴리오 관리 방법은 [기여 기록 및 협업 가이드](contribution-guide.md)를 함께 확인합니다.

## 이것만은 반드시 지킵니다

1. 작업을 시작할 때와 커밋하기 전에 항상 `git status`를 확인합니다.
2. `main`에서 직접 작업하거나 푸시하지 않습니다.
3. 최신 `main`에서 자신의 작업 브랜치를 새로 만듭니다.
4. `git add .`보다 내가 수정한 파일을 지정해서 추가합니다.
5. 다른 팀원의 변경을 삭제하거나 덮어쓰지 않습니다.
6. 커밋하기 전에 `git diff`와 `git diff --staged`를 확인합니다.
7. 원격에 올린 뒤에는 Pull Request와 리뷰를 거쳐 `main`에 병합합니다.
8. 잘 모르는 오류가 나오면 명령을 반복하거나 강제 옵션을 사용하지 말고 화면을 캡처해 도움을 요청합니다.

다음 명령은 팀 합의와 충분한 이해 없이 사용하지 않습니다.

```text
git push --force
git reset --hard
git clean -fd
git checkout -- <파일>
```

이 명령들은 다른 사람의 커밋이나 복구하지 못한 로컬 작업을 없앨 수 있습니다.

## Git과 GitHub의 차이

- **Git**: 내 컴퓨터에서 파일 변경 이력을 저장하고 브랜치를 관리하는 프로그램입니다.
- **GitHub**: Git 저장소를 인터넷에서 공유하고 이슈, Pull Request와 리뷰를 관리하는 서비스입니다.

인터넷이 없어도 로컬에서 수정·커밋할 수 있지만 `fetch`, `pull`, `push`는 원격 GitHub 저장소와 통신하므로 인터넷과 저장소 권한이 필요합니다.

## 꼭 알아야 하는 구조

```text
Working tree → Staging area → Local repository → Remote repository
  파일 수정       git add          git commit          git push
```

- **Working tree(작업 트리)**: 현재 폴더에 보이는 실제 파일입니다.
- **Staging area(스테이징 영역)**: 다음 커밋에 포함하기로 선택한 변경입니다.
- **Local repository(로컬 저장소)**: 내 컴퓨터의 커밋과 브랜치 기록입니다.
- **Remote repository(원격 저장소)**: GitHub에 있는 팀 공유 저장소입니다.

`git add`는 GitHub에 업로드하는 명령이 아닙니다. 다음 커밋에 넣을 변경을 고르는 명령입니다. `git commit`도 내 컴퓨터에 기록할 뿐이며, `git push`를 해야 GitHub에 올라갑니다.

## 최초 한 번 설정

### 저장소 복제

저장소를 아직 받지 않았다면 원하는 상위 폴더에서 실행합니다.

```bash
git clone https://github.com/lambdaramji/HOOD-SEEK.git
cd HOOD-SEEK
```

같은 저장소를 매번 다시 `clone`하지 않습니다. 이미 받은 폴더에서는 `pull`이나 `fetch`로 최신 변경을 가져옵니다.

### 작성자 확인

```bash
git config user.name
git config user.email
```

값이 없거나 다른 사람의 계정이면 이 저장소에서만 설정합니다.

```bash
git config user.name "본인 이름"
git config user.email "GitHub에 등록된 이메일 또는 noreply 이메일"
```

공용 PC에서는 다른 사람의 이름과 이메일로 커밋하지 않도록 매번 확인합니다.

## 매 작업의 표준 순서

아래 순서를 그대로 따라 하면 됩니다.

### 1. 현재 상태 확인

```bash
git status
git branch --show-current
```

수정 중인 파일이 남아 있으면 먼저 자신의 작업인지 확인합니다. 누구의 작업인지 모르는 변경은 삭제하거나 커밋하지 않습니다.

### 2. 최신 `main` 받기

작업 트리가 깨끗할 때 실행합니다.

```bash
git switch main
git pull --ff-only origin main
```

`--ff-only`는 로컬 `main`에 예상하지 못한 별도 커밋이 있을 때 자동으로 복잡한 병합을 만들지 않고 멈춰 줍니다.

### 3. 작업 브랜치 만들기

```bash
git switch -c feat/이름-이슈번호-작업
```

예시:

```bash
git switch -c feat/jaebeom-31-tcp-client
git switch -c fix/jeongin-12-uart-timeout
git switch -c docs/suhyun-40-demo-guide
git switch -c test/byungjik-18-cat-angle
```

브랜치 이름은 영문 소문자와 `-`를 사용하고 `<종류>/<이름>-<이슈번호>-<작업>` 형식을 권장합니다. 이슈 번호를 넣으면 PR과 Projects 보드가 자동으로 연결됩니다. 작업 전에 이슈가 없으면 먼저 만듭니다 ([CONTRIBUTING.md](../CONTRIBUTING.md)).

| 접두사 | 용도 |
|---|---|
| `feat/` | 새 기능 |
| `fix/` | 버그 수정 |
| `docs/` | 문서 작업 |
| `test/` | 테스트 추가·수정 |
| `chore/` | 설정·정리 작업 |

### 4. 파일 수정 후 변경 확인

```bash
git status
git diff
```

- `git status`: 어떤 파일이 수정·추가·삭제됐는지 확인합니다.
- `git diff`: 아직 스테이징하지 않은 실제 변경 내용을 확인합니다.

의도하지 않은 파일, 비밀번호, API 키, 빌드 결과물과 개인 설정이 포함되지 않았는지 확인합니다.

### 5. 필요한 파일만 스테이징

```bash
git add flutter_app/lib/screens/dashboard_screen.dart
git add flutter_app/test/dashboard_screen_test.dart
```

폴더 전체가 모두 자신의 작업일 때는 폴더를 지정할 수 있습니다.

```bash
git add flutter_app/lib
```

스테이징 결과를 다시 확인합니다.

```bash
git diff --staged
git status
```

잘못 추가한 파일은 내용은 유지하면서 스테이징에서만 뺄 수 있습니다.

```bash
git restore --staged <파일경로>
```

### 6. 검사 실행

Flutter 변경이라면 다음 검사를 통과해야 합니다.

```bash
cd flutter_app
dart format --output=none --set-exit-if-changed lib test
flutter analyze
flutter test
cd ..
```

검사가 실패하면 원인을 고치고 변경을 다시 스테이징합니다. 실패한 검사를 숨기거나 테스트를 삭제해서 통과시키지 않습니다.

### 7. 커밋

```bash
git commit -m "feat(voice): add fan voice commands"
```

권장 형식은 `종류(영역): 작업 결과`입니다.

- `feat(flutter): add voice command button`
- `fix(esp32): stop fan on heartbeat timeout`
- `test(fusion): cover cat detection failure`
- `docs(git): add beginner workflow`
- `chore(ci): check Flutter formatting`

“수정”, “작업함”, “최종”, “진짜 최종”처럼 결과를 알 수 없는 메시지는 사용하지 않습니다. 서로 관계없는 변경은 한 커밋에 섞지 않습니다.

### 8. 내 브랜치를 GitHub에 푸시

첫 푸시에서는 현재 브랜치 이름을 확인한 뒤 실행합니다.

```bash
git branch --show-current
git push -u origin feat/이름-이슈번호-작업
```

이후 같은 브랜치에서는 다음만 실행해도 됩니다.

```bash
git push
```

`main`이 표시되면 푸시하지 말고 작업 브랜치를 만들었는지 다시 확인합니다.

### 9. Pull Request 생성

GitHub에서 방금 푸시한 브랜치로 Pull Request를 만듭니다.

- Base: `main`
- Compare: 자신의 작업 브랜치
- 작업 목적과 주요 변경 작성
- 실행한 검사와 결과 작성
- 화면 변경이면 캡처 첨부
- 미완료 사항과 관련 이슈 연결

작성자 외 팀원 한 명 이상이 변경을 확인한 뒤 병합합니다. 수정 요청을 받으면 같은 브랜치에서 수정·커밋·푸시하면 기존 Pull Request가 자동으로 갱신됩니다.

### 10. 병합 후 정리

Pull Request가 GitHub에서 병합된 것을 확인한 뒤 실행합니다.

```bash
git switch main
git pull --ff-only origin main
git branch -d feat/이름-이슈번호-작업
```

이 저장소는 squash merge를 쓰기 때문에 **정상적으로 병합된 브랜치에서도 `-d`는 항상 실패합니다.** 병합이 안 된 것이 아닙니다. GitHub에서 해당 PR이 `Merged`로 표시된 것을 확인했다면 `-D`로 지웁니다.

```bash
git branch -D feat/이름-이슈번호-작업
```

PR이 병합됐는지 확인하지 않은 채로 `-D`를 쓰지 않습니다. 이유는 아래 「병렬 작업에서 충돌 줄이기」의 squash 절에 있습니다.

## 작업 중 `main`이 바뀐 경우

다른 팀원의 Pull Request가 먼저 병합됐다면 자신의 브랜치에 최신 `main`을 합칩니다.

```bash
git status
git fetch origin
git merge origin/main
```

작업 트리가 깨끗한 상태에서 실행합니다. 충돌이 없으면 검사 후 푸시합니다.

```bash
flutter analyze
flutter test
git push
```

**push 직전에도 한 번 더 실행합니다.** 작업이 하루 걸렸다면 시작할 때 받은 `main`은 이미 낡았습니다. 이 단계를 건너뛰면 낡은 `main` 위에서 Pull Request가 올라갑니다.

Git 협업에 익숙하지 않다면 이미 공유한 커밋의 이력을 다시 쓰는 `rebase`보다 `merge`를 우선합니다. 팀원이 함께 사용하는 브랜치에서 임의로 rebase하거나 force push하지 않습니다.

## 병렬 작업에서 충돌 줄이기

### 어디서 충돌이 나는가

담당별로 디렉터리가 갈려 있어 대부분의 작업은 물리적으로 겹치지 않습니다. 실제로 부딪히는 곳은 좁습니다.

| 경로 | 충돌 위험 | 이유 |
|---|---|---|
| `docs/2026-10-19-s3-s10-design-v0.1.md` | **높음** | 단일 기준 설계서. 여러 트랙이 같은 절을 함께 고칩니다 |
| `docs/ai-integration-decisions.md` | **높음** | `P-00x` 미결 표를 여러 담당이 함께 고칩니다 |
| `contracts/*.schema.json` | 중간 | 계약을 바꾸면 예제와 송신·수신 파서가 함께 움직입니다. 동결(10/20) 후에는 `contract-change` + PM 승인 |
| `README.md`, `AGENTS.md`, `CLAUDE.md` | 중간 | 구성이 바뀔 때 여러 PR이 동시에 건드립니다 |
| `flutter_app/`, `safety_core/`, `vision_node/`, `voice_node/`, `firmware/`, `inbox/<본인 이름>/` | 낮음 | 담당자 단독 영역입니다 |

**높음** 경로는 먼저 팀에 알리고 한 사람이 한 PR로 짧게 끝냅니다. 이 파일들을 건드리는 PR을 이틀 이상 열어두지 않습니다.

### git이 잡아주지 못하는 충돌

같은 줄을 고쳐야 git이 충돌로 알려줍니다. **다른 파일의 상태를 서술한 문서**는 그 파일이 바뀌어도 충돌이 나지 않고, 조용히 틀린 내용이 됩니다.

실제 사례가 있습니다. PR #3은 19시간 열려 있는 동안 `main`에 세 개의 PR이 병합됐지만 git 충돌은 한 번도 나지 않았습니다. `mergeable_state`가 끝까지 `clean`이었습니다. 대신 그 문서가 주장한 「README에 ATLAS가 등장하지 않는다」와 「README가 Pi Zero로 적고 있다」가 그 사이 사실이 아니게 됐습니다.

→ PR이 하루 넘게 열려 있었다면 병합 전에 **내가 쓴 내용이 지금도 사실인지** 사람이 다시 확인합니다. 특히 다른 문서나 코드의 현재 상태를 인용한 부분을 봅니다.

### squash 병합에서 주의할 점

이 저장소는 `main`에 **squash merge**로 합칩니다. 브랜치의 여러 커밋이 하나로 합쳐진 새 커밋이 만들어지므로, 원래 브랜치의 끝은 `main`의 조상이 되지 않습니다. 두 가지 귀결이 있습니다.

1. `git branch -d`가 항상 실패합니다. 병합이 안 된 것이 아닙니다. 위 「10. 병합 후 정리」를 따릅니다.
2. **병합이 끝난 브랜치에 이어서 커밋하지 않습니다.** 같은 변경이 두 번 올라가 진짜 충돌이 납니다. 후속 작업은 최신 `main`에서 새 브랜치를 땁니다.

## 충돌이 발생했을 때

충돌은 Git이 두 변경 중 무엇을 선택할지 자동으로 판단하지 못했다는 뜻이지, 저장소가 망가졌다는 뜻이 아닙니다.

### 1. 충돌 파일 확인

```bash
git status
```

파일 안에는 다음 표시가 생길 수 있습니다.

```text
<<<<<<< HEAD
내 브랜치의 내용
=======
가져오려는 브랜치의 내용
>>>>>>> origin/main
```

### 2. 올바른 최종 내용으로 직접 편집

위 표시를 포함해 필요 없는 내용을 제거합니다. 한쪽을 무조건 고르지 말고 두 작업의 의도를 확인합니다. 다른 팀원의 담당 코드라면 당사자와 함께 결정합니다.

### 3. 해결한 파일 추가와 병합 완료

```bash
git add <해결한 파일>
git status
git commit -m "merge main into feature branch"
```

아직 해결 방법을 모르겠다면 병합 시작 전 상태로 돌아갈 수 있습니다.

```bash
git merge --abort
```

`git merge --abort` 후 `git status`를 확인하고 도움을 요청합니다.

## 자주 생기는 상황

### 내가 어느 브랜치인지 모르겠습니다

```bash
git branch --show-current
git status
```

### 수정한 파일이 커밋에 포함되지 않았습니다

`git add`하지 않은 파일은 커밋되지 않습니다.

```bash
git status
git add <파일>
git commit -m "종류(영역): 작업 결과"
```

### 커밋에 잘못된 파일을 넣기 직전입니다

아직 커밋하지 않았다면 스테이징에서만 뺍니다.

```bash
git restore --staged <파일>
```

### 다른 브랜치로 이동할 수 없습니다

현재 수정사항과 이동할 브랜치의 파일이 충돌할 수 있어 Git이 보호하는 상황입니다.

가장 안전한 방법은 현재 작업을 완성해 커밋하는 것입니다. 정말 잠시 보관해야 한다면 다음을 사용할 수 있습니다.

```bash
git stash push -m "작업 내용 설명"
git switch <다른 브랜치>
```

돌아온 뒤 복원합니다.

```bash
git switch <원래 브랜치>
git stash pop
```

`stash`는 잊기 쉬우므로 `git stash list`로 남은 작업을 확인합니다.

### 커밋 메시지를 잘못 적었습니다

아직 푸시하지 않은 가장 최근 커밋만 다음으로 수정할 수 있습니다.

```bash
git commit --amend -m "올바른 메시지"
```

이미 푸시했다면 임의로 이력을 바꾸지 말고 그대로 두거나 팀원에게 먼저 문의합니다.

### Pull Request에 다른 사람의 변경이 섞였습니다

Base가 `main`, Compare가 자신의 작업 브랜치인지 먼저 확인합니다. 브랜치를 잘못 만들었거나 예상하지 못한 커밋이 있으면 force push로 숨기지 말고 커밋 목록과 `git log --oneline --graph --decorate -10` 결과를 공유해 도움을 요청합니다.

## 필수 용어

| 용어 | 뜻 |
|---|---|
| Repository(저장소) | 파일과 Git 변경 이력을 보관하는 프로젝트 공간 |
| Clone | 원격 저장소를 내 컴퓨터에 처음 복제하는 것 |
| Working tree | 현재 컴퓨터에서 직접 수정 중인 파일 상태 |
| Staging area | 다음 커밋에 포함하도록 선택한 변경 목록 |
| Commit | 변경 내용과 작성자를 하나의 이력 단위로 저장한 것 |
| Commit hash | `30f8950` 같은 커밋 고유 식별자 |
| Branch | 다른 작업과 분리된 커밋 작업선 |
| `main` | 팀이 검토를 마친 기준 코드가 모이는 기본 브랜치 |
| HEAD | 현재 체크아웃하여 보고 있는 커밋 또는 브랜치 위치 |
| Remote | GitHub처럼 로컬 밖에 있는 연결된 저장소 |
| `origin` | clone할 때 기본으로 붙는 원격 저장소 별칭 |
| Fetch | 원격 변경을 내려받되 현재 파일에는 합치지 않는 것 |
| Pull | 원격 변경을 내려받고 현재 브랜치에 합치는 것 |
| Push | 로컬 커밋을 원격 저장소로 올리는 것 |
| Merge | 두 브랜치의 변경 이력을 하나로 합치는 것 |
| Merge conflict | 같은 부분의 변경을 Git이 자동으로 합치지 못한 상태 |
| Pull Request(PR) | 브랜치 변경을 검토하고 `main`에 합쳐 달라는 GitHub 요청 |
| Review | PR의 코드·문서·검사 결과를 다른 팀원이 확인하는 과정 |
| Issue | 할 일, 버그, 요구사항과 논의를 추적하는 GitHub 항목 |
| Tag | 특정 커밋에 버전처럼 고정된 이름을 붙인 것 |
| Stash | 아직 커밋하지 않은 변경을 임시로 치워 두는 기능 |
| Revert | 기존 커밋을 삭제하지 않고 반대 변경을 새 커밋으로 만드는 것 |
| Rebase | 커밋의 기반을 옮겨 이력을 다시 작성하는 기능; 공유 브랜치에서는 충분한 이해와 팀 합의 없이 사용하지 않음 |

## 명령어 빠른 참고표

| 목적 | 명령 |
|---|---|
| 현재 상태 | `git status` |
| 현재 브랜치 | `git branch --show-current` |
| 변경 내용 | `git diff` |
| 커밋 예정 변경 | `git diff --staged` |
| 최근 이력 | `git log --oneline --graph --decorate -10` |
| 브랜치 목록 | `git branch` |
| 브랜치 이동 | `git switch <브랜치>` |
| 새 브랜치 생성·이동 | `git switch -c <브랜치>` |
| 파일 스테이징 | `git add <파일>` |
| 스테이징 취소 | `git restore --staged <파일>` |
| 커밋 | `git commit -m "메시지"` |
| 원격 정보 받기 | `git fetch origin` |
| 최신 main 받기 | `git pull --ff-only origin main` |
| 첫 브랜치 푸시 | `git push -u origin <브랜치>` |
| 이후 푸시 | `git push` |
| 원격 목록 확인 | `git remote -v` |

## 도움을 요청할 때 함께 보낼 정보

문제가 발생하면 오류 문구를 생략하지 말고 다음 결과와 함께 공유합니다.

```bash
git status
git branch --show-current
git log --oneline --graph --decorate -10
git remote -v
```

비밀번호, 토큰과 개인정보는 가린 뒤 공유합니다. 오류가 발생한 직후에는 강제 명령을 실행하지 않아야 다른 팀원이 상태를 확인하고 안전하게 복구할 수 있습니다.

## 작업 완료 체크리스트

- [ ] `main`이 아닌 내 작업 브랜치인가
- [ ] `git status`에서 의도한 파일만 변경됐는가
- [ ] `git diff`와 `git diff --staged`를 확인했는가
- [ ] 비밀정보와 생성 파일이 포함되지 않았는가
- [ ] 관련 포맷·분석·테스트를 통과했는가
- [ ] 커밋 메시지가 작업 결과를 설명하는가
- [ ] 내 GitHub 계정으로 브랜치를 푸시했는가
- [ ] Base가 `main`인 Pull Request를 만들었는가
- [ ] 검사 결과와 미완료 사항을 PR에 기록했는가
- [ ] 다른 팀원의 리뷰 후 병합했는가
