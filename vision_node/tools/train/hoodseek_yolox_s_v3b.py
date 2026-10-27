"""v3b: v3 recipe + hard negatives (chair/jacket/bag at the user spot, no person) so clothing alone is not a person."""
import importlib.util
import os

_spec = importlib.util.spec_from_file_location("v3", os.path.join(os.path.dirname(os.path.abspath(__file__)), "hoodseek_yolox_s_v3.py"))
_v3 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v3)


class Exp(_v3.Exp):
    def __init__(self):
        super().__init__()
        self.exp_name = "hoodseek_s_v3b"
        self.train_ann = "train_v3b.json"
