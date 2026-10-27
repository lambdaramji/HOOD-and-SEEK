#!/bin/sh
# mlx_stream이 쓰는 CSV를 따라가며 경과 시간(sec)과 안정 여부를 한 줄씩 보여준다.
# 기록 창과 별개의 SSH 창에서 실행한다. 파일을 읽기만 하므로 mlx_stream과 충돌하지 않는다.
#   /data/mlx/mlx_watch.sh /data/mlx/box_hotpack.csv
# 안정 = 최근 30초 동안 max 변동폭 <= 0.6°C (±0.3). 표시되는 초는 CSV의 sec 값 그대로다.
# 열 위치는 헤더에서 찾는다. 배포된 mlx_stream(9열)과 저장소 v3 소스(11열) 모두 동작한다.
f="${1:-/data/mlx/box_hotpack.csv}"
[ -f "$f" ] || { echo "파일 없음: $f  (mlx_stream을 먼저 실행하세요)"; exit 1; }
tail -n +1 -f "$f" | awk -F, '
NR==1 { for (k=1; k<=NF; k++) c[$k]=k; next }
{
  t=$(c["sec"]); m=$(c["max"]); T[NR]=t; M[NR]=m; lo=m; hi=m
  for (i=NR; i>1 && t-T[i]<=30; i--) { if (M[i]<lo) lo=M[i]; if (M[i]>hi) hi=M[i] }
  if (t-T[i]<=30) i--
  span=t-T[i+1]
  st=(span>=29 && hi-lo<=0.6) ? "안정 ✓" : "변동중"
  printf "%5.0f초  max %5.1f  배경 %5.1f  delta %5.1f  %3d px  30초변동 %4.1f  %s\n", t, m, $(c["median"]), $(c["delta"]), $(c["hot_px"]), hi-lo, st
  fflush()
}'
