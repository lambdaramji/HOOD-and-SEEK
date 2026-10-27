#!/usr/bin/env bash
# ONNX -> uint8-I/O int8 TFLite with the deployed model's tensor names.
#   ~/hoodseek-train/convert.sh <in.onnx> <out.tflite>
set -euo pipefail
cd "$(dirname "$0")"
in=$1 out=$2 work=models/tf_$(basename "$in" .onnx)
./venv-conv/bin/onnxslim "$in" "${in%.onnx}_slim.onnx" >/dev/null
./venv-conv/bin/onnx2tf -i "${in%.onnx}_slim.onnx" -o "$work" -oiqt -iqd uint8 -oqd uint8 \
  -cind image models/calib64.npy "[[[[0.0,0.0,0.0]]]]" "[[[[1.0,1.0,1.0]]]]" > "$work.log" 2>&1
./venv-conv/bin/python rename_io.py "$work/$(basename "${in%.onnx}")_slim_full_integer_quant.tflite" "$out"
