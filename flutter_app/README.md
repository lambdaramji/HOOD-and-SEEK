# Flutter 대시보드 (Pi 5)

SafetyCore(Pi 4)의 `state`를 받아 1920×1080 화면에 그리고, 사용자 터치를 `ui_cmd`로 보내는 앱이다. **판정하지 않는다.** 색·문구는 state의 enum과 `reasons` 코드를 옮긴 매핑이고, 임계값 비교·잠금 해제 판단은 앱에 없다 (`lib/screens/labels.dart` 1-2행, 설계서 §5.4).

화면은 실제 LG 인덕션이 연결된 앱처럼 「인덕션」·「가열 중」으로 쓰지만, 연결된 쿡탑은 ESP32 LED 판 **시뮬레이터**다(불변식 ⑧, D-047).

- 화면 설계: [UI 설계 v1](../docs/2026-10-24-flutter-ui-design-v1.md), 목업 [`docs/ui-mockup/`](../docs/ui-mockup/)
- 계약: [`contracts/README.md`](../contracts/README.md) — state·ui_cmd·hello
- 경고음: [`assets/audio/README.md`](assets/audio/README.md)

## 실행

```bash
cd flutter_app
flutter pub get
flutter run -d chrome          # 데모 화면 (웹은 늘 데모)
```

기기 빌드 값은 `--dart-define`으로 준다 (`lib/main.dart`).

