"""Held-out bright-floor check for v3c (frames never used in training), judged at P_person_score.

  ./venv-conv/bin/python -I eval_light.py model1.tflite [model2.tflite ...]
Empty scenes (must stay < P_person_score): raw-1025-light/L1b, L1e (fixed camera, lights all on).
People on the white floor (should be >= P_person_score): ~/hoodseek-measure/v3c-test-2026-10-24/frames/{p09,p12,p11}.
The doll near the floor band (person must stay < P_person_score, cat >= 0.35): Pi 5 measure dumps d02/d06/d07/d08.
These folders are this PC's held-out sets of 10/24~25; another run would point at its own held-out captures.
"""
import json
import os
import pathlib
import sys

import cv2

here = pathlib.Path(__file__).parent
sys.path.insert(0, str(here))
from compare import Detector  # noqa: E402

repo = pathlib.Path(os.environ.get("HOODSEEK_REPO", pathlib.Path.home() / "HOOD-SEEK"))
PS = json.load(open(repo / "profiles/default.json"))["P_person_score"]
sets = [("빈 L1b", here / "raw-1025-light/L1b", False), ("빈 L1e", here / "raw-1025-light/L1e", False)]
sets += [(f"사람 {i}", pathlib.Path.home() / "hoodseek-measure/v3c-test-2026-10-24/frames" / i, True) for i in ("p09", "p12", "p11")]
M = pathlib.Path.home() / "hoodseek-measure"
sets += [(f"인형 {r[:8]} {i}", M / r / "frames" / i, False) for r, i in (("v3d-moved-2026-10-25", "d07"), ("v3d-moved-2026-10-25", "d02"),
         ("v3d-moved-2026-10-25", "d06"), ("v3d-moved-2026-10-25", "d08"), ("v3d-2026-10-24", "d06"), ("v3d-2026-10-24", "d07"))]
sets += [("빈 옮긴뒤", here / "raw-1025-light/chk_moved", False)]
imgs = {name: [cv2.imread(str(f)) for f in sorted(d.iterdir()) if f.suffix in (".ppm", ".png")] if d.is_dir() else [] for name, d, _ in sets}
print(f"P_person_score {PS} | 빈 장면은 0이어야, 사람은 많을수록 좋음")
for m in sys.argv[1:]:
    det = Detector(m)
    cells = []
    for name, _, person in sets:
        if not imgs[name]:
            cells.append(f"{name} 없음")  # held-out folder missing (e.g. people deleted for #72)
            continue
        s = [(det(i).get("person") or (0,))[0] for i in imgs[name]]
        c = ""
        if name.startswith("인형"):
            cs = [max(((det(i).get(k) or (0,))[0] for k in ("cat", "dog")), default=0) for i in imgs[name]]
            c = f", cat≥0.35 {sum(x >= 0.35 for x in cs)}"
        cells.append(f"{name} 사람 {sum(x >= PS for x in s)}/{len(s)} (최고 {max(s):.2f}){c}")
    print(f"{pathlib.Path(m).name}: " + " | ".join(cells))
