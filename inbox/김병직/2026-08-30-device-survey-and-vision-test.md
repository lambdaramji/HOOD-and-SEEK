# Pi 4 · Pi 5 실기기 조사와 사람 인식 시험 기록

- 작성자: 김병직
- 작성일: 2026-08-30
- 상태: draft
- 카테고리: vision-ai
- 관련 이슈/PR: 미정
- 조사 대상: Pi 4 `<Pi4 IP>`, Pi 5 `<Pi5 IP>` (SSH 직접 접속)

> ## 현재 상태 (2026-08-31 기준)
>
> **Pi 5에서 사람·고양이 검출이 정상 동작합니다. 3.7~3.8 FPS로 합격선(2 FPS)을 넘습니다.**
>
> | 항목 | 상태 |
> |---|---|
> | 사람 검출 (`person_detection`) | ✅ 동작 · 3.7 FPS |
> | 사물 검출 (`obj_detect`, 고양이 포함) | ✅ 동작 · 3.8 FPS |
> | 원인이었던 백엔드 설정 | ✅ 해결 (`NPU_QNN_HTP` → `GPU`) |
> | 플랫폼 원본 모델 파일 | ❌ 없음 — 공개 YOLOX로 대체해 동작 |
> | **탑뷰 각도 고양이 검출** | ⚠️ **실패 — 신규 리스크** ([4장](#4-고양이-검출--각도에-따라-성패가-갈립니다)) |
> | 얼굴 인식 (`face_recognition`) | ⚠️ Pi5 사용 불가 — `.bin`은 퀄컴 모델 ([1-6](#1-6-얼굴-인식-모델은-파일이-있어도-로드되지-않습니다-2026-08-31-추가)). **`face_recognition_float`(onnx)는 정상** |
>
> **[회신 2026-09-03] 처음 「배포 결함 세 가지」로 본 것은 대부분 결함이 아니었습니다.** 모델 파일 부재는 별도 제공 대상이 아니기 때문이고, `NPU_QNN_HTP`와 `.bin`은 모두 **퀄컴 전용이라 Pi5에 해당하지 않는 것**이었습니다. 남는 것은 **Pi5에 부적합한 기본값이 배포됐다**는 사실뿐입니다. → [LG 회신 검토](../임재범/2026-09-03-lg-answer-review.md)
>
> **2장은 시간 순 조사 기록이라 "실행되지 않음" 같은 중간 상태가 나옵니다.** 그 부분만 떼어 인용하지 마세요. 결론은 [3장](#3-원인-확정--백엔드-설정-해결)·[4장](#4-고양이-검출--각도에-따라-성패가-갈립니다)에 있습니다.
>
> **[회신 2026-09-03 추가] 위 FPS는 `VisionService1` 경로에서 측정한 값이고, LG는 그 인터페이스를 RPi5 미지원으로 회신했습니다.** 측정 자체는 유효하지만 **공식 미지원 경로의 참고값**이므로 아키텍처 확정 근거로 쓰지 않습니다. 권장 경로(video profile + OpenCV 직접 추론 / nnstreamer) 재측정이 필요합니다. 이 표는 조사 당시 기록이므로 고치지 않고 단서만 답니다. → [LG 회신 검토](../임재범/2026-09-03-lg-answer-review.md)

## 표기 규칙

이 문서의 모든 항목에는 출처가 붙어 있습니다. **다른 팀원이 같은 결론에 도달할 수 있어야 하고, 확인된 것과 아직 추정인 것이 섞이면 안 되기 때문입니다.**

| 표기 | 뜻 |
|---|---|
| **[실측]** | 기기에서 직접 실행해 확인. 실행한 명령과 날짜를 함께 적습니다 |
| **[문서]** | 배포받은 개발 문서 또는 기기의 설정 파일 인용. 파일 경로를 적습니다 |
| **[추정]** | 아직 확인하지 못한 해석·가설. **확인 방법을 함께 적습니다** |

**[추정]으로 표시된 항목을 확정 사실처럼 인용하지 말아 주세요.**

[실측]은 2026-08-30에 수행했고, **`(2026-08-31 추가)`로 표시한 항목은 다음 날 추가 확인한 것**입니다. 처음 `[추정]`으로 적었던 백엔드 원인은 같은 날 시험으로 **`[실측]` 확정**했습니다([3장](#3-원인-확정--백엔드-설정-해결)). 아직 `[추정]`으로 남은 것은 탑뷰 고양이 미검출의 원인([4-3](#4-3-한계-추정))과 몇몇 부수 항목입니다.

---

## 이 문서가 답하는 미결 항목

[의사결정 대장](../../docs/ai-integration-decisions.md)의 `P-00x` 중 아래 두 건에 **실측 증거를 제출합니다.**

| 항목 | 질문 | 이 문서의 답 | 근거 |
|---|---|---|---|
| **P-001** (P0) | 실제 카메라는 HD-3000인가 IMX500인가? | **HD-3000 확정.** 두 보드 모두에서 장치명 확인 | [1-5](#1-5-카메라-p-001-답) |
| **P-002** (P0) | 보드에 내장 YOLOv8 W8A8 모델·라벨·VisionService plugin이 실제 존재하는가? | **서비스는 Pi 5에만 존재. 모델·라벨 파일은 두 보드 모두 없음** | [1-2](#1-2-보드별-서비스-차이), [1-4](#1-4-없는-것) |

아래 두 건에는 **근거를 보탭니다.**

| 항목 | 질문 | 이 문서가 보태는 것 |
|---|---|---|
| **P-009** (P0) | 안전 판단을 Pi4로, 비전 추론을 Pi5로 맞바꾸는가? | [P-009 제안서](../../docs/decision/P-009-pi4-pi5-role-reallocation.md)는 **성능**을 근거로 듭니다. 여기에 **`VisionService1`이 Pi 4에 아예 없다**는 실측이 더해집니다 → [6장](#6-p-009에-대한-실측-근거) |
| **P-007** (P1) | Ultralytics AGPL-3.0 조건을 수용할 수 있는가? | 플랫폼 내장 모델이 YOLOv8·YOLO11이라는 [문서] 근거 → [5장](#5-어제-문서에서-수정이-필요한-부분) |

[ATLAS AI 검증 보고서](../../docs/archive/atlas-ai-decision-and-validation-report.md)와의 관계는 [7장](#7-atlas-ai-검증-보고서와의-관계)에 정리했습니다.

---

## 요약

**Pi 5에서 사람·고양이 검출이 실제로 동작하는 것을 확인했습니다.**

**기기 조사**

1. **Pi 5용 ATLAS 이미지는 존재하고 이미 돌고 있습니다.** [실측] → [1-1](#1-1-보드-두-대)
   [어제 문서](2026-08-29-atlas-platform-baseline.md)에서 최우선 멘토 문의로 올린 항목입니다. **해결됐으므로 그 문서를 고쳐야 합니다.**
2. **비전 서비스가 Pi 5에만 있습니다.** Pi 4에는 없습니다. [실측] → [1-2](#1-2-보드별-서비스-차이)
3. **플랫폼 검출 모델은 설정만 있고 파일이 없습니다.** [실측] → [1-4](#1-4-없는-것)
   **[회신 2026-09-03]** 이 부재는 배포 누락이 아니라 **원본 모델이 별도 제공 대상이 아니기 때문**입니다.

**시험 결과**

4. **공개 YOLOX로 대체하면 동작합니다.** 출력 텐서 이름과 후보 수(8400)가 설정과 일치합니다. [실측] → [2장](#2-시험-기록-yolox로-대체해보기)
5. **동작하지 않던 원인은 백엔드 설정이었습니다.** `NPU_QNN_HTP`(퀄컴 NPU 전용) → `GPU`로 바꾸자 검출이 나왔습니다. [실측] → [3장](#3-원인-확정--백엔드-설정-해결)

| 모델 | 결과 | 처리율 |
|---|---|---:|
| `person_detection` | 사람 검출 | 3.7 FPS |
| `obj_detect` | person·cat·bottle·laptop 등 동시 검출 | 3.8 FPS |

합격선 2 FPS를 넘습니다. **멘토에게 모델 파일을 받지 않아도 진행할 수 있습니다.**

**새로 발견한 리스크**

6. **고양이 검출이 각도에 좌우됩니다.** 같은 조건에서 **옆모습은 20초에 69회, 탑뷰는 0회**였습니다. [실측] → [4장](#4-고양이-검출--각도에-따라-성패가-갈립니다)
   계획된 카메라 설치가 **후드 탑뷰**이고 인식 실패 시 대비책도 탑뷰 화면 표시라, **두 전제가 함께 흔들립니다.**

---

## 1. 기기 조사

### 1-1. 보드 두 대

**[실측]** 두 기기에서 `cat /etc/os-release`, `uname -a`, `cat /proc/device-tree/model` 실행

| | Pi 4 | Pi 5 |
|---|---|---|
| OS 이름 | (os-release 출력 없음) | `NAME="ATLAS Platform"` |
| 버전 | — | `26.06.0-246.scarthgap.s6 (bootes)` |
| 커널 | `6.12.25-v8` | `6.12.25-v8-16k` |
| 하드웨어 | Raspberry Pi 4 | `Raspberry Pi 5 Model B Rev 1.1` |

**[실측]** `command -v python3`, `command -v apt opkg`, `ls -l /bin/sh` → 두 보드 모두 **Python 없음 / 패키지 관리자 없음 / BusyBox 기반**

**[실측]** `/etc`에 쓰기 시도 시 `Read-only file system`. `/data`와 `/tmp`는 쓰기 가능

**[문서]** 받은 API 명세 PDF 헤더의 빌드 번호는 `Build 246`이며, 위 [실측] Pi 5 버전 `26.06.0-246`과 동일합니다.
→ **[추정]** 명세 문서 헤더의 `raspberrypi4` 표기는 그 문서 세트의 빌드 타깃일 뿐이고 내용은 Pi 5에도 적용된다고 봅니다. **확인 방법:** 멘토에게 문서 적용 범위 질의.

### 1-2. 보드별 서비스 차이

**[실측]** 두 기기에서 `busctl list --no-pager | grep -o "com\.atlas\.[A-Za-z0-9]*" | sort -u`

| | Pi 4 | Pi 5 |
|---|---:|---:|
| `com.atlas.*` 서비스 수 | 30 | **53** |

**Pi 5에만 있는 것 (조사 목적과 관련된 것만):**
`VisionService1`, `VisionPipeline1`, `MediaPlayer1`, `MediaController1`, `WindowManager1`, `EcoaiAgentsOrchestrator1`, `EcoaiVectordb1`

**두 보드 모두 없는 것:** `VoiceManager1`, `VoiceConductor1`

**두 보드 모두 있는 것:** `Camera1`, `PeripheralManager1`, `NotificationManager1`, `TimedateManager1`, `ThinqClient1`, `AiCapabilityProvider1`

### 1-3. 플랫폼이 제공하는 비전 모델

**[실측]** Pi 5에서 실행

```
busctl call com.atlas.VisionService1 /com/atlas/VisionService1 \
  com.atlas.VisionService1 GetSupportedModels
```

반환된 4개:

| id | type | 입력 (설정값) |
|---|---|---|
| `face_recognition` | face recognition | RGB 480×360 |
| `face_recognition_float` | face recognition float | RGB 480×360 |
| `obj_detect` | object detection | RGB 640×480 |
| `person_detection` | person detection | RGB 640×480 |

**[문서]** Pi 5의 `/etc/vision-pipeline/models/person_detection.json` 및 `obj_detect.json`

| | `"model"` 값 | decode 옵션의 클래스 필터 |
|---|---|---|
| `person_detection` | `/data/ecoai/assets/models/object_detection/yolov11_det_w8a8.tflite` | `person_only#hard_classwise` |
| `obj_detect` | `.../yolov8_det_w8a8.tflite` | `all#hard_classwise` |

**[문서]** `person_detection.json`에서 그대로 인용:

```json
"output_tensor_names": ["boxes", "scores", "class_idx"]
"backend": { "type": "NPU_QNN_HTP", "accelerator": "QNN_HTP" }
```

decode 옵션: `640:480`, `0#80`(COCO 80종), `8400#100`(후보 8400개, 최대 100개 출력), 라벨 파일 `/data/ecoai/assets/labels/object_detection/coco_labels.txt`

**[실측]** 조회 메서드는 **설정 파일만 보고 목록을 만듭니다.** 아래 1-4에서 모델 파일이 없는데도 4개가 반환되는 것이 근거입니다. 목록에 나온다고 사용 가능한 것이 아닙니다.

**[회신 2026-09-03 결론]** 이 목록은 LG가 내부에서 동작을 시험한 모델 목록이고, 기기에서 실제로 쓸 수 있는 모델 전체를 뜻하지는 않습니다. **위 해석과 같습니다.** 다만 회신도 이 메서드의 정확한 의미를 단정하지 않았으므로 인터페이스 동작의 확정 근거로는 쓰지 않습니다.

### 1-4. 없는 것

**[실측]** Pi 5에서 `ls -R /data/ecoai/assets/models`, `find`로 확인

```
/data/ecoai/assets/models/object_detection/    ← 디렉터리 자체가 없었음
/data/ecoai/assets/labels/object_detection/    ← 없었음
```

실제로 있던 모델은 `face_recognition`(arc_face.onnx, scrfd_10g_320x180.onnx)과 `speaker_recognition`뿐이었습니다.

**[실측]** 그 상태에서 세션 시작 시 `journalctl -u vision-service` 출력:

```
Cannot find the model file[0] /data/ecoai/assets/models/object_detection/yolov11_det_w8a8.tflite
[ERROR] Failed to start VisionPipeline: [com.atlas.VisionPipeline1.StartFailed]
```

**→ P-002에 대한 답입니다.** 대장이 요구한 증거 중 **경로 목록과 실행 로그**를 제출합니다. 버전과 SHA-256은 파일 자체가 없어 산출할 수 없습니다.

### 1-5. 카메라 (P-001 답)

**[실측]** 두 보드에서 `for d in /sys/class/video4linux/*/name; do cat $d; done`

```
video0: Microsoft® LifeCam HD-3000
video1: Microsoft® LifeCam HD-3000
```

**→ P-001에 대한 답입니다. IMX500 AI Camera가 아니라 USB UVC 웹캠입니다.**

**[실측]** `Camera1` 서비스의 `DeviceList` 속성 = `["camera1"]`. 플랫폼에서 부르는 기기 이름은 `/dev/video0`이 아니라 **`camera1`** 입니다.

**[실측]** `Camera1.GetFormat` 반환값: `format=YUV, width=640, height=480, fps=30`

**[실측]** 단독 캡처 동작 확인 — Pi 4(2026-08-30 18:03), Pi 5(19:27) 양쪽에서 성공.
**[추정]** 캡처한 프레임의 색조가 청록으로 크게 치우쳐 있었습니다. 화이트밸런스 문제로 보이며 검출 정확도에 영향을 줄 수 있습니다. **확인 방법:** 조명 조건을 바꿔 재촬영하고, 모델이 도는 뒤 검출률과 대조.

> 카메라는 P-009 제안에 따라 **2026-08-30 Pi 5로 옮겼습니다.** 이 문서의 Pi 5 시험은 옮긴 뒤 수행한 것입니다.

### 1-6. 얼굴 인식 모델은 파일이 있어도 로드되지 않습니다 (2026-08-31 추가)

**[실측]** `face_recognition`은 모델 파일이 **실제로 존재**합니다.

```
/data/ecoai/assets/models/face_recognition/arc_face.onnx            174,387,950 bytes
/data/ecoai/assets/models/face_recognition/arc_face_quantized.bin    43,983,448 bytes
```

그런데 세션을 시작하면 실패합니다.

```
E tensordec-recogarcface.cc:741] arcface_setOption error. setFaceRecognitionParam failed
[ERROR] Failed to start VisionPipeline:
        Cannot find the extension. No plugin registered for model format: .bin
```

> **[회신 2026-09-03] 원인이 확인됐고, 저희 초기 해석은 틀렸습니다.**
>
> 회신 결론: ECO.AI 플러그인은 모두 `*.so`로 `/usr/lib/ecoai/strategy`에 정상 포함돼 있습니다. `*.bin`은 **퀄컴 전용 모델 형식**이라 **Pi 5에서 쓸 수 없고**, Pi 4·Pi 5에서 face_recognition을 쓰려면 **ONNX 모델**이 필요합니다.
>
> | | 저희 초기 해석 | 실제 (회신) |
> |---|---|---|
> | 원인 | `.bin` 플러그인이 **누락**됨 | 플러그인은 정상 포함. **`.bin`이 퀄컴 모델이라 Pi5에서 못 씀** |
> | 성격 | 배포 결함 | **결함 아님.** 하드웨어가 다른 것 |
>
> → 3장의 백엔드 문제와 **뿌리가 같습니다.** `NPU_QNN_HTP`(퀄컴 백엔드)와 `.bin`(퀄컴 모델)이 짝을 이루고, Pi5에는 둘 다 해당 하드웨어가 없습니다. **"별개 문제"로 본 저희 판단도 틀렸습니다.**
>
> **조치:** Pi5에서 얼굴 인식이 필요하면 onnx를 쓰는 **`face_recognition_float`** 를 사용합니다. 실제로 그쪽은 정상 동작했습니다.
>
> 근거: [LG 회신 검토](../임재범/2026-09-03-lg-answer-review.md)

| 모델 | 백엔드 | 모델 파일 | 결과 |
|---|---|---|---|
| `face_recognition` | `NPU_QNN_HTP` (퀄컴) | `.onnx` + **`.bin`(퀄컴 모델)** | **Pi5에서 사용 불가** |
| `face_recognition_float` | `GPU` | `.onnx` | ✅ 동작 (12초에 124건) |

**[미확인]** LG가 언급한 `/usr/lib/ecoai/strategy` 플러그인 목록은 확인하지 못했습니다(작성 시점에 기기 접속 불가). **확인 방법:** 기기에서 `ls /usr/lib/ecoai/strategy/`.

> **주의.** 이 시험은 처음에 잘못 측정했습니다. **다른 세션이 돌고 있는 상태에서 실행**해 실패 원인을 오판했고, 잔여 세션을 정리한 뒤 재시험해 위 로그를 얻었습니다. 절차는 [8-2](#8-2-기기-작업-시-주의-실측)에 있습니다.

---

## 2. 시험 기록: YOLOX로 대체해보기

> ## ⚠️ 이 장은 **시간 순 조사 기록**입니다
>
> **최종 결과가 아닙니다.** 2-4에서 "추론이 실행되지 않는다"고 나오지만, 그 원인을 [3장](#3-원인-확정--백엔드-설정-해결)에서 찾아 **해결했습니다.**
>
> **현재 상태: 사람·고양이 검출 모두 정상 동작합니다 (3.7~3.8 FPS).**
>
> 결과만 필요하시면 [3장](#3-원인-확정--백엔드-설정-해결)과 [4장](#4-고양이-검출--각도에-따라-성패가-갈립니다)을 보세요. 이 장은 "무엇을 어떻게 배제해 나갔는지"를 남긴 것입니다.

### 2-1. 배경

**[실측]** 공개 저장소(SDK의 `models/download_model.sh`가 사용하는 주소)에 HTTP HEAD 요청:

```
403  yolov11_det-tflite-w8a8.zip
403  yolo11_det-tflite-w8a8.zip
403  yolov11-tflite-w8a8.zip
200  yolox-tflite-w8a8.zip
```

→ 플랫폼이 쓰는 모델은 공개돼 있지 않고, **YOLOX는 받을 수 있습니다.**

**[문서]** SDK 예제 `ecoai-tutorial/ecoai-examples/yolox/yolox_ecoai_detector.cpp` 6~9행 주석:
> Expected outputs: "boxes" [1,N,4], "scores" [1,N], "class_idx" [1,N].

→ 1-3의 설정이 기대하는 텐서 이름과 동일합니다.

**[실측]** 내려받은 `yolox.tflite`(9.0MB)에 `strings` 실행 → `boxes`, `scores`, `class_idx`, `image` 문자열 확인

### 2-2. 모델 읽기: 성공

**[실측]** `/data/ecoai/assets/models/object_detection/`에 `yolox.tflite` 업로드 후, 설정이 찾는 이름으로 심볼릭 링크 생성. `/etc`가 읽기 전용이라 설정을 못 고쳐 이 방법을 썼습니다.

세션 시작 후 `journalctl -u vision-service` 출력:

```
ecoai_getTensorsInfo: tensor[boxes]     dimension[0]=4  dimension[1]=8400  dimension[2]=1
ecoai_getTensorsInfo: tensor[scores]    dimension[0]=8400  dimension[1]=1
ecoai_getTensorsInfo: tensor[class_idx] dimension[0]=8400  dimension[1]=1
```

→ 1-3의 설정값 `8400#100`과 후보 개수가 **정확히 일치**합니다. `Cannot find the model file` 오류는 사라졌습니다.

### 2-3. 카메라 연결: 성공

**[실측]** 처음에는 GStreamer `shmsink`로 직접 프레임을 공급했으나 실패:

```
streaming stopped, reason not-negotiated (-4)   (from: shmsrc0)
```

**[문서]** 실패한 파이프라인 문자열(로그에 출력됨)의 앞부분:
```
shmsrc socket-path=/tmp/vision_socket ! videoconvert ! videoscale ! video/x-raw,...
```
→ **[추정]** `shmsrc` 바로 뒤에 형식 지정이 없어 외부 공급자의 영상 형식을 알 수 없습니다. 그래서 외부 `shmsink` 공급 방식은 이 파이프라인과 맞지 않는다고 봅니다. **확인 방법:** 멘토에게 정식 프레임 공급 방식 질의.

**[실측]** 정식 경로로 성공한 순서:

```
busctl introspect com.atlas.Camera1 ... | grep DeviceList
  → .DeviceList  property  as  1 "camera1"        ← 기기 이름은 "camera1"

Camera1.Open("camera1")            → 핸들 반환
Camera1.GetFormat(핸들)            → format=YUV, 640x480, fps=30
Camera1.Start(핸들)
VisionService1.CreateVisionSession(model=person_detection, camera_handle=핸들, format=2, 640x480)
VisionService1.Start(세션id)       → 성공
```

**[실측]** `format` 값을 0~6으로 바꿔가며 시도한 결과:

| 경로 | 되는 값 | 실패 시 오류 |
|---|---|---|
| `camera_handle` 사용 | **2** | `Invalid camera format: 0` / `: 1` |
| `shmsrc` 사용 (실패 경로) | 1 | 2는 `format=(string)JPEG`로 해석돼 링크 실패 |

→ **경로에 따라 같은 숫자의 의미가 다릅니다.**

**[실측]** 기동 후 `journalctl` 출력: `Vision pipeline FPS: 30.0105` (약 3초 간격으로 반복, 안정적)

### 2-4. 추론: 이 시점에는 실행되지 않았음 → [3장에서 해결](#3-원인-확정--백엔드-설정-해결)

> **이 절의 "0건"은 백엔드를 고치기 전의 기록입니다.** 원인은 설정값이었고, 바꾼 뒤에는 정상 검출됩니다. 이 절을 단독으로 인용하지 마세요.

**[실측]** `busctl monitor`로 12~15초씩 3회 관찰 → `VisionResult` 시그널 **0건**. 카메라 위치를 두 번 변경했으나 동일.

**[실측] 판별 실험** — 사람만 거르는 `person_detection` 대신 80종 전부를 잡는 `obj_detect`로 동일 시험:

| 모델 | 클래스 필터 | 검출 수 |
|---|---|---:|
| `person_detection` | `person_only` | 0 |
| `obj_detect` | `all` (80종) | **0** |

→ 화면에 무엇이 있든 하나는 잡혀야 하는데 0입니다. **사람이 안 보여서가 아닙니다.**

**[실측]** `/proc/stat`의 `cpu` 행에서 user+nice+system 틱 차이 측정:

```
12초 동안 시스템 전체 CPU 증가: 152틱 = 1.52초   (4코어 기준 약 3%)
```

→ 30 FPS 추론이 실제로 돌면 CPU가 거의 포화돼야 합니다.

위 세 [실측]을 종합하면 **영상만 흘러가고 추론 단계가 실행되지 않는 상태**였습니다. 로그의 "30 FPS"는 카메라 영상 처리량이지 추론 횟수가 아닙니다.

> **이 판단은 당시 `[추정]`이었고, [3장](#3-원인-확정--백엔드-설정-해결)의 시험으로 `[실측]` 확정했습니다.** 원인은 백엔드 설정이었고 고친 뒤 CPU tick이 152 → 1,359로, 검출은 0건 → 정상으로 바뀌었습니다.

---

## 3. 원인 확정 — 백엔드 설정 (해결)

> **이 절은 처음에 `[추정]`으로 썼다가, 같은 날 시험으로 `[실측]` 확정했습니다.**

### 3-1. 원인

**[문서]** `person_detection.json` / `obj_detect.json`:

```json
"backend": { "type": "NPU_QNN_HTP", "accelerator": "QNN_HTP" }
```

**[실측] (2026-08-31 추가) 런타임이 이 이름을 deprecated로 경고합니다.**

```
W ecoai_adapter.cc:50] This backend name is deprecated: NPU_QNN_HTP, using htp_qnn instead.
```

→ `NPU_QNN_HTP`는 낡은 표기이며 런타임이 **`htp_qnn`으로 대체**해 실행합니다.

**[문서]** ECO.AI 레퍼런스 「백엔드 이름 표」: `htp_qnn`은 퀄컴 QNN HTP(NPU) 실행

**[실측]** Pi 5의 SoC는 `/proc/device-tree/compatible` = `raspberrypi,5-model-b brcm,bcm2712` 입니다. **퀄컴 NPU가 없습니다.**

즉 **이 백엔드 초기화에 실패하면서 오류를 내지 않고 프레임마다 건너뛰고 있었습니다.** 파이프라인은 30 FPS로 도니 겉으로는 정상처럼 보입니다.

**[실측] (2026-08-31 추가) 같은 이미지 안에서 백엔드 값이 갈립니다.** 아래는 저희가 **수정하지 않은 원본** 기준입니다.

| 설정 파일 | `"type"` |
|---|---|
| `face_recognition.json` | `NPU_QNN_HTP` |
| `face_recognition_float.json` | **`GPU`** |

→ **[회신 2026-09-03]** 이 혼재 자체의 배포 기준은 답변되지 않았습니다. 다른 모델 설정에도 같은 기본값이 남아 있는지 **전수 확인이 필요합니다.** **확인 방법:** [확인·결정 목록](2026-08-30-open-questions-and-decisions.md) `B-16`.

### 3-2. 확정 근거 — 세 단계로 좁혔습니다

**① 대조 실험 (시스템 수정 전).**
같은 기기의 `face_recognition_float.json`은 **[문서]** `Backend:GPU`를 쓰고, 그 모델 파일은 실제로 존재합니다. 이것으로 세션을 돌려봤습니다.

| | 사물 검출 (QNN) | **얼굴 인식 (GPU)** |
|---|---:|---:|
| 12초간 시스템 CPU tick | 152 | **4,563** |
| `VisionResult` 수신 | 0건 | **124건** |
| 로그 | 없음 | `{ "performance": 9.98639, "detectedFaces": [] }` |

→ **파이프라인·카메라 경로·신호 발행은 모두 정상**이며, 제 관측 방법도 맞았습니다. 문제는 사물 검출 경로에만 있습니다. 그리고 `GPU`가 이 기기에서 검증된 값임을 알 수 있습니다.

**② 설정 변경.** `/`를 잠시 쓰기 가능으로 바꾸고 두 파일의 백엔드만 고쳤습니다.

```diff
-    "type": "NPU_QNN_HTP",
-    "accelerator": "QNN_HTP",
+    "type": "GPU",
+    "accelerator": "GPU",
```

**③ 결과 [실측].**

| 모델 | 검출 | 처리율 |
|---|---|---:|
| `person_detection` | `detectedPeople: [{"name": "person", ...}]` | **3.7 FPS** |
| `obj_detect` | `detectedObjects` — person·bottle·laptop·potted plant 등 동시 검출 | **3.8 FPS** |

`performance` 값과 실제 신호 수신 횟수가 일치해 **FPS로 해석**했습니다 (얼굴 인식 9.99 ↔ 124건/12초, 사물 검출 3.8 ↔ 36건/12초).

**[검출 모델 통합 제안](2026-08-29-vision-model-consolidation.md)의 합격선이 2 FPS이므로 통과합니다.**

### 3-3. 이것이 바꾸는 것

**YOLOX 대체가 유효합니다.** 플랫폼 원본 모델(`yolov11_det_w8a8.tflite`) 없이 검출이 됩니다.
→ **멘토에게 모델 파일을 받지 않아도 진행할 수 있습니다.** [확인·결정 목록](2026-08-30-open-questions-and-decisions.md)의 문의 우선순위가 바뀝니다.

> **용어 주의.** 이 문서에서 "모델을 받아야 한다"는 것은 **플랫폼 원본인 YOLO11/YOLOv8 파일**을 말합니다. YOLOX는 이미 확보해 기기에 올려둔 대체품입니다.

---

## 4. 고양이 검출 — 각도에 따라 성패가 갈립니다

**이번 조사에서 가장 중요한 발견입니다.**

### 4-1. 실험

노트북 화면에 고양이 사진을 띄워 카메라 앞에 두고 `obj_detect`으로 관찰했습니다. **바꾼 변수는 사진 속 고양이의 각도 하나뿐**이며, 카메라·설정·거리·조명은 모두 동일했습니다.

| 사진 | 20초간 `cat` 검출 | 함께 검출된 것 |
|---|---:|---|
| **탑뷰** (위에서 내려다본 고양이) | **0회** | `laptop`, `person` |
| **옆모습** | **69회** | `laptop`, `person`, **`cat`** |

옆모습일 때 **[실측]** 결과:

```
cat     x:185.5  y:255.6  w:172.2  h:79.5
laptop  x:110.4  y:224.7  w:291.4  h:220.8
person  x:150.1  y: 83.4  w:366.5  h:375.3
```

고양이 박스가 **노트북 화면 전체가 아니라 화면 속 고양이 영역만** 잡았습니다. 모델이 화면 내용을 제대로 해석하고 있다는 뜻이며, 모아레·반사 때문에 못 잡는 것이 아님을 보여줍니다.

### 4-2. 왜 중요한가

**[문서]** [ATLAS AI 검증 보고서](../../docs/archive/atlas-ai-decision-and-validation-report.md)의 대상 환경: *"후드 고정 탑뷰"*
**[문서]** 주차별 실행 계획의 대비책: *"인식 실패 시 태블릿에 고양이 사진·영상 표시"*

**두 전제가 모두 탑뷰에서 흔들립니다.** 계획된 카메라 설치가 탑뷰이고, 인식 실패 시 대비책도 탑뷰에서는 같이 실패합니다.

→ 탑뷰를 유지하면 `DetectCutOff`(반려동물 차단) 시연이 성립하지 않을 수 있습니다.

### 4-3. 한계 [추정]

이 실험은 **화면에 띄운 사진 한 장**입니다. 실물 고양이나 인형이 탑뷰에서 어떻게 나오는지는 확인하지 못했습니다.

**[추정]** COCO 학습 데이터의 고양이는 대부분 옆모습·정면이라 탑뷰가 학습 분포를 벗어난 것으로 봅니다.
**확인 방법:** 실물 인형을 탑뷰·측면에서 각각 촬영해 같은 방식으로 비교. 그리고 검출 임계값(**[문서]** `obj_detect.json`의 `0.4#0.55#0.5#0.4`, 첫 값이 score 0.4)을 낮췄을 때 잡히는지 확인.

### 4-4. 대응 후보

| 안 | 내용 | 확인 필요 |
|---|---|---|
| 1 | 카메라를 **비스듬히** 달아 측면 성분 확보 | 조리면 전체가 화각에 들어오는지 |
| 2 | 탑뷰 유지 + **탑뷰 데이터로 미세조정** | 학습·양자화·배포 공수 |
| 3 | 검출 임계값 하향 | 오검출 증가 폭 |

`P-003`(ROI 침범 판정 방식)과 카메라 최종 설치 위치가 이 결과에 직접 걸립니다.

---

---

## 5. 어제 문서에서 수정이 필요한 부분

**[ATLAS 플랫폼 기준](2026-08-29-atlas-platform-baseline.md)**

| 대상 | 조치 | 근거 |
|---|---|---|
| 「★ Pi 5용 ATLAS 이미지가 제공되는지」 | **해결됨으로 내림** | [1-1](#1-1-보드-두-대) [실측] |
| 비전 서비스 유무 | **보드마다 다르다**는 사실 추가 | [1-2](#1-2-보드별-서비스-차이) [실측] |

**[검출 모델 통합 제안](2026-08-29-vision-model-consolidation.md)**

| 대상 | 조치 | 근거 |
|---|---|---|
| 통합 대상 모델 | EcoAI YOLOX → **플랫폼 제공 모델**로 변경 | [1-3](#1-3-플랫폼이-제공하는-비전-모델) [실측] |
| 라이선스 근거 | **재검토 필요** | 아래 |
| 사람+고양이 동시 검출 | `person_detection`이 아니라 **`obj_detect`**을 써야 함 | [1-3] [문서] `person_only` vs `all` |

라이선스 건: 어제 문서에 *"YOLOX는 Apache-2.0이라 AGPL 문제가 해소된다"* 고 적었으나, 플랫폼이 제공하는 것은 YOLOv8·YOLO11입니다. **[추정]** 이들은 Ultralytics 계열이라 AGPL-3.0일 가능성이 높습니다. 다만 플랫폼이 배포하는 것을 사용하는 경우 우리 배포 의무와는 상황이 다를 수 있습니다. **확인 방법:** 멘토에게 제공 모델의 라이선스 표기 질의.

---

## 6. P-009에 대한 실측 근거

[P-009 제안서](../../docs/decision/P-009-pi4-pi5-role-reallocation.md)는 비전 추론을 Pi 4 → Pi 5로 옮기자고 제안하며, 근거로 **성능**을 듭니다.

> 비전 성능 | Pi4 A72 1.5GHz. 합격선 2 FPS | Pi5 A76 2.4GHz. 여유 확보

**여기에 성능과 무관한 근거를 하나 더합니다.**

**[실측]** `VisionService1`이 **Pi 4에는 없습니다**([1-2](#1-2-보드별-서비스-차이)). Pi 5에만 있습니다.

즉 Pi 4에서 비전을 하려면 플랫폼 서비스를 쓸 수 없고 ECO.AI를 직접 빌드해 올리는 길밖에 없습니다. **성능이 아쉬운 문제가 아니라 경로 자체가 다릅니다.**

| 선택지 | 플랫폼 서비스 | 추가 확인 |
|---|---|---|
| **비전을 Pi 5로** (P-009 B안) | **사용 가능** | Pi 5 부하 여유 |
| 비전을 Pi 4에 유지 (A안) | **사용 불가** — 서비스 부재 | **[실측]** Pi 4 `/usr/lib`에 `libopencv_*`가 보이지 않음. ECO.AI 예제가 OpenCV를 링크하므로 실행 가능 여부 확인 필요 |
| Pi 4에도 Video 프로파일 이미지 | 가능해짐 | 제공 여부를 멘토에게 확인해야 함 |

**[추정]** 팀장 방침이 "매뉴얼을 최대한 활용"이라면 B안이 그 방침에 가장 부합합니다. **확인 방법:** 회의에서 방침과 대조.

> **참고.** 이 문서의 Pi 5 시험은 P-009가 제안한 배선(카메라를 Pi 5로)에서 수행했습니다. 결과적으로 **B안 구성이 실제로 동작 가능하다는 것까지는 확인**됐습니다. 다만 추론 자체는 3장의 이유로 아직 돌지 않았습니다.

---

## 7. ATLAS AI 검증 보고서와의 관계

[ATLAS AI 의사결정 및 통합 검증 보고서](../../docs/archive/atlas-ai-decision-and-validation-report.md)(이수현)는 단계적 전략을 제시합니다.

> **Baseline:** 타깃 Atlas OS의 내장 `VisionService` 및 내장 모델(`YOLOv8 W8A8`)을 보드에서 원형 그대로 우선 실측합니다.
> **Fallback:** **내장 artifact가 부재하거나** 탑뷰 성능 기준을 충족하지 못할 경우, 외부 경로로 전환하여 비교 검증합니다.

**이 문서의 실측이 그 분기점에 답합니다.**

| 보고서의 전제 | 실측 결과 |
|---|---|
| 내장 `VisionService` | **Pi 5에 있음.** Pi 4에는 없음 ([1-2](#1-2-보드별-서비스-차이)) |
| 내장 모델 `YOLOv8 W8A8` | **설정은 있으나 파일이 없음** ([1-4](#1-4-없는-것)) |
| 대상 환경 "Raspberry Pi 4 (4GB)" | 비전은 Pi 4에서 불가. **[회신 2026-09-03 결론] 확정** — Pi 4(Headless)에서는 비전 AI를 쓸 수 없다 |

→ **Baseline 단계를 현재 상태로는 실행할 수 없습니다.** 보고서가 정의한 Fallback 조건("내장 artifact가 부재")이 이미 충족됐습니다.

~~다만 **[추정]** 모델 파일만 확보되면 Baseline이 되살아납니다.~~

**[회신 2026-09-03] 이 추정은 반증됐습니다. Baseline은 되살아나지 않습니다.** 두 이유가 겹칩니다.

1. 원본 모델 파일은 **별도 제공 대상이 아닙니다.** 배포 누락이 아니므로 "확보"할 대상이 없습니다.
2. 더 근본적으로 **`VisionService` 인터페이스 자체가 RPi5 미지원**입니다. 모델이 있어도 Baseline 경로는 공식 경로가 아닙니다.

**따라서 Fallback(외부 경로)이 유일한 경로입니다.** 단 그 외부 경로도 LG 권장 형태 — video profile 이미지 + OpenCV 직접 추론, 또는 nnstreamer — 로 맞춰야 합니다. → [LG 회신 검토](../임재범/2026-09-03-lg-answer-review.md)

보고서가 제안한 외부 경로 후보(`YOLOv8n LiteRT W8A32` / `W8A8`)와 이 문서의 YOLOX 시험은 **같은 Fallback 갈래**입니다. 중복 작업이 되지 않도록 회의에서 조율이 필요합니다.

---

## 8. 재현 방법

**[실측]** 아래 명령으로 이 문서의 내용을 재확인할 수 있습니다.

```
# 기기에 어떤 서비스가 있는지
busctl list --no-pager | grep -o "com\.atlas\.[A-Za-z0-9]*" | sort -u

# 지원한다고 하는 비전 모델 (설정 기준)
busctl call com.atlas.VisionService1 /com/atlas/VisionService1 \
  com.atlas.VisionService1 GetSupportedModels

# 카메라 기기 이름
busctl introspect com.atlas.Camera1 /com/atlas/Camera1 com.atlas.Camera1 | grep DeviceList

# 카메라 단독 동작 확인 (비전 세션이 안 떠 있을 때)
gst-launch-1.0 -q v4l2src device=/dev/video0 num-buffers=1 \
  ! videoconvert ! jpegenc ! filesink location=/tmp/test.jpg

# 실패 원인
journalctl -u vision-service --no-pager -n 40
```

### 8-1. 검출 세션을 띄우는 순서 [실측]

**이 순서를 지켜야 동작합니다.** 하나라도 빠지면 오류 없이 결과만 안 나옵니다.

```
1. Camera1.Open("camera1")                    → handle
2. Camera1.SetFormat(handle, JPEG 640x480 30) ← ★ 필수
3. Camera1.Start(handle)
4. VisionService1.CreateVisionSession(model, camera_handle=handle, format=2, 640x480)
5. VisionService1.Start(vision_id)
```

**2번이 핵심입니다.** 카메라 기본값이 `YUV`인데 비전 세션은 `format=2`(JPEG)를 기대합니다. 안 맞으면 공유메모리에 데이터가 들어오지 않고 다음 오류만 반복됩니다.

```
[ERROR] Timeout! No data available to read from eventfd
[ERROR] shared memory read fail
```

파이프라인은 "started successfully"로 뜨기 때문에 **겉으로는 정상처럼 보입니다.** 처음 시험할 때 이전 세션이 카메라를 JPEG로 남겨둔 덕에 우연히 동작했고, 재부팅 후 재현되지 않아 원인을 찾는 데 시간이 걸렸습니다.

### 8-2. 기기 작업 시 주의 [실측]

- **시험 전에 잔여 세션이 없는지 확인하세요.** 다른 비전 세션이 돌고 있으면 새 세션의 `Start`가 실패하고, CPU 측정값도 그 세션 것이 섞입니다. **실제로 이것 때문에 `face_recognition` 시험을 한 번 오판했습니다.**
  ```
  journalctl -u vision-service --no-pager -n 5 | grep result   # 결과가 흐르면 세션이 살아 있음
  # 유휴 상태의 시스템 CPU 증가는 12초에 100 안팎입니다
  ```
- **`camera.service`를 재시작하지 마세요.** 공유메모리 FD 연결이 끊겨 `Camera1.Start`가 `Unknown error`로 실패하고, `vision-service`를 함께 재시작해도 복구되지 않습니다. **재부팅이 유일한 복구 수단이었습니다.**
- **`/etc` 파일을 수정하면 소유자가 `root:root`로 바뀝니다.** `vision-service`는 `vision` 계정으로 돌아 파일을 못 읽고, `[ModelRegistry] Failed to parse` → `Invalid model ID`로 이어집니다. 수정 후 반드시 되돌리세요.
  ```
  chown vision:vision <파일> && chmod 640 <파일>
  ```
- **카메라를 `Camera1`이 잡고 있으면 `gst-launch`로 열 수 없고, 반대도 마찬가지입니다.** 그래서 **검출 결과와 원본 프레임을 동시에 얻을 수 없습니다.** 화면에 겹쳐 보려면 `result_type=1`(VIDEO_OVERLAY) 출력을 받는 앱이 필요합니다. 이 문서의 검증 이미지는 세션을 멈춘 뒤 따로 캡처해 좌표를 그린 것이라 **엄밀한 동시 촬영이 아닙니다.**
- BusyBox라 `head -5`가 아니라 **`head -n 5`**. **`timeout` 명령이 없습니다**
- **`pkill -f <문자열>`은 그 문자열이 포함된 자기 SSH 명령까지 죽입니다.** `pidof`로 PID를 찾아 끄세요 (실제로 두 번 겪었습니다)
- `busctl call`에 서명이 필요한 메서드는 서명을 **한 덩어리로** 넘깁니다. `SetFormat ia{sv} <handle> 4 ...`, `Capture iis ...` 형식입니다

---

## 9. 기기에 남긴 것

**[실측] Pi 5는 현재 검출이 동작하는 상태로 두었습니다.** 원복하면 다시 동작하지 않게 되므로 유지했습니다.

**① 추가한 파일 (`/data`, 순수 추가)**

```
/data/ecoai/assets/models/object_detection/yolox.tflite              9.0MB  (실제 파일)
/data/ecoai/assets/models/object_detection/yolov11_det_w8a8.tflite → yolox.tflite
/data/ecoai/assets/models/object_detection/yolov8_det_w8a8.tflite  → yolox.tflite
/data/ecoai/assets/labels/object_detection/coco_labels.txt          80줄
```

설정을 고칠 수 없던 시점에 **모델에 원래 이름표를 링크로 붙인 것**입니다. 라벨 파일은 SDK의 `ecoai-tutorial/common/coco/coco80_classes.h`에서 추출했습니다. `person`=1행(인덱스 0), `cat`=16행(인덱스 15).

**② 수정한 플랫폼 설정 (`/etc`)** — 팀 확인 필요

```
/etc/vision-pipeline/models/person_detection.json
/etc/vision-pipeline/models/obj_detect.json
    "type"/"accelerator": NPU_QNN_HTP/QNN_HTP  →  GPU/GPU
```

소유자는 `vision:vision` 640으로 되돌렸고, **`/`는 다시 읽기 전용으로 잠갔습니다.**

**③ 기기에 남긴 설명 파일**

`/data/backup-20260830/README.txt` — 무엇을 왜 바꿨는지, 소유자 함정, 되돌리는 절차를 적어 두었습니다. `/etc`를 열어본 팀원이 문서와 다른 값을 보고 혼란스럽지 않도록 하기 위해서입니다.

```
/data/backup-20260830/
├── README.txt
├── original-qnn/     ← 플랫폼 원본
└── working-gpu/      ← 현재 동작 중인 설정
```

**되돌리려면:**

```
mount -o remount,rw /
cp /data/backup-20260830/original-qnn/*.json /etc/vision-pipeline/models/
chown vision:vision /etc/vision-pipeline/models/*.json
chmod 640 /etc/vision-pipeline/models/*.json
mount -o remount,ro /
systemctl restart vision-service
```

> **플랫폼 시스템 파일을 수정한 상태입니다.** 이대로 유지할지 팀 확인이 필요합니다.

---

## 10. 미확인 사항

> **해결된 항목** — ~~백엔드를 바꾸면 추론이 도는지~~([3장](#3-원인-확정--백엔드-설정-해결)), ~~YOLOX가 유효한 대체품인지~~(유효함), ~~기기 처리율~~(3.7~3.8 FPS)

| 항목 | 확인 방법 | 담당 | 우선순위 |
|---|---|---|---|
| **탑뷰에서 실물 고양이·인형이 검출되는지** | 인형을 탑뷰·측면에서 각각 촬영해 비교 ([4-3](#4-3-한계-추정)) | 김병직·이수현 | **최우선** |
| **카메라 최종 설치 각도** | 위 결과에 따라 결정. **검출 성패가 걸린 항목이며, 발끝 기준 ROI 판정의 성립 여부도 함께 걸립니다** | 김병직 | **최우선** |
| 검출 임계값을 낮추면 탑뷰가 잡히는지 | `obj_detect.json`의 `0.4#0.55#0.5#0.4` 첫 값 하향. `/etc` 수정이라 remount 필요 | 김병직 | 높음 |
| 플랫폼 설정 수정을 유지할지 | 팀 결정 ([9장](#9-기기에-남긴-것)) | 팀 | 높음 |
| 검출 정확도(Recall·오검출률) | 시험 데이터로 측정. 처리율은 확인 완료 | 김병직 | 중간 |
| 플랫폼 제공 모델의 라이선스 | 멘토 질의 (`P-007`) | 미정 | 중간 |
| 비전을 어느 보드에서 돌릴지 | 팀 결정 (`P-009`, [6장](#6-p-009에-대한-실측-근거)) | 팀 | 중간 |
| Pi 4에 OpenCV 런타임이 있는지 | `P-009`에서 A안을 고를 때만 필요 | 미정 | 낮음 |
| 화면 오버레이로 실시간 확인하는 방법 | `result_type=1` 출력을 받는 앱 필요 ([8-2](#8-2-기기-작업-시-주의-실측)) | 미정 | 낮음 |

---

## 11. 근거 목록

**직접 접속 (2026-08-30)**
- Pi 4 `root@<Pi4 IP>`
- Pi 5 `root@<Pi5 IP>`

**기기 내 파일 (Pi 5)**
- `/etc/vision-pipeline/models/person_detection.json`
- `/etc/vision-pipeline/models/obj_detect.json`
- `journalctl -u vision-service` 로그

**SDK 소스 (개발 PC, `Ro92n/AI-Native-OS-Samples/ecoai-tutorial/`)**
- `ecoai-examples/yolox/yolox_ecoai_detector.cpp` — 출력 텐서 규격
- `common/yolox/yolox_detection_helpers.h` — 후처리 방식
- `common/coco/coco80_classes.h` — 클래스 인덱스
- `models/download_model.sh` — 모델 배포 주소

**배포받은 개발 문서** (기밀 표기 있음 — 원문은 저장소에 올리지 않고 문서명만 인용)
- ECO.AI 레퍼런스 「백엔드 이름 표」, 「모델 입출력 명세」
- `com.atlas.VisionService1`, `com.atlas.Camera1` D-Bus 명세

**팀 문서**
- 설계도 ② 전 부품 도착 후 최종 데모 구성 V4.0 (2026-08-13)
- [ATLAS 플랫폼 기준](2026-08-29-atlas-platform-baseline.md)
- [검출 모델 통합 제안](2026-08-29-vision-model-consolidation.md)
