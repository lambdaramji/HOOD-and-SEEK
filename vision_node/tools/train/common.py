"""Shared preprocessing: the exact letterbox vision-node uses (RGB, scale to fit, centered, black padding)."""
import cv2
import numpy as np


def letterbox(bgr, size=640):
    h, w = bgr.shape[:2]
    scale = min(size / w, size / h)
    nw, nh = max(1, int(np.floor(w * scale))), max(1, int(np.floor(h * scale)))
    left, top = (size - nw) // 2, (size - nh) // 2
    canvas = np.zeros((size, size, 3), np.uint8)
    canvas[top:top + nh, left:left + nw] = cv2.resize(cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB), (nw, nh), interpolation=cv2.INTER_LINEAR)
    return canvas, (left, top, nw / w, nh / h)
