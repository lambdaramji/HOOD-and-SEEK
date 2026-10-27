"""v3e dataset = v3d recipe + the doll where its top reaches the floor band (cat, no person) x3.

  python3 -I build_v3e.py             # v3e (needs models/hoodseek_s_v3d_int8.tflite labels in raw-1025-doll)
  python3 -I build_v3e.py --no-doll   # v3d = the same without section 4 -> train_v3d.json (model cap_doll.py reads)
v3d read a doll outside/at the far edge as a person when its upper part reached the white floor band (Pi 5 10/25:
d07 16/30, d02 4/30). v3e adds raw-1025-doll/B1..B6 (fixed camera, box moved back, labels = v3d cat box >= 0.8,
no person box) x REPEAT_DOLL2.

v3d notes:
v3c (empty scenes x3, synthetic exposure copies only) stopped the white-floor false person but lost real people on
the white floor (held-out p12 2/15). v3d adds raw-1025-person (144 frames, fixed camera, lights all on, labeled
locally with label_tool2.py: 107 person, 37 empty) x REPEAT_PERSON2.

v3c notes:
Why: on 10/24 the camera auto-exposure blew the floor at the top of the frame to white (mean 233~250); v3b then read
that band as a person (empty scene up to 0.39) and also lost people standing on it (p12 7/15). Training had no frame
like that. Added:
  - real empty scenes, no person (fixed camera): raw-1025-light/L1a,L1c,L1d (lights all on), L2a (half), L3a (off),
    O1 (chair at the user spot), O2 (bag/jacket on the floor there) x REPEAT_NEG
  - exposure copies (gain 1.5~2.6, clipped) of every person frame (labels kept) x REPEAT_PERSON, dolls and
    negatives x 1, so a white floor alone is not a person but people on it still are
Held out (never trained): raw-1025-light/L1b, L1e, ~/hoodseek-measure/v3c-test-2026-10-24.
Person copies under ds/train2017/aug are person photos too (#72): delete them with raw-1024-person.
"""
import json
import pathlib
import random
import sys

import cv2
import numpy as np

here = pathlib.Path(__file__).parent
sys.path.insert(0, str(here))
from common import letterbox  # noqa: E402

REPEAT_NEG, REPEAT_PERSON, REPEAT_PERSON2, REPEAT_DOLL2 = 2, 3, 3, 3
rng = random.Random(1025)
ds = here / "ds"
j = json.load(open(ds / "annotations/train_v3b.json"))
anns_by_img = {}
for a in j["annotations"]:
    anns_by_img.setdefault(a["image_id"], []).append(a)
first = {}
for im in j["images"]:
    first.setdefault(im["file_name"], im)
next_img, next_ann = 40_000_000, 40_000_000


def add(file_name, boxes, repeat):
    global next_img, next_ann
    for _ in range(repeat):
        next_img += 1
        j["images"].append({"id": next_img, "file_name": file_name, "width": 640, "height": 640})
        for cat, b in boxes:
            next_ann += 1
            j["annotations"].append({"id": next_ann, "image_id": next_img, "category_id": cat, "bbox": b,
                                     "area": b[2] * b[3], "iscrowd": 0})


# 1. real bright-floor empty scenes
(ds / "train2017/neg2").mkdir(exist_ok=True)
srcs = []
for tag in ("L1a", "L1c", "L1d", "L2a", "L3a", "O1", "O2"):
    srcs += sorted((here / "raw-1025-light" / tag).glob("*.png"))
for f in srcs:
    name = f"{f.parent.name}_{f.stem}.png"
    canvas, _ = letterbox(cv2.imread(str(f)))
    cv2.imwrite(str(ds / "train2017/neg2" / name), cv2.cvtColor(canvas, cv2.COLOR_RGB2BGR))
    add("neg2/" + name, [], REPEAT_NEG)
# 2. exposure copies of existing letterboxed frames (black letterbox bars stay black)
(ds / "train2017/aug").mkdir(exist_ok=True)
n_aug = {}
for prefix, repeat in (("person/", REPEAT_PERSON), ("ours/", 1), ("neg/", 1)):
    for fn, im in sorted(first.items()):
        if not fn.startswith(prefix):
            continue
        img = cv2.imread(str(ds / "train2017" / fn)).astype(np.float32)
        gain, gamma = rng.uniform(1.5, 2.6), rng.uniform(0.8, 1.0)
        out = np.clip(255.0 * (np.clip(img * gain, 0, 255) / 255.0) ** gamma, 0, 255).astype(np.uint8)
        name = prefix.rstrip("/") + "_" + pathlib.Path(fn).name
        cv2.imwrite(str(ds / "train2017/aug" / name), out)
        add("aug/" + name, [(a["category_id"], a["bbox"]) for a in anns_by_img.get(im["id"], [])], repeat)
        n_aug[prefix] = n_aug.get(prefix, 0) + 1
# 3. people on the white floor (raw-1025-person/labels.json, label_tool2 format, letterboxed like vision-node)
(ds / "train2017/person2").mkdir(exist_ok=True)
labels = json.load(open(here / "raw-1025-person/labels.json"))
n_p2 = n_p2_box = 0
for name, boxes in sorted(labels.items()):
    if not isinstance(boxes, list):
        continue
    canvas, (left, top, sx, sy) = letterbox(cv2.imread(str(here / "raw-1025-person" / name)))
    cv2.imwrite(str(ds / "train2017/person2" / name), cv2.cvtColor(canvas, cv2.COLOR_RGB2BGR))
    conv = [(1 if b["cls"] == "person" else 17, [b["box"][0] * sx + left, b["box"][1] * sy + top,
             (b["box"][2] - b["box"][0]) * sx, (b["box"][3] - b["box"][1]) * sy]) for b in boxes]
    add("person2/" + name, conv, REPEAT_PERSON2)
    n_p2 += 1
    n_p2_box += len(conv)
# 4. the doll near the floor band, labeled cat only (raw-1025-doll/<tag>/labels.json); skipped for v3d
NO_DOLL = "--no-doll" in sys.argv[1:]
out_name = "train_v3d.json" if NO_DOLL else "train_v3e.json"
(ds / "train2017/doll2").mkdir(exist_ok=True)
n_d2 = 0
for lab_file in [] if NO_DOLL else sorted((here / "raw-1025-doll").glob("*/labels.json")):
    for name, boxes in sorted(json.load(open(lab_file)).items()):
        canvas, (left, top, sx, sy) = letterbox(cv2.imread(str(lab_file.parent / name)))
        cv2.imwrite(str(ds / "train2017/doll2" / name), cv2.cvtColor(canvas, cv2.COLOR_RGB2BGR))
        add("doll2/" + name, [(17, [b["box"][0] * sx + left, b["box"][1] * sy + top, (b["box"][2] - b["box"][0]) * sx,
                                     (b["box"][3] - b["box"][1]) * sy]) for b in boxes], REPEAT_DOLL2)
        n_d2 += 1
json.dump(j, open(ds / "annotations" / out_name, "w"))
print(f"하얀 바닥 사람 {n_p2}장(상자 {n_p2_box}) x{REPEAT_PERSON2} | 띠 근처 인형 {n_d2}장 x{REPEAT_DOLL2}")
print(f"{out_name}: 이미지 {len(j['images'])} | 하얀 바닥 빈 장면 {len(srcs)}장 x{REPEAT_NEG} | 노출 사본 {n_aug} "
      f"(사람 x{REPEAT_PERSON}, 나머지 x1)")
