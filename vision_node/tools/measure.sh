#!/usr/bin/env bash
# #49 측정: 조건 하나를 Pi 5에서 N프레임 찍고 결과·원본 프레임을 PC로 가져와 manifest에 한 줄 추가한다.
#
#   vision_node/tools/measure.sh <id> <kind> <position> <angle> <in_roi> [note]
#     kind   = doll | negative | person
#     in_roi = true | false | none
#   예) vision_node/tools/measure.sh d01 doll in_center 90 true
#       vision_node/tools/measure.sh n01 negative empty_led_off - none
#
# 환경 변수: PI5(필수, 예: export PI5=root@<Pi5 IP>) OUT(기본 ~/hoodseek-measure/49-오늘날짜)
#            FRAMES(30) DUMP_EVERY(5) ROTATE(0) THREADS(2)
# 원본 프레임에는 사람이 찍힐 수 있다. 저장소에 커밋하지 않는다(#72).
set -euo pipefail
OUT=${OUT:-$HOME/hoodseek-measure/49-$(date +%F)}
FRAMES=${FRAMES:-30} DUMP_EVERY=${DUMP_EVERY:-5} ROTATE=${ROTATE:-0} THREADS=${THREADS:-2}
[[ $# -ge 5 ]] || { sed -n '2,12p' "$0"; exit 2; }
# 저장 프레임이 0장이면 아래 scp가 실패해 manifest 줄 없이 끝난다
(( FRAMES >= DUMP_EVERY )) || { echo "FRAMES($FRAMES)는 DUMP_EVERY($DUMP_EVERY) 이상이어야 함" >&2; exit 2; }
# ponytail: 실제 IP는 저장소에 두지 않는다 (설계서 §8.3)
PI5=${PI5:?"PI5를 지정하세요. 예: export PI5=root@<Pi5 IP>"}
id=$1 kind=$2 position=$3 angle=$4 in_roi=$5 note=${6:-}
[[ $id =~ ^[A-Za-z0-9_-]+$ ]] || { echo "id는 영문·숫자·_·-만" >&2; exit 2; }
case $kind in doll|negative|person) ;; *) echo "kind = doll|negative|person" >&2; exit 2 ;; esac
case $in_roi in true|false|none) ;; *) echo "in_roi = true|false|none" >&2; exit 2 ;; esac
[[ "$position$angle$note" != *,* ]] || { echo "쉼표는 쓸 수 없음 (CSV)" >&2; exit 2; }
mkdir -p "$OUT/frames/$id"
[[ -f $OUT/manifest.csv ]] || echo "id,kind,position,angle,in_roi,note" > "$OUT/manifest.csv"
! grep -q "^$id," "$OUT/manifest.csv" || { echo "이미 있는 id: $id" >&2; exit 2; }

ssh -o BatchMode=yes "$PI5" "cd /data/hoodseek && export LLVM_PROFILE_FILE=/dev/null && mkdir -p log/49/frames/$id && \
  ./bin/hoodseek_vision --camera /dev/video0 --model /data/ecoai/assets/models/object_detection/yolox.tflite \
  --threads $THREADS --rotate $ROTATE --max-frames $FRAMES --dump-dir log/49/frames/$id --dump-every $DUMP_EVERY \
  --output log/49/$id.jsonl 2>log/49/$id.stderr"
scp -q -o BatchMode=yes "$PI5:/data/hoodseek/log/49/$id.jsonl" "$OUT/"
scp -q -o BatchMode=yes "$PI5:/data/hoodseek/log/49/frames/$id/*" "$OUT/frames/$id/"
echo "$id,$kind,$position,$angle,$in_roi,$note" >> "$OUT/manifest.csv"
python3 -I - "$OUT/$id.jsonl" <<'EOF'
import json, sys
f = [r for r in map(json.loads, open(sys.argv[1])) if "seq" in r]
top = lambda c: max((r["best"][c]["score"] for r in f if r["best"][c]), default=None)
print(f"{len(f)} frames | " + " | ".join(f"{c} {top(c):.3f}" if top(c) is not None else f"{c} -" for c in ("cat", "dog", "teddy bear", "person")))
EOF
