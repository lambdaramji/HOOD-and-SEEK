"""Print a TFLite model's input/output tensors (name, shape, dtype, quantization) and operator counts."""
import collections
import sys

import tflite

TYPES = {v: k for k, v in vars(tflite.TensorType).items() if not k.startswith("_")}
OPS = {v: k for k, v in vars(tflite.BuiltinOperator).items() if not k.startswith("_")}
buf = open(sys.argv[1], "rb").read()
m = tflite.Model.GetRootAsModel(buf, 0)
g = m.Subgraphs(0)


def show(kind, idx):
    t = g.Tensors(idx)
    q = t.Quantization()
    qs = f"scale {q.ScaleAsNumpy()[0]:.6g} zp {q.ZeroPointAsNumpy()[0]}" if q and q.ScaleLength() else "float/none"
    print(f"{kind} {t.Name().decode()} shape {list(t.ShapeAsNumpy())} {TYPES[t.Type()]} {qs}")


for i in range(g.InputsLength()):
    show("in ", g.Inputs(i))
for i in range(g.OutputsLength()):
    show("out", g.Outputs(i))
ops = collections.Counter(OPS.get(m.OperatorCodes(g.Operators(i).OpcodeIndex()).BuiltinCode(), "?") for i in range(g.OperatorsLength()))
print("ops", g.OperatorsLength(), dict(ops.most_common(12)))
print("signatures", m.SignatureDefsLength(), "description", m.Description())
