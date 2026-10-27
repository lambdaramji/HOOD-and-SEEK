# Pi5 화면 출력 장애 조사 기록 — 원인은 micro-HDMI 케이블

- 작성자: 정정인
- 작성일: 2026-09-20
- 상태: 해결
- 카테고리: [test-calibration-demo](../archive/category/test-calibration-demo.md)
- 조사 대상: Pi5(`atlas-video-246`) HDMI 출력, ATLAS 디스플레이 스택

> 같은 날 문서: [디스플레이 2종 검증 가이드](2026-09-20-display-touch-verification.md) · [센서 임계값 재측정](2026-09-20-demo-box-threshold-measurement.md)

---

## 요약

Pi5에 7인치 HDMI 디스플레이를 연결했으나 화면이 나오지 않았습니다. 약 8시간 추적한 결과 **원인은 micro-HDMI 케이블 불량**이었습니다 [실측]. 케이블 교체로 완전히 해결됐습니다.

추적 과정에서 **주최측 이미지의 `/boot/config.txt`가 제3자에 의해 변조된 사실**을 발견했고, 스페어 보드에서 원본을 확보해 복구했습니다.

| 결과 | |
|---|---|
| 영상 출력 | ✅ 정상 (EDID 256B, 네이티브 1024x600) |
| 마우스 | ✅ 동작 |
| 터치 | 디스플레이 1 ❌ 컨트롤러 없음 / 디스플레이 2 ✅ 멀티터치 |
| `config.txt` | ✅ 원본 복구 완료 |

---

## 1. 증상

- 화면 검정, 패널 뒷면 LED 빨강(대기)
- 간헐적으로 흰 화면 깜박임 → `Out of Range` → 다시 대기
- Pi5는 정상 부팅, ssh 접속 정상

## 2. 원인 — micro-HDMI 케이블 불량 [실측]

케이블의 **DDC 선이 죽어** EDID를 읽지 못했고, 영상선(TMDS)도 불안정했습니다.

### 2-1. 통제 실험

| 조합 | EDID | 화면 |
|---|---|---|
| 맥 + HDMI-HDMI 케이블 + 패널 | 읽힘 | **정상** |
| 작업 Pi5 + 문제의 micro-HDMI + 패널 | **0 bytes** | 깜박임 → Out of Range |
| **스페어 Pi5(원본 설정)** + 같은 케이블 + 패널 | **0 bytes** | 깜박임 → Out of Range |
| 작업 Pi5 + **정상 케이블** + 패널 | **256 bytes** | **정상** |

보드와 설정을 바꿔도 같은 케이블이면 실패, 케이블만 바꾸면 성공. **공통 실패 요인은 케이블 하나**입니다.

### 2-2. 클럭 의존 열화

불량 케이블에서 픽셀 클럭이 올라갈수록 나빠졌습니다. 전형적인 케이블 불량 신호입니다.

| 모드 | 픽셀 클럭 | 증상 |
|---|---|---|
| 800x600 | 39.79 MHz | 깜박임 |
| 1024x600 | 49.12 MHz | 깜박임 |
| 1280x720 | 74.25 MHz | **신호 없음** |

### 2-3. 핵심 지표 — `EDID` 바이트 수

```bash
wc -c < /sys/class/drm/card0-HDMI-A-1/edid
```

**`0`이면 케이블을 먼저 의심합니다.** 패널이 EDID를 못 보내면 커널이 VESA 기본값(1024x768)으로 넘겨짚고, 패널이 그걸 못 받아 `Out of Range`가 뜹니다. 정상은 `128` 또는 `256`입니다.

---

## 3. 패널 정보 [실측]

| 항목 | 값 |
|---|---|
| 모델 표기 | `JOYSX` (맥 `system_profiler` 인식명) |
| **네이티브 해상도** | **1024x600** (EDID preferred mode) |
| EDID | 256 bytes (base + CEA extension) |
| 지원 모드 | 30개 (1920x1080, 1280x720, 1024x768 … 포함) |
| 터치 | **없음** |

> 맥에서 `1280x720`으로 표시된 것은 macOS가 목록에서 다른 모드를 고른 결과입니다. **네이티브는 1024x600이 맞습니다.**

---

## 4. `/boot/config.txt` 변조 사고

### 4-1. 경위

작업 Pi5의 `config.txt`가 원본과 전혀 다른 내용으로 바뀌어 있었습니다. 팀원이 이전에 다른 AI 도구와 작업하며 변경된 것으로 확인됐습니다. 그 도구가 제시한 "원본"도 **`<truncated 310 bytes>`로 잘린 조각**이어서 복구 기준이 되지 못했습니다.

### 4-2. 세 버전 비교 [실측]

