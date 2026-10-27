# 디스플레이 2종 배선·동작·터치 확인 — 현장 가이드

- 작성자: 정정인
- 작성일: 2026-09-20
- 상태: 디스플레이 1·2 검증 완료 · **v3**
- 카테고리: [test-calibration-demo](../archive/category/test-calibration-demo.md) (관련: [flutter-ui](../archive/category/flutter-ui.md))
- 조사 대상: 디스플레이 1(기존, 터치 불가 파악됨) · 디스플레이 2(주최측 제공, 터치 지원)

> 같은 날 센서 작업은 [시연 박스 센서 임계값 재측정](2026-09-20-demo-box-threshold-measurement.md)에 따로 있습니다.

---

## v1에서 바뀐 것 (2026-09-20 같은 날 개정)

LG 배포 문서와 Flutter 샘플을 확인한 결과 v1의 전제가 틀렸습니다.

| v1 | v2 | 근거 |
|---|---|---|
| Pi5 브라우저로 `touch_test.html`을 열어 검증 | **ATLAS Flutter 앱은 네이티브로 실행됨.** 브라우저 경로는 보조 수단으로 강등 | `08-flutter-cli-guide` |
| 컴포지터가 없으면 마우스 커서 개념이 없다 | **틀림.** 임베더가 마우스 커서를 기본 지원 | 샘플 `atlas/runner/flutter_embedder_options.h` |
| 배선 언급 없음 | **0장 신설.** 터치는 별도 USB이고 포트를 잘못 고르면 화면만 나옴 | — |

---

## v3에서 바뀐 것 (2026-09-20 저녁, 실기 검증 후)

디스플레이 1 검증을 마치고 확정된 사실로 갱신했습니다. 상세 경위는 [Pi5 화면 출력 장애 조사 기록](2026-09-20-pi5-display-troubleshooting.md).

| v2 | v3 | 근거 |
|---|---|---|
| 배선에 `5V+Touch`를 Pi5 USB에 연결하라 | **연결하지 않는다.** 터치 미탑재이고 Pi5 USB 과전류를 유발 | [실측] |
| `HDMI0`(전원 포트 쪽)에 연결 | **포트는 물리 위치로 판단하지 않는다.** `status`로 확인 | [실측] |
| 터치 확인 절차 중심 | **디스플레이 1은 터치 미탑재 확정.** 마우스만 검증 | [실측] |
| 해상도 미확정 | **1024x600** (EDID preferred) | [실측] |
| — | **EDID 0바이트 = 케이블 불량 신호** 항목 신설 | [실측] |
| — | **디스플레이를 연결·급전한 상태로 부팅** 규칙 신설 | [실측] |

---

## 디스플레이 1 — 검증 완료 (2026-09-20)

| 항목 | 결과 |
|---|---|
| 영상 출력 | ✅ 정상 |
| EDID | ✅ 256 bytes |
| 네이티브 해상도 | ✅ **1024x600** (`JOYSX`) |
| **마우스** | ✅ **커서 표시·이동·클릭 동작** |
| 터치 | ❌ **미탑재** — `5V+Touch` 연결 시에도 `lsusb`에 장치 없음 |
| 앱 실행 | ✅ `com.atlas.app.settings` 전체화면 표시 |

**확정 배선**

```text
  HDMI IN   ──────▶  Pi5 micro-HDMI     (정상 케이블 — 아래 주의)
  5V        ──────▶  별도 5V 어댑터
  5V+Touch  ──────▶  연결하지 않음       ★ USB 과전류 유발
```

> ⚠️ **불량 micro-HDMI 케이블 때문에 하루를 썼습니다.** 화면이 안 나오면 `wc -c < /sys/class/drm/card0-HDMI-A-1/edid`부터 보세요. `0`이면 케이블입니다.

> ⚠️ **디스플레이를 먼저 연결·급전하고 Pi5를 켭니다.** 부팅 후 연결하면 Weston이 출력을 다시 열거하지 않습니다. 이미 켜져 있으면 `systemctl restart window-manager.service`.

