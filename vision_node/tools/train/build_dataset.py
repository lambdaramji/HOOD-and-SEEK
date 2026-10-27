"""Build the YOLOX fine-tune dataset in ~/hoodseek-train/ds (COCO layout, all 80 COCO classes).

  python3 -I build_dataset.py
ds/train2017/ours/*.png   our doll/empty snaps, letterboxed 640x640 like vision-node (originals, not Roboflow's JPEGs)
ds/train2017/coco -> coco/val2017 (symlink)
annotations/train.json      ours(train) x REPEAT + COCO val2017 minus the held-out 1000
annotations/val_coco.json   held-out 1000 COCO images: person/cat/dog AP before vs after (regression check)
annotations/val_ours.json   ours(valid)
Doll boxes are COCO "cat" (category id 17 -> YOLOX class 15, what vision-node reports as cat).
"""
import json
import pathlib
import random
import sys

import cv2

here = pathlib.Path(__file__).parent
sys.path.insert(0, str(here))
from common import letterbox  # noqa: E402

REPEAT, HOLDOUT, CAT = 10, 1000, 17
ds = here / "ds"
(ds / "annotations").mkdir(parents=True, exist_ok=True)
(ds / "train2017/ours").mkdir(parents=True, exist_ok=True)
(ds / "val2017").mkdir(parents=True, exist_ok=True)
for split_dir in (ds / "train2017", ds / "val2017"):
    link = split_dir / "coco"
    if not link.exists():
        link.symlink_to(here / "coco/val2017")
if not (ds / "val2017/ours").exists():
    (ds / "val2017/ours").symlink_to(ds / "train2017/ours")

coco = json.load(open(here / "coco/annotations/instances_val2017.json"))
ids = sorted(im["id"] for im in coco["images"])
random.Random(0).shuffle(ids)
held = set(ids[:HOLDOUT])


def coco_part(keep):
    ims = [dict(im, file_name="coco/" + im["file_name"]) for im in coco["images"] if keep(im["id"])]
    keep_ids = {im["id"] for im in ims}
    return ims, [a for a in coco["annotations"] if a["image_id"] in keep_ids]


def ours(split, repeat, start_img, start_ann):
    rf = json.load(open(here / "rf-v1" / split / "_annotations.coco.json"))
    by_img = {}
    for a in rf["annotations"]:
        by_img.setdefault(a["image_id"], []).append(a["bbox"])
    ims, anns = [], []
    for im in rf["images"]:
        name = im["file_name"].split("_png")[0] + ".png"
        src = here / "raw-1023/snap" / name
        canvas, (left, top, sx, sy) = letterbox(cv2.imread(str(src)))
        cv2.imwrite(str(ds / "train2017/ours" / name), cv2.cvtColor(canvas, cv2.COLOR_RGB2BGR))
        boxes = [[x * sx + left, y * sy + top, w * sx, h * sy] for x, y, w, h in by_img.get(im["id"], [])]
        for r in range(repeat):
            start_img += 1
            ims.append({"id": start_img, "file_name": "ours/" + name, "width": 640, "height": 640})
            for b in boxes:
                start_ann += 1
                anns.append({"id": start_ann, "image_id": start_img, "category_id": CAT, "bbox": b,
                             "area": b[2] * b[3], "iscrowd": 0})
    return ims, anns


def save(name, ims, anns):
    json.dump({"images": ims, "annotations": anns, "categories": coco["categories"]}, open(ds / "annotations" / name, "w"))
    print(f"{name}: 이미지 {len(ims)} 상자 {len(anns)}")


base_img, base_ann = 10_000_000, 10_000_000
tr_ims, tr_anns = coco_part(lambda i: i not in held)
o_ims, o_anns = ours("train", REPEAT, base_img, base_ann)
save("train.json", tr_ims + o_ims, tr_anns + o_anns)
save("val_coco.json", *coco_part(lambda i: i in held))
save("val_ours.json", *ours("valid", 1, base_img + 900_000, base_ann + 900_000))
