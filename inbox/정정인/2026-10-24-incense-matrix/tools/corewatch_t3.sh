#!/bin/sh
# corewatch_t3.sh <core.log> — T3(손난로)용: Core state 로그를 따라가며 경과 초·온도 metric·30 s 변동·hazard·팬을 1초에 한 줄 (#65 10/24)
#   안정 ✓ = 최근 30 s metric 변동폭 ≤ 0.6 °C (Q2·mlx_watch.sh와 같은 기준). 경과 초 = 이 창을 켠 때부터. Ctrl-C로 닫아도 기록은 계속.
f=${1:?사용: corewatch_t3.sh /data/mlx/runs/T3_1024/core.log}
t0=$(date +%s)
tail -n 0 -f "$f" | awk -v t0="$t0" '
/\[state\]/ {
  split($1, a, "."); t = a[1] - t0
  if (t == last) next
  last = t
  h = ""; if (match($0, /"hazard":"[a-z_]*"/)) h = substr($0, RSTART + 10, RLENGTH - 11)
  fl = ""; if (match($0, /"fan":[{]"level":[0-9]/)) fl = substr($0, RSTART + RLENGTH - 1, 1)
  mx = ""; if (match($0, /"max_c":[0-9.]*/)) mx = substr($0, RSTART + 8, RLENGTH - 8)
  md = ""; if (match($0, /"median_c":[0-9.]*/)) md = substr($0, RSTART + 11, RLENGTH - 11)
  m = ""; if (match($0, /"metric":[0-9.]*/)) m = substr($0, RSTART + 9, RLENGTH - 9)
  lk = ""; if (match($0, /"lock":"[a-z_]*"/)) lk = substr($0, RSTART + 8, RLENGTH - 9)
  n++; T[n] = t; M[n] = m + 0; lo = M[n]; hi = M[n]
  for (i = n; i > 0 && t - T[i] <= 30; i--) { if (M[i] < lo) lo = M[i]; if (M[i] > hi) hi = M[i] }
  span = (i > 0) ? 30 : t - T[1]
  st = (span >= 30 && hi - lo <= 0.6) ? "안정 ✓" : "변동중"
  printf "%4d s  metric %6s  (max %6s  median %5s)  30초변동 %4.1f %s  %-8s 팬 %s  %s\n", t, m, mx, md, hi - lo, st, h, fl, lk
  fflush()
}'