| | **원본** (스페어 보드) | 변조본 (작업 Pi5) | AI가 제시한 조각 |
|---|---|---|---|
| 크기 | **2904 bytes** | **116 bytes** | 약 420 bytes (잘림) |
| `dtoverlay=vc4-kms-v3d` | ✅ | ✅ | ❌ |
| `dtoverlay=vc4-kms-dsi-7inch` | **없음** | **추가됨** | ❌ |
| `hdmi_cvt` 계열 | 전부 주석 | 없음 | 활성이라 주장 |
| `disable_overscan=1` | ✅ | 삭제됨 | ? |
| `start_x=1` | ✅ | 삭제됨 | ? |
| `enable_uart=1` | ✅ | 삭제됨 | ? |
| `dtparam=pciex1` / `pciex1_gen=3` | ✅ | 삭제됨 | ? |
| `dtoverlay=pciex1-compat-pi5,no-mip` | ✅ | 삭제됨 | ? |
| `dtparam=uart0_console` | ✅ | 삭제됨 | ? |

원본의 활성 설정은 8줄이고 나머지는 라즈베리파이 기본 주석 템플릿입니다.

```
disable_overscan=1
start_x=1
enable_uart=1
dtoverlay=vc4-kms-v3d
dtparam=pciex1
dtparam=pciex1_gen=3
dtoverlay=pciex1-compat-pi5,no-mip
dtparam=uart0_console
```

`cmdline.txt`는 원본과 작업 Pi가 동일했습니다(공백 하나 차이). `video=` 파라미터는 원래 없습니다.

### 4-3. 원본 해시 — 앞으로 이걸로 검증합니다

| 파일 | 크기 | SHA-256 |
|---|---|---|
| `config.txt` | 2904 | `ee8ed89c95454437a55bcc997d4c48351dbef2cb16bed75878af8099837d0299` |
| `cmdline.txt` | 104 | `cb0fa4362245a6950cf603739e066a8d66940d3ef63073753862ced6ae311735` |

```bash
sha256sum /boot/config.txt /boot/cmdline.txt
```

### 4-4. 복구 절차

```bash
# 스페어 보드에서 원본 확보 (읽기 전용)
scp root@<스페어IP>:/boot/config.txt  ./config.txt.ORIGINAL

# 작업 Pi5에 복원
scp ./config.txt.ORIGINAL root@<작업PiIP>:/data/
ssh root@<작업PiIP>
cp /boot/config.txt /data/config.txt.modified-116bytes   # 변조본 보존
cp /data/config.txt.ORIGINAL /boot/config.txt
sha256sum /boot/config.txt                                # ee8ed89c... 확인
sync && reboot
```

### 4-5. 재발 방지 (제안)

- **SD 이미지의 `/boot`는 수정하지 않습니다.** 수정이 필요하면 팀에 먼저 알립니다
- AI 도구에 보드 설정 변경을 맡기지 않습니다
- 변경이 불가피하면 **변경 전 원본을 `/data`와 개발 PC 양쪽에 백업**하고 해시를 남깁니다

---

## 5. 그밖에 확인된 것

### 5-1. ATLAS 디스플레이 스택 [실측]

| 항목 | 값 |
|---|---|
| 컴포지터 | **Weston 15.0.0** (`v15.0-atlas1-13-gdb138ab3+`) |
| 실행 파일 | `/usr/bin/window-manager` |
| 설정 | `/usr/share/weston/weston.ini` |
| 백엔드 | `drm-backend.so`, shell은 `atlas-shell.so` |
| 서비스 | `window-manager.service` |
| 특이사항 | `require-outputs=none` — **출력이 없어도 정상 기동**합니다 |

`weston.ini`의 가상 출력(`rdp-0`, `pipewire`)이 `mirror-of=HDMI-A-1`로 잡혀 있어, **기본 물리 출력은 `HDMI-A-1`**입니다.

### 5-2. HDMI 핫플러그를 컴포지터가 잡지 않습니다 [실측]

부팅 후에 HDMI를 연결하면 커널은 감지하지만 **Weston은 출력을 다시 열거하지 않습니다.** 부팅 시점에 빈 포트(`HDMI-A-2`)를 잡고 있던 사례를 확인했습니다.

→ **디스플레이를 먼저 연결·급전하고 Pi5를 켭니다.** 이미 켜져 있으면:

```bash
systemctl restart window-manager.service
```

### 5-3. `5V+Touch` 포트를 Pi5 USB에 연결하면 과전류 [실측]

패널의 `5V+Touch` 포트를 Pi5 USB에 꽂자 **USB 포트 전원이 차단되어 마우스·카메라가 전부 사라졌습니다.**

→ 이 패널은 터치가 없으므로 **`5V+Touch`는 연결하지 않습니다.** 전원은 별도 어댑터의 `5V`로만 공급합니다.

