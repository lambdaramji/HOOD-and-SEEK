"""Rename onnx2tf TFLite I/O tensors to the deployed model's names (image, boxes, scores, class_idx), drop signatures.

  venv-conv/bin/python rename_io.py <in.tflite> <out.tflite>
Outputs are matched by shape/dtype: [1,N,4] -> boxes, int32 [1,N] -> class_idx, the other [1,N] -> scores.
"""
import sys

from tensorflow.lite.tools import flatbuffer_utils as fu

m = fu.read_model(sys.argv[1])
g = m.subgraphs[0]
g.tensors[g.inputs[0]].name = b"image"
for i in g.outputs:
    t = g.tensors[i]
    t.name = b"boxes" if len(t.shape) == 3 else (b"class_idx" if t.type == 2 else b"scores")  # TensorType INT32 = 2
m.signatureDefs = []
fu.write_model(m, sys.argv[2])
print("saved", sys.argv[2])
