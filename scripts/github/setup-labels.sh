#!/usr/bin/env bash
# HOOD&SEEK 라벨 일괄 생성·갱신. 여러 번 실행해도 안전합니다(이미 있으면 건너뜀).
# 쓰기 호출마다 PACE_SECONDS(기본 60초) 쉽니다 → 30개면 약 30분.
# 사용: scripts/github/setup-labels.sh [owner/repo]
set -euo pipefail
source "$(dirname "$0")/_pace.sh"
REPO="${1:-lambdaramji/HOOD-SEEK}"

# 형식: 이름|색(hex, # 없이)|설명
LABELS=$(cat <<'EOF'
type: task|0E8A16|구현·수정·통합 작업
type: bug|D73A4A|기대와 다르게 동작
type: measurement|1D76DB|실기기 실측·시험 기록
type: decision|5319E7|팀 승인이 필요한 선택
type: docs|0075CA|문서만 바꿈 (CI 없음)
track: T1 safety-core|006B75|Pi 4 SafetyCore 골격 — 정정인
track: T2 policy|006B75|판정 상태기계·프로파일 — 남현지
track: T3 esp32|006B75|ESP32 2종·박스 — 정정인·김병직
track: T4 vision|006B75|비전 노드 — 김병직
track: T5 voice|006B75|음성 노드 — 이수현
track: T6 flutter|006B75|Flutter 대시보드 — 임재범
track: T7 integration|006B75|통합·영상·문서 — 전원
area: safety_core|C2E0C6|safety_core/ (전환 전: packages/, pi5_gateway/, pi4_sensor_node/)
area: vision_node|C2E0C6|vision_node/
area: voice_node|C2E0C6|voice_node/
area: flutter_app|C2E0C6|flutter_app/
area: firmware|C2E0C6|firmware/
area: contracts|C2E0C6|contracts/
area: profiles|C2E0C6|profiles/
area: deploy|C2E0C6|deploy/
area: docs|C2E0C6|docs/, inbox/
area: ci|C2E0C6|.github/
Must|B60205|데모 Must. 없으면 제출 불가
Should|FBCA04|데모 Should
Could|C5DEF5|여유 있을 때
safety-invariant|B60205|CLAUDE.md 안전 불변식 관련. PM 리뷰 필수
contract-change|E99695|contracts/ 변경. 양쪽 파서 동시 PR, v 증가
needs-decision|5319E7|결정권자 답변 대기
blocked|000000|선행 이슈·외부 회신 대기
needs-hardware|BFD4F2|실기기가 있어야 검증 가능 (사무실 시간에 배치)
good first issue|7057FF|처음 합류한 사람이 잡기 좋은 작업
EOF
)

existing=$(gh label list --repo "$REPO" --limit 200 --json name --jq '.[].name')

while IFS='|' read -r name color desc; do
  [[ -z "$name" ]] && continue
  if grep -Fxq "$name" <<< "$existing"; then
    echo "skip $name"
    continue
  fi
  gh_paced label create "$name" --repo "$REPO" --color "$color" --description "$desc" >/dev/null
  echo "ok  $name"
done <<< "$LABELS"

echo "완료: $REPO 라벨"