### 5-4. 터치 장치 오인 정정

조사 중 `lsusb`에서 발견한 `30fa:0400`을 터치 컨트롤러로 추정했으나, **USB 마우스였습니다.** `5V+Touch` 연결 상태에서 `lsusb`에 아무 장치도 열거되지 않는 것으로 **패널에 터치 컨트롤러가 없음**을 확인했습니다.

### 5-5. 미해결 — `home` 앱이 약 2분 30초마다 SEGV [실측]

```
18:30:40  home[9230]  시작
18:33:18  [ERROR][elinux_egl_surface.cc(102)] eglSwapBuffers failed: eglGetError: EGL_SUCCESS
18:33:19  Main process exited, code=dumped, status=11/SEGV
18:33:44  home[11539] 시작
18:36:08  SEGV
```

`com.atlas.app.home`이 렌더링 중 반복적으로 죽습니다. **주최측 문의 대상**입니다. 우리 앱과는 무관하지만 시연 중 홈 화면을 쓴다면 문제가 됩니다.

부수적으로 `LLVM Profile Error: Failed to write file "default.profraw": Read-only file system`이 함께 나옵니다. 프로파일링 빌드가 읽기 전용 루트에 쓰려다 실패하는 것으로 무해합니다.

---

### 5-6. 15.8인치 터치 모니터는 외부 전원 USB 허브가 필수 [실측]

디스플레이 2(smartmon 158TF3, 15.8인치 멀티터치)의 터치 USB를 **Pi5에 직접 연결하면 USB 호스트 컨트롤러가 죽습니다.** 장치는 인식되지만 약 10초 뒤 버스 전체가 내려갑니다.

```
[201.251] usb 1-2: New USB device found, idVendor=0416, idProduct=c168
[201.266] usb 1-2: Product: MTouch
[211.604] xhci-hcd.0: xHCI host not responding to stop endpoint command
[211.631] xhci-hcd.0: xHCI host controller not responding, assume dead
[211.639] xhci-hcd.0: HC died; cleaning up
[211.646] usb 1-1: USB disconnect        ← 카메라까지 같이 사라짐
```

**복구는 재부팅뿐입니다.** 소프트웨어로 살릴 수 없습니다.

**외부 전원 USB 허브를 거치면 해결됩니다** [실측].

```
usb 1-1:   USB 2.0 Hub (1a40:0101)
usb 1-1.2: WingCool Inc. TouchScreen (27c0:0858)
hid-multitouch 0003:27C0:0858.0001 ... 바인딩 성공
```

허브 경유 후 `HC died` 재발 없음, 멀티터치 정상 동작을 확인했습니다.

> 7인치 디스플레이 1에서도 `5V+Touch`를 Pi5 USB에 연결했을 때 같은 과전류 현상이 있었습니다(5-3). **패널 전원은 반드시 Pi 밖에서 공급합니다.**

---

## 6. 「Pi5 화면이 안 나올 때」 체크리스트

다음 사람이 오늘을 반복하지 않도록 순서대로 정리했습니다.

```bash
# 0. 배선 — 디스플레이를 연결·급전한 상태로 Pi5를 부팅했는가?

# 1. 커넥터 감지
grep . /sys/class/drm/*/status
#    Writeback-*는 항상 connected. HDMI-A-1 / HDMI-A-2만 본다

# 2. ★ EDID — 0이면 케이블 의심
wc -c < /sys/class/drm/card0-HDMI-A-1/edid
#    0        → 케이블·젠더 교체 (DDC 단선)
#    128/256  → 정상

# 3. 모드 목록
cat /sys/class/drm/card0-HDMI-A-1/modes
#    맨 위가 패널 네이티브. VESA 기본값만 나오면 EDID 실패

# 4. 실제 출력 상태 — 어느 커넥터에 CRTC가 붙었는가
grep -E 'connector\[|crtc\[|mode:|	enable=|	active=' /sys/kernel/debug/dri/0/state
#    HDMI-A-1의 crtc=(null) 이면 컴포지터가 다른 출력을 잡은 것
#    → systemctl restart window-manager.service

# 5. 컴포지터
systemctl status window-manager.service --no-pager
journalctl -u window-manager.service --no-pager | tail -n 30

# 6. 앱
abusctl call com.atlas.AppManager1 ListRunningApps
abusctl call com.atlas.AppManager1 Start com.atlas.app.settings
```

**증상별 1차 용의자**

| 증상 | 먼저 볼 것 |
|---|---|
| `Out of Range` | EDID 0바이트 → 케이블 |
| 화면 깜박임 | 케이블 (클럭 낮출수록 나아지면 확정) |
| 무신호·빨간 LED | 컴포지터가 다른 커넥터 사용 → WM 재시작 |
| 검은 화면(신호는 감) | 띄운 앱이 없음 → `ListAllApps` 후 `Start` |
| USB 장치 전부 사라짐 | USB 과전류 → 패널 `5V+Touch` 분리 후 재부팅 |

