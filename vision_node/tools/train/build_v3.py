"""v3 dataset: train.json's COCO part + our doll snaps x REPEAT + in-scene person frames x REPEAT (person + cat boxes).

  python3 -I build_v3.py [REPEAT]
Person frames come from raw-1024-person/labels.json (label_tool2.py), letterboxed like vision-node. They stay on this PC
only (#72): ds/train2017/person/ is deleted together with raw-1024-person after validation.
"""
import json
import pathlib
import sys

import cv2

here = pathlib.Path(__file__).parent
sys.path.insert(0, str(here))
from common import letterbox  # noqa: E402

REPEAT = int(sys.argv[1]) if len(sys.argv) > 1 else 3
CAT, PERSON = 17, 1
ds = here / "ds"
base = json.load(open(ds / "annotations/train.json"))
coco_ims = [im for im in base["images"] if im["file_name"].startswith("coco/")]
coco_ids = {im["id"] for im in coco_ims}
ims, anns = list(coco_ims), [a for a in base["annotations"] if a["image_id"] in coco_ids]
# our doll snaps: first copy of each from train.json, repeated
seen, doll = set(), []
for im in base["images"]:
    if im["file_name"].startswith("ours/") and im["file_name"] not in seen:
        seen.add(im["file_name"])
        doll.append(im)
doll_anns = {}
for a in base["annotations"]:
    doll_anns.setdefault(a["image_id"], []).append(a)
next_img, next_ann = 20_000_000, 20_000_000


def add(file_name, boxes):
    global next_img, next_ann
    for _ in range(REPEAT):
        next_img += 1
        ims.append({"id": next_img, "file_name": file_name, "width": 640, "height": 640})
        for cat, b in boxes:
            next_ann += 1
            anns.append({"id": next_ann, "image_id": next_img, "category_id": cat, "bbox": b, "area": b[2] * b[3], "iscrowd": 0})


for im in doll:
    add(im["file_name"], [(a["category_id"], a["bbox"]) for a in doll_anns.get(im["id"], [])])
labels = json.load(open(here / "raw-1024-person/labels.json"))
(ds / "train2017/person").mkdir(exist_ok=True)
n_person = n_cat = n_frames = 0
for name, boxes in sorted(labels.items()):
    if not isinstance(boxes, list):
        continue
    canvas, (left, top, sx, sy) = letterbox(cv2.imread(str(here / "raw-1024-person" / name)))
    cv2.imwrite(str(ds / "train2017/person" / name), cv2.cvtColor(canvas, cv2.COLOR_RGB2BGR))
    conv = []
    for b in boxes:
        x1, y1, x2, y2 = b["box"]
        conv.append((PERSON if b["cls"] == "person" else CAT, [x1 * sx + left, y1 * sy + top, (x2 - x1) * sx, (y2 - y1) * sy]))
        n_person += b["cls"] == "person"
        n_cat += b["cls"] == "cat"
    add("person/" + name, conv)
    n_frames += 1
json.dump({"images": ims, "annotations": anns, "categories": base["categories"]}, open(ds / "annotations/train_v3.json", "w"))
print(f"train_v3.json: 이미지 {len(ims)} (COCO {len(coco_ims)}, 인형 {len(doll)}x{REPEAT}, 사람 장면 {n_frames}x{REPEAT}) "
      f"| 사람 장면 상자: person {n_person}, cat {n_cat}")
