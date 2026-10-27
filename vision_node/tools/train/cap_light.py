"""Empty-scene capture for v3c: raw YUY2 from Pi 5 (stdout only) -> PNG frames, after a person check.

  ./venv-conv/bin/python -I cap_light.py <tag> <pi5 ssh target> [n=30]
Frames go to raw-1025-light/<tag>/ only if the original COCO model sees no person >= 0.15 in any frame
(people and their reflections must not be kept, #72). Prints v3b person max and floor brightness.
"""
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
v3b = Detector(str(here / "models/hoodseek_s_v3b_int8.tflite"))
po = [(orig(f).get("person") or (0, None)) for f in frames]
pv = [(v3b(f).get("person") or (0,))[0] for f in frames]
bad = [(i, round(s, 2), [round(v) for v in b]) for i, (s, b) in enumerate(po) if s >= 0.15]
floor = np.mean([f[27:130, 317:635].mean() for f in frames])
print(f"{tag}: {len(frames)}장 | 원래 모델 person ≥ 0.15 {len(bad)}장 {bad[:4]} | v3b person 최고 {max(pv):.2f}, ≥0.35 {sum(p >= 0.35 for p in pv)}장 | 바닥 밝기 {floor:.0f} | 전체 {np.mean([f.mean() for f in frames]):.0f}")
if bad:
    print("  → 사람·반사 가능성이 있어 저장하지 않음")
else:
    out = here / "raw-1025-light" / tag
    out.mkdir(parents=True, exist_ok=True)
    for i, f in enumerate(frames):
        cv2.imwrite(str(out / f"{tag}_{i:03d}.png"), f)
    print(f"  → 저장 {out}")
