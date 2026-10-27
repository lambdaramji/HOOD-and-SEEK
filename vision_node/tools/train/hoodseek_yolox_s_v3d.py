"""v3d: v3b recipe on train_v3d.json (`build_v3e.py --no-doll`). Only used to auto-label the dolls for v3e (cap_doll.py)."""
import importlib.util
import os

_spec = importlib.util.spec_from_file_location("v3", os.path.join(os.path.dirname(os.path.abspath(__file__)), "hoodseek_yolox_s_v3.py"))
_v3 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v3)


class Exp(_v3.Exp):
    def __init__(self):
        super().__init__()
        self.exp_name = "hoodseek_s_v3d"
        self.train_ann = "train_v3d.json"
