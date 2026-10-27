"""Per-condition Q8 metrics for several TFLite models on saved person-free frames, judged like SafetyCore would.

  venv-conv/bin/python q8_compare.py "<frames dir glob>" model1.tflite model2.tflite ...
pet slot = best of cat/dog (vision-node cat_classes 15,16); hit = score >= P_cat_score ∧ roi_overlap >= P_cat_overlap.
Also counts the label that won the slot (cat vs dog) and person >= P_person_score frames.
"""
import glob
import json
import os
import pathlib
import sys

import cv2

repo = pathlib.Path(os.environ.get("HOODSEEK_REPO", pathlib.Path.home() / "HOOD-SEEK"))  # checkout with vision_node/ and profiles/
sys.path.insert(0, str(pathlib.Path(__file__).parent))
sys.path.insert(0, str(repo / "vision_node/tools"))
from compare import Detector  # noqa: E402
from detection_table import longest_run, pet  # noqa: E402

prof = json.load(open(repo / "profiles/default.json"))
P = lambda k: prof[k] if not isinstance(prof[k], dict) else prof[k]["value"]  # noqa: E731
CS, CO, PS = P("P_cat_score"), P("P_cat_overlap"), P("P_person_score")
roi = json.load(open(repo / "vision_node/config/roi.json"))["points"]
models = sys.argv[2:]
dets = [Detector(m) for m in models]
print(f"판정값 P_cat_score {CS} · P_cat_overlap {CO} · P_person_score {PS}")
print("조건 | " + " | ".join(f"모델{i + 1} hit(최장) cat/dog person≥{PS}" for i in range(len(models))))
for d in sorted(glob.glob(sys.argv[1])):
    files = sorted(pathlib.Path(d).glob("*.p[pn][mg]"))
    cells = []
    for det in dets:
        flags, labels, persons = [], [], 0
        for f in files:
            best = det(cv2.imread(str(f)))
            rec = {"best": {k: ({"score": v[0], "bbox": v[1]} if (v := best.get(k)) else None) for k in ("cat", "dog", "person", "teddy bear")}}
            x = pet(rec, roi)
            flags.append(bool(x) and x[0] >= CS and x[1] >= CO)
            labels.append(x[2] if x and x[0] >= CS else "-")
            persons += bool(best.get("person")) and best["person"][0] >= PS
        cells.append(f"{sum(flags)}/{len(files)}({longest_run(flags)}) {labels.count('cat')}/{labels.count('dog')} p{persons}")
    print(f"{pathlib.Path(d).name} | " + " | ".join(cells))
