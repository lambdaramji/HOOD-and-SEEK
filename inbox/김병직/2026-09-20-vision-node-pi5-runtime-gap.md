# jaebeom PR #1(`pi4/vision/`) 실기 검증 — Pi5 OpenCV videoio/imgcodecs 부재로 실행 불가

- 작성자: 김병직
- 작성일: 2026-09-20
- 상태: draft
- 카테고리: vision-ai
- 관련 이슈/PR: [jaebeom/HOOD-SEEK#1](https://github.com/jaebeom/HOOD-SEEK/pull/1) (이 저장소 [PR #30](https://github.com/yujh5537/HOOD-SEEK/pull/30)은 이 PR로 대체되어 CLOSED 상태)
- 조사 대상: Pi 4 `<Pi4 IP>`, Pi 5 `<Pi5 IP>` (SSH 직접 접속, tailscale 네트워크 경유)

## 표기 규칙

| 표기 | 뜻 |
|---|---|
| **[실측]** | 기기·코드에서 직접 실행해 확인 |
| **[문서]** | 개발 문서·설정 파일·소스 인용 |
| **[추정]** | 아직 확인하지 못한 해석·가설 (확인 방법 병기) |

## 이 문서가 답하는 미결 항목

- [확인·결정 목록](2026-08-30-open-questions-and-decisions.md) **B-10** — "Pi 4와 Pi 5 모두 OpenCV 런타임이 있나요?" (담당: 병직, 확인 방법으로 `python3 -c "import cv2"` / `pkg-config --modversion opencv4` 제안됨). **이 문서로 부분 종료** — 아래 "이전 문서 수정 필요 사항" 참고.

## 요약

1. **PR의 코드는 ATLAS SDK로 정상 크로스 컴파일된다.** [실측 — 시험 기록 1](#시험-기록)
2. **Pi5의 `yolox.tflite` 해시가 PR이 PC에서 검증한 모델 해시와 정확히 일치한다.** PR의 "Pi 실제 모델 해시 확인" 미검증 항목 하나가 이걸로 해소된다. [실측 — 시험 기록 2](#시험-기록)
3. **그런데 바이너리가 Pi5에서 실행되지 않는다.** ATLAS 이미지에는 `libopencv-core`/`libopencv-imgproc`만 opkg로 설치되어 있고, PR 코드가 요구하는 `libopencv_videoio`·`libopencv_imgcodecs`와 그 종속 라이브러리 전체가 없다. **코드 결함이 아니라 플랫폼 패키지 구성 문제다.** [실측 — 시험 기록 3~5](#시험-기록)
4. **카메라(Microsoft LifeCam HD-3000, USB UVC)는 Pi4·Pi5 어느 쪽에 꽂아도 즉시 인식된다.** 다만 ECO.AI 런타임과 모델 파일은 Pi5에만 있어 두 보드의 카메라·런타임이 분리되면 테스트가 불가능하다. [실측 — 시험 기록 0](#시험-기록)
5. **Pi4는 애초에 ECO.AI 런타임을 전혀 갖고 있지 않다.** `inbox/김병직/2026-08-29-atlas-platform-baseline.md`의 「Pi 4(Headless)에서는 비전 AI를 쓸 수 없다」는 기존 결론과 일치한다.

## 확인된 사실 [실측]/[문서]

### 보드 배치와 역할

- **[실측]** `<Pi4 IP>` = `Raspberry Pi 4 Model B Rev 1.2`, `<Pi5 IP>` = `Raspberry Pi 5 Model B Rev 1.1`. 둘 다 `NAME="ATLAS Platform"`, `VERSION="26.06.0-246.scarthgap.s6 (bootes)"`로 동일 빌드.
- **[실측]** 시험 시작 시점에 카메라(Microsoft® LifeCam HD-3000, USB UVC, `uvcvideo` 드라이버)는 Pi5의 `/dev/video0`/`/dev/video1`에 연결되어 있었다. 이 문서 작성 중 현장에서 카메라를 Pi4로 옮겼다가 다시 Pi5로 옮기는 시험을 했고, **양쪽 다 재부팅 없이 즉시 `uvcvideo`가 인식**했다(`dmesg`에 `Found UVC 1.00 device Microsoft® LifeCam HD-3000` 로그 확인).
- **[실측]** Pi4에는 CSI 카메라 커널 모듈(`unicam`, `imx*`, `ov5647` 계열)이 전혀 로드되어 있지 않다. `lsmod`에 `bcm2835_codec`/`bcm2835_v4l2`(ISP/코덱 헬퍼)만 있고 센서 드라이버가 없다 — CSI 카메라 자체가 물려있지 않다는 뜻이며, USB UVC 카메라와는 무관하다.
- **[실측]** Pi4에는 ECO.AI 관련 파일이 `/opt/tests/atest/ptest/com.test.EcoaiAgentsOrchestrator1.xml` 테스트 매니페스트 하나만 있고, `/usr/lib/ecoai/`, `.tflite` 모델, `vision-service` 유닛 모두 없다. `journalctl -u vision-service`도 빈 결과. → 기존 결론(Headless 프로파일에 Vision AI 미포함)과 일치.
- **[실측]** Pi5에는 `/usr/lib/ecoai/strategy/`에 `libecoai-tflite-strategy.so`, `libecoai-onnx-strategy.so`, `libecoai-executorch-strategy.so`가 있고, `/data/ecoai/assets/models/object_detection/`에 `yolox.tflite`, `yolov8_det_w8a8.tflite`(symlink→yolox), `yolov11_det_w8a8.tflite`(symlink→yolox)가 있다. `object_detection` 라벨 파일 80개 클래스 순서 확인 — `person`=0, `cat`=15, `dog`=16으로 PR 문서의 서술과 일치.

### 빌드

- **[실측]** `atlas-dev:latest` 도커 이미지(`~/Ro92n/atlas_docker_env/Dockerfile`에 정의, ATLAS SDK `26.06.0-unofficial.scarthgap.s6-6-ai` 내장) 안에서 `jaebeom/HOOD-SEEK`의 `feat/person-pet-vision` 브랜치(`pi4/vision/`)를 `-DHOODSEEK_ECOAI=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release`로 크로스 컴파일 → **성공**. Clang 20.1.1, `opencv4` pkg-config로 4.9.0 인식, `ecoai-inference` pkg-config로 1.0.1 인식.
- **[실측]** 생성된 바이너리: `ELF 64-bit LSB pie executable, ARM aarch64, ... for GNU/Linux 5.15.0`. SHA-256 = `841cd6a86a7c872b087da3b4e7b99644e1c67b4e2c55d85a5cbe6cc2f3042010`.
  - PR 문서(`pi4/vision/개발_검증_ATLAS_시연_가이드.md` 6.2절)가 기록한 해시는 `0c517e0a195c237389855eec757b5e651bf69ba0f4dd1ba0420a4f8815151116`로 **다르다.** **[추정]** 디버그 심볼·빌드 경로·타임스탬프 등 비결정적 빌드 메타데이터 차이일 가능성이 높고, 소스가 다르다는 뜻은 아닐 수 있다. **확인 방법:** `strip` 후 `.text` 섹션만 비교하거나, PR 작성자가 쓴 정확한 cmake 옵션·SDK 빌드 날짜를 재현해 다시 비교.
- **[실측]** 모델 해시 일치 — Pi5의 `/data/ecoai/assets/models/object_detection/yolox.tflite` sha256 = `ad392c6e80edc5dd2a6e0464acdf25062c3d8e5fb6d005362a18b86ada5781c0`. PR 5절이 기록한 PC 검증 모델 해시와 **정확히 동일.**

### 실행 실패 — 라이브러리 부재

- **[실측]** 체크섬 검증(`sha256sum -c SHA256SUMS`) 통과 후 실행 시:
  ```
  ./hoodseek_vision: error while loading shared libraries: libopencv_videoio.so.409: cannot open shared object file: No such file or directory
  ```
- **[실측]** Pi5에 opkg로 설치된 OpenCV 관련 패키지는 `libopencv-core`, `libopencv-imgproc` 둘뿐이다(`/var/lib/opkg/info/libopencv-*.control` 기준). `libopencv-videoio`, `libopencv-imgcodecs` 패키지 자체가 없다.
- **[실측]** ATLAS SDK 자체(`/opt/atlas-sdk-x86_64/sysroots/armv8a-atlas-linux`)에는 `libopencv_videoio.so.4.9.0`, `libopencv_imgcodecs.so.4.9.0`이 존재해 **버전은 동일(4.9.0)**하다. `readelf -d`로 확인한 두 라이브러리의 NEEDED 목록:
  - `libopencv_videoio.so`: `libgstbase/gstreamer/gobject/glib/gstapp/gstriff/gstpbutils/gstvideo/gstaudio`(GStreamer 8종) + `libavcodec.so.60/libavformat.so.60/libavutil.so.58/libswscale.so.7`(FFmpeg) + `libgphoto2.so.6/libgphoto2_port.so.12`
  - `libopencv_imgcodecs.so`: `libjpeg.so.62`, `libwebp.so.7`, `libpng16.so.16`, `libtiff.so.6`
- **[실측]** Pi5에는 `libjpeg.so.62`, `libpng16.so.16`은 이미 있다(다른 패키지의 종속성으로 보임). `libwebp`는 SDK sysroot에서 구했으나, **`libtiff.so.6`은 ATLAS SDK 전체를 뒤져도 어디에도 없다** (`find /opt/atlas-sdk-x86_64/sysroots -iname "libtiff*"` 결과 없음). GStreamer 8종·FFmpeg 4종·libgphoto2 2종도 SDK/기기 어디에도 없다.
- **결론:** `videoio`(카메라 캡처)는 GStreamer+FFmpeg+libgphoto2까지 통째로 없어 수작업 복사로 해결할 범위가 아니다. `imgcodecs`(사진 파일 읽기/쓰기)는 `libtiff`가 SDK에도 없어 막힌다. **`--camera`, `--image` 두 실행 경로 모두 지금 이 이미지에서는 실행 자체가 안 된다.**

## 시험 기록

| # | 명령 | 결과 |
|---:|---|---|
| 0 | `ssh root@<IP> 'ls -la /dev/video*'`, `dmesg \| grep -i camera` (Pi4/Pi5 각각) | 카메라는 UVC로 양쪽 다 즉시 인식. Pi4는 CSI 센서 모듈 없음 |
| 1 | `docker run --rm -v jaebeom-vision:/app/jaebeom-vision atlas-dev:latest bash -lc 'source .../environment-setup-armv8a-atlas-linux && cmake -S pi4/vision -B build -DHOODSEEK_ECOAI=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build build'` | 빌드 성공, aarch64 ELF 생성 |
| 2 | `ssh root@<Pi5 IP> sha256sum /data/ecoai/assets/models/object_detection/yolox.tflite` | PR 문서 기록 해시와 일치 |
| 3 | `scp -r vision-kit/. root@<Pi5 IP>:/data/hoodseek-vision/` 후 `sha256sum -c SHA256SUMS` | 전송 무결성 확인 |
| 4 | `./hoodseek_vision --image ... --model ... --source pi5 --backend cpu ...` (라이브러리 경로 지정 없이) | `libopencv_videoio.so.409` 로드 실패 |
| 5 | SDK sysroot에서 `libopencv_videoio.so.409`·`libopencv_imgcodecs.so.409`를 `/data/hoodseek-vision/libs/`에 복사(시스템 설치 아님) 후 `LD_LIBRARY_PATH=/data/hoodseek-vision/libs ./hoodseek_vision ...` | `libavformat.so.60` 로드 실패로 진행 중단 |
| 6 | `readelf -d`로 videoio/imgcodecs의 NEEDED 전수 확인, `libtiff.so.6`을 SDK 전역에서 재검색 | 없음 확인. 여기서 중단 |

## 남은 문제 [추정]과 확인 방법

- **[추정]** 이 ATLAS 이미지의 OpenCV 빌드가 원래 `videoio`/`imgcodecs`를 지원하지 않도록(또는 별도 배포 경로로) 의도된 것인지, 아니면 배포 누락인지 불명확하다. **확인 방법:** LG에 "Headless/Video 프로파일 opkg 피드에 `libopencv-videoio`/`libopencv-imgcodecs`(및 GStreamer/FFmpeg/libgphoto2/libtiff)가 존재하는가" 재문의.
- **[추정]** OpenCV를 `videoio`/`imgcodecs` 없이 다시 빌드(즉 카메라는 V4L2 raw로 직접 열고, 사진 디코딩은 별도 경량 라이브러리로 대체)하면 우회가 가능할 수 있다. **확인 방법:** PR 코드가 `opencv_videoio`/`opencv_imgcodecs`에 실제로 의존하는 범위(단순 `VideoCapture`/`imread`/`imwrite` 호출 지점)를 확인하고, V4L2 직접 캡처로 대체하는 공수를 추정.
- **[추정]** 빌드 해시 불일치(`841cd6a8...` vs PR 기록 `0c517e0a...`)는 비결정적 빌드 산출물 차이로 보이나 확정하지 않았다. **확인 방법:** 위 "빌드" 절 참고.

## 이전 문서 수정 필요 사항

| 문서 | 현재 내용 | 고쳐야 할 내용 |
|---|---|---|
| [확인·결정 목록](2026-08-30-open-questions-and-decisions.md) `B-10` | "Pi 4와 Pi 5 모두 OpenCV 런타임이 있나요?"가 미답으로 남아 있고 확인 방법이 `python3 -c "import cv2"` | **Pi5에 OpenCV는 있으나 `core`/`imgproc`뿐**이라고 갱신. 제안된 확인 명령(`python3`, `pkg-config`)은 **기기에 둘 다 없어 실행 불가**하므로 `ldconfig -p \| grep opencv` + `find / -iname libopencv*`로 교체 제안. 새 질문(videoio/imgcodecs 배포 여부)은 신규 B 항목으로 분리 필요 |
| jaebeom/HOOD-SEEK#1 PR 본문·가이드 | "Pi 접속 후 남은 확인 항목"에 모델 해시 확인만 있고 라이브러리 존재 여부는 별도 항목 없음 | 라이브러리 부재로 실행 자체가 안 된다는 내용을 PR에 코멘트로 추가 필요 (별도 조치 예정) |

## 재현 방법

```bash
# 1. PR 브랜치 확보 (읽기 전용, 별도 스크래치 디렉터리에)
gh repo clone jaebeom/HOOD-SEEK <dir> -- --branch feat/person-pet-vision --depth 1

# 2. atlas-dev 이미지로 크로스 빌드
docker run --rm -v "<dir>:/app/jaebeom-vision" atlas-dev:latest bash -lc '
  source /opt/atlas-sdk-x86_64/environment-setup-armv8a-atlas-linux
  cmake -S /app/jaebeom-vision/pi4/vision -B /tmp/build-vision \
    -DHOODSEEK_ECOAI=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
  cmake --build /tmp/build-vision -j"$(nproc)"'

# 3. Pi5에 배포 후 라이브러리 부재 재현
scp /tmp/build-vision/hoodseek_vision root@<Pi5 IP>:/data/hoodseek-vision/
ssh root@<Pi5 IP> '/data/hoodseek-vision/hoodseek_vision --image x --model /data/ecoai/assets/models/object_detection/yolox.tflite --source pi5 --backend cpu --config x'
# → error while loading shared libraries: libopencv_videoio.so.409: cannot open shared object file

# 4. opkg 패키지 목록으로 videoio/imgcodecs 부재 확인
ssh root@<Pi5 IP> 'ls /var/lib/opkg/info/libopencv-*.control'
# → libopencv-core.control, libopencv-imgproc.control만 존재
```

## 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| ATLAS opkg 피드에 `libopencv-videoio`/`libopencv-imgcodecs` 및 종속 패키지가 존재하는지 | LG 재문의 (신규 B 항목으로 상정) | 병직 |
| PR 코드가 `videoio`/`imgcodecs` 없이도 동작하도록 재작성 가능한 범위 | `pi4/vision/src/` 내 `cv::VideoCapture`/`cv::imread`/`cv::imwrite` 호출 지점 확인 | PR 작성자(수현/Codex)와 협의 필요 |
| 빌드 해시 불일치 원인 | "남은 문제" 절 참고 | 미정 |
| 카메라 실제 라이브 스트림 기반 present/absent 전환, FPS/지연/RSS/온도 측정 | 위 라이브러리 문제가 해소된 뒤 진행 | 병직 |

## 근거

- [jaebeom/HOOD-SEEK#1](https://github.com/jaebeom/HOOD-SEEK/pull/1) 및 `pi4/vision/개발_검증_ATLAS_시연_가이드.md` (커밋 시점 기준)
- [yujh5537/HOOD-SEEK#30](https://github.com/yujh5537/HOOD-SEEK/pull/30) (CLOSED, #1로 대체)
- [ATLAS 플랫폼 기준](2026-08-29-atlas-platform-baseline.md)
- [Pi 4 · Pi 5 실기기 조사와 사람 인식 시험 기록](2026-08-30-device-survey-and-vision-test.md)
- [확인·결정 목록](2026-08-30-open-questions-and-decisions.md) B-10
- [LG 회신 검토](../임재범/2026-09-03-lg-answer-review.md)
- 로컬 ATLAS SDK: `~/Ro92n/atlas_docker_env/` (`atlas-dev:latest` 도커 이미지, `atlas-sdk-x86_64-armv8a-generic_arm64-toolchain-26.06.0-unofficial.scarthgap.s6-6-ai.sh`)
