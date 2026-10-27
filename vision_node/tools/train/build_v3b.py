"""v3b dataset = train_v3.json + hard negatives (no person): chairs/jacket/bag at the user spot, empty labels, x REPEAT.
Sources: raw-1023/frames/h0* (cap.sh, every frame) and raw-1024-neg (snap_view auto, last frame of each burst dropped
in case the person walking back shows). Motivation: v3 read a chair with the jacket worn during capture as a person
(Pi 5 n03 48/60 >= 0.35, 2026-10-24).
"""
import json
import os
import pathlib
import sys

import cv2

here = pathlib.Path(__file__).parent
sys.path.insert(0, str(here))
from common import letterbox  # noqa: E402

REPEAT = int(sys.argv[1]) if len(sys.argv) > 1 else 3
ds = here / "ds"
j = json.load(open(ds / "annotations/train_v3.json"))
srcs = sorted((here / "raw-1023/frames").glob("h0*/*.ppm"))
snaps = sorted((here / "raw-1024-neg").glob("neg_*.png"))
burst, last = [], None
for f in snaps:
    t = os.path.getmtime(f)
    if last is not None and t - last > 3.0 and burst:
        srcs += burst[:-1]
        burst = []
    burst.append(f)
    last = t
srcs += burst[:-1]
(ds / "train2017/neg").mkdir(exist_ok=True)
next_img = 30_000_000
for f in srcs:
    name = f"{f.parent.name}_{f.stem}.png"
    canvas, _ = letterbox(cv2.imread(str(f)))
    cv2.imwrite(str(ds / "train2017/neg" / name), cv2.cvtColor(canvas, cv2.COLOR_RGB2BGR))
    for _ in range(REPEAT):
        next_img += 1
        j["images"].append({"id": next_img, "file_name": "neg/" + name, "width": 640, "height": 640})
json.dump(j, open(ds / "annotations/train_v3b.json", "w"))
print(f"train_v3b.json: 이미지 {len(j['images'])} (사람 아님 {len(srcs)}장 x{REPEAT} 추가)")
