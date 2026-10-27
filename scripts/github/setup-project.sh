#!/usr/bin/env bash
# HOOD&SEEK GitHub Projects(v2) 보드 생성 + 커스텀 필드 + 저장소 연결.
# 뷰와 내장 자동화는 CLI로 만들 수 없어 끝에 안내를 출력합니다 (docs/github-projects-guide.md 참고).
# 쓰기 호출마다 PACE_SECONDS(기본 60초) 쉽니다 → 약 8분.
#
# 선행: gh auth refresh -s project,read:project   (project 스코프 필요)
# 사용: scripts/github/setup-project.sh [owner] [owner/repo] [제목]
set -euo pipefail
source "$(dirname "$0")/_pace.sh"
OWNER="${1:-lambdaramji}"
REPO="${2:-lambdaramji/HOOD-SEEK}"
TITLE="${3:-HOOD&SEEK 10/19–10/28 개발}"

# 이미 같은 제목의 프로젝트가 있으면 재사용
NUM=$(gh project list --owner "$OWNER" --format json --limit 50 \
  | python3 -c 'import json,sys; t=sys.argv[1]; print(next((p["number"] for p in json.load(sys.stdin)["projects"] if p["title"]==t), ""))' "$TITLE")

if [[ -z "$NUM" ]]; then
  NUM=$(gh_paced project create --owner "$OWNER" --title "$TITLE" --format json | python3 -c 'import json,sys; print(json.load(sys.stdin)["number"])')
  echo "ok  프로젝트 #$NUM 생성"
else
  echo "skip 프로젝트 #$NUM 이미 있음"
fi

if gh_paced project link "$NUM" --owner "$OWNER" --repo "$REPO" >/dev/null 2>&1; then
  echo "ok  저장소 연결"
else
  echo "skip 저장소 연결(이미 연결됐거나 권한 없음)"
fi

field_exists() {
  gh project field-list "$NUM" --owner "$OWNER" --format json --limit 100 \
    | python3 -c 'import json,sys; n=sys.argv[1]; print("1" if any(f["name"]==n for f in json.load(sys.stdin)["fields"]) else "")' "$1"
}

create_select() { # name, options(comma)
  if [[ -n "$(field_exists "$1")" ]]; then echo "skip 필드 $1"; return; fi
  gh_paced project field-create "$NUM" --owner "$OWNER" --name "$1" --data-type SINGLE_SELECT --single-select-options "$2" >/dev/null
  echo "ok  필드 $1"
}
create_plain() { # name, type(TEXT|DATE|NUMBER)
  if [[ -n "$(field_exists "$1")" ]]; then echo "skip 필드 $1"; return; fi
  gh_paced project field-create "$NUM" --owner "$OWNER" --name "$1" --data-type "$2" >/dev/null
  echo "ok  필드 $1"
}

# Status는 기본 제공(Todo / In Progress / Done). 아래 옵션으로 바꾸려면 웹 UI에서 편집:
#   Backlog · Todo · In Progress · In Review · Blocked · Done
create_select "트랙"     "T1 safety-core,T2 policy,T3 esp32,T4 vision,T5 voice,T6 flutter,T7 integration"
create_select "우선순위" "Must,Should,Could"
create_select "시나리오" "M1,M2,M3,M4,M5,M6,M7,M8,M9,M10,M11,해당 없음"
create_select "실기기"   "필요,불필요"
create_plain  "목표일"   DATE
create_plain  "근거"     TEXT   # 설계서 절·D-xx·Q-번호

cat <<EOF

프로젝트: https://github.com/users/$OWNER/projects/$NUM

웹 UI에서 마저 할 것 (5분):
 1. Status 옵션을 Backlog·Todo·In Progress·In Review·Blocked·Done 으로 편집
 2. 뷰 4개 추가 — docs/github-projects-guide.md 의 표 그대로
 3. Workflows(⋯ 메뉴) 에서 켜기:
      Auto-add to project: is:issue,pr is:open   (쉼표는 값 사이에만. is:issue,is:pr 은 오류)
      Item added to project → Status: Todo
      Pull request merged  → Status: Done
      Item closed          → Status: Done
      Item reopened        → Status: Todo
      Auto-archive: is:closed updated:<@today-2w   (상대 날짜는 @today 기준)
 4. 기존 열린 이슈·PR을 보드에 추가: 보드 우측 상단 "+ Add item" → "Add items from repository"
EOF
