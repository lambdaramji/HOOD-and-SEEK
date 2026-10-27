# vision-node (Pi 5)

카메라 프레임에서 사람·반려동물 후보를 찾아 SafetyCore(Pi 4)로 `vision` 메시지를 보내는 C++17 노드다. **관측만 보고한다.** 재실·고양이 잠금 판정, 디바운스, 점수 기준(`P_person_score`·`P_cat_score`·`P_cat_overlap`·`P_cat_hits`)은 모두 SafetyCore가 `profiles/default.json` 값으로 한다 (`src/vision.hpp` 13-14행, 설계서 §3.4).

- 계약: [`contracts/vision.schema.json`](../contracts/vision.schema.json), [`contracts/README.md`](../contracts/README.md)
- 모델 학습·변환·Pi 5에 넣기·되돌리기: [`tools/train/README.md`](tools/train/README.md)
- 측정 절차와 기대 해시: [`inbox/김병직/2026-10-22-q8-run-sheet.md`](../inbox/김병직/2026-10-22-q8-run-sheet.md)

## 파일

| 경로 | 하는 일 |
|---|---|
| `src/main.cpp` | 명령행, 입력 모드 3가지, 멈춤 감시, 메인 루프 |
| `src/vision.{hpp,cpp}` | Config·ROI 읽기와 검사, 후처리(점수 하한·클래스별 NMS), ROI 겹침, `vision` 메시지 만들기 |
| `src/link.{hpp,cpp}` | SafetyCore TCP 클라이언트 (hello·seq·재접속) |
| `src/ecoai_detector.{hpp,cpp}` | ECO.AI 추론 래퍼 (TFLite, cpu/gpu). `-DHOODSEEK_ECOAI=ON`일 때만 빌드 |
| `src/v4l2_camera.{hpp,cpp}` | V4L2 mmap YUYV 읽기, 가장 새 프레임만 씀, 180° 회전 |
| `src/latest_frame.hpp` | 카메라 읽기 스레드 → 추론 루프. 가장 새 프레임 한 장만 넘긴다(쌓지 않음) |
| `src/preview.{hpp,cpp}` | 카메라 카드 영상: 127.0.0.1 HTTP MJPEG (`--preview-port`일 때만) |
| `config/roi.json` | 조리면(LED 판 + 한 칸) 다각형. 기기 사본의 정본 |
| `tests/` | `vision_tests.cpp`(단위), `preview_tests.cpp`(영상 루프백), `cli_test.py`(프로세스·TCP), `detection_table_test.py` |
| `tools/roi_click.py` | 화면을 찍어 `roi.json`을 만든다 (`--points`로 숫자 입력도 됨) |
| `tools/live_view.py` | PC 카메라 또는 Pi 5 영상을 ROI와 함께 본다 |
| `tools/measure.sh`, `tools/detection_table.py` | #49·Q8 측정과 표 만들기 |
| `tools/train/` | YOLOX-S 미세조정·변환 파이프라인 |

## 동작

1. 카메라를 먼저 연다. 640×480 YUYV만 받는다(MJPEG 미지원). 버퍼 4개를 돌리며 읽을 때마다 쌓인 옛 프레임은 버리고 가장 새 프레임만 쓴다. 추론이 약 290 ms라 그대로 두면 1.4 s 지난 프레임을 보게 되었기 때문이다 (`src/v4l2_camera.cpp` 129-138행). 카메라는 별도 스레드가 자기 속도로 계속 읽고(카메라 카드 영상용), 추론은 그중 가장 새 프레임만 가져간다(`src/latest_frame.hpp`). 읽기가 실패하면 예전처럼 `camera read failed`로 끝나고 systemd가 다시 띄운다.
2. 카메라가 정한 크기가 ROI의 `frame_w`·`frame_h`와 다르거나, ROI `rotate`가 `--rotate`와 다르면 **시작을 거부한다** (`src/main.cpp`).
3. 모델 입력 하나(NHWC/NCHW, FLOAT32·UINT8·INT8), 출력은 이름으로 `boxes`·`scores`·`class_idx`를 요구한다 (`src/ecoai_detector.cpp`). 전처리는 BGR→RGB, letterbox, /255.
4. 후처리: 점수 0.10 미만 버림 → 원래 좌표로 되돌림 → 클래스별 NMS(IoU 0.45), 최대 300개.
5. 사람(COCO 0)과 반려동물(COCO 15 cat·16 dog — 인형이 dog로 잡혀서 함께 둠)에서 각각 가장 높은 후보 하나씩을 보낸다. 반려동물 후보에는 `roi_overlap`(상자와 ROI 다각형의 겹친 넓이 ÷ 상자 넓이)을 붙인다.

