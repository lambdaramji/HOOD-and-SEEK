# 공통: GitHub 쓰기 호출 사이에 텀을 둡니다. (한 번에 많이 올려 계정이 잠긴 적이 있음)
# 모든 setup-*.sh / seed-issues.sh 가 source 합니다. 직접 실행하는 파일이 아닙니다.
#
#   PACE_SECONDS   쓰기 호출 한 번마다 쉬는 초. 기본 60 (2026-10-19 실측: 20초 간격에서 1회 원인 미상 실패, 60초에서는 18건 연속 성공).
#   gh_paced ...   gh 를 실행하고 PACE_SECONDS 만큼 쉼.
#                  - 403/429(rate limit·abuse)면 15분 쉬고 딱 1회 재시도, 또 실패하면 스크립트 중단.
#                  - 422 "submitted too quickly"(콘텐츠 생성 남용 감지)면 재시도 없이 즉시 중단 (1시간 뒤 재실행).
#                  - 그 밖의 실패는 응답 원문을 stderr에 남기고 rc를 그대로 돌려줌 (호출 쪽 set -e 가 멈춤).
#
# 읽기(list, api GET)는 텀 없이 gh 를 직접 써도 됩니다. 쓰기(create, api -X POST, link, field-create)만 gh_paced 로.

PACE_SECONDS="${PACE_SECONDS:-60}"

gh_paced() {
  local out rc
  if out=$(gh "$@" 2>&1); then rc=0; else rc=$?; fi
  if (( rc != 0 )) && grep -qiE 'too quickly|HTTP 422' <<<"$out"; then
    echo "!! 콘텐츠 생성 남용 감지(422) 응답. 재시도하지 않고 여기서 중단합니다. 1시간 뒤 PACE_SECONDS=60 이상으로 다시 실행하면 이미 만든 항목은 건너뜁니다." >&2
    echo "$out" >&2
    exit 1
  fi
  if (( rc != 0 )) && grep -qiE 'HTTP 4(03|29)|rate limit|abuse|secondary limit|retry later' <<<"$out"; then
    echo "!! rate limit/abuse 응답. 15분 대기 후 1회만 재시도: gh $*" >&2
    sleep 900
    if out=$(gh "$@" 2>&1); then rc=0; else rc=$?; fi
    if (( rc != 0 )); then
      echo "!! 재시도도 실패. 여기서 중단합니다. 1시간 뒤 같은 스크립트를 다시 실행하면 이미 만든 항목은 건너뜁니다." >&2
      echo "$out" >&2
      exit 1
    fi
  fi
  if (( rc != 0 )); then echo "!! gh $* 실패 (rc=$rc):" >&2; echo "$out" >&2; fi
  printf '%s\n' "$out"
  sleep "$PACE_SECONDS"
  return $rc
}
