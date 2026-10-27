#!/usr/bin/env bash
# seed-issues.tsv 의 행을 이슈로 만듭니다. 같은 제목이 이미 있으면 건너뜁니다.
# 한 번에 MAX_PER_RUN(기본 10)개까지만 만들고 멈춥니다. 다시 실행하면 이어서 만듭니다.
# 쓰기 호출마다 PACE_SECONDS(기본 60초) 쉽니다 → 10개면 약 10분.
# 먼저 TSV를 열어 필요 없는 행을 지웁니다. assignee 열은 D-29 트랙 담당 기준이며 바꿔도 됩니다.
# 선행: setup-labels.sh, setup-milestones.sh 를 먼저 실행 (없는 라벨·마일스톤은 gh가 거부)
# 사용: MAX_PER_RUN=10 scripts/github/seed-issues.sh [owner/repo] [tsv]
set -euo pipefail
source "$(dirname "$0")/_pace.sh"
REPO="${1:-lambdaramji/HOOD-SEEK}"
TSV="${2:-$(dirname "$0")/seed-issues.tsv}"
MAX_PER_RUN="${MAX_PER_RUN:-10}"

existing=$(gh issue list --repo "$REPO" --state all --limit 500 --json title --jq '.[].title')
made=0
remaining=0

while IFS=$'\t' read -r title labels milestone assignee body; do
  [[ -z "$title" ]] && continue
  if grep -Fxq "$title" <<< "$existing"; then
    echo "skip $title"
    continue
  fi
  if (( made >= MAX_PER_RUN )); then
    remaining=$((remaining + 1))
    continue
  fi
  if ! url=$(gh_paced issue create --repo "$REPO" --title "$title" --label "$labels" --milestone "$milestone" --assignee "$assignee" --body "$body"); then
    echo "!! 이슈 생성 실패, 여기서 멈춥니다: $title" >&2
    echo "   (응답 원문은 위 stderr. 이미 만든 $made 개는 그대로이며 재실행하면 건너뜁니다.)" >&2
    exit 1
  fi
  echo "ok  $url  $title"
  made=$((made + 1))
done < <(tail -n +2 "$TSV")

echo "이번 실행: $made 개 생성. 남은 행: $remaining 개."
if (( remaining > 0 )); then
  echo "→ 30분 이상 지난 뒤 같은 명령을 다시 실행하면 이어서 만듭니다."
fi
