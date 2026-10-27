"""YOLOX-S fine-tune for the HOOD&SEEK doll (COCO cat) on top of the COCO checkpoint. All 80 classes kept.

  cd ~/hoodseek-train && TORCH_FORCE_NO_WEIGHTS_ONLY_LOAD=1 venv-train/bin/python YOLOX/tools/train.py \
      -f hoodseek_yolox_s.py -d 1 -b 12 --fp16 -c weights/yolox_s.pth
"""
import os

from yolox.exp import Exp as BaseExp


class Exp(BaseExp):
    def __init__(self):
        super().__init__()
        self.depth, self.width = 0.33, 0.50
        self.exp_name = "hoodseek_s"
        self.output_dir = os.path.expanduser("~/hoodseek-train/runs")
        self.data_dir = os.path.expanduser("~/hoodseek-train/ds")
        self.train_ann, self.val_ann = "train.json", "val_coco.json"
        self.num_classes = 80
        self.input_size = self.test_size = (640, 640)
        self.multiscale_range = 2
        # Fine-tune: short schedule, low LR, milder augmentation than COCO training from scratch.
        self.max_epoch, self.warmup_epochs, self.no_aug_epochs = 25, 1, 5
        self.basic_lr_per_img = 0.002 / 64.0
        self.mosaic_scale = (0.5, 1.5)
        self.mixup_prob, self.degrees, self.shear = 0.5, 5.0, 1.0
        self.eval_interval, self.print_interval = 5, 50
        self.save_history_ckpt = False
        self.data_num_workers = 4
