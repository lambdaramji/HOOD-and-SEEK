"""v3: v2-a recipe (CSPDarknet weights frozen, BN running stats not updated; PAFPN + head trained) on train_v3.json, which adds in-scene
person frames labeled person (and the doll as cat where present). Aim: keep v2-a's doll->cat and stop the scene bias
that lowered natural-pose person scores and read walking legs as cat (Q8 re-measure 2026-10-23 p10/p11).
"""
import importlib.util
import os

_spec = importlib.util.spec_from_file_location("v2a", os.path.join(os.path.dirname(os.path.abspath(__file__)), "hoodseek_yolox_s_v2a.py"))
_v2a = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v2a)


class Exp(_v2a.Exp):
    def __init__(self):
        super().__init__()
        self.exp_name = "hoodseek_s_v3"
        self.train_ann = "train_v3.json"