---

## 7. 재현 방법

```bash
ssh root@<Pi5>
grep . /sys/class/drm/*/status
wc -c < /sys/class/drm/card0-HDMI-A-1/edid
cat /sys/class/drm/card0-HDMI-A-1/modes
sha256sum /boot/config.txt
systemctl status window-manager.service --no-pager
abusctl call com.atlas.AppManager1 ListRunningApps
```

### 참고 — 런타임 EDID 주입 (SD 수정 없이 모드 강제)

케이블이 정상이면 필요 없습니다. EDID를 못 읽는 패널을 임시로 쓸 때만 사용합니다. **재부팅하면 사라집니다.**

```bash
mount | grep debugfs || mount -t debugfs none /sys/kernel/debug
cat <128바이트 EDID 파일> > /sys/kernel/debug/dri/0/HDMI-A-1/edid_override
echo on > /sys/class/drm/card0-HDMI-A-1/status     # 강제 연결 (해제는 detect)
systemctl restart window-manager.service
```

`weston.ini`도 SD 수정 없이 bind mount로 덮어쓸 수 있습니다. **권한을 644로 맞춰야** window-manager 사용자가 읽습니다.

```bash
cp /usr/share/weston/weston.ini /data/weston.ini
chmod 644 /data/weston.ini
# /data/weston.ini 편집 후
mount --bind /data/weston.ini /usr/share/weston/weston.ini
systemctl reset-failed window-manager.service
systemctl restart window-manager.service
# 원복: umount /usr/share/weston/weston.ini
```

---

## 8. 주최측·멘토 문의 사항

### Q1. `com.atlas.app.home` 앱이 약 2분 30초마다 SEGV로 종료됩니다

ATLAS Video `26.06.0-246.scarthgap.s6` / Raspberry Pi 5에서 HDMI 디스플레이(1024x600) 사용 중, 기본 홈 앱이 반복적으로 비정상 종료됩니다. 직전 로그는 `[ERROR][elinux_egl_surface.cc(102)] eglSwapBuffers failed: eglGetError: EGL_SUCCESS`이고 `status=11/SEGV`로 끝납니다. 알려진 문제인지, 회피 방법이 있는지 확인 부탁드립니다.

### Q2. 제공 이미지의 `/boot/config.txt`가 변경되어 스페어 보드에서 복구했습니다

작업 중 `config.txt`가 원본(2904 bytes)에서 116 bytes로 변경되어 있었습니다. 스페어 보드의 원본으로 복구했고 SHA-256(`ee8ed89c…`)까지 일치 확인했습니다. 추가로 확인이 필요한 사항이 있으면 알려주세요.

### Q3. 디스플레이 1의 터치 미지원 확인

7인치 패널에 `5V+Touch` 포트가 있으나 Pi5 USB 연결 시 장치가 열거되지 않습니다(`lsusb`에 없음). 터치 컨트롤러 미탑재로 이해하면 되는지 확인 부탁드립니다.

또한 두 디스플레이 모두 패널 전원을 Pi5 USB에서 끌어가면 **USB 호스트 컨트롤러가 정지(`HC died`)** 했습니다. 15.8인치 터치 모니터는 외부 전원 USB 허브를 거쳐야 정상 동작합니다. 시연 구성에 허브가 필요한 것으로 이해하면 되는지 확인 부탁드립니다.

---

## 9. 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| ~~디스플레이 2 미검증~~ | **종료.** 1920x1080 + 멀티터치 동작 확인 (외부 전원 USB 허브 필수) | — |
| 시연에 어느 디스플레이를 쓸 것인가 | 7인치(터치 없음) vs 15.8인치(터치 있음, 후드에 크다) `합의 필요` | 팀 |
| `home` 앱 SEGV 원인 | 주최측 회신 (Q1) | 정인 |
| 우리 `flutter_app`을 IPK로 빌드할 Docker 컨테이너 미구축 | `02-dev-env-setup` 참고, 일정 합의 필요 | 정인·수현 |
| Pi4·Pi5 IP가 재부팅마다 바뀜 | 공유기에서 MAC 고정 할당 | 정인 |

**MAC 주소** — 고정 할당용

| 기기 | MAC |
|---|---|
| 작업 Pi5 | `88:A2:9E:3C:CF:85` |
| 스페어 Pi5 | `88:A2:9E:3C:CE:CA` |
| Pi4 | `DC:A6:32:88:FB:FA` |

---

*HOOD & SEEK · Team Ro92n*
