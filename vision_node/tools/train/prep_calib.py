"""Calibration set for int8 quantization: N person-free training snaps, letterboxed, RGB 0..1 NHWC float32."""
import pathlib
import random
import sys

import cv2
import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from common import letterbox  # noqa: E402

files = sorted((pathlib.Path(__file__).parent / "raw-1023/snap").glob("*.png"))
random.Random(0).shuffle(files)
batch = np.stack([letterbox(cv2.imread(str(p)))[0] for p in files[: int(sys.argv[2])]]).astype(np.float32) / 255.0
np.save(sys.argv[1], batch)
print("calib", batch.shape, batch.dtype, f"{batch.min():.2f}~{batch.max():.2f}")