---

## 디스플레이 2 — 검증 완료 (2026-09-20)

**모델**: smartmon 158TF3 DEX IPS 멀티터치 portable HDR (15.8인치)

| 항목 | 결과 |
|---|---|
| 영상 출력 | ✅ **1920x1080 @ 60Hz** (148.5MHz) |
| EDID | 0 bytes — 이 패널은 EDID를 내보내지 않음 [실측] |
| **터치** | ✅ **멀티터치 동작** |
| 마우스 | ✅ |
| 터치 좌표 매핑 | ✅ 누른 위치가 정상 반응 (축 반전 없음) |

**터치 장치** [실측]

```
Vendor=27c0 Product=0858  "WingCool Inc. TouchScreen"
드라이버: hid-multitouch
Handlers: mouse0 event4  (터치)  /  mouse1 event5 (마우스 에뮬레이션)
PROP=2      → INPUT_PROP_DIRECT (직접 터치 화면)
KEY=400     → BTN_TOUCH
ABS=260800000000003 → ABS_X/Y + ABS_MT_SLOT·MT_POSITION_X/Y·MT_TRACKING_ID
                      → 멀티터치 프로토콜 B
```

`hexdump -C /dev/input/event4`로 원시 이벤트를 확인한 결과 [실측]

| 항목 | 값 |
|---|---|
| `BTN_TOUCH` | 접촉 1 / 해제 0 정상 |
| `ABS_MT_TRACKING_ID` | 터치마다 증가, 뗄 때 `-1` |
| `ABS_MT_POSITION_X` 관측 범위 | 약 341 ~ 16180 |
| `ABS_MT_POSITION_Y` 관측 범위 | 약 431 ~ 9598 |

화면 전 영역에서 좌표가 고르게 나옵니다. 나중에 캘리브레이션이 필요하면 이 범위를 기준으로 삼습니다.

### ★ 외부 전원 USB 허브가 필수입니다

터치 USB를 **Pi5에 직접 연결하면 USB 호스트 컨트롤러가 죽습니다.** 터치 장치는 인식되지만 약 10초 뒤 버스 전체가 내려가고 카메라까지 사라집니다.

```
xhci-hcd.0: xHCI host controller not responding, assume dead
xhci-hcd.0: HC died; cleaning up
```

15.8인치 패널이 Pi5 USB 전류 한도를 넘기기 때문입니다. **복구하려면 재부팅해야 합니다.**

**확정 배선**

```text
  HDMI        ──────▶  Pi5 micro-HDMI
  터치 USB     ──────▶  【외부 전원 USB 허브】 ──▶ Pi5 USB     ★ 허브 필수
  모니터 전원  ──────▶  모니터 자체 어댑터
```

허브를 거치니 `HC died`가 재발하지 않았고 터치도 안정적으로 동작했습니다 [실측].

### 앱이 다시 안 뜰 때

터치로 앱을 닫은 뒤 `Start`가 `""`만 반환하고 화면에 안 나오는 경우가 있습니다. `Start`는 이미 실행 중인 앱에는 동작하지 않습니다.

```bash
abusctl call com.atlas.AppManager1 GetStatus com.atlas.app.settings   # window: hidden 확인
abusctl call com.atlas.AppManager1 Stop com.atlas.app.settings
sleep 3
abusctl call com.atlas.AppManager1 Start com.atlas.app.settings
# AppRestartFailed 가 나오면
systemctl reset-failed am.com.atlas.app.settings.service
abusctl call com.atlas.AppManager1 Start com.atlas.app.settings
```

---

## 시연용 선택 — 결정 필요

| | 디스플레이 1 (7인치) | 디스플레이 2 (15.8인치) |
|---|---|---|
| 해상도 | 1024x600 | 1920x1080 |
| 터치 | ❌ 컨트롤러 없음 | ✅ 멀티터치 |
| 마우스 | ✅ | ✅ |
| 추가 장비 | — | **외부 전원 USB 허브** |
| 설치 적합성 | 후드 크기에 맞음 | 주방 후드에 붙이기엔 큼 |