기본 Config (`src/vision.hpp` 15-24행). `--config` 파일로 바꿀 수 있지만 저장소와 기기에는 따로 두지 않는다.

| 키 | 기본 | 규칙 |
|---|---|---|
| `detection_floor` | 0.10 | (0, 0.10] — 낮추기만 된다 |
| `nms_iou` | 0.45 | [0, 1] |
| `watch_classes` | 0, 15, 16, 77 | COCO id 0..79 |
| `cat_classes` | 15, 16 | 같음 |
| `resize_mode` | letterbox | |

모르는 키는 거부한다.

### SafetyCore로 보내는 것

- 봉투 `{"v":1,"type":"vision","src":"vision","seq","t_ms","p"}`, 한 줄 4096바이트 이하.
- 연결마다 `hello`(seq 0, `node:"vision-node"`)를 먼저 보내고 seq는 1부터 다시 센다.
- 추론 한 번에 메시지 하나(약 3 Hz, `contracts/README.md`).
- 끊기면 1 s에 한 번 다시 붙는다. 연결 대기 300 ms, 보내기 대기 200 ms, `TCP_USER_TIMEOUT` 2 s. **끊긴 동안 만든 메시지는 쌓지 않고 버린다.**
- SafetyCore가 보내는 `state` 줄은 읽고 버린다.
- 이 노드가 죽거나 2 s 넘게 안 보내면 SafetyCore가 vision을 만료시켜 presence를 unknown으로 둔다(불변식 ③, 설계서 §2.4).

### 카메라 카드 영상 (UI 설계 v1 §3.2)

Pi 5 앱의 카메라 카드에 vision-node가 이미 읽은 프레임을 보여 준다. 카메라는 한 프로그램만 열 수 있어 앱이 따로 열 수 없기 때문이다. **표시 전용이다.** SafetyCore로 가지 않고 판정에 쓰지 않으며, 저장하지 않는다.

