"""Run TFLite detectors on saved frames with vision-node's exact pre/post-processing and print the best box per class.

  venv-conv/bin/python compare.py <frames_dir_glob> <model.tflite> [<model2.tflite> ...]
Also prints what Pi 5 vision-node recorded for the same frame (frame_00000N.ppm <-> seq N in <id>.jsonl next to frames/).
"""
import glob
import json
import pathlib
import sys

import cv2
import numpy as np
from ai_edge_litert.interpreter import Interpreter

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from common import letterbox  # noqa: E402

WATCH = {0: "person", 15: "cat", 16: "dog", 77: "teddy bear"}


class Detector:
    def __init__(self, path):
        self.it = Interpreter(model_path=path, num_threads=4)
        self.it.allocate_tensors()
        self.inp = self.it.get_input_details()[0]
        self.out = {d["name"]: d for d in self.it.get_output_details()}

    def value(self, name):
        d = self.out[name]
        v = self.it.get_tensor(d["index"])
        scale, zp = d["quantization"]
        return v.astype(np.float32) if name == "class_idx" or scale == 0 else (v.astype(np.float32) - zp) * scale

    def __call__(self, bgr):
        x, (left, top, sx, sy) = letterbox(bgr)
        self.it.set_tensor(self.inp["index"], x[None])
        self.it.invoke()
        boxes, scores, classes = self.value("boxes")[0], self.value("scores")[0], self.value("class_idx")[0]
        h, w = bgr.shape[:2]
        keep = scores >= 0.10
        b = boxes[keep].copy()
        b[:, [0, 2]] = np.clip((b[:, [0, 2]] - left) / sx, 0, w)
        b[:, [1, 3]] = np.clip((b[:, [1, 3]] - top) / sy, 0, h)
        s, c = scores[keep], np.round(classes[keep]).astype(int)
        order = np.argsort(-s, kind="stable")
        kept = []
        for i in order:
            if b[i, 2] <= b[i, 0] or b[i, 3] <= b[i, 1]:
                continue
            if any(c[i] == c[j] and iou(b[i], b[j]) > 0.45 for j in kept):
                continue
            kept.append(i)
        best = {}
        for i in kept:
            if c[i] in WATCH and WATCH[c[i]] not in best:
                best[WATCH[c[i]]] = (float(s[i]), [int(round(v)) for v in b[i]])
        return best


def iou(a, b):
    ix = max(0.0, min(a[2], b[2]) - max(a[0], b[0]))
    iy = max(0.0, min(a[3], b[3]) - max(a[1], b[1]))
    inter = ix * iy
    return inter / ((a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - inter + 1e-9)


def fmt(best):
    return " ".join(f"{k} {v[0]:.2f}" for k, v in sorted(best.items())) or "-"


if __name__ == "__main__":
    dets = [Detector(p) for p in sys.argv[2:]]
    for d in sorted(glob.glob(sys.argv[1])):
        d = pathlib.Path(d)
        rec = {}
        jl = d.parent.parent / f"{d.name}.jsonl"
        if jl.exists():
            rec = {r["seq"]: r for r in map(json.loads, open(jl)) if "seq" in r}
        for f in sorted(d.glob("*.ppm")):
            seq = int(f.stem.split("_")[1])
            pi = rec.get(seq, {}).get("best", {})
            pi_s = " ".join(f"{k} {v['score']:.2f}" for k, v in sorted(pi.items()) if v) or "-"
            outs = [fmt(det(cv2.imread(str(f)))) for det in dets]
            print(f"{d.name}/{f.stem[-2:]} | Pi5 기록: {pi_s} | " + " | ".join(f"모델{i + 1}: {o}" for i, o in enumerate(outs)))
