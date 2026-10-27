# 서드파티 구성 요소

Team Ro92n이 작성한 코드와 문서는 [MIT 라이선스](LICENSE)입니다(D-064). 아래 외부 구성 요소는 각자의 라이선스를 따르며, 이들에는 MIT 라이선스가 적용되지 않습니다.

## 저장소에 들어 있는 것

| 구성 요소 | 위치 | 출처 | 라이선스 | 원문 |
|---|---|---|---|---|
| nlohmann/json 3.11.3 (단일 헤더) | `safety_core/third_party/nlohmann/json.hpp` | <https://github.com/nlohmann/json> | MIT | 파일 머리 `SPDX-License-Identifier: MIT` |
| MLX90640 API | `safety_core/third_party/mlx90640/` | pimoroni/mlx90640-library 커밋 `a92229a`(Melexis 라이브러리 포크), 수정 없음 | Apache-2.0 | 같은 폴더 `LICENSE`, [`README.md`](safety_core/third_party/mlx90640/README.md) |
| Noto Sans KR (Regular·Medium·Bold) | `flutter_app/assets/fonts/` | Noto CJK 글꼴 | SIL Open Font License 1.1 | `flutter_app/assets/fonts/OFL.txt` |

**안내 음성과 차임은 팀이 만든 파일이라 MIT입니다.** 음성(`flutter_app/assets/audio/`, `results/`)은 팀이 Qwen3-TTS 1.7B(VoiceDesign·Base)로 합성했고 사람 목소리를 녹음·복제하지 않았습니다. 합성 모델의 라이선스(Apache-2.0) 전문은 `flutter_app/assets/audio/MODEL-LICENSE.txt`, 제작 기록은 [`README.md`](flutter_app/assets/audio/README.md)·[`results/README.md`](flutter_app/assets/audio/results/README.md)에 있습니다. 차임(`CAUTION`·`APP_START`·`WARNING`·`TOUCH_FEEDBACK`)은 `make_chimes.py`로 Python 표준 라이브러리만 써서 만들었습니다.

## 빌드·실행·학습에 쓰지만 저장소에 넣지 않은 것

라이선스는 각 배포처 기준입니다.

| 구성 요소 | 쓰는 곳 | 비고 |
|---|---|---|
| Flutter SDK, Dart 패키지 `sqlite3`, `flutter_lints`(개발용) | `flutter_app/` | `flutter_app/pubspec.yaml` |
| PlatformIO `espressif32@6.10.0` 플랫폼(Arduino-ESP32 코어·ESP-IDF) | `firmware/esp32_cooktop/`, `firmware/esp32_fan_controller/` | 두 `platformio.ini`의 `platform`·`framework = arduino` |
| FastLED 3.9.16 | `firmware/esp32_cooktop/` | PlatformIO가 빌드 때 받음(`platformio.ini`) |
| OpenCV, nlohmann_json, sdbus-c++ 2.0 | `vision_node/`, `safety_core/`(D-Bus 빌드) | ATLAS SDK·시스템 패키지 |
| whisper.cpp, sherpa-onnx | `voice_node/` | 빌드 때 경로 지정(`WHISPER_SOURCE_DIR`, `SHERPA_ONNX_ROOT`) |
| SenseVoiceSmall·Silero VAD 모델 | `voice_node/` | 기기에만 둠 |
| Qwen3-TTS 1.7B(VoiceDesign·Base), mlx-audio | 안내 음성 제작(Mac) | 앱에 넣지 않음. 모델 라이선스 전문 `flutter_app/assets/audio/MODEL-LICENSE.txt` |
| YOLOX(Megvii-BaseDetection) 학습 코드, 사전학습 가중치 `yolox_s.pth`·`yolox_x.pth` | `vision_node/tools/train/` | [`tools/train/README.md`](vision_node/tools/train/README.md) 절차에서 받음. 미세조정은 `yolox_s.pth`에서 시작, `yolox_x.pth`는 라벨 미리 채우기용 |
| COCO 2017 val 이미지·주석 | `vision_node/tools/train/` | 학습 데이터와 떼어 둔 회귀 평가용 |
| PyTorch, onnx, onnx2tf, TensorFlow 2.19 등 pip 패키지 | `vision_node/tools/train/` | `tools/train/README.md`의 설치 명령 |
| v3e 고양이·사람 검출 가중치 | `vision_node/` | YOLOX-S 사전학습 가중치에서 미세조정, 학습 데이터에 COCO 포함. 사람 사진으로 학습해 저장소·Release에 넣지 않음. sha256과 보관 위치는 [`deploy/README.md`](deploy/README.md) 파일 표 |
| LG ATLAS SDK·ECO.AI 런타임, LG 글꼴·브랜드 자산 | 전체 | LG 제공 자료라 저장소에 넣지 않음(D-047·D-063, `FONT_DIR`) |
| jsonschema 4.23.0 | `contracts/validate.py` | 계약 검증 도구(CI) |

쿡탑은 실제 인덕션이 아니라 ESP32와 LED 판으로 만든 **시뮬레이터**입니다(불변식 ⑧).
