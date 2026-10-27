"""Doll capture for v3e: raw YUY2 from Pi 5 (stdout only) -> PNG + auto cat label (v3d cat box, score >= 0.8).

  ./venv-conv/bin/python -I cap_doll.py <tag> <pi5 ssh target> [n=30]
Saves to raw-1025-doll/<tag>/ with labels.json (label_tool2 format, cls "cat", no person box) unless the original
COCO model sees a person >= 0.15 that is not inside the doll box (a real person or reflection, #72).
Frames whose v3d cat score is < 0.8 are left out of labels.json.
"""
import json
import pathlib
import subprocess
import sys

import cv2
import numpy as np

here = pathlib.Path(__file__).parent
sys.path.insert(0, str(here))
from compare import Detector  # noqa: E402

tag, target = sys.argv[1], sys.argv[2]
n = int(sys.argv[3]) if len(sys.argv) > 3 else 30
cmd = ("LLVM_PROFILE_FILE=/dev/null gst-launch-1.0 -q v4l2src device=/dev/video0 num-buffers=%d ! "
       "video/x-raw,format=YUY2,width=640,height=480 ! fdsink fd=1" % (n + 5))
raw = np.frombuffer(subprocess.run(["ssh", "-o", "BatchMode=yes", target, cmd], capture_output=True, timeout=120).stdout, np.uint8)
k = raw.size // 614400
frames = [cv2.cvtColor(f, cv2.COLOR_YUV2BGR_YUYV) for f in raw[: k * 614400].reshape(k, 480, 640, 2)[5:]]
if not frames:
    sys.exit("프레임 0장 — Pi 5 vision 서비스를 멈췄는지(카메라는 한 프로그램만 씀), ssh가 비밀번호 없이 되는지 확인")
orig = Detector(str(here / "ref/yolox.tflite"))
v3d = Detector(str(here / "models/hoodseek_s_v3d_int8.tflite"))


def inside(p, c):  # the person candidate is the doll itself when it mostly overlaps the doll box
    ix = max(0, min(p[2], c[2]) - max(p[0], c[0])); iy = max(0, min(p[3], c[3]) - max(p[1], c[1]))
    inter = ix * iy
    return inter / ((p[2] - p[0]) * (p[3] - p[1]) + (c[2] - c[0]) * (c[3] - c[1]) - inter + 1e-9) >= 0.4 or inter >= 0.8 * (p[2] - p[0]) * (p[3] - p[1])


labels, bad, vp = {}, [], []
for i, f in enumerate(frames):
    o, d = orig(f), v3d(f)
    cat = d.get("cat")
    po = o.get("person")
    if po and po[0] >= 0.15 and not (cat and inside(po[1], cat[1])):
        bad.append((i, round(po[0], 2), [round(v) for v in po[1]]))
    vp.append((d.get("person") or (0,))[0])
    if cat and cat[0] >= 0.8:
        labels[f"{tag}_{i:03d}.png"] = [{"cls": "cat", "box": [int(round(v)) for v in cat[1]]}]
print(f"{tag}: {len(frames)}장 | cat 라벨 {len(labels)}장 | v3d 가짜 사람 ≥0.35 {sum(p >= 0.35 for p in vp)}장(최고 {max(vp):.2f}) | 인형 밖 사람 후보 {len(bad)} {bad[:3]}")
if bad:
    print("  → 사람·반사 가능성이 있어 저장하지 않음")
else:
    out = here / "raw-1025-doll" / tag
    out.mkdir(parents=True, exist_ok=True)
    for i, f in enumerate(frames):
        cv2.imwrite(str(out / f"{tag}_{i:03d}.png"), f)
    (out / "labels.json").write_text(json.dumps(labels, indent=1))
    print(f"  → 저장 {out}")
