#!/bin/sh
# corewatch.sh [core.log] — Core state 로그(journalctl -o short-unix)를 따라가며 경과 초·hazard·팬·PM2.5를 1초에 한 줄 (#65 10/24 D1core)
#   경과 초 = 이 스크립트를 켠 때부터. Ctrl-C로 닫아도 기록(core.log)은 계속된다.
f=${1:-/data/mlx/runs/D1core_1024/core.log}
t0=$(date +%s)
tail -n 0 -f "$f" | awk -v t0="$t0" '
/\[state\]/ {
  split($1, a, "."); t = a[1] - t0
  if (t == last) next
  last = t
  h = ""; if (match($0, /"hazard":"[a-z_]*"/)) h = substr($0, RSTART + 10, RLENGTH - 11)
  fl = ""; if (match($0, /"fan":[{]"level":[0-9]/)) fl = substr($0, RSTART + RLENGTH - 1, 1)
  pm = ""; if (match($0, /"pm25":[0-9.]*/)) pm = substr($0, RSTART + 7, RLENGTH - 7)
  ts = ""; if (match($0, /"tsize":[0-9.]*/)) ts = substr($0, RSTART + 8, RLENGTH - 8)
  b = ""; if (match($0, /"baseline":[0-9.nul]*/)) b = substr($0, RSTART + 11, RLENGTH - 11)
  lk = ""; if (match($0, /"lock":"[a-z_]*"/)) lk = substr($0, RSTART + 8, RLENGTH - 9)
  r = ""; if (match($0, /"reasons":[[][^]]*[]]/)) r = substr($0, RSTART + 10, RLENGTH - 10)
  printf "%4d s  %-8s 팬 %s  PM2.5 %8s  tsize %-5s 기준 %-6s %-14s %s\n", t, h, fl, pm, ts, b, lk, r
  fflush()
}'
