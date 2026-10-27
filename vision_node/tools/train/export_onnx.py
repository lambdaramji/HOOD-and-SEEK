"""YOLOX-S checkpoint -> ONNX with the same I/O as the deployed Qualcomm AI Hub yolox.tflite.

  venv-train/bin/python export_onnx.py <checkpoint.pth> <out.onnx>

Input  image     [1,3,640,640] RGB 0..1 (onnx2tf turns it into NHWC [1,640,640,3], as vision-node feeds it)
Output boxes     [1,8400,4] x1,y1,x2,y2 in 640x640 input pixels
       scores    [1,8400]   obj * best class probability
       class_idx [1,8400]   COCO 0..79 (int32)
YOLOX itself wants BGR 0..255: that flip and scale are folded into the Focus stem conv weights (no extra ops).
Boxes (pixels) and scores (0..1) are decoded per level and never share a tensor: YOLOX's own decode concatenates
them, and one int8 scale for both rounds every score to 0 [실측 2026-10-23 onnx2tf full-int8].
"""
import pathlib
import sys

import torch

sys.path.insert(0, str(pathlib.Path(__file__).parent / "YOLOX"))
from yolox.exp import get_exp  # noqa: E402


def fold_rgb01_into_stem(model):
    # Focus concatenates 4 pixel patches of 3 channels each; the conv after it sees 12 channels (BGR per patch).
    conv = model.backbone.backbone.stem.conv.conv
    w = conv.weight.data.clone()
    for g in range(4):
        conv.weight.data[:, 3 * g:3 * g + 3] = w[:, 3 * g:3 * g + 3].flip(1) * 255.0


class Wrap(torch.nn.Module):
    def __init__(self, model):
        super().__init__()
        self.model = model

    def forward(self, image):
        head = self.model.head
        boxes, scores = [], []
        for k, feat in enumerate(self.model.backbone(image)):
            f = head.stems[k](feat)
            cls_out = head.cls_preds[k](head.cls_convs[k](f))
            reg_feat = head.reg_convs[k](f)
            reg_out, obj_out = head.reg_preds[k](reg_feat), head.obj_preds[k](reg_feat)
            h, w = reg_out.shape[2:]
            yv, xv = torch.meshgrid(torch.arange(h), torch.arange(w), indexing="ij")
            grid = torch.stack((xv, yv), 2).reshape(1, h * w, 2).float()
            reg = reg_out.flatten(2).permute(0, 2, 1)
            stride = head.strides[k]
            xy, wh = (reg[..., :2] + grid) * stride, torch.exp(reg[..., 2:4]) * stride
            boxes.append(torch.cat([xy - wh / 2, xy + wh / 2], dim=-1))
            scores.append((obj_out.sigmoid() * cls_out.sigmoid()).flatten(2).permute(0, 2, 1))
        best, class_idx = torch.cat(scores, 1).max(dim=-1)
        return torch.cat(boxes, 1), best, class_idx.to(torch.int32)


ckpt, out = sys.argv[1], sys.argv[2]
exp = get_exp(None, "yolox-s")
model = exp.get_model()
state = torch.load(ckpt, map_location="cpu", weights_only=False)
model.load_state_dict(state["model"] if "model" in state else state)
model.eval()
fold_rgb01_into_stem(model)
wrap = Wrap(model).eval()
dummy = torch.rand(1, 3, 640, 640)
with torch.no_grad():
    b, s, c = wrap(dummy)
    print("torch out", tuple(b.shape), tuple(s.shape), tuple(c.shape), c.dtype)
    torch.onnx.export(wrap, dummy, out, input_names=["image"], output_names=["boxes", "scores", "class_idx"],
                      opset_version=13, dynamo=False)
print("saved", out)
