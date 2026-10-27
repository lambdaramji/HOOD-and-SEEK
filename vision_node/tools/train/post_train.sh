#!/usr/bin/env bash
# After training: COCO hold-out AP, ONNX export, int8 TFLite, Q8-style comparison. Output: runs/post_train-<tag>.txt
#   ./post_train.sh <run dir name> <model tag> <exp file>     e.g. ./post_train.sh hoodseek_s_v2a hoodseek_s_v2a hoodseek_yolox_s_v2a.py
set -uo pipefail
cd "$(dirname "$0")"
run=${1:-hoodseek_s} tag=${2:-hoodseek_s_v1} exp=${3:-hoodseek_yolox_s.py}
ck=runs/$run/last_epoch_ckpt.pth
out=models/${tag}_int8.tflite
export TORCH_FORCE_NO_WEIGHTS_ONLY_LOAD=1 PYTHONPATH=YOLOX
filter() { grep -v -E 'INFO|WARNING|TfLite|XNNPACK|^W0000|^I0000'; }
{
  echo "== $(date '+%H:%M') 체크포인트 $ck"
  ./venv-train/bin/python YOLOX/tools/eval.py -f "$exp" -c "$ck" -b 8 -d 1 --conf 0.001 --fp16 --fuse > "runs/eval-coco-$tag.log" 2>&1
  echo "-- COCO 떼어 둔 1000장"; grep -E 'IoU=0.50:0.95 \| area=   all \| maxDets=100' "runs/eval-coco-$tag.log" | head -1
  grep -E '\| *(person|cat|dog|teddy bear) *\|' "runs/eval-coco-$tag.log" | head -2
  ./venv-train/bin/python export_onnx.py "$ck" "models/$tag.onnx" 2>&1 | grep -E 'saved' || { echo "!! ONNX 내보내기 실패 — 여기서 멈춤"; exit 1; }
  ./convert.sh "models/$tag.onnx" "$out" 2>&1 | tail -n 1 || { echo "!! 변환 실패 — 여기서 멈춤 (models/tf_$tag.log)"; exit 1; }
  sha256sum "$out"
  echo "-- Q8 사진 (모델1 원래 COCO 모델 / 모델2 $tag)"
  ./venv-conv/bin/python q8_compare.py "$HOME/hoodseek-measure/q8-2026-10-23/frames/[nd]0*" ref/yolox.tflite "$out" 2>&1 | filter
  echo "-- cap.sh 묶음 (학습에 안 씀)"
  ./venv-conv/bin/python q8_compare.py "$HOME/hoodseek-train/raw-1023/frames/t*" ref/yolox.tflite "$out" 2>&1 | filter | grep -v -E '판정값|조건'
  echo "== $(date '+%H:%M') 끝"
} > "runs/post_train-$tag.txt" 2>&1
