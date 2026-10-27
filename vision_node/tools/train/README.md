# 고양이 인형 미세조정 모델 만들기 (YOLOX-S → Pi 5 int8 TFLite)

Pi 5 vision-node가 쓰는 검출 모델을 시연 장면에 맞춰 다시 학습하는 도구다. 결과와 근거는 [2026-10-24 모델 문서](../../../inbox/김병직/2026-10-24-cat-model-finetune.md)에 있다.
COCO 80종을 그대로 두므로 vision-node 코드·설정(`cat_classes` 15·16, 출력 `boxes`·`scores`·`class_idx`)은 바뀌지 않는다. 바뀌는 것은 Pi 5의 모델 파일 하나다.

## 지켜야 할 것

- **사람이 찍힌 사진은 이 PC 밖으로 내보내지 않는다(#72).** 저장소·Roboflow·Slack에 올리지 않는다. 사람 라벨은 `label_tool2.py`(로컬)로 한다. 인형·빈 장면 사진만 외부 라벨 도구를 써도 된다.
- 사람 라벨은 `label_tool2.py`(로컬)로만 한다(PM 결정 2026-10-24, #72). 미리 채우기는 `bgsub_prefill.py`·`teacher_prefill.py`도 이 PC 안에서 돈다.
- 빌드 PC에 있는 사람 사진 위치와 10/27 전 삭제 목록은 [v3e 문서 「#72 관련」](../../../inbox/김병직/2026-10-25-cat-model-v3e.md#72-관련)에 모아 둔다.
- 실제 IP는 저장소에 넣지 않는다. `PI5=root@<Pi5 IP>`·`--pi5`로 넘긴다.
- 학습 데이터·모델·측정 원자료는 저장소 밖 `~/hoodseek-train/`, `~/hoodseek-measure/`에 둔다.
- 저장소가 `~/HOOD-SEEK`가 아니면 `export HOODSEEK_REPO=<저장소 경로>`. `q8_compare.py`·`snap_view.py`·`cap.sh`가 이 값으로 `profiles/`·ROI·`measure.sh`를 찾는다.

## 준비 (빌드 PC, NVIDIA GPU)

```bash
mkdir -p ~/hoodseek-train && cp "${HOODSEEK_REPO:-$HOME/HOOD-SEEK}"/vision_node/tools/train/* ~/hoodseek-train/ && cd ~/hoodseek-train
mkdir -p models runs
python3 -m venv venv-train && ./venv-train/bin/pip install torch torchvision --index-url https://download.pytorch.org/whl/cu128
./venv-train/bin/pip install loguru tabulate pycocotools opencv-python-headless thop ninja psutil tqdm onnx onnxruntime tensorboard
python3 -m venv venv-conv && ./venv-conv/bin/pip install "tensorflow==2.19.0" "tf_keras==2.19.0" onnx2tf onnx onnxslim onnx_graphsurgeon sng4onnx psutil ai-edge-litert onnxruntime opencv-python-headless
python3 -m venv venv && ./venv/bin/pip install tflite numpy            # inspect_tflite.py
git clone --depth 1 https://github.com/Megvii-BaseDetection/YOLOX.git
mkdir -p weights && curl -L -o weights/yolox_s.pth https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_s.pth
curl -L -o weights/yolox_x.pth https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_x.pth   # v3e 미리 채우기용 큰 모델(teacher_prefill.py)
mkdir -p coco && (cd coco && curl -LO http://images.cocodataset.org/zips/val2017.zip && curl -LO http://images.cocodataset.org/annotations/annotations_trainval2017.zip \
  && unzip -q val2017.zip && unzip -q -j annotations_trainval2017.zip annotations/instances_val2017.json -d annotations)
python3 -c "import numpy as np; np.save('calibration_image_sample_data_20x128x128x3_float32.npy', np.random.default_rng(0).random((20,128,128,3), dtype=np.float32))"
mkdir -p ref && scp root@<Pi5 IP>:/data/ecoai/assets/models/object_detection/yolox.tflite ref/   # 원래 모델(비교 기준)
sed -i 's/^\( *\)from yolox\.layers import COCOeval_opt as COCOeval$/\1raise ImportError  # hoodseek: use pycocotools/' YOLOX/yolox/evaluators/coco_evaluator.py
grep -n "hoodseek: use pycocotools" YOLOX/yolox/evaluators/coco_evaluator.py   # 한 줄 나와야 한다
```

`sed` 줄: YOLOX 평가기는 빠른 평가 확장을 ninja로 빌드하려다 실패하면 멈춘다. 그 import를 `raise ImportError`로 바꿔(들여쓰기 유지) 바로 아래 `except ImportError`의 pycocotools로 가게 한다.
`npy` 한 줄은 onnx2tf가 인터넷에서 받으려다 실패하는 시험용 샘플을 대신한다.

## 순서

| 단계 | 명령 | 결과 |
|---|---|---|
| 1. 인형 사진 | Pi 5 vision 서비스를 멈추고 `python3 snap_view.py --pi5 root@<Pi5 IP> --tag led1` (`s` 저장, `r` 자동 저장) | `raw-1023/snap/*.png` |
| 2. 인형 라벨 | 사람 없는 사진만 Roboflow 가능(무료 플랜은 공개 — 비공개 워크스페이스 권장, 클래스 `cat`, Preprocessing·Augmentation 끔, COCO JSON) → `rf-v1/` | `rf-v1/{train,valid}/_annotations.coco.json` |
| 3. 사람 사진 | `python3 snap_view.py --pi5 … --tag per --out ~/hoodseek-train/raw-1024-person` (자동 저장으로 걸어오기·서기·옷 바꾸기) | `raw-1024-person/*.png` |
| 4. 사람 라벨 | `python3 label_tool2.py ~/hoodseek-train/raw-1024-person` (로컬, `p` 사람·`c` 인형) | `raw-1024-person/labels.json` (v3b 라벨은 이 도구가 아니라 Roboflow에서 했다 — #72 위반, [모델 문서 「#72 관련」](../../../inbox/김병직/2026-10-24-cat-model-finetune.md#72-관련). 다시 할 때는 이 도구로) |
| 5. 사람 아님 | 사람 자리에 의자·겉옷·가방만 두고 `PI5=… ./cap.sh h01 negative jacket_chair_center none -` 또는 `snap_view.py --tag neg --out ~/hoodseek-train/raw-1024-neg --delay 5 --duration 8` | `raw-1023/frames/h0*`, `raw-1024-neg/*.png` |
| 6. 데이터 묶기 | `python3 -I build_dataset.py && python3 -I build_v3.py 3 && python3 -I build_v3b.py 3` | `ds/annotations/train_v3b.json` |
| 7. 보정 사진 | `./venv-conv/bin/python prep_calib.py models/calib64.npy 64` | int8 양자화 보정용 |
| 8. 학습 | `TORCH_FORCE_NO_WEIGHTS_ONLY_LOAD=1 PYTHONPATH=YOLOX ./venv-train/bin/python YOLOX/tools/train.py -f hoodseek_yolox_s_v3b.py -d 1 -b 12 --fp16 -c weights/yolox_s.pth` | `runs/hoodseek_s_v3b/last_epoch_ckpt.pth` (RTX 5060 Laptop 약 18분) |
| 9. 평가·변환·비교 | `./post_train.sh hoodseek_s_v3b hoodseek_s_v3b hoodseek_yolox_s_v3b.py` | `models/hoodseek_s_v3b_int8.tflite`, `runs/post_train-hoodseek_s_v3b.txt` |
| 10. Pi 5 확인 | 모델 교체 뒤 Q8 실행 시트의 `measure.sh` 조건(인형 d·빈 장면 n·사람 p·물건 n) | 아래 「합격 기준」 |

`hoodseek_yolox_s_v3b.py`는 `hoodseek_yolox_s.py`(데이터·학습률) → `_v2a.py`(CSPDarknet 가중치 고정, BN running 통계 갱신 중지 — 학습 중 정규화는 배치 통계) → `_v3.py`(사람 장면 데이터)를 차례로 물려받는다.

## v3e — 하얀 바닥과 띠 근처 인형 보강 (2026-10-24~25)

카메라 자동 노출 때문에 화면 위쪽 바닥이 하얗게 날아가면(밝기 233~250), v3b는 그 띠를 사람으로 보고 그 위에 선 사람은 덜 잡았다. 보강 1·2차(v3c·v3d)를 거쳐 v3e로 고쳤다. 근거와 시험 기록은 [v3e 문서](../../../inbox/김병직/2026-10-25-cat-model-v3e.md)에 있다. 위 1~9단계(v3b 데이터)를 먼저 해 둔다.

| 단계 | 명령 (`~/hoodseek-train`, Pi 5 vision 서비스 정지) | 결과 |
|---|---|---|
| 11. 빈 장면(조명별) | 화면 밖에 사람이 없을 때 `./venv-conv/bin/python -I cap_light.py <tag> root@<Pi5 IP>`. 학습: L1a·L1c·L1d(형광등 모두 켬), L2a(절반), L3a(끔), O1(사용자 자리에 의자), O2(그 바닥에 가방·겉옷). 시험: L1b·L1e | `raw-1025-light/<tag>/*.png` (원래 모델이 사람·반사를 보면 저장하지 않음) |
| 12. 하얀 바닥 사람 | 형광등 모두 켬, `python3 snap_view.py --pi5 root@<Pi5 IP> --tag per2 --out ~/hoodseek-train/raw-1025-person --delay 5 --duration 90 --auto 0.6` 후 `r`. 걸어오기·서기·발 위치·비스듬히·붙어 서기·걸어 나가기 | `raw-1025-person/*.png` |
| 13. 미리 채우기 | `python3 -I bgsub_prefill.py raw-1025-person raw-1025-light/L1a raw-1025-light/L1c raw-1025-light/L1d` → `prefill_bg.json`, `TORCH_FORCE_NO_WEIGHTS_ONLY_LOAD=1 PYTHONPATH=YOLOX ./venv-train/bin/python teacher_prefill.py raw-1025-person 0.25` → `prefill.json`, 그 뒤 아래 한 줄로 합침(배경 빼기 우선) | `raw-1025-person/prefill.json` |
| 14. 사람 라벨 | `python3 label_tool2.py ~/hoodseek-train/raw-1025-person 2` (반사에는 상자를 그리지 않음) | `raw-1025-person/labels.json` |
| 15. v3d 만들기 | `python3 -I build_v3e.py --no-doll` → 18단계 학습 명령에서 `-f hoodseek_yolox_s_v3d.py` → `./post_train.sh hoodseek_s_v3d hoodseek_s_v3d hoodseek_yolox_s_v3d.py` | `models/hoodseek_s_v3d_int8.tflite`(16단계 인형 라벨용) |
| 16. 띠 근처 인형 | 인형만 놓고 화면 밖으로 나간 뒤 `./venv-conv/bin/python -I cap_doll.py <tag> root@<Pi5 IP>`. B1 바깥 오른쪽, B2 먼 쪽 테두리 위, B3 먼 쪽 바깥, B4 먼 쪽 오른쪽 모서리, B5 먼 쪽 왼쪽 모서리, B6 바깥 왼쪽 | `raw-1025-doll/<tag>/` + `labels.json`(v3d cat 상자 ≥ 0.8, 사람 상자 없음) |
| 17. 데이터 묶기 | `python3 -I build_v3e.py` | `ds/annotations/train_v3e.json` |
| 18. 학습 | `TORCH_FORCE_NO_WEIGHTS_ONLY_LOAD=1 PYTHONPATH=YOLOX ./venv-train/bin/python YOLOX/tools/train.py -f hoodseek_yolox_s_v3e.py -d 1 -b 12 --fp16 -c weights/yolox_s.pth` (노트북은 전원 연결 — 배터리면 반복이 4~13배 느림) | `runs/hoodseek_s_v3e/` (약 22분) |
| 19. 평가·변환·시험 | `./post_train.sh hoodseek_s_v3e hoodseek_s_v3e hoodseek_yolox_s_v3e.py && ./venv-conv/bin/python -I eval_light.py models/hoodseek_s_v3b_int8.tflite models/hoodseek_s_v3e_int8.tflite` | `models/hoodseek_s_v3e_int8.tflite` |

13단계 합치기:
```bash
python3 -I -c 'import json,pathlib; r=pathlib.Path("raw-1025-person"); b=json.loads((r/"prefill_bg.json").read_text()); t=json.loads((r/"prefill.json").read_text()); (r/"prefill_teacher.json").write_text(json.dumps(t)); (r/"prefill.json").write_text(json.dumps({k: b.get(k) or t.get(k) or [] for k in set(b)|set(t)}))'
```
v3d는 16단계 인형 라벨을 만드는 데만 쓴다(`cap_doll.py`가 `models/hoodseek_s_v3d_int8.tflite`를 읽음). v3d = v3e에서 인형만 뺀 데이터다.

## 변환이 지키는 것 (`export_onnx.py`, `convert.sh`, `rename_io.py`)

- 입력 `image` [1,640,640,3] uint8(scale 1/255). vision-node가 RGB letterbox(640×480 → 위아래 80 px 검은 여백)로 넣는 그대로다. YOLOX가 원하는 BGR 0~255는 첫 합성곱 가중치에 섞어 넣는다.
- 출력 `boxes` [1,8400,4]·`scores` [1,8400] uint8, `class_idx` [1,8400] int32. 상자와 점수는 끝까지 다른 텐서로 흐른다. 한 텐서에 붙이면 int8 눈금 하나 때문에 점수가 전부 0이 된다 [실측 2026-10-23].
- 확인: `./venv/bin/python inspect_tflite.py models/hoodseek_s_v3e_int8.tflite`

## 합격 기준 (Pi 5, 판정값 `profiles/default.json`)

- 인형 가운데·화면 끝·카메라 쪽 끝: 반려동물 칸 hit 30/30, 라벨 cat
- 사람(팔을 몸 옆에·뒷짐·걸어오기·손 보임): person 0.35 이상이 원래 모델 이상
- 사람 없음(빈 장면·의자·겉옷·가방, **학습에 없던 배치**): person 0.35 이상 0
- COCO 떼어 둔 1,000장 person AP가 원래 모델(53.9)에서 크게 떨어지지 않을 것
- (v3e부터) 바닥이 하얗게 날아간 빈 장면: person 0.35 이상 0. 그 바닥에 선 사람(30 cm 팔 옆·뒷짐): 15장 중 8장 이상
- (v3e부터) 인형을 조리면 바깥·먼 쪽에 두어 위쪽이 바닥 띠에 걸칠 때: person 0.35 이상 0

## Pi 5에 넣기·되돌리기 (Pi 5에 쓰는 명령)

지금 Pi 5 모델은 v3e(sha256 `31a988c1…`, 2026-10-25)다. 모델 파일은 저장소에 넣지 않는다(PM 결정 2).

```bash
scp models/hoodseek_s_v3e_int8.tflite root@<Pi5 IP>:/data/hoodseek/models-test/
ssh root@<Pi5 IP> 'systemctl stop hoodseek-vision-node && cp /data/hoodseek/models-test/hoodseek_s_v3e_int8.tflite /data/ecoai/assets/models/object_detection/yolox.tflite && sha256sum /data/ecoai/assets/models/object_detection/yolox.tflite && systemctl start hoodseek-vision-node'
# v3b로 되돌리기(하얀 바닥 오검출 있음): models-test/hoodseek_s_v3b_int8.tflite (8d3f76d3…)
# 원래 모델로 되돌리기(인형을 cat으로 못 잡음): models-test/yolox.tflite.qaihub-ad392c6e
ssh root@<Pi5 IP> 'systemctl stop hoodseek-vision-node && cp /data/hoodseek/models-test/yolox.tflite.qaihub-ad392c6e /data/ecoai/assets/models/object_detection/yolox.tflite && systemctl start hoodseek-vision-node'
```
