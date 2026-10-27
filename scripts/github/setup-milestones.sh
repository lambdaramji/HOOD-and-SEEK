#!/usr/bin/env bash
# HOOD&SEEK 마일스톤 생성 — D-29 일정의 게이트를 그대로 옮긴 것.
# 데모 시나리오 M1~M11과 헷갈리지 않도록 마일스톤은 G(게이트) 번호를 씁니다.
# 쓰기 호출마다 PACE_SECONDS(기본 60초) 쉽니다 → 8개면 약 8분.
# 사용: scripts/github/setup-milestones.sh [owner/repo]
# 마감(due_on)은 UTC. 18:00 KST = 09:00Z, 13:00 KST = 04:00Z, 자정 KST = 전날 15:00Z.
set -euo pipefail
source "$(dirname "$0")/_pace.sh"
REPO="${1:-lambdaramji/HOOD-SEEK}"

# 형식: 제목|마감(UTC ISO8601)|설명
MILESTONES=$(cat <<'EOF'
G1 환경·계약 동결|2026-10-20T09:00:00Z|10/20 18:00. atlas-dev 2대 구축, Pi4/Pi5 IP·SSH, uart3/uart5·I2C 실측, 계약 v1 JSON 동결, whisper.cpp 빌드 판정. 동결 전 PM 결정 Q-S4~Q-S10 마감.
G2 1차 통합|2026-10-22T15:00:00Z|10/22. Pi4↔Pi5↔ESP32×2 전 링크 연결. TCP state 수신, UART ACK, 양쪽 timeout fail-safe 실증.
G3 임계값 확정|2026-10-23T15:00:00Z|10/23. 손난로·인센스·가습기 재실측 → profiles/default.json 수치 확정 (S1-5).
G4 Must 시나리오 완료|2026-10-24T15:00:00Z|10/24. M1·M2·M3·M6·M7·M8·M10 리허설 통과. 미달 시 10/25 Should 전부 삭제.
G5 Should 통합|2026-10-25T15:00:00Z|10/25. M9 고양이 → M11 Pi4 전원 → 음성 순. flutter-atlas ipk 1차.
G6 기능 동결·E2E|2026-10-26T09:00:00Z|10/26 18:00. E2E 3회 연속 리허설, arc ipk 시도(실패 시 systemd 유지). 이후 버그 수정만.
G7 코드 프리즈·제출|2026-10-28T04:00:00Z|10/27 22:00 코드 프리즈·태그·README, 10/28 13:00 제출. public 전환 전 삭제 목록 처리.
G8 라이브 시연|2026-11-05T09:00:00Z|10/29~11/5. 시연 안정화, Could 보강, 발표 PPT·Q&A 리허설.
EOF
)

existing=$(gh api "repos/$REPO/milestones?state=all&per_page=100" --jq '.[].title')

while IFS='|' read -r title due desc; do
  [[ -z "$title" ]] && continue
  if grep -Fxq "$title" <<< "$existing"; then
    echo "skip $title (이미 있음)"
    continue
  fi
  gh_paced api -X POST "repos/$REPO/milestones" \
    -f title="$title" -f due_on="$due" -f description="$desc" -f state=open >/dev/null
  echo "ok  $title"
done <<< "$MILESTONES"

echo "완료: $REPO 마일스톤"
