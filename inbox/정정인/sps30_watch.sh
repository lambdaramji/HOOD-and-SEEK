#!/bin/sh
# sps30_test가 쓰는 CSV를 따라가며 경과 시간과 연기 판정 3조건 충족 여부를 한 줄씩 보여준다.
# 기록 창과 별개의 SSH 창에서 실행한다. 파일을 읽기만 하므로 기록과 충돌하지 않는다.
#   /data/mlx/sps30_watch.sh /data/mlx/box_incense.csv
# 연기 조건(셋 모두): tsize < 0.60, coarse(PM10/PM2.5) < 1.20, fine(PC0.5/PC10) > 0.90
f="${1:-/data/mlx/box_incense.csv}"
[ -f "$f" ] || { echo "파일 없음: $f  (sps30_test를 먼저 실행하세요)"; exit 1; }
tail -n +1 -f "$f" | awk -F, '
NR==1 { for (k=1; k<=NF; k++) c[$k]=k; next }
{
  ts=$(c["tsize"]); co=$(c["coarse_pm10_pm25"]); fn=$(c["fine_pc05_pc10"])
  a=(ts<0.60); b=(co<1.20); d=(fn>0.90); n=a+b+d
  printf "%5.0f초  PM2.5 %6.1f  tsize %.3f%s  coarse %.3f%s  fine %.3f%s  → %d/3 %s\n", $(c["sec"]), $(c["pm25"]), ts, (a?"✓":" "), co, (b?"✓":" "), fn, (d?"✓":" "), n, (n==3?"★ 연기 판정":"")
  fflush()
}'