- 켜기: `--preview-port 7431`(카메라 모드만, 1024..65535). 기기에서는 env 파일의 `PREVIEW_ARGS`. 없으면 꺼짐. 포트 값이 틀려도 영상만 꺼진다.
- **배포 순서: 바이너리 → 유닛(`$PREVIEW_ARGS`가 있는 새 유닛) → env의 `PREVIEW_ARGS` 줄.** 이 옵션을 모르는 옛 바이너리는 시작을 거부하고, systemd가 1 s마다 다시 띄우는 동안 presence가 unknown으로 남는다. **되돌릴 때는 거꾸로** env 줄을 먼저 지우고 옛 바이너리로 바꾼다. 명령과 단계별 확인은 [`deploy/README.md` 「vision-node 카메라 카드 영상 켜기」](../deploy/README.md#vision-node-카메라-카드-영상-켜기-144).
- 주소: `http://127.0.0.1:7431/` — 루프백에만 열린다. 경로·요청 내용은 보지 않는다. 동시에 2개까지.
- 응답: `HTTP/1.0 200 OK`, `Content-Type: multipart/x-mixed-replace; boundary=hoodseekframe`. 이어서 프레임마다
  `--hoodseekframe\r\nContent-Type: image/jpeg\r\nContent-Length: N\r\n\r\n` + JPEG N바이트 + `\r\n`.
- 프레임: 480×360 JPEG(품질 70, 640×480 카메라 프레임을 가로세로 3/4로 줄임 — 화질보다 반응성), **카메라 프레임을 60 ms에 한 장까지(최대 약 16 FPS, 30 fps 카메라에서는 한 장 걸러 15 FPS)**. 추론 속도와 상관없다. 카메라 읽기 스레드는 받는 쪽이 있을 때만 프레임을 복사하고, 인코딩·전송은 별도 스레드가 한다. 200 ms 안에 못 받는 쪽은 끊는다.
- 지연: 읽은 프레임을 추론을 기다리지 않고 넘기고, 전송 스레드는 eventfd로 바로 깨어난다(#148). 상한 16 FPS는 같은 Pi 5에서 도는 추론과 앱 디코딩의 CPU를 아끼려는 것이다.
- 화면 시각은 앱이 프레임을 받은 시각으로 쓴다(왼쪽 위 「● 실시간 HH:MM:SS」, #151). 2 s 동안 프레임이 없으면 앱이 「영상 일시 중지」를 덮는다(UI 설계 §3.2).
- JPEG는 기기 이미지의 `libturbojpeg.so.0`을 실행 중에 불러 만든다(SDK에 헤더가 없음). 라이브러리가 없거나 포트를 못 열면 stderr에 `preview off`를 남기고 **영상만 끄고 판정은 계속한다.** PC 빌드는 OpenCV imgcodecs를 쓴다.

### roi.json

```json
{"frame_w": 640, "frame_h": 480, "rotate": 0, "points": [[x, y], ...], "note": "..."}
```

- 키는 이 다섯 개만. 크기 1..4096, `rotate` 0 또는 180, 점 3..32개, 화면 안, 볼록·넓이 있음·테두리 순서.
- `tools/roi_click.py`의 검사는 같은 규칙이지만 모르는 키와 4096 상한은 보지 않는다. 노드가 거부하면 그 두 가지부터 본다.
- 카메라나 박스를 다시 고정하면 다시 잡는다(#123). 바꾼 뒤 `sha256sum vision_node/config/roi.json`을 Q8 실행 시트에 남긴다.
- 10/26부터 ROI는 **시연 박스 범위**(박스 안쪽 테두리, 화면의 73 %)다. 조리면 밖이라도 박스 안이면 반려동물 잠금 후보가 된다(PM 결정 10/26, 병직·현지). 프레임은 카메라 카드 영상에서 `wget`으로 한 장 받아 `tools/roi_click.py`로 찍었다(BusyBox `nc`는 요청을 다 보내면 쓰기 쪽을 닫아 서버가 끊는다).

## 명령행

```
hoodseek_vision --camera /dev/video0 --model yolox.tflite [--backend cpu|gpu] [--threads 2] [--rotate 0|180]
                [--config config.json] [--output out.jsonl] [--dump-dir DIR --dump-every N]
                [--max-frames N] [--stall-ms 10000] [--core <Pi4 IP>:7420 --roi roi.json] [--preview-port 7431]
hoodseek_vision --replay proposals.jsonl|- [--config ...] [--output ...] [--max-frames N] [--core ... --roi ...]   # PC 시험
hoodseek_vision --export-input photo.jpg --input-width 640 --input-height 640 --tensor-output rgb.f32 [--layout nhwc|nchw] [--resize-mode stretch|letterbox]   # PC 전용
```

- 입력 모드는 셋 중 하나만. `--camera`는 ECO.AI 빌드에서만, `--export-input`은 PC 빌드에서만 된다.
- `--core`를 주면 `--roi`가 꼭 있어야 한다. `--core`는 IPv4만 받는다.
- `--threads` 1..16(기본 2), `--stall-ms` 200..120000, `--max-frames` 0이면 끝없이.
- `--dump-dir`·`--dump-every`는 같이 주고 카메라 모드에서만 PPM을 쓴다.
- 환경 변수는 읽지 않는다. 기기 값은 systemd env 파일이 명령행으로 넘긴다.

출력(`--output`, 기본 stdout)은 JSON 한 줄씩: `start` 이벤트 → 프레임마다 기록(`seq`·`t_ms`·`infer_ms`·라벨별 `best`) → `stop` 또는 `fault`.

| 종료 코드 | 뜻 |
|---|---|
| 0 | 정상 종료 (`--max-frames` 도달 등) |
| 2 | 예외 (설정 오류, 카메라·모델 열기 실패 등) |
| 3 | 멈춤 감시: `--stall-ms` 동안 프레임·추론이 진행되지 않음 (`source_stalled`) |

## 빌드와 시험

PC(시험용):

```bash
cmake -S vision_node -B vision_node/build
cmake --build vision_node/build
ctest --test-dir vision_node/build --output-on-failure
```

- 필요: OpenCV 4(pkg-config `opencv4`), nlohmann_json, Python 3 + `cv2`·`numpy`·`jsonschema`(시험용).
- CI에는 vision_node 잡이 없다. 기기 의존이라 atlas-dev에서 확인하고 PR에 남긴다 (`.github/workflows/system.yml` 머리 주석).

Pi 5(기기용) — 마스터 PC `atlas-dev` Docker 안에서:

```bash
source /opt/atlas-sdk-x86_64/environment-setup-armv8a-atlas-linux
cmake -S vision_node -B build-vision -DHOODSEEK_ECOAI=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-vision
sha256sum build-vision/hoodseek_vision
```

- 기기 이미지에는 OpenCV core·imgproc만 있다(videoio·imgcodecs 없음). 그래서 카메라는 V4L2로 직접 읽는다.
- [추정] 위 명령은 9/20 inbox 기록(옛 경로 `pi4/vision`)을 지금 경로로 옮긴 것이다. 확인 방법: 다음 기기 빌드 때 출력과 sha256을 PR에 남긴다.

## Pi 5 배포

유닛: [`deploy/systemd/hoodseek-vision-node.service`](../deploy/systemd/hoodseek-vision-node.service), env 예시: [`deploy/systemd/vision-node.env.example`](../deploy/systemd/vision-node.env.example).

| 기기 경로 | 무엇 |
|---|---|
| `/data/hoodseek/bin/hoodseek_vision` | 실행 파일 |
| `/data/hoodseek/vision-node.env` | `CORE`(Pi 4 주소:7420)·`ROTATE`·`ROI`·`THREADS` |
| `/data/hoodseek/config/roi.json` | 저장소 `config/roi.json` 사본 |
| `/data/ecoai/assets/models/object_detection/yolox.tflite` | 모델 (지금 v3e) |
| `/data/hoodseek/models-test/` | 되돌리기용 모델 사본 |
| `/data/share/usr/lib/systemd/system/hoodseek-vision-node.service` | 유닛 (`/`가 읽기 전용) |

```bash
# Pi 5에서. 바이너리를 바꿀 때
systemctl stop hoodseek-vision-node
cp /data/hoodseek/bin/hoodseek_vision /data/hoodseek/bin/hoodseek_vision.bak-<옛 해시 앞 8자>
cp <새 바이너리> /data/hoodseek/bin/hoodseek_vision
sha256sum /data/hoodseek/bin/hoodseek_vision /data/hoodseek/config/roi.json \
          /data/ecoai/assets/models/object_detection/yolox.tflite
systemctl start hoodseek-vision-node
journalctl -u hoodseek-vision-node -n 20
```

- **부팅 뒤 자동으로 안 뜬다.** `/data`가 늦게 마운트되어 `.wants` 링크가 무시된다. 부팅 뒤 `systemctl start hoodseek-vision-node`를 사람이 한 번 실행한다(#46).
- `Restart=always`라 카메라가 빠지면 다시 꽂힐 때까지 1 s마다 재시작한다.
- root로 돈다(`/dev/video0` 권한, `/etc` 읽기 전용).
- 프레임 기록은 `/dev/null`로 버리고, 고장은 stderr로 journal에 남는다.
- 지금 기기 값: 모델 v3e `31a988c1…` [실측 #67 정인 10/25], ROI `3f51757f…`(시연 박스 범위, PM 결정 10/26 — 이전 `b3eda292…` LED 판 + 한 칸) [실측 10/26 15:41 배포 뒤 Pi 5 `sha256sum /data/hoodseek/config/roi.json`, #67 병직 배포 기록]. 바꾸면 Q8 실행 시트와 #67에 해시를 남긴다.
