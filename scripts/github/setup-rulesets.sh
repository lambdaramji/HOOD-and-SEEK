#!/usr/bin/env bash
# 브랜치·태그 Ruleset + 저장소 병합 설정. GitHub Pro(비공개 저장소에서 Ruleset 사용 가능) 전제.
# 쓰기 호출마다 PACE_SECONDS(기본 60초) 쉽니다 → 약 3분. 재실행하면 같은 이름의 Ruleset은 건너뜁니다.
# 사용: scripts/github/setup-rulesets.sh [owner/repo]
#
# 만드는 것
#  1. "main 보호" (active)      main 삭제·force-push 금지, PR 필수(승인 1, Code Owners, 오래된 승인 무효, 스레드 해결),
#                               필수 status check = system_ci_result + flutter_result, bypass 없음(PM 포함)
#  2. "제출 태그 보호" (active)  v* 태그 삭제·갱신 금지 (v1.0.0-submission 보존)
#  3. 브랜치 이름 패턴 규칙은 만들지 않습니다 — [문서] GitHub Docs "About rulesets": 커밋 메타데이터·브랜치/태그 이름 형식 제한은 Enterprise 플랜 조직용.
#     [실측 2026-10-19] POST /rulesets (branch_name_pattern) exit=1. 브랜치 형식은 CONTRIBUTING.md 약속으로만 지킵니다.
#  4. 저장소 설정: 병합 후 브랜치 자동 삭제, auto-merge 허용(CI 끝나면 자동 병합 버튼)
set -euo pipefail
source "$(dirname "$0")/_pace.sh"
REPO="${1:-lambdaramji/HOOD-SEEK}"

existing=$(gh api "repos/$REPO/rulesets" --jq '.[].name')
have() { grep -Fxq "$1" <<< "$existing"; }

if have "main 보호"; then echo "skip main 보호"; else
gh_paced api -X POST "repos/$REPO/rulesets" --input - >/dev/null <<'JSON'
{
  "name": "main 보호",
  "target": "branch",
  "enforcement": "active",
  "bypass_actors": [],
  "conditions": { "ref_name": { "include": ["~DEFAULT_BRANCH"], "exclude": [] } },
  "rules": [
    { "type": "deletion" },
    { "type": "non_fast_forward" },
    { "type": "pull_request",
      "parameters": {
        "required_approving_review_count": 1,
        "dismiss_stale_reviews_on_push": true,
        "require_code_owner_review": true,
        "require_last_push_approval": false,
        "required_review_thread_resolution": true
      } },
    { "type": "required_status_checks",
      "parameters": {
        "strict_required_status_checks_policy": false,
        "do_not_enforce_on_create": true,
        "required_status_checks": [
          { "context": "system_ci_result" },
          { "context": "flutter_result" }
        ]
      } }
  ]
}
JSON
echo "ok  main 보호"
fi

if have "제출 태그 보호"; then echo "skip 제출 태그 보호"; else
gh_paced api -X POST "repos/$REPO/rulesets" --input - >/dev/null <<'JSON'
{
  "name": "제출 태그 보호",
  "target": "tag",
  "enforcement": "active",
  "bypass_actors": [],
  "conditions": { "ref_name": { "include": ["refs/tags/v*"], "exclude": [] } },
  "rules": [
    { "type": "deletion" },
    { "type": "update" }
  ]
}
JSON
echo "ok  제출 태그 보호"
fi


# 저장소 병합 설정 (gh repo edit = PATCH /repos). 병합 방식 3종은 기존 contribution-guide 표대로 모두 허용.
gh_paced repo edit "$REPO" --delete-branch-on-merge --enable-auto-merge >/dev/null
echo "ok  병합 후 브랜치 자동 삭제, auto-merge 허용"

cat <<EOF

완료. 확인: https://github.com/$REPO/settings/rules
주의: 필수 status check(system_ci_result, flutter_result)는 새 워크플로가 main에 한 번 돌아야 목록에 보입니다.
      이 스크립트는 그 전에 실행해도 되지만(이름으로 등록됨), 템플릿 PR 자체는 PR CI가 그 이름으로 돌기 때문에 통과합니다.
EOF
