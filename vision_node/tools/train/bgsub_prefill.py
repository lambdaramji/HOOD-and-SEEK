"""Person boxes by background subtraction (fixed camera): frame vs median of empty scenes with the same lighting.

  python3 -I bgsub_prefill.py <person folder> <empty folder> [<empty folder> ...]
People stand beyond the box, so they appear in the floor band above the box's top edge (found per run from the
empty median). Foreground = |frame - background| in that band (plus blobs touching it from below, e.g. a hand reaching
over the cooktop); glass reflections lower down are ignored. Writes <person folder>/prefill_bg.json and compares with
the teacher prefill (prefill.json) where the teacher was confident.
"""
import json
import pathlib
import sys

import cv2
import numpy as np

root = pathlib.Path(sys.argv[1])
empties = [cv2.imread(str(f)) for d in sys.argv[2:] for f in sorted(pathlib.Path(d).glob("*.png"))]
bg = np.median(np.stack(empties), axis=0).astype(np.uint8)
g = cv2.GaussianBlur(cv2.cvtColor(bg, cv2.COLOR_BGR2GRAY).astype(np.float32), (5, 5), 0)
edge = int(np.median([np.argmax(-cv2.Sobel(g, cv2.CV_32F, 0, 1)[:220, x - 20:x + 20].mean(1)) for x in (60, 320, 580)]))
print(f"빈 장면 {len(empties)}장 중앙값을 배경으로 씀 | 박스 윗변 y {edge}")


def box_of(img):
    d = cv2.absdiff(cv2.GaussianBlur(img, (5, 5), 0), cv2.GaussianBlur(bg, (5, 5), 0)).max(axis=2)
    m = (d > 40).astype(np.uint8)
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))
    m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, np.ones((15, 15), np.uint8))
    n, lab, st, _ = cv2.connectedComponentsWithStats(m)
    keep = [i for i in range(1, n) if st[i, cv2.CC_STAT_TOP] < edge - 5 and st[i, cv2.CC_STAT_AREA] >= 400]
    if not keep:
        return None
    x1 = min(st[i, 0] for i in keep); y1 = min(st[i, 1] for i in keep)
    x2 = max(st[i, 0] + st[i, 2] for i in keep); y2 = max(st[i, 1] + st[i, 3] for i in keep)
    return [int(x1), int(y1), int(min(639, x2)), int(min(479, y2))]


def iou(a, b):
    ix = max(0, min(a[2], b[2]) - max(a[0], b[0])); iy = max(0, min(a[3], b[3]) - max(a[1], b[1]))
    inter = ix * iy
    return inter / ((a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - inter + 1e-9)


teacher = json.loads((root / "prefill.json").read_text()) if (root / "prefill.json").exists() else {}
out, ious, only_bg, only_t = {}, [], 0, 0
for f in sorted(root.glob("*.png")):
    b = box_of(cv2.imread(str(f)))
    out[f.name] = [{"cls": "person", "box": b}] if b else []
    t = teacher.get(f.name) or []
    if b and t:
        ious.append(iou(b, t[0]["box"]))
    only_bg += bool(b) and not t
    only_t += bool(t) and not b
(root / "prefill_bg.json").write_text(json.dumps(out, indent=1))
n_box = sum(1 for v in out.values() if v)
print(f"{len(out)}장 | 배경 빼기 상자 {n_box}장 | 선생님(YOLOX-X ≥0.25)과 둘 다 있음 {len(ious)}장, IoU 중앙 {np.median(ious) if ious else 0:.2f} "
      f"(0.5 이상 {sum(i >= 0.5 for i in ious)}장) | 배경 빼기만 {only_bg}장 | 선생님만 {only_t}장")