| 이름 | 기본 | 뜻 | 비었을 때 |
|---|---|---|---|
| `CORE_HOST` | 빈 값 | SafetyCore 주소 | **데모 화면**으로 뜬다 |
| `CORE_PORT` | 7420 | SafetyCore 포트 | |
| `FONT_DIR` | 빈 값 | LG 글꼴·로고·배경을 읽을 기기 폴더 | Noto Sans KR, 로고 없음, 기본 배경 |
| `AUDIO_SINK` | `pulsesink` | GStreamer 출력 요소 | 기본 출력(Pi 5는 HDMI) |
| `CAMERA_URL` | `http://127.0.0.1:7431/` | 카메라 카드 영상(vision-node `--preview-port`, #144) | 영상 없이 그림과 판정 칩만 |
| `HOOD_CARE_FILE` | `/data/hoodseek/app/hood-care.json` | 기록 탭 후드 케어 누적 파일(UI 설계 v1 §3.3 C) | 쓸 수 없으면 「저장하지 못해요 · 앱을 켠 뒤만 누적」. 폴더는 root가 `mkdir -p /data/hoodseek/app && chmod 0777 /data/hoodseek/app`로 한 번 만든다(앱 계정이 설치마다 바뀜) |
| `HISTORY_DB` | `/data/hoodseek/app/hoodseek.db` | 기록 탭 DB — SQLite 파일 하나, 서버 없음. 타임라인 줄·끝난 조리 요약을 남기고 켤 때 읽어 온다 | 기기 `libsqlite3.so.0`을 FFI(`sqlite3` 패키지)로 연다. 못 열면 앱 메모리로만 세고 상단에 「앱을 켠 뒤 기록」. 폴더는 `HOOD_CARE_FILE`과 같음 |

- 웹 빌드는 TCP·소리·LG 글꼴을 쓰지 않는다(조건부 import stub). `CORE_HOST`를 줘도 데모다.
- Pi 5 패키징 (#69): 아래 「Pi 5에 올리기」. ATLAS 엔진은 Flutter 3.27.4로 고정이고, CI는 최신 stable이다. 시험은 두 버전에서 모두 통과하게 쓴다(의미 트리 대신 위젯 값을 본다 — `test/dashboard_screen_test.dart`).
- 주소·sink 이름은 저장소에 적지 않는다.

### Pi 5에 올리기

[실측 10/25 정인, 마스터 PC `atlas-dev`, 배포 기록 #67]. 저장소 `flutter_app/`에는 ATLAS 플랫폼 폴더가 없어서 빌드 전에 한 번 만든다.

```bash
# atlas-dev 안. 빌드 폴더에는 flutter_app/과 함께 contracts/·profiles/도 둔다
#   (core_state_test·labels_test가 ../contracts/examples·../profiles/default.json을 읽어서, 빼면 시험 38개가 실패)
source /opt/atlas-sdk-x86_64/environment-setup-armv8a-atlas-linux
flutter-atlas create --no-pub --platforms atlas --project-name hood_seek flutter_app
rm flutter_app/test/widget_test.dart          # create가 만드는 기본 시험
cd flutter_app                                 # 빌드 전에 앱 시험을 돌린다(10/25 99개 통과)
flutter-atlas build atlas --ipk --release --target-sysroot="$SDKTARGETSYSROOT" \
  --dart-define=CORE_HOST=<Pi 4 주소> --dart-define=FONT_DIR=<기기 폴더>
arc install build/atlas/arm64/release/ipk/<ipk 파일> -d rpi5
```

- 실행은 Pi 5 홈 화면 아이콘으로 한다. `arc run -s com.atlas.app.hood_seek -d rpi5`는 「Service not found」를 내지만 앱은 뜬다.
- 바꾸기 전 `libapp.so`를 백업하고 sha256을 #67에 남긴다(10/25 배포는 `/data/hoodseek/app-bak-<해시>`).
- 경고음 출력 장치는 ATLAS AudioManager 활성 출력(`usb_speaker`)이 정한다 — `deploy/README.md`.
- 화면이 1024×768로 잡히면(배선을 바꾸거나 화면을 나중에 켰을 때 EDID 0바이트) `deploy/README.md` 「Pi 5 화면 1080p EDID」 절차. `window-manager`를 재시작하면 홈도 꺼져서 `arc run -s com.atlas.app.home -d rpi5`로 다시 켠다. 터치는 허브 USB 선이 Pi 5에 꽂혀 있어야 잡힌다.

### FONT_DIR에 둘 파일

LG 브랜드 파일은 저장소에 넣지 않는다(공개 저장소, 설계 v1 §2.3). 기기 폴더에 아래 이름 그대로 둔다 (`lib/brand_fonts.dart`).

- `LGEITextTTF-{Light,Regular,SemiBold,Bold}.ttf` → 글꼴 `LG EI Text`
- `LGEIHeadlineTTF-{Thin,Light,Regular,Semibold}.ttf` → 글꼴 `LG EI Headline`
- `lge_logo_kr_heritagered_grey_rgb.png` → 상단 바·시작 화면 로고 (높이 128로 줄여 읽음)
- `LG_Electronics_Gradient_02_RGB.jpg` → 시작 화면 배경 (폭 1920으로 줄여 읽음)

빠진 파일은 로그 한 줄만 남기고 대체한다. 기동 전 확인:

```bash
PI5=<Pi 5 주소> FONT_DIR=<기기 폴더> flutter_app/tool/check_brand_fonts.sh   # 다 있으면 「LG files ok」
```

대체 글꼴 Noto Sans KR은 KS X 1001 2,350자만 남긴 subset이다(`pubspec.yaml`). 그 밖의 한글은 빈칸으로 나올 수 있다.

## 구조

| 경로 | 하는 일 |
|---|---|
| `lib/main.dart` | dart-define을 읽고 TCP 또는 데모 피드를 고른 뒤 LG 파일을 읽고 앱을 띄운다 |
| `lib/app.dart` | `MaterialApp`, 글꼴, 홈 = `DashboardScreen` |
| `lib/data/tcp_feed.dart` | SafetyCore TCP 클라이언트 |
| `lib/data/line_framer.dart` | `\n` 단위 줄 자르기, 한 줄 4096바이트 상한 |
| `lib/data/core_state.dart` | state 계약 파서·직렬화, enum |
| `lib/data/ui_command.dart` | `FAN_SET`·`IND_SET`(0..3)·`ACK_ALERT` |
| `lib/data/kitchen_feed.dart` | 피드 인터페이스, 데모 피드와 리허설 장면 |
| `lib/screens/dashboard_screen.dart` | 최상위 화면: 끊김·시작 타이머, 경고 창 고르기, 경고음, 그래프 표본, 탭·도크 |
| `lib/screens/home_view.dart` | 홈: 카메라·헤드라인·게이지·인덕션·후드·음성 카드 |
| `lib/screens/sensor_view.dart` | 센서 탭: 그래프 2장(60초/10분/1시간), 최근 일, 센서 상태 |
| `lib/screens/history_view.dart`, `history_log.dart` | 기록 탭: 조리 리듬(A)·지켜 준 기록(⑥)·필터 타임라인. 앱을 켠 뒤 받은 state만 메모리에 |
| `lib/screens/hood_care_card.dart`, `lib/data/hood_care*.dart` | 기록 탭 후드 케어(C): 가중 가동 누적·청소 권장, JSON 저장(웹은 stub) |
| `lib/screens/overlays.dart` | 경고 창, 해제 조건, 기기 시트, 음성 결과, 토스트, 시작 화면 |
| `lib/screens/labels.dart` | state → 배지·헤드라인·게이지 문구·사유 이름 |
| `lib/widgets/hs.dart` | 디자인 토큰(`Hs`, `Tone`), 공용 위젯, 동작 줄이기 |
| `lib/audio/` | 경고음 재생기(`gst-launch-1.0`), 웹·시험용 무음 |
| `lib/brand*.dart` | FONT_DIR 읽기, 웹 stub |

### SafetyCore 연결 (`lib/data/tcp_feed.dart`)

- 접속하면 `hello`(seq 0, `node:"flutter-dashboard"`)를 보내고, 연결마다 seq를 다시 센다. 보내는 봉투는 `v` 1, `src:"ui"`.
- **끊김은 소켓이 아니라 state 간격으로 본다.** state가 3 s 안 오면 소켓을 버리고 1 s 뒤 다시 붙는다(FIN 없는 단절, F3). 연결 대기 2 s, Linux에서는 `TCP_USER_TIMEOUT` 2 s — 케이블이 빠진 동안 쌓인 `ui_cmd`가 나중에 집행되지 않게 한다.
- 형식이 틀린 줄, seq가 거꾸로 간 줄, 4096바이트를 넘는 줄은 버리고 화면은 그대로 둔다.
- 계약에 없는 필드는 무시하고, 모르는 `reasons` 코드는 원문 그대로 보여 준다. 필드 사이의 의미 검사는 SafetyCore 몫이다.
- state `v`: 계약대로 받는다. state v2(#116, 잠금 해제 남은 초)는 #119에서 v1·v2를 둘 다 받게 한다. Pi 5 앱을 Pi 4 Core보다 먼저 올려도 끊기지 않는다.

### 화면 규칙

- 시작 화면은 첫 state가 오고 1.5 s가 지나면 넘어간다. 켜고 8 s 안에 state가 없으면, 또는 받은 뒤 3 s 끊기면 「연결 끊김」이다. 끊긴 동안 마지막 값을 지금 값처럼 그리지 않는다.
- 경고 창은 한 번에 하나, 우선순위 고장 > 해제 대기 > 위험 > 반려동물 > 자리 비움 (`_wanted`, 설계 v1 §4.3). 사용자가 닫은 창은 조건이 풀릴 때까지 다시 안 뜬다. 재실 중 온도 경고는 창 없이 카드만 바뀐다.
- 기기 시트: 인덕션은 지금보다 높은 단을 `allowed_on`일 때만, 후드는 `fan.min` 아래를 못 누른다(불변식 ②). 끄기·내리기는 늘 보낼 수 있다.
- 「위험 확인」 버튼은 `ACK_WAIT`일 때만 나오고 `ACK_ALERT`만 보낸다. 잠금은 Core가 푼다(불변식 ⑨).
- `vision.fresh=false`면 사람·고양이를 그리지 않는다(불변식 ③). 「모든 센서 작동 중」은 fresh ∧ 열화상 ∧ 미세먼지가 다 정상일 때만.
- `IND_NOACK`·`FAN_NOACK`은 위험·잠금·경고 표시를 덮지 않는다.
- 카메라 카드: vision-node가 Pi 5 루프백으로 내보내는 MJPEG(`CAMERA_URL`, #152 뒤 최대 약 16 FPS)를 그린다. **표시 전용** — 칩·테두리는 계속 state 값으로만 정한다. 왼쪽 위는 프레임을 받은 시각 「● 실시간 HH:MM:SS」(#151), 2 s 프레임이 없으면 점이 회색·흐리게 + 「영상 일시 중지」, 한 장도 못 받으면 그림. 데모(`CORE_HOST` 없음)·웹에서는 영상을 쓰지 않는다(설계 v1 §3.2).
- 경고음: `alert`가 `NONE` 아닌 값으로 바뀔 때 `<alert>.wav`를 한 번. 끊기면 `OFFLINE.wav` 한 번(앱 쪽 이름, 계약 alert 아님).

### 데모 피드

`CORE_HOST`가 비면 데모다. 마지막 장면을 0.5 s마다 다시 보내고, 화면 위쪽 리허설 칩으로 장면을 바꾼다. 장면은 모두 계약 파서를 통과시킨 손으로 쓴 state다(`kitchen_feed.dart`). **누른 명령은 기록만 하고 state에 반영하지 않는다** — 데모에서 버튼을 눌러도 화면 값은 그대로다.

## 시험

CI(`.github/workflows/flutter.yml`)와 같은 순서로 돌린다.

```bash
cd flutter_app        # core_state_test가 ../contracts/examples를 읽는다
flutter pub get
dart format lib test && git diff --exit-code -- lib test
flutter analyze       # info 경고도 실패
flutter test
flutter build web
```

| 시험 | 보는 것 |
|---|---|
| `core_state_test.dart` | 계약 예제 전부 파싱·왕복, invalid 예제 거부 |
| `tcp_feed_test.dart` | hello seq 0, 잘못된 줄·거꾸로 간 seq 버림, 재접속, 3 s 무수신 재접속 |
| `line_framer_test.dart` | 줄 경계, 쪼개진 UTF-8, 상한 넘은 뒤 회복 |
| `dashboard_screen_test.dart` | state → 화면 매핑, 불변식 ②③⑨, 끊김, 음성, 경고음 규칙, 로고 |
| `gst_clip_player_test.dart` | 경고음 재생기 (가짜 프로세스) |
| `brand_fonts_test.dart` | FONT_DIR 파일이 없을 때 |
