"""Pre-fill person boxes with a big teacher model (YOLOX-X COCO, fp32 on this PC's GPU) for label_tool2.

  TORCH_FORCE_NO_WEIGHTS_ONLY_LOAD=1 PYTHONPATH=YOLOX ./venv-train/bin/python teacher_prefill.py [folder] [conf]
Frames are people (#72): they stay on this PC. Writes <folder>/prefill.json (largest-score person box per frame,
score >= conf, default 0.25) and prints a score histogram. Open the frames only in label_tool2.py.
"""
import json
import pathlib
import sys

import cv2
import numpy as np
import torch

here = pathlib.Path(__file__).parent
sys.path.insert(0, str(here / "YOLOX"))
from yolox.exp import get_exp  # noqa: E402
from yolox.utils import postprocess  # noqa: E402

root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else here / "raw-1025-person"
conf = float(sys.argv[2]) if len(sys.argv) > 2 else 0.25
exp = get_exp(None, "yolox-x")
model = exp.get_model().cuda().eval()
model.load_state_dict(torch.load(here / "weights/yolox_x.pth", map_location="cuda")["model"])
size = 640


def prep(img):
    # YOLOX ValTransform: top-left resize keeping aspect, pad 114, BGR 0~255 CHW
    r = min(size / img.shape[0], size / img.shape[1])
    pad = np.full((size, size, 3), 114, np.uint8)
    pad[: int(img.shape[0] * r), : int(img.shape[1] * r)] = cv2.resize(img, (int(img.shape[1] * r), int(img.shape[0] * r)))
    return torch.from_numpy(pad.transpose(2, 0, 1).astype(np.float32)).unsqueeze(0).cuda(), r


pre, best = {}, []
files = sorted(root.glob("*.png"))
with torch.no_grad():
    for f in files:
        img = cv2.imread(str(f))
        x, r = prep(img)
        out = postprocess(model(x), exp.num_classes, 0.05, 0.45, class_agnostic=False)[0]
        boxes = []
        if out is not None:
            out = out.cpu().numpy()
            p = out[out[:, 6] == 0]  # COCO class 0 = person
            if len(p):
                s = p[:, 4] * p[:, 5]
                i = int(np.argmax(s))
                best.append(float(s[i]))
                if s[i] >= conf:
                    x1, y1, x2, y2 = (p[i, :4] / r).tolist()
                    boxes.append({"cls": "person", "box": [max(0, round(x1)), max(0, round(y1)), min(639, round(x2)), min(479, round(y2))]})
            else:
                best.append(0.0)
        else:
            best.append(0.0)
        pre[f.name] = boxes
(root / "prefill.json").write_text(json.dumps(pre, indent=1))
b = np.array(best)
print(f"{len(files)}장 | 사람 상자 미리 채움(점수 ≥ {conf}) {sum(1 for v in pre.values() if v)}장 | "
      f"점수 분포 0.5 이상 {(b >= 0.5).sum()}, 0.25~0.5 {((b >= 0.25) & (b < 0.5)).sum()}, 0.05~0.25 {((b > 0) & (b < 0.25)).sum()}, 없음 {(b == 0).sum()}")
