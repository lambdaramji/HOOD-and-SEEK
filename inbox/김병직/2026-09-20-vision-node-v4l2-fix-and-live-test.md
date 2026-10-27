# jaebeom PR #1(`pi4/vision/`) V4L2 직접 캡처로 수정 — Pi5 실기 라이브 카메라 검증 성공

- 작성자: 김병직
- 작성일: 2026-09-20
- 상태: draft
- 카테고리: vision-ai
- 관련 이슈/PR: [jaebeom/HOOD-SEEK#1](https://github.com/jaebeom/HOOD-SEEK/pull/1) 후속. 코드 수정은 `jaebeom/HOOD-SEEK`의 `fix/atlas-native-camera-io` 브랜치(로컬 커밋 완료, PR 오픈은 별도 승인 대기)
- 조사 대상: Pi 5 `<Pi5 IP>` (SSH 직접 접속)

## 표기 규칙

| 표기 | 뜻 |
|---|---|
| **[실측]** | 기기·코드에서 직접 실행해 확인 |
| **[문서]** | 개발 문서·설정 파일·소스 인용 |
| **[추정]** | 아직 확인하지 못한 해석·가설 (확인 방법 병기) |

## 이 문서가 답하는 미결 항목

- [jaebeom PR #1 실기 검증](2026-09-20-vision-node-pi5-runtime-gap.md)이 남긴 "OpenCV videoio/imgcodecs 부재로 실행 불가" 문제를 해소한다.
- [확인·결정 목록](2026-08-30-open-questions-and-decisions.md) **B-20**(videoio/imgcodecs 배포 여부 문의)은 여전히 LG 쪽 확인이 필요하지만, **우리 쪽에서 우회할 수 있는 방법을 찾아 실기 동작을 확인**했으므로 긴급도를 낮출 수 있다.

## 요약

1. **PR 코드가 실제로 카메라를 쓰던 방식(`cv::CAP_V4L2`)을 그대로 살려, OpenCV의 videoio/imgcodecs 모듈 자체를 링크하지 않도록 고쳤다.** 리눅스 V4L2 API를 직접 호출하는 코드로 카메라 캡처를 대체했다. 필요한 라이브러리는 전부 이미 기기에 있어 추가 설치가 필요 없다. [확인된 사실](#확인된-사실-실측)
2. **Pi5 실기에서 실제 카메라로 라이브 테스트에 성공했다.** 사람이 카메라 앞에 서자 `person_detected`가 실제로 `true`로 바뀌고, 연속 프레임 조건을 채운 뒤 `presence:present`로 전환됐다. 중간의 한 프레임짜리 오검출에도 상태가 흔들리지 않았다. [시험 기록](#시험-기록)
3. **카메라가 실제로 180도 뒤집혀 마운트되어 있었다.** 실측 사진으로 확인했고, 물리적 설치 방향이 아직 확정되지 않아 하드코딩 대신 `--rotate 180` 옵션을 추가했다. [시험 기록](#시험-기록)
4. **`--image`/`--export-input`/`--annotated-output`은 여전히 이 기기에서 못 쓴다.** `imgcodecs` 자체가 없기 때문이며, 이번 수정은 그 경로를 명확한 에러로 막아둔 것이지 해결한 것은 아니다. [남은 문제](#남은-문제-추정과-확인-방법)

## 확인된 사실 [실측]

### 코드가 이미 V4L2만 쓰고 있었다

**[문서]** `jaebeom/HOOD-SEEK`(`feat/person-pet-vision`) `pi4/vision/src/main.cpp`의 기존 카메라 오픈 코드:

```cpp
capture.open(options.at("--" + kind), kind == "camera" ? cv::CAP_V4L2 : cv::CAP_ANY)
```

카메라 경로는 이미 `cv::CAP_V4L2`만 요청하고 있어 GStreamer·FFmpeg가 실제로 필요하지 않았다. 문제는 `libopencv_videoio.so`가 빌드 시점에 GStreamer·FFmpeg·libgphoto2 지원까지 함께 컴파일되어 있어, **실행 시점에 그 백엔드들을 한 번도 안 써도 동적 링커가 해당 라이브러리 전부를 열려고 시도**하기 때문이었다(ELF의 `DT_NEEDED`는 심볼 단위가 아니라 라이브러리 단위로 즉시 로드된다).

`--image`/`--export-input`이 쓰는 `cv::imread`/`cv::imwrite`도 마찬가지로 `imgcodecs`가 필요 없는 클래스(jpg)만 다루지만, `libopencv_imgcodecs.so`에 `libtiff` 지원이 같이 빌드되어 있어 tiff를 한 번도 안 읽어도 로드 자체가 막혔다.

### 수정 내용

- 새 파일 `pi4/vision/src/v4l2_camera.{hpp,cpp}`: 리눅스 V4L2 API(`open`/`ioctl`/`mmap`)를 직접 호출하는 `V4L2Camera` 클래스. YUYV로 받아 ITU-R BT.601 공식으로 BGR 변환한다. 필요한 건 커널 API와 `libc`뿐이라 추가 라이브러리가 필요 없다.
- `pi4/vision/src/main.cpp`: `--camera` 경로를 `cv::VideoCapture` 대신 `V4L2Camera`로 교체. `--image`/`--video`/`--export-input`/`--annotated-output`은(이 기기에 `imgcodecs`가 없으므로) `HOODSEEK_ECOAI` 빌드에서 명확한 `runtime_error`로 막는다.
- `pi4/vision/CMakeLists.txt`: `opencv_videoio`/`opencv_imgcodecs` 링크를 `HOODSEEK_ECOAI`가 꺼져 있을 때(PC/CTest 빌드)만 하도록 분리. **PC 쪽 `validate_photos.py` 등 기존 검증 파이프라인은 영향받지 않는다** — 그 경로는 `--export-input`을 쓰고, PC 빌드는 여전히 `imgcodecs`를 링크한다.
- **[실측]** 수정 후 빌드의 `readelf -d` NEEDED 목록: `libopencv_imgproc.so.409`, `libopencv_core.so.409`, `libecoai-inference.so.1`, `libstdc++.so.6`, `libgcc_s.so.1`, `libc.so.6`, `ld-linux-aarch64.so.1` — **`videoio`·`imgcodecs`가 완전히 빠졌고 나머지는 전부 Pi5에 실제로 있는 것들이다.**

### 카메라가 180도 뒤집혀 있었다

**[실측]** 현장에서 후드 대용 태블릿 거치대에 카메라를 매단 상태로 한 프레임을 raw PPM(무압축, 코덱 불필요)으로 뽑아 확인했다. 태블릿 몸체가 프레임 위쪽에, 옆에서 작업 중이던 팀원이 아래쪽에 나와 실제 배치와 반대였다. 최종 후드 설치 시 카메라 방향이 이것과 같다는 보장이 없어 하드코딩하지 않고, `V4L2Camera` 생성자와 `--rotate 180` CLI 옵션으로 필요할 때만 `cv::rotate(frame, frame, cv::ROTATE_180)`을 적용하도록 했다. 회전 보정 후 다시 뽑은 프레임에서 태블릿이 아래, 사람이 위로 정상 확인됐다.

## 시험 기록

| # | 명령 | 결과 |
|---:|---|---|
| 1 | `atlas-dev` 도커에서 수정한 `pi4/vision`을 `-DHOODSEEK_ECOAI=ON -DBUILD_TESTING=OFF`로 재빌드 | 빌드 성공, `readelf -d`로 videoio/imgcodecs 제거 확인 |
| 2 | 수정 바이너리를 Pi5 `/data/hoodseek-vision/`에 배포, `--camera /dev/video0 --model .../yolox.tflite --source pi5 --backend cpu` 6초 실행 | `libecoai-tflite-strategy.so` 로드 로그, 실제 프레임 13개 처리, `presence` 정상 전환·`SIGINT`에 `reason:"stopped"`로 종료 |
| 3 | 사람이 카메라 앞에 20초간 서서 라이브 실행 | `person score 0.65` → 2프레임 연속 조건 충족 후 `presence:present`. 이후 한 프레임 `cat`(score 0.33) 오검출에도 `present` 유지. 76프레임 평균 처리 313.7ms(**약 3.2 FPS**, Pi5 CPU/XNNPACK) |
| 4 | 압축 없는 PPM 프레임 덤프 도구(`dump_frame.cpp`, `v4l2_camera.cpp` 재사용)로 원본 프레임 확인 | 상하 반전 확인. 사진 근거는 세션 산출물로 보존(저장소에는 포함하지 않음) |
| 5 | `--rotate 180` 추가 후 같은 방식으로 재확인 | 반전 해소 확인 |

## 남은 문제 [추정]과 확인 방법

- **[추정]** `--rotate 180`이 최종 실제 후드 설치에서도 필요한 값인지는 아직 모른다. 오늘 확인한 건 **임시 태블릿 거치대의 장착 방향**뿐이다. **확인 방법:** 최종 카메라 브래킷이 확정된 뒤 같은 raw 프레임 덤프로 재확인.
- **[추정]** `--image`/`--export-input`/`--annotated-output`을 이 기기에서 쓰려면 결국 `imgcodecs`가 필요하다. 가벼운 대안(직접 libjpeg 헤더를 프로젝트에 벤더링하거나, PNG로 대체)은 검토했으나 이번 수정 범위에 넣지 않았다. **확인 방법:** 실제 정지 사진 시연이 데모에 필요한지부터 팀에서 확인하고, 필요하면 별도 작업으로 진행.
- **[추정]** YUYV 640x480 고정 해상도가 이 카메라의 최적 설정인지 확인하지 않았다. **확인 방법:** 다른 해상도로 `VIDIOC_S_FMT`를 요청해 카메라가 몇 가지 모드를 지원하는지 확인.

## 재현 방법

```bash
# jaebeom/HOOD-SEEK의 fix/atlas-native-camera-io 브랜치(또는 동일 diff)를 받아
# atlas-dev 도커에서 재빌드
docker run --rm -v "<repo>:/app" atlas-dev:latest bash -lc '
  source /opt/atlas-sdk-x86_64/environment-setup-armv8a-atlas-linux
  cmake -S /app/pi4/vision -B /tmp/build -DHOODSEEK_ECOAI=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
  cmake --build /tmp/build -j"$(nproc)"
  readelf -d /tmp/build/hoodseek_vision | grep NEEDED'

# Pi5 배포 및 실행
scp /tmp/build/hoodseek_vision root@<PI5_IP>:/data/hoodseek-vision/
ssh root@<PI5_IP> '/data/hoodseek-vision/hoodseek_vision --camera /dev/video0 --rotate 180 \
  --model /data/ecoai/assets/models/object_detection/yolox.tflite \
  --source pi5 --backend cpu --threads 2 --config config.example.json \
  --output demo.jsonl'
```

## 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| 최종 후드 설치 방향에서 `--rotate` 값 재확인 | 브래킷 확정 후 raw 프레임 덤프 재확인 | 병직 |
| `--image`/`--export-input`을 이 기기에서도 쓸지 여부와 방법 | 팀 논의 후 결정 | 미정 |
| jaebeom PR #1에 이 수정 반영 여부 | `fix/atlas-native-camera-io` 브랜치 push·PR 오픈 승인 | 병직 |
| 다른 해상도/포맷에서의 동작 | `VIDIOC_ENUM_FRAMESIZES`로 지원 목록 확인 | 미정 |

## 근거

- [jaebeom PR #1 실기 검증](2026-09-20-vision-node-pi5-runtime-gap.md) — 이 문서가 해소하는 선행 문제
- [jaebeom/HOOD-SEEK#1](https://github.com/jaebeom/HOOD-SEEK/pull/1)
- [확인·결정 목록](2026-08-30-open-questions-and-decisions.md) B-20
