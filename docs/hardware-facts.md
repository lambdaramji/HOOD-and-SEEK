# 하드웨어 실측 사실 (SoT)

| 항목 | 내용 |
|---|---|
| 작성 | 임재범 (Claude Code 보조) |
| 작성일 | 2026-10-20 |
| 상태 | 활성 — 새 실측이 나오면 이 문서에 행을 더합니다 |
| 카테고리 | 하드웨어 · 실측 기준 |
| 기준 시점 | main `7ebfa21`(#92), GitHub PR·이슈는 2026-10-20 22:20 KST까지 + PR #92(10-21 병합). 설계서 행 번호는 §4.4 갱신(v0.4 보강, 2026-10-20) 뒤 기준 |
| 이슈 | #85 |

기기에서 직접 실행하거나 관찰한 결과만 모은 기준 문서입니다. 통합(#64)·재실측(#65)·보고서에서 「실제로 확인된 값」을 찾을 때 여기서 시작합니다. 원래 기록은 맨 끝 [근거 목록](#근거-목록)에 있고, 이 문서의 모든 행은 그중 하나를 가리킵니다.

**범위:** 설계값·판정 규칙의 기준은 [설계서](2026-10-19-s3-s10-design-v0.1.md)이고, 이 문서는 기기 실측 사실의 기준입니다. 둘이 다르면 [다른 문서와 다른 점](#다른-문서와-다른-점-고칠-곳)에 올리고 설계서를 고칩니다.

## 표기와 수록 기준

- **모든 행이 [실측]입니다.** 값과 오류 메시지는 원문 그대로 옮겼습니다.
- **날짜**는 기록한 사람이 적은 실제 날짜입니다. Pi 4·Pi 5는 NTP가 동작하지 않아 기기 시계가 실제와 다릅니다(§11). 기기 로그의 시각을 옮길 때는 「기기 시계」라고 붙였습니다.
- **넣지 않은 것:** [추정]·계획·제안, 호스트(Mac·PC) 단위 시험, 기기 IP·MAC·센서 시리얼 번호, LG 문서·회신 원문(P-010).
- **출처 약어**(`[WG §8-B]`처럼 대괄호)는 [근거 목록](#근거-목록)에 있습니다. GitHub 항목은 「#44 댓글 (작성자, 날짜)」처럼 적었습니다.
- **갱신 규칙**
  - 새 실측은 해당 절 표에 행을 더합니다.
  - 값이 바뀌면 옛 행을 지우지 않고 [대체된 값](#대체된-값)으로 옮깁니다.
  - 이 문서와 다른 문서가 다르면 [다른 문서와 다른 점](#다른-문서와-다른-점-고칠-곳)에 적고 그 문서 담당자에게 요청합니다.

## 요약 — 자주 찾는 값

| 항목 | 값 | 절 |
|---|---|---|
| Pi 4 | `Raspberry Pi 4 Model B Rev 1.2`, 커널 `6.12.25-v8`, ATLAS `26.06.0-246`, 루트 읽기 전용 | §1 |
| Pi 5 | `Raspberry Pi 5 Model B Rev 1.1`, 커널 `6.12.25-v8-16k`, 같은 ATLAS 빌드 | §2 |
| ESP32 UART | 팬 `/dev/ttyAMA3`(uart3, 물리 7·29), 쿡탑 `/dev/ttyAMA5`(uart5, 물리 32·33). 루프백 통과 | §1 |
| I2C | 100 000 Hz. `0x33` MLX90640 · `0x57` EEPROM · `0x68` DS3231 · `0x69` SPS30 | §3 |
| 센서 주기 | MLX90640 서브페이지 2 Hz → 전체 프레임 1 Hz, SPS30 1 Hz. 3개 프로세스 600 s 동시 읽기에 공백 없음 | §3 |
| 팬 ESP32 | FireBeetle ESP32 V3.0(CH340), `Serial2` GPIO16/17, 신호선 1 kΩ 직렬(10/20 교체, 그전 220 Ω). 무명령 3.01 s → `FAILSAFE timeout FAN 3`. 워치독 리셋 중 팬 약 0.4 s 정지 | §4 |
| SafetyCore ↔ 팬 | 기동 1 s 안에 `ack=true`, `ACK FAN 3` 1.00~1.01 s 간격 | §4 |
| 쿡탑 ESP32 | CP2102 DevKit, `Serial2` G16/G17 ↔ Pi 4 uart5. 약·중·강 `ACK` 각 10/10, 무명령 3 s → `FAILSAFE timeout IND 0 LOCK 1`, 워치독 멈춤 후 2.31 s 리셋. LED 판 데이터 `G18`, 지그재그·GRB | §5 |
| 카메라 | `Microsoft® LifeCam HD-3000` USB UVC, V4L2 640×480 YUYV | §6 |
| 고양이 인형 검출 | 웅크림·얼굴이 카메라 쪽이면 `dog` 0.75~0.82 15/15, `cat` 0회. 세워 두면 `person` 0.83~0.87 | §6 |
| 비전 추론 (Pi 5) | 4스레드 p95 311 ms. 10분 연속 3.59 FPS, infer 평균 277.2 ms | §6 |
| 전원 | Pi 5 PD 5 V 5 A 협상, 부하 중 저전압 0. Pi 4는 5 V 3 A 어댑터로 교체 | §9 |
| 부팅 자동 기동 | **안 됨** (Pi 4·Pi 5 모두). 재부팅 뒤 `systemctl start` 수동 | §10 |
| 열화상 경계 | 시연 박스에서 `max − median` 16·22는 유효, 정상 상한 8은 무효(빈 상태 11.86) | §12 |
| 연기 판정 근거 | `tsize < 0.49` ∧ 기준선 대비 PM2.5 +50 ∧ 새 샘플 3개 연속 → 인센스 78.7 s 첫 판정, 가습기 오탐 0 (09-26, 이전 인센스 제품). **10/24 시연 제품(HEM 골드)에서는 `tsize` 조건이 성립하지 않음** — 비포화 1,000~2,000 구간 연기 0.46~1.45, 연기 없이 팬 1단부터 0.49 미만 [IM] | §12 |

## 1. Pi 4 (SafetyCore 보드)

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| `Raspberry Pi 4 Model B Rev 1.2`, 커널 `6.12.25-v8`, 이미지 `ATLAS Headless 26.06.0-246` | 2026-10-19 | `uname -r` 등 | #44 댓글, #46 댓글 (ImymemineJ, 10-19) |
| `NAME="ATLAS Platform"`, `VERSION="26.06.0-246.scarthgap.s6 (bootes)"` (Pi 5와 같은 빌드) | 2026-09-20 | SSH 접속 | [RG 보드 배치와 역할] |
| `/dev/mmcblk0p2 on / type ext4 (ro,relatime,norecovery)`. `/etc`·`/root` 읽기 전용, `/data` = `/mnt/rawdata/data` bind 마운트 | 2026-10-19 | `mount`, journal | #46 댓글 (ImymemineJ, 10-19) |
| `/boot`는 `vfat (rw,...)` | 2026-10-19 | `mount` | #44 댓글 (ImymemineJ, 10-19) |
| 두 보드 모두 Python 없음, 패키지 관리자 없음, BusyBox 기반 | 2026-08-30 | `command -v python3`, `command -v apt opkg`, `ls -l /bin/sh` | [DS §1-1] |
| `com.atlas.*` 서비스 30개, `VisionService1` 없음 | 2026-08-30 | `busctl list --no-pager \| grep -o "com\.atlas\.[A-Za-z0-9]*" \| sort -u` | [DS §1-2] |
| overlay 전: `/dev/ttyAMA*` 없음, `/dev/serial0 -> ttyS0`(콘솔) | 2026-10-19 14:14 | `ls -l /dev/ttyAMA* /dev/ttyS* /dev/serial*` | #44 댓글 (ImymemineJ, 10-19) |
| `config.txt` 끝에 `dtoverlay=uart3`·`dtoverlay=uart5`를 추가하고 재부팅하면 `/dev/ttyAMA3`(204, 67)·`/dev/ttyAMA5`(204, 69)가 생김. 둘 다 `root dialout`. `ttyAMA1`(uart0)은 Bluetooth serdev가 점유해 `/dev` 노드 없음. 콘솔 `console=ttyS0,115200` 유지 | 2026-10-19 14:14 | `dmesg \| grep -i -E "ttyAMA\|uart\|pl011"` | #44 댓글 (ImymemineJ, 10-19), 설계서 §4.5 |
| 루프백: 물리 7(GPIO4)↔29(GPIO5)에서 `/dev/ttyAMA3`가 `hello-uart3` 수신, 물리 32(GPIO12)↔33(GPIO13)에서 `/dev/ttyAMA5`가 `hello-uart5` 수신. 점퍼 없는 쪽은 수신 없음 | 2026-10-19 14:51 | `stty -F … 115200 raw -echo; cat &; echo hello-… >` | #44 댓글 (ImymemineJ, 10-19) |
| USB 시리얼: CH340 `1a86:7523`은 열거되지만 `ttyUSB`가 생기지 않음(`modinfo: ERROR: Module ch341 not found.`). 내장 드라이버는 `cp210x  ftdi_sio  generic  pl2303`과 `cdc-acm`. `/dev/serial/` 없음 | 2026-09-26, 2026-10-19 | `lsusb`, `modules.builtin`, `/sys/bus/usb-serial/drivers/` | [WG §2-C], #44 댓글 (ImymemineJ, 10-19) |
| `/etc/group`: `dialout:x:20:`, `i2c:x:11002:peripheral`, `spi:x:11003:peripheral`, `gpio:x:11004:peripheral`, `perm_peripheral:x:21025:` | 2026-10-19 | `grep -E "^(i2c\|dialout…)" /etc/group` | #46 댓글 (ImymemineJ, 10-19) |
| CSI 카메라 모듈(`unicam`, `imx*`, `ov5647` 계열) 미로드. `lsmod`에 `bcm2835_codec`·`bcm2835_v4l2`만 있음 | 2026-09-20 | `lsmod` | [RG 보드 배치와 역할] |
| ECO.AI 관련 파일은 `/opt/tests/atest/ptest/com.test.EcoaiAgentsOrchestrator1.xml` 하나뿐. `/usr/lib/ecoai/`, `.tflite` 모델, `vision-service` 유닛 없음 | 2026-09-20 | `journalctl -u vision-service`(빈 결과) | [RG 보드 배치와 역할] |

## 2. Pi 5 (vision·voice·Flutter 보드)

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| `Raspberry Pi 5 Model B Rev 1.1`, `NAME="ATLAS Platform"`, `26.06.0-246.scarthgap.s6 (bootes)`, 커널 `6.12.25-v8-16k` | 2026-08-30 | `cat /etc/os-release`, `uname -a`, `cat /proc/device-tree/model` | [DS §1-1] |
| `/proc/device-tree/compatible` = `raspberrypi,5-model-b brcm,bcm2712` | 2026-08-30 | 위 파일 조회 | [DS §3-1] |
| `com.atlas.*` 서비스 53개. Pi 5에만 `VisionService1`·`VisionPipeline1`·`MediaPlayer1`·`MediaController1`·`WindowManager1`·`EcoaiAgentsOrchestrator1`·`EcoaiVectordb1` | 2026-08-30 | `busctl list …` | [DS §1-2] |
| 루트 읽기 전용: `LLVM Profile Error: Failed to write file "default.profraw": Read-only file system` | 2026-09-20 | gst 실행 | [P5 §5-5], [WG §2-D] |
| GPIO UART `/dev/ttyAMA0`(GPIO14/15)는 `dtparam=uart0_console`로 콘솔이 점유 | 2026-09-26 | 원본 `config.txt` | [WG §2-C] |
| 원본 `/boot/config.txt` 2904 bytes, SHA-256 `ee8ed89c95454437a55bcc997d4c48351dbef2cb16bed75878af8099837d0299`. `cmdline.txt` 104 bytes, `cb0fa4362245a6950cf603739e066a8d66940d3ef63073753862ced6ae311735`. 활성 8줄: `disable_overscan=1` `start_x=1` `enable_uart=1` `dtoverlay=vc4-kms-v3d` `dtparam=pciex1` `dtparam=pciex1_gen=3` `dtoverlay=pciex1-compat-pi5,no-mip` `dtparam=uart0_console` | 2026-09-20 | `sha256sum /boot/config.txt /boot/cmdline.txt` (스페어 보드) | [P5 §4-2, §4-3] |
| opkg OpenCV는 `libopencv-core`·`libopencv-imgproc`뿐(`videoio`·`imgcodecs` 없음). python3·pkg-config 없음 | 2026-09-20 | `ls /var/lib/opkg/info/libopencv-*.control` | [RG 실행 실패] |
| `libjpeg.so.62`·`libpng16.so.16`은 있음. `libtiff.so.6`, GStreamer 8종, FFmpeg 4종, libgphoto2 2종은 SDK·기기 어디에도 없음 | 2026-09-20 | `find … -iname "libtiff*"`, `readelf -d` | [RG 실행 실패] |
| PulseAudio 17.0 시스템 모드(`/var/run/pulse/native`). `aplay`·`arecord`·`paplay`·`parecord` 없음. gst `wavenc`·`rawaudioparse`·`level`·`volume` 없음 | 2026-09-26 | `pactl get-default-sink` | [WG §2-D] |
| `/dev/video0`은 `root:video`, video 그룹 구성원은 pipewire뿐 | 2026-10-19 | 기기 확인 | PR #77 본문, `deploy/systemd/hoodseek-vision-node.service` 주석 |
| `/data/share/usr/lib/systemd/system` 유닛은 `start` 정상. `multi-user.target.wants` 링크는 재부팅 때 무시됨(부팅 26 s 뒤 inactive, `/data` 마운트 7.27 s) | 2026-10-19 | `systemctl` | #61 댓글 (Jik-Kim, 10-19) |
| `Restart=always`: `kill -9` 1 s 뒤 재기동. 1.5 s 간격 6연속 kill에도 `active`, `NRestarts=7`(`StartLimitIntervalUSec=0`, `StartLimitBurst=5`) | 2026-10-19 (journal 날짜는 기기 시계 `Sep 26`) | `systemctl show … NRestarts`, `kill -9` | #61 댓글 (Jik-Kim, 10-19) |

## 3. I2C 버스와 센서 (Pi 4)

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| 버스 속도 `clock-frequency` = `0 1 134 160` → 100 000 Hz. `config.txt` 251행 `dtparam=i2c_arm=on` | 2026-10-19 17:10 | `od -An -tu1 /proc/device-tree/soc/i2c@7e804000/clock-frequency` | #45 댓글 (ImymemineJ, 10-19) |
| `i2cdetect -y 1`: `0x33` MLX90640, `0x57` ZS-042 EEPROM(AT24C32), `0x68` DS3231, `0x69` SPS30 | 2026-10-19 15:15 | `i2cdetect -y 1` | #45 댓글 (ImymemineJ, 10-19) |
| DS3231(ZS-042) 배선: VCC → 물리 17(3.3V), GND → 14, SDA → 3, SCL → 5(공유). 32K·SQW 미연결, 배터리 없음. 배터리 없이도 발진 정상(초 레지스터 `+1` 591회, `+2` 8회) | 2026-10-19 | 배선 기록, `i2cget -y 1 0x68 0x00` 1 s 루프 | #45 댓글 (ImymemineJ, 10-19) |
| 프로세스 3개 동시 600 s: 1.6 s를 넘는 공백 0. MLX 814행 간격 중앙 1.00 s·최대 1.10 s, `drop` 0. SPS30 604행 중앙 1.00·최대 1.10, CRC 메시지 0. DS3231 실패 0/600 | 2026-10-19 15:15 | `mlx_stream 8080 2 …` / `sps30_test log … 600` / `i2cget` 루프 | #45 댓글 (ImymemineJ, 10-19) |
| **MLX90640 refresh 2 Hz → CSV 1초에 1행**(814행 / 827 s). 체스판 서브페이지 2장이 전체 프레임 1장이라 **전체 프레임은 1 Hz** | 2026-10-19 | 위 로그 분석 | #45 댓글 (ImymemineJ, 10-19) |
| MLX90640 측정 설정: 방사율 0.95, 체스 모드, 18-bit ADC, 768픽셀(32×24) | 2026-09-02 | `mlx_stream` (마스터 PC 크로스 컴파일) | [SM §1, §2] |
| 배포된 `mlx_stream` CSV의 `delta` 열은 **`p95 − median`**(`max − median` 아님). 첫 행 `2.0,39.10,34.33,28.10,6.22,...` | 2026-09-26 | CSV 대조 | [DB §A-1] |
| SPS30 제품 코드 `00080000`, 펌웨어 `2.3`, 상태 레지스터 `0x00000000`. 평소 PM2.5 3.8~5.6 µg/m³ | 2026-10-19 | `sps30_test` | #45 댓글 (ImymemineJ, 10-19) |
| SPS30은 3.3V 직결(레벨 컨버터 없음)로 `0x69` 응답 | 2026-09-20 | `i2cdetect -y 1` | [WG §1-B] |
| SPS30 PM2.5가 1,000을 넘은 직후에만 기록이 끊김(공백 4.0~49.8 s, 5곳). `[!] CRC 불일치` 줄 없음, 상태 레지스터 `0x00000000` | 2026-09-26 | demo-box CSV | [DB §B-결과], [SH 측정 시 주의] |
| SafetyCore sensor 스레드 → Pi 5에서 받은 state: `thermal {valid: true, max_c 34.3~35.3, median_c 26.1~26.4}`, `pm {valid: true, pm25 20.5, tsize 0.47}`. 같은 시각대 `mlx_test` avg 26.0~26.6 °C, `sps30_test` PM2.5 22.9~23.4 | 2026-10-20 (기록 시각은 [다른 값 7](#출처끼리-다른-값)) | `arc build` 기기, Pi 5 수신 | PR #82 본문(#83) |
| F4: SPS30 5 V 선만 15 s 분리하면 마지막 샘플 약 3.1 s 뒤 `pm.valid=false`. 다시 연결하면 `[sensor] SPS30 측정 시작` → `pm.valid=true`. 분리 동안 MLX `thermal.valid=true` 유지(I2C 버스 멈춤 없음) | 2026-10-20 (같음) | 선 분리 | PR #82 본문(#83) |

## 4. 팬 ESP32와 UART

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| 보드 DFRobot FireBeetle ESP32 V3.0(CH340). `Chip is ESP32-D0WDQ6 (revision v1.0)`. BOOT 버튼이 없어(RST만) `Failed to connect to ESP32: No serial data received.` → `IO0`–GND 점퍼 + RST로 기록 | 2026-09-26, 2026-10-19 | esptool | [WG 펌웨어 굽기 결과], PR #78 본문 |
| CH340은 마스터 PC(Ubuntu)에서 `/dev/ttyUSB0`, Pi 5에 직결하면 `ls: /dev/ttyUSB*: No such file or directory` | 2026-09-26 | `lsusb`, `ls /dev/ttyUSB*` | [WG §2-C] |
| 배선: Pi 4 `/dev/ttyAMA3` ↔ ESP32 `Serial2`(GPIO16 RX ← Pi 물리 7, GPIO17 TX → Pi 물리 29). 두 신호선 직렬 저항은 처음 **220 Ω**(색 띠 빨강-빨강-갈색, 「2.2 kΩ」으로 잘못 읽었다가 정정) → 10/20 **1 kΩ**로 교체(README대로). GND 직결 | 2026-10-19, 2026-10-20 | 실배선 | PR #78 본문, PR #78 댓글 (ImymemineJ, 10-20 정정) |
| Pi 4에서 주고받은 줄(기기 시계 = uptime): `73.12 RX READY FAILSAFE FAN 3` / `74.28 TX FAN 2` / `74.29 RX ACK FAN 2` / `FAN 9` → `ERR invalid-command` / `75.80 TX FAN 1` / `75.80 RX ACK FAN 1` / `78.81 RX FAILSAFE timeout FAN 3`(마지막 ACK 뒤 3.01 s) | 2026-10-19 | Pi 4 `ttyAMA3` 송수신 | PR #78 본문 |
| 워치독(`wdt_test` 빌드): `0.05 s dbg hang → 2.02 s task_wdt: … loopTask … Aborting. → 2.40 s dbg reset_reason=6 / READY FAILSAFE FAN 3`. 리셋 동안 팬 정지 약 0.4 s | 2026-10-19 | `H` 입력으로 loop 정지 | PR #78 본문, `firmware/esp32_fan_controller/README.md` 「한계」 |
| 넘친 줄(`7f90998` 뒤): `XXXXXXXXXXXXXXXYFAN 0` → `ERR command-too-long` 1줄, ACK 없음 → 이어 보낸 `FAN 1` → `ACK FAN 1` | 2026-10-19 | Pi 4 `ttyAMA3` | PR #78 댓글 (ImymemineJ, 10-19) |
| 220 Ω 배선 상태(처음 「2.2 kΩ」으로 기록): ① `FAN 1·2·3·0·1` → `ACK FAN 1·2·3·0·1` 5/5 ② `FAN 9` → `ERR invalid-command` ③ 잡음 20자+`FAN 0` → `ERR command-too-long` 1회, `FAN 0` 미실행 ④ 무명령 3 s → `FAILSAFE timeout FAN 3` ⑤ Pi 4를 끄면(ESP32는 USB 전원) `ACK FAN 1`(2.2 s) → `FAILSAFE timeout FAN 3`(5.2 s), 이후 약 240 s 리셋·잡음 없음 ⑥ GND 점퍼가 빠지면 양방향이 깨져 `ERR` 반복 | 2026-10-20 | 실기 왕복 | PR #78 댓글 (ImymemineJ, 10-20) |
| SafetyCore UART 링크: 기동 1 s 안에 `[uart:fan] ack=true`. ESP32 USB 디버그에 `ACK FAN 3` 1.00~1.01 s 간격 6회. Pi 5 state `fan {level 3, min 3, ack: true}` | 2026-10-20 | `arc build` 기기 | PR #82 본문 |
| F5 (220 Ω 배선 때): ESP32 TX 선 분리 → `[uart:fan] ack=false`, 재연결 → `ack=true`. Pi 4 재부팅 직후 첫 명령이 잡음에 붙어 `ERR invalid-command` → 즉시 재전송으로 `ack=true` | 2026-10-20 | 선 분리, 재부팅 | PR #82 본문 |
| 1 kΩ로 바꾼 뒤: SafetyCore `[uart:fan] ack=true`, Pi 4 전원 차단(F1) → 팬 최고 속도 → 재연결 후 `ack=true` | 2026-10-20 | 실기 | PR #78 댓글 (ImymemineJ, 10-20 정정) |

## 5. 쿡탑 ESP32·LED 판

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| 보드 CP2102 DevKit. Pi 4 `/dev/ttyAMA5`(uart5, 물리 32·33) ↔ ESP32 `Serial2`(G16 RX, G17 TX), GND 공통. 두 신호선 1 kΩ 직렬 | 2026-10-20 | 실배선 | PR #87 본문, [CF], PR #87 댓글 (ImymemineJ, 10-20) |
| 부팅: `READY FAILSAFE IND 0 LOCK 1`, USB 디버그 `dbg wdt 2s init=0 add=0` | 2026-10-20 | Pi 4 + 쿡탑 ESP32 | PR #87 본문 |
| SafetyCore(main, #56·#47)와 연결 → `[uart:cook] ack=true`. 첫 명령은 부팅 잡음에 붙어 `ERR` → 즉시 재전송으로 회복 | 2026-10-20 | Pi 4 실기 | PR #87 본문 |
| 약·중·강 10 s씩 연속 송신: `ACK IND n`·`ACK LOCK 0` 각 10/10, 이어서 `FAILSAFE timeout IND 0 LOCK 1` | 2026-10-20 | Pi 4 `ttyAMA5` 송신 | PR #87 본문 |
| `IND 2` + `LOCK 1`: 파란 링 점멸이 화면 위에 덧씌워짐. SafetyCore 정지 3 s 뒤 빨간 링 + 소등 (육안) | 2026-10-20 | 실기 육안 | PR #87 본문 |
| fail-safe 즉시 소등 펌웨어(PR #92): `IND 3` 15 s 송신 뒤 명령 중단 → 3 s 뒤 강불이 **한 번에** 소등 + 빨간 링 점멸 (육안, 밝기 25/255) | 2026-10-20 | Pi 4 `ttyAMA5` 송신, 실기 육안 | PR #92 본문 |
| 워치독(`wdt_test` 빌드): USB `H` → `dbg hang` 2.08 s → `dbg reset_reason=6`·`READY FAILSAFE IND 0 LOCK 1` 4.39 s (멈춤 후 2.31 s 리셋) | 2026-10-20 | `pio run -e wdt_test` | PR #87 본문, [CF] |
| F1: 중불·팬 2단 송신 중 Pi 4 어댑터 분리 → 쿡탑 빨간 링 + 소등, 팬 최고 속도 (육안). 재연결 후 SafetyCore 기동 → 팬·쿡탑 2 s 안 `ack=true` | 2026-10-20 | 전원 분리 | PR #87 본문 |
| LED 판 데이터: ESP32 `G18` → 입력 커넥터 `DIN`. 출력 커넥터(`DOUT`)에 데이터를 넣으면 아무것도 켜지지 않음 | 2026-10-20 | 실배선 | [CF] |
| LED 판 배열: 앞면 왼쪽 아래가 0번, 줄마다 방향이 바뀌는 지그재그. 색 순서 GRB(빨강·주황·노랑이 맞게 나옴) | 2026-10-20 | `z` 시험, 육안 | PR #87 본문, [CF] |
| 밝기 76/255(약 30 %)도 아크릴 없이 보면 눈부심 | 2026-10-20 | 육안 | [CF] |
| 매트릭스 기판 칸마다 `104`(0.1 µF) 칩 커패시터가 붙어 있음 | 2026-10-20 | 실물 사진 | [LP], PR #87 |

PR #87·#92의 시각은 Pi 4 시계(NTP 없음) 기준이라, 위에는 상대 시간만 옮겼습니다.

## 6. 카메라·비전 (Pi 5)

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| 두 보드: `video0: Microsoft® LifeCam HD-3000`, `video1: Microsoft® LifeCam HD-3000`. IMX500이 아니라 USB UVC (P-001) | 2026-08-30 | `for d in /sys/class/video4linux/*/name; do cat $d; done` | [DS §1-5], PR #10 본문 |
| Pi 4·Pi 5 어느 쪽에 꽂아도 재부팅 없이 `uvcvideo`가 인식(`Found UVC 1.00 device Microsoft® LifeCam HD-3000`) | 2026-09-20 | `ls -la /dev/video*`, `dmesg \| grep -i camera` | [RG 시험 0], [P9 선행 확인] |
| V4L2 직접 캡처 640×480 YUYV | 2026-10-19 | vision-node 실기 | PR #77 본문 |
| 사람 앞 20초: `person score 0.65`, 2프레임 연속 뒤 `presence:present`. 76프레임 평균 처리 313.7 ms(약 3.2 FPS, CPU/XNNPACK). 임시 거치대에서 카메라가 180도 뒤집혀 있어 `--rotate 180` | 2026-09-20 | 라이브 | [VF 시험 3~5] |
| 2스레드 3.39 FPS, infer 평균 293.5 / p95 422 / 최대 451 ms. 4스레드 3.52 FPS, p95 311 ms | 2026-10-19 (임시 거치) | 라이브 | #61 댓글 (Jik-Kim, 10-19), PR #77 본문 |
| 10분 연속: 2156프레임 3.59 FPS, 간격 최대 435 ms, infer 평균 277.2 / p99 339 / 최대 434 ms, RSS 78 MB 고정, 최고 44.1 °C, 저전압 0 | 2026-10-19 | 라이브 | #61 댓글 (Jik-Kim, 10-19), PR #77 본문 |
| 캡처 지연(`now − buf.timestamp`, 25프레임): 수정 전 평균 1403 / 중앙 1419 / 최대 1715 ms → 가장 새 버퍼만 쓴 뒤 평균 218 / 중앙 187 / 최대 313 ms | 2026-10-19 | 측정 출력 사본 | PR #77 본문 |
| 고양이 없는 장면에서 cat 최대 0.3317, dog 최대 0.3127(사람 머리카락을 털로 오인) | 2026-10-19 (임시 거치) | 라이브 | #61 댓글, #49 댓글 (Jik-Kim, 10-19) |
| 시연 고양이 인형(가로 30 × 세로 17 × 높이 15 cm), 자세마다 15프레임, 사람 없음: ① 웅크림·얼굴이 카메라 쪽 → `dog` 15/15(0.75~0.82), ROI 겹침 0.40~0.43, `cat`·`teddy bear` 0 ② 등이 카메라 쪽 → `dog` 1장(0.11), `teddy bear` 0.35 이상 1/15(0.10~0.42) ③ 세우고 등·옆이 카메라 쪽 → `person` 0.83~0.87 ④ 몸통은 밖·앞발만 조리면 안 → 검출 없음. 어느 자세에서도 `cat` 0회 | 2026-10-20 | Pi 5 라이브, LifeCam 640×480, `--threads 2` | PR #89 본문 |
| 320×240 ROI로 시작하면 `camera size 640x480 differs from the ROI calibration; recalibrate config/roi.json`, 종료 코드 2. `--stall-ms 200`이면 종료 코드 3, `source_stalled` | 2026-10-19 | 실기 | PR #77 본문 |
| `yolox.tflite` sha256 `ad392c6e80edc5dd2a6e0464acdf25062c3d8e5fb6d005362a18b86ada5781c0`(9/20·10/19 같음). 라벨 `person`=0, `cat`=15, `dog`=16 | 2026-09-20, 2026-10-19 | `sha256sum` | [RG], PR #77 본문 |
| vision-node → 수신기: 끊김 → 1 s 뒤 재접속, hello부터 다시. 38줄 모두 스키마 통과, 줄 최대 223 B | 2026-10-19 18:14 | Pi 5 실기 | #61 댓글 (Jik-Kim, 10-19), PR #77 본문 |
| OpenCV videoio가 없어 `./hoodseek_vision: error while loading shared libraries: libopencv_videoio.so.409: cannot open shared object file: No such file or directory` | 2026-09-20 | 기기 실행 | [RG 시험 4·5], PR #32 본문 |

**플랫폼 VisionService 경로 (2026-08-30, 지금은 쓰지 않음 — D-022)**

| 사실 | 출처 |
|---|---|
| 백엔드 `NPU_QNN_HTP`: `VisionResult` 0건(12~15초씩 3회), 12초 CPU 152틱. `GPU`로 바꾸면 `person_detection` 3.7 FPS, `obj_detect` 3.8 FPS | [DS §2-4, §3-2], PR #14 본문 |
| `Camera1.SetFormat(handle, JPEG 640×480 30)`을 빼면 `[ERROR] Timeout! No data available to read from eventfd` 반복 | [DS §8-1] |
| `camera.service`를 재시작하면 `Camera1.Start`가 `Unknown error`, 재부팅만 복구 | [DS §8-2] |
| `/etc` 파일을 고치면 소유자가 `root:root`로 바뀌어 `Invalid model ID`. 복구 `chown vision:vision <파일> && chmod 640 <파일>` | [DS §8-2] |
| 화면에 띄운 고양이 사진 20초: 탑뷰 `cat` 0회, 옆모습 69회 | [DS §4-1], PR #14 본문 |

## 7. 음성·마이크·스피커 (Pi 5)

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| ReSpeaker Lite `2886:0019`, `USB-Audio - ReSpeaker Lite`, sink/source `s16le 2ch 16000Hz`. USB 펌웨어 모드. `amixer -c 2 scontrols`에 조절 항목 없음 | 2026-09-26 | `lsusb`, `cat /proc/asound/cards`, `pactl list short sinks` | [WG §2-D] |
| 녹음 `320000` B = 5.00초, `peak=14743 (-6.9 dBFS)`, `rms=1294 (-28.1 dBFS)`, 클리핑 없음. 배경 소음 약 -36 dBFS. 재생 `Execution ended after 0:00:05.001803571`. 기본 음량 46%(-20 dB) → 100% | 2026-09-26 | `gst-launch-1.0 pulsesrc …`, `… pulsesink` | [WG §2-D] |
| LifeCam 마이크도 Pi 5 입력으로 잡힘 `alsa_input.usb-Microsoft...mono-fallback` | 2026-09-26 | `pactl list short sources` | [WG §2-D] |

## 8. 디스플레이·터치 (Pi 5)

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| 디스플레이 1(7"): EDID 256 bytes, 네이티브 1024x600(EDID preferred), 지원 모드 30개. 터치 없음(`5V+Touch`를 연결해도 `lsusb`에 장치 없음) | 2026-09-20 | `wc -c < /sys/class/drm/card0-HDMI-A-1/edid`, `cat …/modes`, `lsusb` | [DT 디스플레이 1], [P5 §3, §5-4] |
| 검정 화면·`Out of Range`의 원인은 micro-HDMI 케이블: 문제 케이블 EDID 0 bytes(작업·스페어 Pi 5 모두), 정상 케이블 256 bytes | 2026-09-20 | EDID 바이트 수 | [P5 §1, §2-1] |
| Weston 15.0.0(`window-manager.service`). 부팅 뒤 HDMI를 연결하면 출력을 다시 열거하지 않음 → `systemctl restart window-manager.service` | 2026-09-20 | — | [P5 §5-1, §5-2] |
| 디스플레이 2: 15.8인치 `1920x1080 @ 60Hz`, 터치 `27c0:0858` `WingCool Inc. TouchScreen`(`hid-multitouch`). X 약 341~16180, Y 약 431~9598 | 2026-09-20 | `hexdump -C /dev/input/event4` | [DT 디스플레이 2] |
| 디스플레이 2 터치 USB를 Pi 5에 직결하면 약 10초 뒤 `xHCI host controller not responding, assume dead` → `HC died; cleaning up`, 카메라도 사라지고 재부팅으로만 복구. 외부 전원 USB 허브를 거치면 재발 없음 | 2026-09-20 | dmesg | [P5 §5-6] |
| 디스플레이 2(smartmon 158TF3, 시연 화면 — PM 10/23 확정): EDID 0 bytes, `modes`는 `1024x768`·`800x600`뿐 → 4:3로 늘어나 보임 | 2026-10-23 | `wc -c < /sys/class/drm/card0-HDMI-A-1/edid`, `cat …/modes` | #64 댓글 (ImymemineJ, 10-23) |
| 1080p EDID([`deploy/pi5/edid-1920x1080.bin`](../deploy/pi5/edid-1920x1080.bin), sha256 `b83c4a1a…`)를 `edid_override`로 넣고 `window-manager` 재시작 → `mode: "1920x1080": 60 148500 1920 2008 2052 2200 1080 1084 1089 1125`, 설정 앱 표시·터치 위치 정상. Pi 5 부팅 뒤마다 다시 넣음 — 명령은 [deploy/README.md](../deploy/README.md#pi-5-화면-1080p-edid-64) | 2026-10-23 | `/sys/kernel/debug/dri/0/state`, 실기 터치 | #64 댓글 (ImymemineJ, 10-23) |
| 디스플레이 2 터치 USB를 Pi 5에 직결(모니터 전원은 별도 USB-C): 꽂자마자 USB 포트 4개 `over-current change`가 약 5 s 간격 6회, LifeCam·ReSpeaker가 끊겼다 붙기 반복, 터치 장치는 열거 안 됨. 빼면 재부팅 없이 복구, vision-node는 `camera read failed`로 자동 재시작 6회 | 2026-10-23 | `dmesg`, `lsusb` | #64 댓글 (ImymemineJ, 10-23) |
| 외부 전원 USB 허브(GenesysLogic `05e3:0610`·`0626`) 경유: 터치가 `USB 2.0 Hub 1a40:0101` 아래 `27c0:0858` `WingCool Inc. TouchScreen`(`event4`·`event5`)으로 열거, 과전류 0 | 2026-10-23 | `dmesg`, `/proc/bus/input/devices` | #64 댓글 (ImymemineJ, 10-23) |

## 9. 전원

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| Pi 5 PD: `max_current` = `0x1388`(5000 mA), PDO 5 V 5 A / 9 V 3 A / 12 V 2.25 A / 15 V 1.8 A, `usb_max_current_enable` = 1 | 2026-10-19 | `/proc/device-tree/chosen/power/*` | #48 댓글 (Jik-Kim, 10-19) |
| Pi 5 4스레드 추론 2분(425프레임): `rpi_volt/in0_lcrit_alarm` 5초 간격 24회 모두 0, dmesg 저전압 0건, SoC 32.0 → 최고 42.45 °C | 2026-10-19 | 같음 | #48 댓글 (Jik-Kim, 10-19) |
| Pi 4 원래 어댑터 `XSC-0502000SEKU` DC 5V 2000mA. 5V 3A로 교체 후 재부팅: `in0_lcrit_alarm=0`, dmesg 전압 경고 없음 | 2026-10-19 19:55 | 라벨, poweroff → 교체 | #48 댓글 (Jik-Kim, 10-19), PR #76 본문 |
| 디스플레이 1의 `5V+Touch`를 Pi 5 USB에 꽂으면 USB 포트 전원이 차단돼 마우스·카메라가 사라짐 | 2026-09-20 | — | [P5 §5-3] |
| Pi 4 케이스 팬(5V 핀 급전)은 `poweroff` 뒤에도 돎 | 문서 2026-09-25 | — | [WG §9] |

## 10. 배포·기동 (arc·ipk·systemd)

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| `arc create safety_core -t native-service` → `Error: Project name must start with a letter and use only lowercase letters and numbers (e.g., 'safetycore')` | 2026-10-19 | 마스터 PC `atlas-dev` (arc 0.5.0) | #46 댓글 (ImymemineJ, 10-19) |
| `docker exec -it atlas-dev arc build ./safetycore` → `Build failed: spawn cmake ENOENT`(SDK env가 `/root/.bashrc`에서만 source됨). `bash -ic "… arc build ./safetycore"` → `Build successful`. TTY가 없으면 `Raw mode is not supported` | 2026-10-19 | 같음 | #46 댓글 (ImymemineJ, 10-19) |
| ipk 안에 systemd 유닛 없음. 바이너리 `ELF 64-bit LSB pie executable, ARM aarch64 … interpreter /lib/ld-linux-aarch64.so.1 … for GNU/Linux 5.15.0`(glibc 동적 링크) | 2026-10-19 | tar 목록, `file` | #46 댓글 (ImymemineJ, 10-19) |
| `arc install … -d rpi4` → `Success`, 설치 직후 바로 실행됨. 사용자 `u0_a5021`, `CapEff: 0000000000000000` | 2026-10-19 17:26 | `busctl`, `/proc/PID/status` | #46 댓글 (ImymemineJ, 10-19) |
| uid 5021은 `/dev/i2c-1`·`/dev/ttyAMA3`·`/dev/ttyAMA5` 모두 `Permission denied`. i2c(11002)·dialout(20) 그룹을 붙이면 `OPEN_OK`. root는 세 장치 모두 `OPEN_OK` | 2026-10-19 | `setpriv --reuid=5021 … sh -c "exec 3<>/dev/…"` | #46 댓글 (ImymemineJ, 10-19) |
| 재부팅 ①(ipk만): `(activatable)`, PID 없음. 호출하면 `Call failed: Launch helper exited with unknown return code 1`, journal `Failed to request bus name (Permission denied)`, dbus `Unknown username "u0_a5021" in message bus configuration file` | 2026-10-19 17:28 | `busctl`, `journalctl -b` | #46 댓글 (ImymemineJ, 10-19) |
| 재부팅 ②(`/data/share/usr/lib/systemd/system` 유닛 + `multi-user.target.wants` 링크): `inactive`/`dead`. `[    8.728691] … Mounted /data.` / `[   18.618391] Reached target Multi-User System.` | 2026-10-19 17:36 | `journalctl -b -o short-monotonic` | #46 댓글 (ImymemineJ, 10-19) |
| SafetyCore 실제 바이너리: `arc build ./safety_core` → `Build successful` → install → `is-active: active`, `USER=root`, D-Bus 이름 소유, `GetState` OK. 재부팅 뒤 `inactive`(`Mounted /data` 8.28 s), 수동 `systemctl daemon-reload && systemctl start` → 기동, `GetState` OK | 2026-10-19 19:56 | arc build/install, `systemctl` | PR #76 본문 |
| F2: root 프로세스 `kill -9` → `Failed with result 'signal'` → `Started …`까지 1.29 s | 2026-10-19 | `systemctl start`, `kill -9` | #46 댓글 (ImymemineJ, 10-19) |

## 11. 네트워크·시간

| 사실 | 날짜 | 방법·명령 | 출처 |
|---|---|---|---|
| 두 보드 모두 Wi-Fi로 가정용 공유기에 접속, IP가 재부팅마다 바뀜 | 2026-09-20 | — | [WG §9], [P5 §9] |
| NTP 미동작: `System clock synchronized: no`, `Timeservers = [  ]`, `TimeserverSynced = False`, `Time zone: n/a (UTC)`. Pi 4 `ping -c1 8.8.8.8` 성공, `ping: bad address 'kr.pool.ntp.org'`, `cat: can't open '/etc/resolv.conf': No such file or directory` | 2026-09-26 | `timedatectl`, `connmanctl clock` | [WG §8-B] |
| RTC: Pi 4 `RTC time: n/a`, Pi 5 `RTC time: Thu 1970-01-01 00:04:05`. 재부팅 직후 Pi 4 약 19일, Pi 5 약 2일 늦음 | 2026-09-26 | `date`, `timedatectl` | [WG §8-B] |
| NTP 서버를 IP로 지정하면 동기화: Pi 4 `Offset: +509us`, Pi 5 `Offset: +222us`(`/data/ntp_sync.sh`). 재부팅하면 다시 적용해야 함 | 2026-09-26 | `timedatectl timesync-status` | [WG §8-B] |
| 10/19 Pi 4 `date` → `Sat Sep 26`(기기 시계가 실제와 다름) | 2026-10-19 | `date` | #46 댓글 (ImymemineJ, 10-19) |
| Pi 5에서 `nc <Pi4> 7420`에 hello → state 즉시 + 500 ms 간격 | 2026-10-19 | `(printf hello; sleep 1.3) \| nc` | PR #76 본문 |
| Mac → Pi 4 SafetyCore TCP 점검 11/11: 첫 state 6 ms, 간격 499~501 ms, 명령 후 10 ms, hello 없으면 5.01 s 뒤 끊김, 4096 B 초과·9번째 연결은 즉시 끊김 | 2026-10-19 | 점검 스크립트(#75 스키마) | PR #76 본문 |
| Pi 5 Wi-Fi 절전이 기본 on(`brcmf_cfg80211_set_power_mgmt: power save enabled`, `Power save: on`). `iw dev wlan0 set power_save off`로 끔, 재부팅 뒤 다시 on. 절전 on 상태에서 Pi 5가 한 번 네트워크에서 사라짐(화면·LED 정상, ping·SSH 불가, 서브넷에 없음) — 절전과의 관계는 미확인 | 2026-10-23 | `dmesg`, `iw dev wlan0 get power_save` | #64 댓글 (ImymemineJ, 10-23) |
| Pi 5 Wi-Fi는 connman 관리(공유기 서비스 `*AR`). `connmanctl disable wifi` 10 s → `enable wifi`: Core `VISION_STALE` 약 2 s 뒤, `hello (vision)` 재접속 약 13 s 뒤, 옛 연결은 재접속 약 2.5 s 뒤 `recv error`로 정리 (F3) | 2026-10-23 | Pi 4 journal `[tcp]`·`[state]` | #64 댓글 (ImymemineJ, 10-23) |

## 12. 데모 박스 측정 (임계값 근거)

**열화상 — 손난로 단계별 (2026-09-26, `box_hotpack.csv` 692행, 2~1407초, 평균 간격 2.03초, `drop` 0) [DB §A-결과]**

| 손난로 | 구간(초) | max | median | `max − median` | hot_px |
|---|---|---|---|---|---|
| 45도 | 41~200 | 42.19±0.34 | 28.31 | 13.89±0.35 (11.31~14.34) | 69~92 |
| 50도 | 274~429 | 45.59±0.12 | 28.36 | 17.24±0.13 | 69~82 |
| 55도 | 480~763 | 49.42±0.22 | 28.21 | 21.21±0.26 | 70~85 |
| 60도 | 816~1104 | 53.13±0.17 | 28.29 | 24.84±0.16 | 78~81 |
| 65도 | 1171~1407 | 56.95±0.10 | 28.34 | 28.61±0.09 | 79~88 |

- 인접 간격 `+2.48`, `+2.73`, `+2.62`, `+3.28`°C로 겹침이 없습니다. 경계 16은 14.34와 16.82 사이, 22는 21.74와 24.36 사이입니다. 경계 8은 OFF 구간이 없어 이 파일로는 검증할 수 없습니다 [DB §A-결과].
- 손난로 없는 상태(`box_off_person.csv`): OFF `max − median` 11.86±0.09(11.62~12.05), 사람이 섰을 때 11.53±0.18, 빈 상태 11.65±0.07. 사람이 서면 median +0.65°C, max는 그대로 [DB OFF·사람·빈 상태].
- 손난로 없이도 약 40°C 열점이 있고, 위치는 디스플레이 1입니다(스트림 영상으로 확인). 자체 발열인지 손난로 잔열인지는 확인되지 않았습니다 [DB OFF·사람·빈 상태].
- 09-02 설치(다른 배치)에서 잰 값은 [SM §3]에 있고, 시연 박스에서는 아래 [대체된 값](#대체된-값)처럼 일부가 무효가 됐습니다.

**미세먼지 — 인센스·가습기 (2026-09-26) [DB §B-결과], [SH]**

| 회차 | n | PM2.5 | tsize | fine |
|---|---|---|---|---|
| 평소 | 62 | 16.5~27.8 | 0.426~0.461 (평균 0.439) | 0.860(전 행 같음) |
| 인센스 (PM2.5 > 50) | 21 | 77.5~1849.3 | 0.394~0.438 (평균 0.417) | 0.792~0.860 |
| 가습기 1단 `box_mist_1.csv` | 65 | 50.8~1902.5 | 0.457~1.512 (중앙 0.917) | 0.191~0.860 |
| 09-02 `mist_2.csv` | 61 | 53.9~1992.1 | 0.509~1.499 (평균 1.059) | 0.200~0.858 |

- 가습기가 닿은 직후 2행은 `tsize 0.457`(`79.7s PM2.5 55.1`, `80.7s PM2.5 55.8`)이고, `82.7s`부터 0.666 이상입니다.
- 1,000 초과 구간 tsize: 연기 0.41~0.44(**09-26 이전 인센스 제품**), 수증기 0.99~1.51.
- **10/24 HEM 골드 에디션(시연 제품)** [IM]: 비포화 1,000~2,000 구간 `tsize` 0.46~1.45(C2·C3·D1·E1 84줄)로 0.41~0.44 밖. 연기 없이 팬만 돌려도 1단부터 0.49 미만(A1 팬 1 0.468~0.501, 팬 2 0.434~0.474, 팬 3 0.438~0.462). 같은 날 이전 제품 1개(P1·P2)는 비포화 PM2.5 > 100 줄의 `tsize` 중앙 0.421·0.425로 09-26 값과 맞음 → **`tsize`는 인센스 제품에 따라 달라 hazard 판정 근거로 쓸 수 없음** — PR #125(D-054)에서 hazard 판정(과한 연기)에서는 빠졌고, 표시 전용 PM_WARN 시작 조건(`small = pm_saturated || tsize < P_tsize`)에는 남음. `P_tsize` 재검토는 [IM] 「이전 문서 수정 필요 사항」의 `2026-10-22-sps30-saturation.md` 행
- 이 CSV로 계산한 규칙 비교(재현 python, [DB §B-결과], [SH 규칙 후보 비교]). 오탐은 box_mist_1 / mist / mist_2 순서입니다.
  - PM2.5 ≥ 50 ∧ `tsize < 0.49` ∧ 3행 연속: 첫 판정 78.7초(+2.0), 오탐 0/0/0.
  - 기준선 +50 ∧ `tsize < 0.49` ∧ 3행 연속: 첫 판정 78.7초(+2.0), 오탐 0/0/0. 기준선 = 기록 첫 30초 PM2.5 중앙값(인센스 회차 20.8). SafetyCore Policy는 이 값이 아니라 부팅 후 `P_baseline_init_s`(60 s) 동안의 중앙값을 기준선으로 씁니다(설계서 §3.1) — 재생 결과가 다르면 이 차이부터 봅니다.
  - 기준선 +30 ∧ `tsize < 0.49` ∧ 3행 연속: 첫 판정 78.7초(+2.0), 오탐 0/1/0.
  - 이 결과가 설계서 §3.3 4·6행의 근거입니다(값은 #65에서 확정).
- 향 10 cm에서 PM2.5 `1057.9 → 1849.3`(사양 0~1,000 밖). 측정 뒤 `sps30_diag` CRC 오류 0/4.

## 출처끼리 다른 값

| # | 항목 | 값 A | 값 B | 메모 |
|---|---|---|---|---|
| 1 | Pi 4 OS 식별 | os-release 출력 없음 [DS §1-1, 08-30] | `NAME="ATLAS Platform"` … [RG, 09-20] | 차이의 원인은 기록에 없음 |
| 2 | MLX `delta` 정의 | `max − median` [SM §2, HS 열화상] | 배포본 CSV는 `p95 − median` [DB §A-1, 09-26] | 임계값 8/16/22는 `max − median`으로 산출된 것이 맞음 [DB 이전 문서 수정 필요 사항] |
| 3 | DS3231 | 09-02 문서는 `0x68` 연결로 적음 [HS, 정인 진행 보고] | 09-20 `0x68` 없음, 연결된 적 없음 [WG §1-C] | 10-19에 연결해 `0x68` 응답(#45). 시간순으로는 대체 |
| 4 | 비전 FPS | 3.7·3.8 (VisionService GPU, 08-30) · 3.2 (V4L2 실험, 09-20) | 3.39·3.52 (2·4스레드) · 3.59 (10분) · 2.78·2.86 (30프레임, 예열 포함) (10-19) | 조건이 모두 다름. 현재 경로 기준은 10-19 값 |
| 5 | `HC died` 원인 | 카메라 쪽 USB 포트 [P9, MP] | 디스플레이 2 터치 USB 직결 때 발생, 카메라가 함께 사라짐 [P5 §5-6] | 같은 사건인지 미확인 |
| 6 | 디스플레이 2 터치 USB ID | 직결 `0416:c168` `MTouch` [P5 §5-6] | 허브 경유 `27c0:0858` `WingCool Inc. TouchScreen` [P5, DT] | 같은 장치의 다른 열거인지 미확인 |
| 7 | 기록 시각 | PR #82(#83) 「10/20 21:11」·「21:16」, PR #78 댓글 「21:5x」 | 각 게시 시각(19:53·21:48 KST)보다 늦음 | 기기 시계는 9/26에 머물러 있어 그 값도 아님. 작성자 확인 필요 |
| 8 | 디스플레이 1 열원 | 원인 [추정] [DB] | 「약 40°C 발열」을 확정처럼 적음 [SH, LC §6] | 사실은 「열점 위치 = 디스플레이 1」까지 |
| 9 | 45도 구간 `max − median` | 13.89±0.35, hot_px 69~92 (순간 하락 포함) [DB §A-결과] | 13.93±0.18, 69~78 (하락 제외) [DB OFF 표] | 같은 데이터, 구간 처리만 다름 |
| 10 | `/data` 마운트 시각 (Pi 4) | 8.73 s (#46, 17:36 재부팅) | 8.28 s (PR #76, 19:56 재부팅) | 재부팅마다 다름. Pi 5는 7.27 s (#61) |

## 대체된 값

| 항목 | 이전 | 이후 | 근거 |
|---|---|---|---|
| 열화상 정상 상한 | 8 (09-02 배치) [HS, SM] | 무효 — 시연 박스 빈 상태 `max − median` 11.86(최대 12.05). 경계 16·22는 유지 | [DB], [SH] |
| hot_px로 사람·손난로 구분 | 손난로 12~28 vs 사람 101~185 (09-02) [SM] | 무효 — 사람 78~99 vs 손난로 69~92로 겹침 | [DB] |
| 55·60도 겹침 | 09-02: 0.9σ로 겹침 [SM §3-2] | 09-26: 간격 +2.62, 겹침 없음 | [DB] |
| SPS30 3조건(`fine > 0.90` 등) | 「수증기 오탐 0건」 [HS] | 인센스에서 `fine` 최대 0.860, 3조건 충족 0행 → 판정 불가 | [DB], [SH], PR #40 |
| 팬 명령 경로 | USB `Serial`(`/dev/ttyACM0` 가정) | CH340은 Pi에서 tty가 안 생김 → GPIO UART `Serial2`(`/dev/ttyAMA3`). USB는 디버그 전용 | [WG §2-C], #44, PR #78 |
| 팬 펌웨어 넘친 줄 처리 | `ERR command-too-long` 뒤 `ERR invalid-command`, 뒷부분을 명령으로 해석 | `7f90998`: ERR 1회, 다음 `\n`까지 버림 | PR #78 댓글 |
| 비전 캡처 | OpenCV videoio(실행 불가) | V4L2 직접 캡처, 가장 새 버퍼만 사용(지연 평균 1.4 s → 0.22 s) | PR #32·#33·#77 |
| Pi 4 어댑터 | 5V 2A `XSC-0502000SEKU` | 5V 3A (10-19 19:55) | PR #76 |
| 팬 신호선 직렬 저항 | 2.2 kΩ (PR #78 댓글 10-20) | 잘못 읽은 값 — 실제 220 Ω였고 10/20 1 kΩ로 교체. §4의 왕복·F5 수치는 220 Ω 때 결과 | PR #78 댓글 (ImymemineJ, 10-20 정정) |
| 부팅 자동 기동 방침 | 「안 뜨면 systemd 폴백」 | `/data` 유닛도 부팅 때 무시됨 → 지금은 수동 start. 방법은 #46에서 결정 | #46 |
| 0902 `sps30_test.cpp`·`mlx_test.cpp` | 센서 로거로 쓰임 | `rand()`로 값을 만드는 코드였음. 09-26 삭제, 실제 로거는 `*_bundle.sh` 산출 소스 | [DB 이전 문서 수정 필요 사항] |
| 연기의 `tsize` (1,000 초과) | 0.41~0.44 → `tsize < 0.49`가 연기 근거 (09-26) [DB], [SH] | 이전 제품에서만 맞음. HEM 골드(시연 제품)는 비포화 0.46~1.45, 팬 바람만으로 0.49 미만 (10/24) | [IM] |

## 다른 문서와 다른 점 (고칠 곳)

이 PR은 아래 문서를 고치지 않습니다. 각 담당자에게 요청합니다.

| 문서·위치 | 지금 문구 | 실측 | 근거 | 요청 |
|---|---|---|---|---|
| 설계서 46·340행, 대장 D-034, BRD 59·114·146·196·297·460행 | MLX90640 2 Hz | refresh(서브페이지) 2 Hz = 전체 프레임 **1 Hz** | #45, §3 | @lambdaramji |
| BRD 306행 | 빌드 환경 「musl + clang 20.1.1」 | 산출 바이너리는 glibc 동적 링크(`interpreter /lib/ld-linux-aarch64.so.1`) | #46, §10 | @lambdaramji |
| 설계서 485행 (§8.2) | `arc create safety_core -t native-service` | 이름 규칙 오류 → `safetycore` | #46, §10 | @lambdaramji |
| 설계서 488·494행 (§8.2), 404행 (§6 F1), BRD 263행 (TR-SYS-04) | `run_after_installation` / 「SafetyCore 자동 시작(run_after_installation / systemd)」 | 재부팅 뒤 자동 기동 안 됨(ipk는 D-Bus 권한 거부, `/data` 유닛은 마운트가 늦어 무시). 지금은 수동 start | #46, PR #76, §10 | @lambdaramji |
| 설계서 367행 (§5.2) | 입력 「V4L2 MJPEG/YUYV 640×480」 | YUYV 640×480만 확인 | PR #77, [VF], §6 | @Jik-Kim |
| 설계서 368행 (§5.2) | 「fork 실측 313.7 ms CPU」(09-20) | 10-19 4스레드 p95 311 ms, 10분 평균 277.2 ms | #61, PR #77, §6 | @Jik-Kim |
| `docs/hardware-connection-and-measurement-plan.md` 36·38행 | ESP32 「USB CDC 115200」, MLX90640 「실물·I2C 주소·배선·사진 미확인」 | ESP32는 GPIO UART `Serial2`. MLX90640 `0x33`·SPS30 `0x69` 응답 확인(사진은 아직 없음) | §3·§4 | @lambdaramji |
| `inbox/정정인/2026-09-02-hardware-status-and-spec.md` §열화상, `…-sensor-metrics-analysis.md` §2 | CSV `delta` = `max − median` | 배포본 CSV `delta` = `p95 − median` | [DB §A-1] (그 문서 「이전 문서 수정 필요 사항」에도 있음) | @ImymemineJ |

## 아직 실측 안 됨

값이 아직 없어서 이 문서에 넣지 못한 항목입니다. 측정되면 위 절로 옮깁니다.

| 항목 | 확인 방법 | 이슈 |
|---|---|---|
| 아크릴 위 LED 밝기·카메라 노출(`BRI`) | 시연 박스에서 아크릴을 덮고 조정 | #65 |
| `IND 3` 10분 점등 때 LED 발열이 열화상 `max − median`에 주는 영향 | #65 Q1을 `IND 3` 상태로도 측정 | #65 |
| 쿡탑 판정 연동 L6·M5·M6 | #64 체크리스트 | #64 |
| LED 판 30 % 풀 점등 전류·전압, Pi 5 부하와 동시 10분 | 전류계·`in0_lcrit_alarm` | #48 |
| 팬 PWM 듀티·MOSFET 발열·팬 기동 전류 | 팬·IRLZ44N 연결 뒤 측정 | #64 |
| Pi 4를 끈 상태의 신호선 역류 전류 | 멀티미터로 ESP32 TX 선 전류 | — (`firmware/esp32_fan_controller/README.md`) |
| 재부팅 뒤 자동 기동 방법 | **결정됨(#46 결정 (4), 10/23): 자동 기동 없음, 재부팅 뒤 수동 기동 1회.** LG 공식 경로는 10/25까지 미회신 — 오면 결론만 #46에 기록 | #46 |
| Pi 5 vision → Pi 4 Core 실제 연결, LAN 케이블 분리(F3), 카메라 분리(F7) | #64 체크리스트 | #64 |
| Flutter 대시보드 TCP 재접속(케이블 10~15 s 분리) | Core 로그 `hello (ui)` 시각 | #64 |
| 단일 프로세스 sensor 스레드의 I2C 간격 | SafetyCore 로그·CSV | #64 |
| SPS30 고농도 샘플 공백의 원인(I2C 오류 vs 센서) | 공백 구간 I2C 오류 로그 | #64, #65 |
| 임계값 재실측(손난로·인센스·가습기 각 3회), 65 °C 단독 danger 여부, 정상 상한 재산정 | #65 측정 절차 | #65 |
| 디스플레이 1 열점이 자체 발열인지 잔열인지 | 디스플레이만 켠 상태 열화상 | #65 |
| 고양이 인형의 ROI 중앙·가장자리·밖 `dog` score·`roi_overlap`, 빈 장면 `cat`·`dog` 최고 score, 실물 고양이 탑뷰 | 시연 박스 라이브 (#65 Q8) | #49, #65 |
| DS3231로 시각 유지(`hwclock`) | 재부팅 뒤 `date` 비교 | — |
| 기록 시각 이상(위 [다른 값 7](#출처끼리-다른-값)) | 작성자 확인 | PR #78, #82 |

## 재현 방법

자주 다시 확인하는 값의 명령입니다. 모두 위 출처에 적힌 명령이고, 기기에서 root로 실행합니다.

```bash
# Pi 4: UART 노드와 권한 (#44)
ls -l /dev/ttyAMA* /dev/ttyS* /dev/serial*
# Pi 4: I2C 장치와 버스 속도 (#45)
i2cdetect -y 1
od -An -tu1 /proc/device-tree/soc/i2c@7e804000/clock-frequency   # 0 1 134 160 = 100 kHz
# Pi 5: 카메라 이름 (DS §1-5)
for d in /sys/class/video4linux/*/name; do cat $d; done
# 재부팅 뒤 /data 마운트 시각 (#46)
journalctl -b -o short-monotonic
# 시간 동기화 상태 (WG §8-B)
timedatectl
```

전원 값은 #48 댓글이 읽은 경로 `/proc/device-tree/chosen/power/max_current`·`usbpd_power_data_objects`와 hwmon `rpi_volt`의 `in0_lcrit_alarm`입니다. 그 댓글에는 명령 원문이 없습니다.

## 근거 목록

**저장소 문서** (main 기준)

| 약어 | 경로 |
|---|---|
| DS | [inbox/김병직/2026-08-30-device-survey-and-vision-test.md](../inbox/김병직/2026-08-30-device-survey-and-vision-test.md) |
| RG | [inbox/김병직/2026-09-20-vision-node-pi5-runtime-gap.md](../inbox/김병직/2026-09-20-vision-node-pi5-runtime-gap.md) |
| VF | [inbox/김병직/2026-09-20-vision-node-v4l2-fix-and-live-test.md](../inbox/김병직/2026-09-20-vision-node-v4l2-fix-and-live-test.md) |
| HS | [inbox/정정인/2026-09-02-hardware-status-and-spec.md](../inbox/정정인/2026-09-02-hardware-status-and-spec.md) |
| SM | [inbox/정정인/2026-09-02-sensor-metrics-analysis.md](../inbox/정정인/2026-09-02-sensor-metrics-analysis.md) |
| DB | [inbox/정정인/2026-09-20-demo-box-threshold-measurement.md](../inbox/정정인/2026-09-20-demo-box-threshold-measurement.md) |
| DT | [inbox/정정인/2026-09-20-display-touch-verification.md](../inbox/정정인/2026-09-20-display-touch-verification.md) |
| P5 | [inbox/정정인/2026-09-20-pi5-display-troubleshooting.md](../inbox/정정인/2026-09-20-pi5-display-troubleshooting.md) |
| WG | [inbox/정정인/2026-09-25-wiring-guide.md](../inbox/정정인/2026-09-25-wiring-guide.md) |
| SH | [inbox/정정인/2026-09-26-sensor-logic-handoff.md](../inbox/정정인/2026-09-26-sensor-logic-handoff.md) |
| LC | [inbox/정정인/2026-10-04-single-display-led-cooktop.md](../inbox/정정인/2026-10-04-single-display-led-cooktop.md) |
| LP | [inbox/정정인/2026-10-19-led-matrix-power-without-capacitor.md](../inbox/정정인/2026-10-19-led-matrix-power-without-capacitor.md) |
| CF | [firmware/esp32_cooktop/README.md](../firmware/esp32_cooktop/README.md) |
| P9 | [docs/decision/P-009-pi4-pi5-role-reallocation.md](decision/P-009-pi4-pi5-role-reallocation.md) |
| MP | [docs/hardware-connection-and-measurement-plan.md](hardware-connection-and-measurement-plan.md) |
| 설계서 | [docs/2026-10-19-s3-s10-design-v0.1.md](2026-10-19-s3-s10-design-v0.1.md) |
| BRD | [docs/2026-10-19-brd-v0.1.md](2026-10-19-brd-v0.1.md) |
| IM | [inbox/정정인/2026-10-24-incense-matrix.md](../inbox/정정인/2026-10-24-incense-matrix.md) (PR #122) |

**GitHub** (lambdaramji/HOOD-SEEK)

- 이슈 댓글: [#44](https://github.com/lambdaramji/HOOD-SEEK/issues/44) UART·USB 시리얼 · [#45](https://github.com/lambdaramji/HOOD-SEEK/issues/45) I2C·센서 주기 · [#46](https://github.com/lambdaramji/HOOD-SEEK/issues/46) arc·ipk·부팅 · [#48](https://github.com/lambdaramji/HOOD-SEEK/issues/48) 전원 · [#49](https://github.com/lambdaramji/HOOD-SEEK/issues/49) 고양이 검출 · [#61](https://github.com/lambdaramji/HOOD-SEEK/issues/61) vision-node 실기
- PR 본문·댓글: [#10](https://github.com/lambdaramji/HOOD-SEEK/pull/10) · [#14](https://github.com/lambdaramji/HOOD-SEEK/pull/14) · [#32](https://github.com/lambdaramji/HOOD-SEEK/pull/32) · [#33](https://github.com/lambdaramji/HOOD-SEEK/pull/33) · [#40](https://github.com/lambdaramji/HOOD-SEEK/pull/40) · [#76](https://github.com/lambdaramji/HOOD-SEEK/pull/76) · [#77](https://github.com/lambdaramji/HOOD-SEEK/pull/77) · [#78](https://github.com/lambdaramji/HOOD-SEEK/pull/78) · [#82](https://github.com/lambdaramji/HOOD-SEEK/pull/82)(#83 포함) · [#87](https://github.com/lambdaramji/HOOD-SEEK/pull/87) · [#89](https://github.com/lambdaramji/HOOD-SEEK/pull/89) · [#92](https://github.com/lambdaramji/HOOD-SEEK/pull/92)