우리 HMI는 7인치 기준으로 설계돼 있습니다. **터치 시연을 우선할지, 실제 설치 형태를 우선할지**는 팀 결정 사항입니다 `합의 필요`.

---

## 표기 규칙

| 표기 | 뜻 | 함께 적을 것 |
|---|---|---|
| **[실측]** | 보드에서 직접 실행해 확인 | 실행한 명령, 대상, 날짜 |
| **[문서]** | 개발 문서·소스 인용 | 파일 경로 또는 문서명과 절 |
| **[추정]** | 아직 확인 못 한 해석 | **확인 방법** |

오류 메시지는 요약하지 않고 출력 그대로 옮깁니다. LG 배포 문서는 **원문을 옮기지 않고 문서명으로만 인용**합니다.

---

## 0. 하드웨어 배선 ★

디스플레이에 포트가 셋입니다. **어디에 꽂느냐로 터치 동작이 갈립니다.**

```text
 [디스플레이]                                    [Pi5]

  HDMI IN   ──── HDMI ─── micro-HDMI 변환 ────▶  HDMI0  (USB-C 전원 포트 쪽)
  5V+Touch  ──── USB-A ───────────────────────▶  USB 포트   ★ 터치 신호 + 전원
  5V        ──── (선택) 별도 5V 어댑터              전원 부족할 때만
```

| 포트 | 무엇 | 어디에 |
|---|---|---|
| **HDMI IN** | 영상만 | Pi5 **HDMI0** (micro-HDMI 변환 필요) |
| **5V + Touch** | **전원 + 터치 신호(USB HID)** | **Pi5 USB 포트** ← 터치 쓰려면 필수 |
| **5V** | 전원만 | 비워두거나, 전력 부족 시 **별도 어댑터** |

### 핵심 규칙

- **터치를 쓰려면 반드시 `5V+Touch`를 Pi5 USB에 꽂습니다.** 터치 신호는 HDMI로 오지 않습니다.
- **`5V`만 꽂으면 화면은 나오고 터치만 안 됩니다.** 가장 흔한 실수이고, "터치 안 되는 디스플레이"로 오해하기 딱 좋습니다. 디스플레이 1이 이 상태일 가능성도 확인해 보세요.
- `5V`와 `5V+Touch`를 **둘 다 Pi USB에 꽂지 마세요.** 전류만 나눠 쓰고 이득이 없습니다. 전력이 부족하면 `5V`를 **별도 어댑터**에 연결합니다.
- Pi5는 micro-HDMI 2포트입니다. **전원 포트에 가까운 `HDMI0`**을 씁니다.
- 디스플레이 1(터치 없음)은 **HDMI + 전원** 둘만 연결하면 됩니다.

### 전원 부족 증상과 확인

Pi5는 전원 어댑터 용량에 따라 USB 공급 전류가 제한됩니다. 부족하면 이런 증상이 납니다.

- 화면이 깜빡이거나 꺼졌다 켜짐 · 터치가 간헐적으로 먹통 · Pi가 갑자기 재부팅

```bash
vcgencmd get_throttled       # 0x0 이면 정상
dmesg | grep -iE "under.?voltage|usb.*power|over.?current" | tail
```

`0x0`이 아니거나 저전압 경고가 보이면 **`5V`를 별도 어댑터로 빼세요.**

### 배선 기록

| 항목 | 값 |
|---|---|
| 디스플레이 1 — HDMI | Pi5 HDMI___ / 미연결 |
| 디스플레이 1 — 전원 | ___ |
| 디스플레이 2 — HDMI | Pi5 HDMI___ |
| 디스플레이 2 — `5V+Touch` | Pi5 USB / 어댑터 / 미연결 |
| 디스플레이 2 — `5V` | Pi5 USB / 어댑터 / 미연결 |
| `vcgencmd get_throttled` | ___ |

---

## 1. ATLAS에서 UI가 어떻게 뜨는가 [문서]

확인을 시작하기 전에 알아야 할 전제입니다.

