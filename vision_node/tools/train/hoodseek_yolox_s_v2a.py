"""v2-a: like hoodseek_yolox_s.py but our snaps repeated 3x (not 10x) and the CSPDarknet backbone weights frozen,
so only PAFPN + head learn. Backbone BN running stats are not updated (momentum 0), but the model trains in train mode,
so backbone BN still normalizes with batch statistics while training; eval/Pi 5 uses the COCO checkpoint's running stats.
Aim: keep v1's doll->cat while losing less person recall
(v1 missed a person close to the box with hands hidden: 12/15 -> 0/15, Q8 re-measure 2026-10-23 p08).

train_r3.json = train.json keeping only the first 3 copies of each ours/ image (made once by hand on 2026-10-23,
no script). v3/v3b override train_ann and do not need it.
"""
import importlib.util
import os

_spec = importlib.util.spec_from_file_location("v1", os.path.join(os.path.dirname(os.path.abspath(__file__)), "hoodseek_yolox_s.py"))
_v1 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v1)


class Exp(_v1.Exp):
    def __init__(self):
        super().__init__()
        self.exp_name = "hoodseek_s_v2a"
        self.train_ann = "train_r3.json"

    def get_model(self):
        model = super().get_model()
        for m in model.backbone.backbone.modules():
            for p in m.parameters(recurse=False):
                p.requires_grad = False
            if hasattr(m, "momentum") and m.__class__.__name__.startswith("BatchNorm"):
                m.momentum = 0.0  # running stats stay as in the COCO checkpoint (train-mode forward still uses batch stats)
        return model