- **ATLAS의 Flutter 앱은 브라우저가 아니라 네이티브로 실행됩니다.** Docker 개발 컨테이너의 `flutter-atlas` CLI로 IPK를 만들어 `opkg`로 설치하고, `com.atlas.AppManager1`로 띄웁니다. (`08-flutter-cli-guide`, 개발 환경은 `02-dev-env-setup`)
- Flutter 엔진은 **3.27.4** 고정입니다. 우리 `flutter_app`의 `sdk: ">=3.5.0 <4.0.0"`과 호환됩니다.
- 실행 인자는 `--bundle=./ --fullscreen -a <appid>` 형태이고, 앱은 **전체화면**으로 뜹니다.
- **마우스 커서는 기본 지원**입니다. 임베더에 `--no-cursor`(`-n`) 플래그가 있고 기본값이 "커서 사용"입니다. (샘플 `atlas/runner/flutter_embedder_options.h`, `runner/main.cc`)
- 렌더 백엔드는 빌드 타임에 `GBM` / `EGLSTREAM` / `X11` / `WAYLAND` 중 하나로 정해집니다. GBM이면 컴포지터 없이 DRM에 직접 그립니다.

> 따라서 **컴포지터나 브라우저가 안 보여도 정상일 수 있습니다.** 그것만으로 "화면을 못 띄운다"고 판단하지 마세요.

---

## 2. HMI 앱으로는 터치를 검증할 수 없습니다 ★

우리 Flutter HMI에는 **누를 수 있는 위젯이 하나도 없습니다.** 코드 전수 확인 결과 `onTap`·`onPressed`·`GestureDetector`·`InkWell`이 **0건**이고, 터치가 닿는 곳은 [dashboard_screen.dart:72](../../flutter_app/lib/screens/dashboard_screen.dart#L72)의 `SingleChildScrollView` 하나뿐입니다. 그마저도 내용이 화면을 넘칠 때만 스크롤됩니다.

[CLAUDE.md](../../CLAUDE.md) 원칙대로 HMI는 **표시 전용**이라 정상입니다. 다만 그래서:

> **우리 앱을 띄워놓고 화면을 눌러봤자 아무 반응이 없는 것이 정상입니다.** 이걸로 "터치가 안 된다"고 판단하면 안 됩니다.

대신 **버튼이 있는 LG 샘플 앱**으로 검증합니다 (3-3).

---

## 3. 계층별 확인

어느 계층에서 끊기는지가 곧 원인입니다. **아래에서 위로** 올라갑니다.

| 계층 | 확인 도구 | 여기서 끊기면 |
|---|---|---|
| 1. 커널·HID | `evtest`, `libinput` | 배선·전원·드라이버 |
| 2. 영상 출력 | 화면에 뭔가 나오는가 | HDMI·해상도 |
| 3. 앱 | LG 샘플 앱의 버튼 | 임베더·입력 라우팅 |

### 3-0. 한 번에 하나씩 ★

**두 디스플레이를 동시에 연결한 채로 터치를 검증하지 마세요.** 화면이 둘이면 터치 좌표가 엉뚱한 화면으로 매핑되는 것이 가장 흔한 실패이고, 그러면 "터치가 안 된다"와 "매핑이 틀렸다"를 구분할 수 없습니다. **단독 연결로 먼저** 검증하고, 통과한 뒤에 2장 동시를 시험합니다.

### 3-1. 커널이 입력 장치를 보는가

```bash
lsusb
ls -l /dev/input/
libinput list-devices 2>/dev/null | grep -iB2 -A6 -E "touch|mouse"
evtest                                  # 장치 고른 뒤 화면을 눌러본다
dmesg | grep -iE "usb|hid|input|touch" | tail -30
```

`evtest`에서 화면을 누를 때 나와야 하는 것:

```
ABS_MT_POSITION_X / ABS_MT_POSITION_Y      멀티터치 좌표
BTN_TOUCH  value 1                          접촉 시작
```

| 확인 | 디스플레이 1 | 디스플레이 2 |
|---|---|---|
| `lsusb`에 터치 장치 | ☐ 이름: ___ | ☐ 이름: ___ |
| `/dev/input/event__` | ___ | ___ |
| `evtest`에 `BTN_TOUCH` | ☐ | ☐ |
| 좌표가 손가락 따라 변함 | ☐ | ☐ |
| 좌표 범위 | X ___~___ Y ___~___ | X ___~___ Y ___~___ |
| 마우스 장치 인식 | ☐ | ☐ |

> **여기서 실패하면 위 계층은 볼 필요 없습니다.** 0장 배선(특히 `5V+Touch`)부터 다시 봅니다.

### 3-2. 영상 출력과 렌더 백엔드

```bash
ls /dev/dri/                                    # card0/renderD128 등 GPU 노드
echo "session=$XDG_SESSION_TYPE wayland=$WAYLAND_DISPLAY x11=$DISPLAY"
ps aux | grep -Ei "weston|wayfire|labwc|cage|Xorg" | grep -v grep
```

| 확인 | 디스플레이 1 | 디스플레이 2 |
|---|---|---|
| 화면에 무언가 출력 | ☐ | ☐ |
| 해상도 | ___ x ___ | ___ x ___ |
| `/dev/dri` 노드 | ☐ | |
| 컴포지터 프로세스 | ☐ 있음 / ☐ 없음(GBM 가능) | |

### 3-3. 앱에서 마우스·터치가 먹는가 ★

**이미 설치된 앱이 있으면 그걸로 바로 검증합니다.** 우리 앱을 포팅하기 전에 확인할 수 있습니다.

```bash
abusctl call com.atlas.AppManager1 ListAllApps          # 설치된 앱 목록
abusctl call com.atlas.AppManager1 ListRunningApps
abusctl call com.atlas.AppManager1 Start {app_id}       # 앱 실행
abusctl call com.atlas.AppManager1 Stop  {app_id}
```

`smart_kitchen_app`(`com.atlas.app.smart_kitchen_app`) 같은 샘플이 설치돼 있으면 최적입니다. 버튼·스위치·탭이 있어서 **누르면 반응이 보입니다**.

| 확인 | 결과 |
|---|---|
| `ListAllApps`에 나온 앱 | ___ |
| 앱이 화면에 전체화면으로 뜸 | ☐ |
| **마우스 커서가 보임** | ☐ |
| 마우스 클릭으로 버튼 눌림 | ☐ |
| **손가락 터치로 버튼 눌림** | ☐ |
| 누른 위치와 반응 위치 일치 | ☐ |

**설치된 앱이 없으면** 3-4로 갑니다.

**좌표가 어긋났다면 어떻게 어긋났는지가 원인을 말해줍니다.**

| 증상 | 원인 |
|---|---|
| 좌우가 뒤집힘 | X축 반전 |
| 상하가 뒤집힘 | Y축 반전 |
| 90도 돌아감 | 화면 회전과 터치 회전이 따로 놂 |
| 가장자리로 갈수록 어긋남 | 스케일·캘리브레이션 |
| 아예 다른 화면에 반응 | 출력 매핑 (3-0 참고) |

### 3-4. 대체 경로 — 마스터 PC에서 패널만 검증

보드에 띄울 앱이 없거나 Pi5가 막히면, **패널 자체가 터치되는 물건인지**는 노트북에서 확인할 수 있습니다. 디스플레이의 HDMI와 `5V+Touch` USB를 노트북에 꽂습니다.

```bash
lsusb
evtest
firefox inbox/정정인/touch_test.html        # 또는 크롬
```

[touch_test.html](touch_test.html)은 네 모서리와 중앙을 눌러 5칸을 초록으로 만드는 페이지입니다. 누른 자리에 원이 찍혀서 축 반전·회전이 바로 보입니다. 외부 의존성이 없어 폐쇄 LAN에서도 동작합니다.

> **한계를 분명히 하세요.** 노트북에서 된다고 ATLAS에서 된다는 보장은 없습니다. 드라이버와 렌더 백엔드가 다릅니다. **계층 1만 검증되는 것**이고, 계층 3은 보드에서 다시 봐야 합니다.

---

## 4. 디스플레이 1의 터치 미동작 원인 기록

주최측·멘토에 설명하려면 **어느 계층에서 끊기는지**가 필요합니다. 하나만 고르세요.

- [ ] **(a) 배선** — `5V+Touch` 포트를 안 썼거나 USB 미연결 → **고칠 수 있음** (0장 참고)
- [ ] **(b) 장치가 열거되지 않음** — `lsusb`·`/dev/input`에 없음 → 패널에 터치 컨트롤러가 없음
- [ ] **(c) 열거되지만 이벤트 없음** — 드라이버/펌웨어
- [ ] **(d) 이벤트는 오지만 좌표가 틀림** — 매핑·캘리브레이션 → **고칠 수 있음**

`lsusb` 출력 그대로:

```
(여기에 붙여넣기)
```

---

## 5. 시연에 어느 쪽을 쓸 것인가

| | 디스플레이 1 | 디스플레이 2 |
|---|---|---|
| 해상도 | | |
| 터치 | ☐ (원인 a/b/c/d) | ☐ |
| 마우스 | ☐ | ☐ |
| 영상 안정성 | | |
| 전원 (`get_throttled`) | | |
| 크기·가독성 | | |
| **시연 채택** | ☐ | ☐ |

> **해상도가 바뀌면 HMI 레이아웃이 바뀝니다.** [dashboard_screen.dart:70](../../flutter_app/lib/screens/dashboard_screen.dart#L70)에서 폭 600px 미만이면 여백이 18px로 줄고, [302행](../../flutter_app/lib/screens/dashboard_screen.dart#L302)에서 900px 기준으로 카드 열 수가 바뀝니다. 채택한 디스플레이에서 **실제로 띄워보고** 카드가 잘리거나 겹치지 않는지 확인하세요.

---

## 재현 방법

```bash
ssh root@<Pi5 IP>                # Pi5
lsusb
ls -l /dev/input/
libinput list-devices
evtest
ls /dev/dri/
vcgencmd get_throttled
dmesg | grep -iE "usb|hid|input|touch|under.?voltage" | tail -30
abusctl call com.atlas.AppManager1 ListAllApps
```

---

## 이전 문서 수정 필요 사항

| 문서 | 고칠 내용 | 상태 |
|---|---|---|
| [deploy/README.md](../../deploy/README.md) | 「Flutter Web」 절이 `flutter run -d chrome`을 전제함. **ATLAS 시연 경로가 아님.** IPK 빌드·설치 경로를 별도 절로 추가하고, 웹은 개발용으로만 표기 | ☐ |
| 이 문서 v1 | 브라우저 전제 → v2에서 교체 완료 | ✅ |

---

## 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| ATLAS 이미지가 어느 렌더 백엔드로 빌드됐는가 (GBM/X11/Wayland) | 3-2 | 정인 |
| 보드에 설치된 앱이 있는가 | `ListAllApps` (3-3) | 정인 |
| 우리 `flutter_app`을 IPK로 빌드하려면 Docker 개발 컨테이너가 필요함. 누가 언제 구축하는가 | `02-dev-env-setup` 참고 후 일정 합의 | 정인·수현 |
| 네이티브에서 MQTT 접속 방식 — web은 `ws://`, 네이티브는 TCP `1883`. `--dart-define` 값이 달라짐 | `mqtt_client_factory_io.dart` 경로로 실행해 확인 | 정인·재범 |
| 터치가 돼도 HMI에 누를 위젯이 없어 시연에서 터치를 보여줄 수 없음 | 터치로 무엇을 조작할지 결정 후 위젯 추가. `minimumFanLevel` 안전 불변식 유지 | 정인·현지·재범 |
| 2장 동시 연결 시 터치 출력 매핑 | 단독 검증 통과 후 시험 | 정인 |

---

*HOOD & SEEK · Team Ro92n*
