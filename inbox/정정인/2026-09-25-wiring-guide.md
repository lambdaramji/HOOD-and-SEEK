# 전체 부품 배선 가이드 (하드웨어 담당 인수용)

- 작성자: 정정인
- 작성일: 2026-09-25
- 상태: 2026-09-26 시연 박스 배선 반영 (§8-A) · DS3231·팬·디스플레이는 미연결
- 카테고리: hardware
- 대상 부품: Pi4, Pi5, 디스플레이 1·2, SPS30, MLX90640, ESP32, LifeCam HD-3000, ReSpeaker Lite, 스피커, 4CH 로직 레벨 컨버터, DS3231 RTC 모듈

---

## 표기 규칙

| 표기 | 뜻 |
|---|---|
| **[실측]** | 기기에서 직접 실행해 확인 (명령·날짜 병기) |
| **[문서]** | 데이터시트·공식 문서·저장소 코드 인용 |
| **[추정]** | 아직 확인 못 함. **확인 방법** 병기 |

핀 번호는 모두 **40핀 헤더의 물리 핀 번호**입니다 (BCM GPIO 번호 아님). Pi4와 Pi5 헤더 배치는 같습니다 [문서: Raspberry Pi GPIO pinout].

---

## 요약 — 뭐가 어디에 붙는가

```
┌──────────── Pi4 (센서 노드, ATLAS Headless) ──────────────────┐
│  40핀 I2C1 (핀 3 SDA · 핀 5 SCL)                               │
│     ├─ MLX90640   0x33   3.3V                                  │
│     ├─ DS3231     0x68   3.3V  ← 아직 미연결 (이번에 연결)      │
│     └─ SPS30      0x69   5V VDD · SEL→GND                       │
│  USB-A ── LifeCam HD-3000 (/dev/video0)                         │
│  USB-C ── 5V/3A 어댑터                                          │
└────────────────────────────────────────────────────────────────┘
                 │ Wi-Fi · MQTT
                 ▼
┌──────────── Pi5 (게이트웨이 + HMI, ATLAS Video) ───────────────┐
│  micro-HDMI ── 디스플레이 1 또는 2 (한 번에 하나)                │
│  USB-A ── 【외부 전원 USB 허브】                                 │
│              ├─ ESP32 (USB CDC, /dev/ttyACM0)                   │
│              ├─ ReSpeaker Lite (USB 오디오)  ── 스피커            │
│              ├─ 디스플레이 2 터치 (5V+Touch)   ※ 디스플레이 2일 때 │
│              └─ 마우스                          ※ 디스플레이 1일 때 │
│  USB-C ── 27W(5V/5A) 어댑터 권장                                 │
│  40핀 ── 아무것도 안 붙임                                        │
└────────────────────────────────────────────────────────────────┘

  ESP32 GPIO26 ──▶ IRLZ44N 게이트 ──▶ 팬(−) / 팬(+) ← 허브 5V 분기
  4CH 로직 레벨 컨버터 ──▶ 현재 구성에서 사용처 없음 (보관)
```

한 줄 원칙: **센서는 전부 Pi4, 사람이 만지는 건 전부 Pi5.**

---

## 0. 시작 전 준비

| 항목 | 이유 |
|---|---|
| 모든 배선은 **두 보드 전원을 뺀 상태**에서 | I2C 핀은 핫플러그 불가. 3.3V 라인을 5V 핀에 잘못 대면 그 자리에서 센서가 죽습니다 |
| 40핀 헤더 **핀 1 위치**를 먼저 찾기 | 보드를 USB 포트가 오른쪽으로 오게 놓으면 헤더 왼쪽 위(SD 카드 쪽 모서리)가 핀 1. 사각 패드로 표시됨 [문서] |
| 점퍼선 색 통일 | 빨강 5V · 주황 3.3V · 검정 GND · 파랑 SDA · 노랑 SCL. 색이 섞이면 사진 찍어 두기 |
| 정상 판정 기준 미리 알기 | Pi4: `i2cdetect -y 1`에 `33 68 69` (+ `57`). Pi5: `lsusb`에 ESP32·ReSpeaker·터치 |

### 납땜이 필요한 곳

| 연결 | 납땜 | 대안 |
|---|---|---|
| I2C 센서 3개 ↔ 40핀 (점퍼선) | 불필요 | 브레드보드 또는 Y 점퍼. 시연 박스에서는 흔들리지 않게 고정 |
| DS3231 헤더 핀 | 보통 불필요 (핀 선납땜 출고가 다수) | 핀이 따로 든 봉지면 납땜 4곳 |
| ESP32 DevKit | 불필요 | 헤더 선납땜 |
| ReSpeaker Lite, 웹캠, 디스플레이 | 불필요 | 전부 커넥터 |
| **IRLZ44N + 팬 + 허브 5V 분기** | **필요 (또는 나사 단자)** | 팬 전류가 흐르는 길이라 점퍼선·브레드보드는 접촉 불량으로 팬이 꺼질 수 있음. ① 만능기판에 납땜 ② **나사 단자대(터미널 블록)** ③ 레버 커넥터(Wago 221류). 나사 단자 달린 MOSFET 모듈 보드가 있으면 납땜 0 |
| 스피커 → ReSpeaker JST 2핀 | 스피커에 플러그가 없으면 필요 | JST PH 피그테일에 납땜 + 수축튜브. 손 크림핑은 비추천 |

### 40핀 헤더에서 이번에 쓰는 핀

| 물리 핀 | 이름 | 쓰는 곳 |
|---|---|---|
| **1** | 3.3V | MLX90640 VCC |
| **2** | 5V | SPS30 VDD |
| **3** | GPIO2 / SDA1 | 센서 3개 SDA 공통 |
| **5** | GPIO3 / SCL1 | 센서 3개 SCL 공통 |
| **6** | GND | SPS30 GND |
| **9** | GND | MLX90640 GND |
| **14** | GND | DS3231 GND |
| **17** | 3.3V | DS3231 VCC |

SDA·SCL은 핀 3·5 하나씩뿐이라 **브레드보드나 Y자 점퍼로 세 갈래**를 냅니다. 총 배선 길이는 ≤10cm 권장 (이전 문서와 동일).

---

## 1. Pi4 — 센서 3종 (I2C1)

### 1-A. MLX90640 열화상

| MLX90640 보드 핀 | → Pi4 물리 핀 | 비고 |
|---|---|---|
| VCC / VIN / 3V3 | **1** (3.3V) | 센서 정격 3.3V [문서: MLX90640 데이터시트]. 보드에 VIN·3V3이 둘 다 있으면 **3V3**에 |
| GND | **9** | |
| SDA | **3** | |
| SCL | **5** | |

- 절대 **5V 핀(2·4)에 대지 않습니다.**
- 확인: `i2cdetect -y 1` → `0x33` [실측 2026-09-20 응답 확인].

### 1-B. SPS30 미세먼지

SPS30 커넥터는 5핀 JST ZH(1.5mm). 핀 번호는 하우징의 삼각형·`1` 표시 쪽이 1번입니다 [문서: Sensirion SPS30 데이터시트 §3 Pin description].

| SPS30 핀 | 이름 | → Pi4 물리 핀 | 비고 |
|---|---|---|---|
| 1 | VDD | **2** (5V) | **5V**입니다. 3.3V로 주면 팬이 안 돌아 측정 불가 |
| 2 | SDA/RX | **3** | 3.3V 로직 직결. 이전 문서 「SDA/SCL 3.3V 직결」과 동일 |
| 3 | SCL/TX | **5** | |
| 4 | SEL | **SPS30 자기 GND(핀 5)에 짧게 점퍼** | **필수.** 띄우면 UART 모드가 되어 I2C에 안 잡힘 |
| 5 | GND | **6** | |

- SEL은 Pi GND까지 끌고 가지 말고 **커넥터 바로 옆에서 핀 5와 묶는** 게 안전합니다. 배선이 빠져도 SEL이 같이 빠지지 않게.
- 배치: **가로** (세로 배치 무효, [실측 2026-09-02]).
- 확인: `i2cdetect -y 1` → `0x69` [실측 2026-09-20]. `0x69`가 뜨면 SEL 배선까지 맞다는 뜻입니다. 이어서 `/data/mlx/sps30_diag` → 시리얼 `1C451D7CCF1D5D86`, 펌웨어 `2.3`.

#### 흡입구·배출구 — 연기를 어디에 대는가

SPS30은 40.6 × 40.6 × 12.2 mm 검은 사각 모듈입니다 [문서: SPS30 데이터시트 §5 Dimensions]. **측정부는 모듈 내부의 광학 챔버**(레이저 다이오드 + 광검출기 + 팬)이고 겉에서 만질 수 있는 부분이 아닙니다. 외부로 드러난 것은 **흡입구(inlet)와 배출구(outlet) 두 개의 구멍**뿐이며, 공기는 `흡입구 → 내부 광학 챔버 → 배출구` 순으로 지나갑니다. 따라서 **연기는 흡입구 쪽에 둡니다.**

- 두 구멍은 커넥터가 있는 면이 아닌 **옆면에 나란히** 있습니다 [추정 — 확인 방법: 아래 바람 방향 시험].
- **흡입구·배출구 구별법 [실측 가능]**: 센서에 전원을 넣고 측정을 시작하면 내부 팬이 돕니다. 얇은 휴지를 1cm 폭으로 잘라 각 구멍 앞 1cm에 대 보세요. **바깥으로 밀려나면 배출구, 안쪽으로 빨려들면 흡입구**입니다.
  ```bash
  /data/mlx/sps30_test log /tmp/airflow.csv 30   # 팬 도는 동안 휴지로 확인
  ```
- 배치는 **가로** — 두 구멍이 옆을 향하고 모듈 바닥이 아래로 [실측 2026-09-02, 세로 배치 무효].
- 두 구멍 앞 **최소 2cm는 비웁니다.** 막히면 유량이 줄어 측정값이 낮게 나옵니다.
- 배출구 공기가 곧바로 흡입구로 되돌아가지 않게 합니다 (벽에 바짝 붙이면 재순환). 시연 박스는 앞이 뚫려 있으므로 구멍을 **뚫린 앞쪽**으로 향하게 둡니다.

**인센스 측정 시**: 향을 흡입구에서 **10~20cm** 떨어뜨리고 연기가 자연스럽게 흘러들게 합니다. 구멍에 바짝 대거나 입으로 불어 넣지 않습니다 — 과농도로 포화되면 `tsize`·`coarse`·`fine` 비율이 왜곡되고, 재현이 안 됩니다.

**하지 말 것** (광학 챔버는 청소·교체 불가):
- 구멍에 이쑤시개·면봉 등 **아무것도 넣지 않기**
- 에어 스프레이(압축 공기) 분사 금지 — 광학면에 먼지가 박힘
- 물·알코올 접촉 금지, 습기 찬 상태에서 측정 금지
- 내부 팬이 도는 중 전원 차단 반복 금지

### 1-C. DS3231 RTC (고정밀 리얼타임 클럭 모듈)

지금까지 **연결된 적 없음** [실측 2026-09-20: `i2cdetect -y 1`에 `0x68` 없음 + 본인 확인].

**왜 필요한가.** Pi4에는 시계를 유지하는 부품이 없어 전원을 끄면 시각을 잃습니다. Pi5는 `observed_at`이 **5초보다 오래됐거나 2초 넘게 미래**인 payload를 버립니다 [문서: `CLAUDE.md` 안전 불변식 `maxAge` 5초 / `maxFutureSkew` 2초]. Pi4 시계가 틀리면 센서 payload가 **전부** 버려지고, Pi5는 3초 안에 유효 데이터가 없으니 `offline` → 팬 3단 + 인덕션 잠금으로 갑니다. 즉 시각 오차는 통신 두절과 같은 결과를 냅니다.
- 인터넷 되는 공유기(지금 Wi-Fi)면 NTP가 부팅 직후 맞춰 주므로 DS3231은 NTP가 붙기 전 창을 메우는 보험입니다.
- 설계도 ②의 **폐쇄 LAN(인터넷 없음)** 으로 가면 NTP 서버가 없어 DS3231이 Pi4 시각의 유일한 근거가 됩니다. 이 경우에도 두 보드를 **한 번은 같은 시각으로 맞춰야** 하고, 이후 drift 보정은 마스터 PC를 LAN NTP 서버로 두는 것이 정석입니다 [추정: ATLAS에 `systemd-timesyncd`/`chrony`가 있는지 — 확인 방법: `timedatectl`, `systemctl status systemd-timesyncd`].
- DS3231을 고른 이유는 온도보상 수정(TCXO)으로 오차 ±2ppm(연 약 1분) [문서: DS3231 데이터시트]. DS1307류는 월 수 분씩 어긋납니다. 이전 문서들은 연결된 것처럼 적혀 있으니 아래 「이전 문서 수정 필요 사항」 참고.

모듈이 두 종류 중 어느 것인지 먼저 봅니다.

| 모양 | 종류 | 연결 방법 |
|---|---|---|
| 파란 기판, 코인 배터리 홀더가 큼, 핀 6개(32K·SQW·SCL·SDA·VCC·GND) | ZS-042형 | 아래 표대로 점퍼 4개 |
| 검은 작은 기판, 암핀 5개짜리 소켓이 붙어 있음 | Pi용 미니 DS3231 | **40핀 헤더 핀 1·3·5·7·9에 그대로 꽂음** (핀 1 = 3.3V 쪽 정렬). 점퍼 불필요 |

ZS-042형 배선:

| DS3231 핀 | → Pi4 물리 핀 | 비고 |
|---|---|---|
| VCC | **17** (3.3V) | **반드시 3.3V.** 5V 금지 — 아래 이유 |
| GND | **14** | |
| SDA | **3** | |
| SCL | **5** | |
| 32K, SQW | 연결 안 함 | |

**5V 금지 이유** [문서: DS3231 데이터시트 VCC 2.3~5.5V + ZS-042 회로도]
1. 모듈의 SDA/SCL 풀업이 VCC에 매달려 있어 5V로 주면 Pi GPIO(3.3V 정격)에 5V가 걸립니다.
2. ZS-042에는 충전식 LIR2032용 충전 회로(다이오드+200Ω)가 있습니다. **CR2032(비충전식)** 를 끼우고 5V를 주면 배터리를 충전하려 들어 위험합니다. 3.3V면 다이오드 강하 후 전압이 배터리보다 낮아 충전 전류가 흐르지 않습니다. [추정: 우리 모듈에 이 회로가 있는지 — 확인 방법: 기판에 `LIR2032` 인쇄 또는 배터리 홀더 옆 다이오드+저항 유무]

- 확인: `i2cdetect -y 1` → `0x68`. ZS-042형은 EEPROM(AT24C32)이 **`0x57`에 같이** 뜹니다. 고장 아닙니다.
- 커널에 붙이기(SD 카드 안 건드림, 재부팅하면 풀림) [추정 — 확인 방법이 곧 아래 명령]:
  ```bash
  ls /sys/class/rtc/                      # Pi4는 비어 있어야 정상 (내장 RTC 없음)
  echo ds3231 0x68 > /sys/bus/i2c/devices/i2c-1/new_device
  ls /sys/class/rtc/                      # rtc0 생기면 드라이버(rtc-ds1307) 내장
  hwclock -r                              # 시각 읽기
  ```
  `rtc0`이 안 생기면 드라이버가 이미지에 없는 것 → 「미확인 사항」으로. `new_device`는 재부팅 시 사라지므로 상시 적용은 `/data`에 스크립트를 두고 부팅 시 실행하는 방법을 따로 정해야 합니다.
- **Pi5에는 DS3231을 붙이지 않습니다.** Pi5는 내장 RTC가 있고 배터리 커넥터(J5)만 있으면 됩니다 [문서: Raspberry Pi 5 공식 문서 RTC 절]. 이전 문서의 「Pi5는 LAN 동기」 방침도 그대로 유효.

### 1-D. LifeCam HD-3000 웹캠

| 웹캠 | → Pi4 |
|---|---|
| USB-A | Pi4 USB 포트 아무 곳 (파란 USB3 포트 권장) |

- 확인: `ls /dev/video*` → `/dev/video0` [실측 2026-09-02].
- Pi4 USB 총 전류 한도 1.2A [문서]. 웹캠 하나는 여유 있음. **Pi4 USB에 다른 걸 더 붙이지 않습니다.**

### 1-E. Pi4 전원

USB-C **5V/3A**. 센서 합산 소비는 SPS30 최대 약 80mA(5V) + MLX90640 약 25mA(3.3V) + 웹캠 수백 mA 수준 [문서: 각 데이터시트]. 3A면 충분합니다.

### 1-F. Pi4 완성 확인

```bash
ssh root@<Pi4 IP>
i2cdetect -y 1
```
기대 출력(ZS-042형 기준):
```
30: -- -- -- 33 -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- 57 -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- 68 69 -- -- -- -- -- --
```
`33`·`69`는 [실측 2026-09-20] 확인. `57`·`68`은 이번 배선 후 채웁니다.

---

## 2. Pi5 — 디스플레이 · 허브 · ESP32 · 음성

### 2-A. 전원부터

| 항목 | 값 | 근거 |
|---|---|---|
| Pi5 어댑터 | **27W (5V/5A) USB-PD** 권장 | 15W 어댑터면 Pi5가 USB 총 공급을 **600mA로 제한** [문서: Raspberry Pi 5 공식 문서 Power supply]. 0920의 `HC died`가 이 한도와 무관하지 않을 수 있음 [추정] |
| 확인 방법 | `hexdump -C /proc/device-tree/chosen/power/max_current` | 마지막 4바이트가 협상된 mA. `00 00 13 88` = 5000mA, `00 00 0b b8` = 3000mA [추정: ATLAS 커널에 이 노드가 있는지 — 명령 자체가 확인] |
| 외부 전원 USB 허브 | **필수** | 디스플레이 2 터치를 직결하면 `xhci-hcd.0: HC died` [실측 2026-09-20] |

**허브 역급전(backfeed) 확인** [추정 — 확인 방법 아래]: 값싼 유전원 허브는 5V를 거꾸로 Pi에 밀어 넣어 부팅 문제를 만듭니다.
- 확인: Pi5 어댑터를 **뺀 채** 허브만 켜고 Pi5에 꽂았을 때 Pi5 빨간 PWR LED가 켜지면 역급전 허브. 다른 허브로 바꾸거나, 허브 업스트림 케이블의 5V선을 끊은 케이블을 씁니다.

### 2-B. 디스플레이 (한 번에 하나만)

두 디스플레이 모두 **micro-HDMI는 Pi5의 같은 포트**에 꽂고, 그 포트에 테이프로 표시해 둡니다. 다른 포트로 옮기면 Weston이 빈 커넥터를 잡는 일이 있었습니다 [실측 2026-09-20, `HDMI-A-2` 바인딩]. 그리고 **디스플레이를 먼저 켜고 HDMI를 꽂은 뒤 Pi5를 켭니다.**

| | 디스플레이 1 (7" 1024×600, 터치 없음) | 디스플레이 2 (smartmon 158TF3 15.8", 터치) |
|---|---|---|
| HDMI IN | Pi5 micro-HDMI (**정상 케이블**) | Pi5 micro-HDMI (**정상 케이블**) |
| `5V` | 별도 5V 어댑터 | 모니터 자체 어댑터 |
| `5V+Touch` | **연결 안 함** (터치 미탑재, USB 과전류 유발) | **외부 전원 허브** → Pi5 |
| 조작 | 마우스 (허브 또는 Pi5 USB 직결) | 터치 |
| 근거 | [실측 2026-09-20] | [실측 2026-09-20] |

- 불량 micro-HDMI 케이블은 라벨 붙여 격리. 증상: EDID 0바이트, `Out of Range`, 깜박임 [실측 2026-09-20].
- 자세한 검증 절차는 [디스플레이 검증 가이드](2026-09-20-display-touch-verification.md).

### 2-C. ESP32 + IRLZ44N + 팬

```
  허브 USB-A ── USB 케이블 ── ESP32 (전원 + serial CDC 겸용)
                                │ GPIO26
                                ▼
                            IRLZ44N
                     G ◀──────┘   D ── 팬(−)
                     S ── GND 공통      팬(+) ── 허브 5V 분기
```

| 연결 | 상세 | 근거 |
|---|---|---|
| ESP32 USB | **허브**에 (Pi5 직결 아님). 전원과 serial을 한 선으로 | 이전 스펙 「USB 허브 (전원 + serial CDC 겸용)」 |
| 시리얼 장치 | `/dev/ttyACM0` | [문서: `deploy/README.md:23` `HOOD_SEEK_ESP32_SERIAL=/dev/ttyACM0`] |
| ESP32 GPIO26 → IRLZ44N **G**(게이트) | 직결 (권장: 100~220Ω 직렬 + 게이트-GND 10kΩ 풀다운, **(제안)**) | [문서: `firmware/.../main.cpp:5` `kFanPwmPin = 26`] |
| IRLZ44N **D**(드레인) | 팬(−) | 이전 스펙 |
| IRLZ44N **S**(소스) | GND 공통 | 이전 스펙 |
| 팬(+) | 허브 5V 분기 (USB 케이블 잘라 빨강 5V·검정 GND) | 이전 스펙 |
| GND 공통 | ESP32 GND · MOSFET S · 팬 전원 GND 모두 한 점 | 이전 스펙 ⚠️ |

#### 팬 미도착 상태에서의 순서 (2026-09-26)

팬을 아직 주문하지 않았으므로 **IRLZ44N·팬·허브 5V 분기는 보류**합니다. 그러나 **ESP32 USB는 지금 연결합니다.** 케이블 하나뿐이고 납땜이 없으며, 팬 없이도 Pi5↔ESP32 시리얼 계약 전체를 검증할 수 있습니다.

| 작업 | 지금 | 팬 도착 후 |
|---|---|---|
| ESP32 펌웨어 굽기 | ✅ **선행 필수** | |
| ESP32 USB → 허브 | ✅ | |
| 시리얼 계약 검증 (`ACK`, fail-safe) | ✅ | |
| IRLZ44N 결선 · 팬 · 5V 분기 | ❌ | ✅ |
| 기동 전류·MOSFET 발열 확인 | ❌ | ✅ [문서: `firmware/esp32_fan_controller/README.md`] |

**★ 펌웨어를 굽지 않으면 USB를 꽂아도 아무 일도 일어나지 않습니다.** 저장소의 펌웨어는 소스일 뿐이고 보드에 올려야 합니다. ESP32를 **Mac(또는 마스터 PC)** 에 직접 꽂고:

```bash
python -m pip install platformio==6.1.19
cd firmware/esp32_fan_controller
pio run
pio run --target upload
pio device monitor        # 부팅 직후 fail-safe 동작 확인
```

#### ★ ESP32 USB 칩 CH340 — ATLAS 커널에 드라이버 없음 [실측 2026-09-26]

현재 ESP32 DevKit의 USB 칩은 **CH340**입니다. 마스터 PC(Ubuntu)에 직결 시 인식:
```
Bus 001 Device 006: ID 1a86:7523 QinHeng Electronics CH340 serial converter
/dev/ttyUSB0
```

그러나 **Pi4·Pi5 ATLAS 커널 모두 CH340 드라이버(`ch341`)가 없습니다.**
```
# Pi5 (6.12.25-v8-16k), Pi4 (6.12.25-v8) 동일
modinfo: ERROR: Module ch341 not found.
$ ls /sys/bus/usb-serial/drivers/
cp210x  ftdi_sio  generic  pl2303
$ grep -E "usb/serial|cdc-acm" /lib/modules/$(uname -r)/modules.builtin
kernel/drivers/usb/class/cdc-acm.ko
kernel/drivers/usb/serial/usbserial.ko
kernel/drivers/usb/serial/cp210x.ko
kernel/drivers/usb/serial/ftdi_sio.ko
kernel/drivers/usb/serial/pl2303.ko
```

→ **이 보드를 Pi5에 꽂으면 시리얼 장치가 생기지 않습니다.** Pi5가 `FAN n`을 보낼 수 없고, ESP32는 3초 뒤 fail-safe로 팬 3단 고정.

| 대안 | 드라이버 | 펌웨어 변경 | 시리얼 경로 | 판정 |
|---|---|---|---|---|
| **CP2102 칩 ESP32 DevKit으로 교체** | `cp210x` 내장 ✅ | 없음 (`esp32dev` 그대로) | `/dev/ttyUSB0` | **(제안) 권장** |
| 네이티브 USB 보드(ESP32-S3/C3) | `cdc-acm` 내장 ✅ | `platformio.ini` 보드·USB CDC 설정 변경 | `/dev/ttyACM0` | 가능하나 변경 범위 큼 |
| 현 보드 + CP2102/FTDI/PL2303 USB-TTL 어댑터를 UART2(GPIO16/17)에 | 내장 ✅ | `Serial` → `Serial2` 변경 | `/dev/ttyUSB0` | 임시방편 |
| `ch341.ko`를 따로 빌드해 `insmod` | — | 없음 | — | 비권장: 커널 버전·서명 정확히 맞춰야 하고 이미지 업데이트마다 깨짐 |

- 교체 보드 확인법: USB 포트 옆 칩 각인이 `CP2102`/`CP2102N`(Silicon Labs)인지. `CH340C`/`CH340G`면 같은 문제.
- **P-009 ARC-003(ESP32를 Pi4 USB에 시험 연결)도 이 보드로는 불가** — Pi4 커널에도 `ch341`이 없음.
- `deploy/README.md:23`의 `HOOD_SEEK_ESP32_SERIAL=/dev/ttyACM0`은 CP2102 보드 기준 **`/dev/ttyUSB0`** 으로 바뀌어야 함 (보드 확정 후 수정).
- 현 보드는 펌웨어·시리얼 프로토콜 검증용으로 마스터 PC에서 계속 사용 가능.

- ATLAS의 UART 주변장치 API는 USB 시리얼(`/com/atlas/PeripheralManager1/UART_ttyUSB0`)을 예시로 듭니다 [문서: dev-docs `com.atlas.PeripheralManager1.UART` dbus spec, Open·Write·Read 절]. 즉 USB 시리얼 장치 사용 자체는 플랫폼이 상정한 경로이며, 문제는 **이 이미지에 포함된 칩 드라이버(cp210x·ftdi·pl2303·cdc-acm)와 현 보드 칩(CH340)의 조합**입니다.
- Pi5 GPIO UART(`/dev/ttyAMA0`, GPIO14/15)는 `dtparam=uart0_console`로 콘솔이 점유 중이고, 바꾸려면 `config.txt` 수정이 필요해 사용 불가 [실측: 원본 `config.txt`].
- **Pi5 직결 확인 — 결론 확정** [실측 2026-09-26]. USB 장치로는 열거되지만 시리얼 장치가 생기지 않음:
  ```
  [root@atlas-video-246-raspberrypi5 ~]# lsusb
  Bus 003 Device 004: ID 1a86:7523
  [root@atlas-video-246-raspberrypi5 ~]# ls /dev/ttyUSB*
  ls: /dev/ttyUSB*: No such file or directory
  ```
- 주최측 문의 (제안): ① CH340 드라이버가 포함된 이미지가 있는지 ② CP2102 보드로 교체 가능한지 ③ ESP32를 Pi와 어떻게 연결하는 것을 상정했는지.

**USB 허브 판정 [추정]**: 같은 ESP32 케이블로 마스터 PC 직결 시 인식됨 → 케이블은 데이터 케이블. 허브를 거치면 Pi5·Mac 모두에서 허브 자체가 목록에 나타나지 않음(Mac에서는 업스트림 케이블 2종 시도). 확인 방법: 허브 업스트림 포트 표시 확인, 다른 장치(마우스 등)를 허브에 꽂아 Mac에서 인식되는지.

#### 펌웨어 굽기 결과 [실측 2026-09-26, 마스터 PC]

Mac에서 `pio run`으로 빌드(RAM 6.6%, Flash 20.9%) → `esptool merge_bin`으로 단일 이미지(`hoodseek_fan_merged.bin`, 339,712 B, SHA-256 `614ac8a97727730c139f691836c9b82a0ef514d19a0d6ac5098cb9ca8c9a094f`) → 마스터 PC에서 기록.

```
Chip is ESP32-D0WDQ6 (revision v1.0)
MAC: 24:0a:c4:a6:64:5c
Wrote 339712 bytes (165801 compressed) at 0x00000000 in 14.7 seconds (effective 185.2 kbit/s)...
Hash of data verified.
```

- 이 보드는 **BOOT 버튼이 없음**(RST만). 자동 전환이 실패해 `Failed to connect to ESP32: No serial data received.` → **`IO0`–GND 점퍼 + RST**로 다운로드 모드 진입 후 기록, 기록 후 점퍼 제거 + RST.
- Ubuntu 24.04는 `pip install`이 `externally-managed-environment`로 거부됨 → `python3 -m venv ~/esptool-venv` 후 `esptool==4.8.1` 설치.
- 시리얼 프로토콜 확인: `FAN 2` → `ACK FAN 2`, 3초 무명령 → `FAILSAFE timeout FAN 3`, `FAN 9` → `ERR invalid-command` 모두 확인 [실측 — 담당자 구두 확인, 출력 원문 미첨부]. 확인 도구: `sudo ~/esptool-venv/bin/python -m serial.tools.miniterm --echo --eol LF /dev/ttyUSB0 115200`
- 부팅 메시지 `READY FAILSAFE FAN 3`은 전원 투입 시 1회만 출력. miniterm 접속으로는 재시작되지 않으므로 보려면 접속 후 RST.

#### 팬 없이 검증할 수 있는 것

Mac에 꽂은 채로 `pio device monitor`에서, 또는 허브에 옮긴 뒤 Pi5에서:

```bash
ssh root@<Pi5 IP>
ls /dev/ttyACM* /dev/ttyUSB*                       # 경로 확인
stty -F /dev/ttyACM0 115200 raw -echo
cat /dev/ttyACM0 &                                 # 수신 시작
echo "FAN 2" > /dev/ttyACM0                        # → ACK FAN 2
echo "FAN 9" > /dev/ttyACM0                        # → ERR invalid-command
                                                   # 3초 무명령 → FAILSAFE timeout FAN 3
kill %1
```

- 이 세 줄이 나오면 **팬만 빼고 전부 검증된 것**입니다 [문서: `firmware/.../main.cpp`, `README.md`].
- PWM 파형까지 눈으로 보려면 GPIO26 ↔ LED ↔ 330Ω ↔ GND를 임시로 답니다. 단계별 밝기가 달라집니다 (선택).
- 경로가 `/dev/ttyUSB0`으로 나오면 `HOOD_SEEK_ESP32_SERIAL`을 그 값으로 바꿔야 합니다 [문서: `deploy/README.md:23`].

#### 팬 구매 규격 (주문 전 확인)

| 항목 | 요구 | 이유 |
|---|---|---|
| 전압 | **5V DC** | 허브 5V 분기 |
| 결선 | **2선식 (빨강·검정)** ★ | 현행 설계는 MOSFET 로우사이드 스위칭. **4핀 PWM 팬을 사면 설계가 달라집니다** — 그때는 MOSFET 없이 GPIO26을 팬 PWM 핀에 직결 |
| 형식 | 블로워(원심) | 후드 구조 |
| 전류 | 정격 확인 후 허브 예산(5V/4A) 대조 | 기동 전류는 정격의 2~3배. ESP32·ReSpeaker·터치와 합산 [추정 — 확인 방법: 팬 정격 라벨 + 허브 총 용량] |
| 플라이백 다이오드 | 1N5819 등을 팬 양단에 (캐소드 → +5V) | 유도성 부하 차단 시 역기전력으로부터 MOSFET 보호. MOSFET **모듈** 보드에는 대개 내장 [추정 — 확인 방법: 보드에 다이오드 실장 여부] |

> 팬 전원을 허브에서 따면 기동 전류가 같은 허브의 터치·마이크에 영향을 줄 수 있습니다 [추정 — 확인 방법: 팬 기동 시 `dmesg`에 USB disconnect·over-current 발생 여부]. 발생하면 팬만 별도 5V 어댑터로 분리하고 GND만 공통으로 둡니다.

- IRLZ44N 핀 순서: 글자 면이 보이게, 다리를 아래로 → **왼쪽부터 G · D · S**. 방열판 탭은 D와 연결 [문서: IRLZ44N 데이터시트].
- 팬 전원과 ESP32가 **같은 허브**에서 나오므로 GND는 자연히 공통이 되지만, MOSFET 소스는 **선으로 직접** 그 GND에 묶어야 합니다.
- **Pi5를 꺼도 팬은 3단으로 돕니다.** 허브 전원이 살아 있고 ESP32 fail-safe가 동작 중인 것이니 정상입니다. 완전히 끄려면 허브 전원을 뺍니다.
- 확인: Pi5에서 `ls /dev/ttyACM*`. 안 보이고 `/dev/ttyUSB0`이 보이면 ESP32 보드가 CP2102/CH340 계열 → 환경 파일의 `HOOD_SEEK_ESP32_SERIAL`을 그 경로로 [추정: 우리 ESP32 보드의 USB 칩 — 확인 방법: `lsusb`에서 `Espressif`(CDC) / `Silicon Labs`(CP210x) / `QinHeng`(CH340) 중 무엇인지].

### 2-D. ReSpeaker Lite + 스피커

음성은 Pi5 HMI의 push-to-talk 버튼 → whisper.cpp/TTS 어댑터 구성입니다 [문서: `docs/final-demo-architecture.md` 음성 행, `inbox/category/voice-interface.md`]. 그래서 마이크는 **Pi5**에 붙습니다.

| 연결 | 상세 | 근거 |
|---|---|---|
| ReSpeaker Lite USB-C | **허브** → Pi5 | USB 오디오 장치로 동작 [문서: Seeed ReSpeaker Lite wiki, USB firmware 모드] |
| 스피커 | ReSpeaker Lite 보드의 **스피커 출력 커넥터** | [추정: 우리 보드 리비전에 스피커 커넥터/앰프가 있는지 — 확인 방법: 기판 실크 `SPK`/JST 커넥터 유무, Seeed wiki 해당 리비전 핀아웃] |
| 대안 | 디스플레이 내장 스피커(HDMI 오디오) | [추정: 디스플레이 2에 스피커가 있는지 — 확인 방법: 제품 뒷면·설명서] |

**연결은 선 두 개뿐입니다.** ReSpeaker Lite가 곧 「마이크 모듈」이고 마이크 2개는 기판에 이미 붙어 있습니다. Pi와는 GPIO·I2S로 잇지 않습니다.

| 선 | 어디서 | 어디로 | 비고 |
|---|---|---|---|
| ① USB-C ↔ USB-A | ReSpeaker Lite USB-C 포트 | **허브** | **데이터 되는 케이블** (충전 전용 X). 전원·마이크 입력·스피커 출력이 이 한 선으로 |
| ② 스피커 2가닥 | 모노 스피커 | ReSpeaker Lite **스피커 커넥터** (JST 2핀, `+`/`−` 실크) | 모노라 극성 바뀌어도 소리는 남 |

- XIAO 소켓·I2S 핀·3.5mm 잭(있다면)은 비워 둡니다.
- ReSpeaker Lite는 **USB 펌웨어**와 **I2S 펌웨어** 두 모드가 있고 USB 오디오 장치로 보이려면 USB 펌웨어여야 합니다 [문서: Seeed ReSpeaker Lite wiki]. 출고 펌웨어가 어느 쪽인지는 [추정] — 확인 방법: **Pi 전에 Mac에 먼저** 꽂아 `시스템 설정 → 사운드`에 입력·출력 장치로 `ReSpeaker Lite`가 뜨는지. 뜨면 USB 펌웨어. 안 뜨면 `dfu-util`로 USB 펌웨어를 올려야 하며 이건 임재범 님과 함께.
- Mac에서 입력 레벨 미터가 말소리에 움직이고, 출력을 ReSpeaker로 바꿔 소리가 스피커에서 나면 **하드웨어는 끝**. 그 다음에 허브로 옮깁니다.
- Pi5에서: `lsusb`에 `2886:` (Seeed) 항목, `cat /proc/asound/cards`에 카드 하나. ATLAS는 weston.ini에 `pipewire` 출력이 있었으므로 오디오 스택이 PipeWire일 가능성 [추정: 확인 방법 `which wpctl pw-cli`, `wpctl status`].

#### Pi5 인식 결과 [실측 2026-09-26]

ReSpeaker Lite는 **USB 펌웨어 모드로 출고**되어 별도 작업 없이 USB 오디오로 잡힘 (위 [추정] 해소):
```
$ lsusb
Bus 003 Device 005: ID 2886:0019
$ cat /proc/asound/cards
 2 [Lite           ]: USB-Audio - ReSpeaker Lite
                      Seeed Studio ReSpeaker Lite at usb-xhci-hcd.1-2, high speed
$ pactl list short sinks
2	alsa_output.usb-Seeed_Studio_ReSpeaker_Lite_0000000001-00.analog-stereo	module-alsa-card.c	s16le 2ch 16000Hz	SUSPENDED
$ pactl list short sources
5	alsa_input.usb-Seeed_Studio_ReSpeaker_Lite_0000000001-00.analog-stereo	module-alsa-card.c	s16le 2ch 16000Hz	SUSPENDED
```
- 오디오 스택은 **PulseAudio 17.0 시스템 모드**(`/var/run/pulse/native`, `pulseaudio.service`, `audio-manager.service`). root의 `pactl`은 `Failed to create secure directory (/root/.config/pulse)` 경고를 내지만 동작함.
- **`aplay`·`arecord`·`paplay`·`parecord` 없음.** 쓸 수 있는 것: `pactl`(upload-sample/play-sample), `gst-launch-1.0`(`audiotestsrc`, `pulsesrc`, `pulsesink`, `filesrc`, `filesink`, `wavparse`, `audioconvert`, `audioresample`). `wavenc`·`rawaudioparse`·`level`·`volume` 없음 → 녹음은 raw PCM으로만 저장 가능.
- LifeCam HD-3000에도 마이크가 있어 입력 장치가 둘(`alsa_input.usb-Microsoft...mono-fallback`). 음성 입력 장치는 이름으로 지정해야 함.
- ReSpeaker 믹서(`amixer -c 2 scontrols`)에 조절 항목 없음. 음량은 `pactl set-sink-volume`으로만.

#### 스피커·마이크 동작 확인 [실측 2026-09-26]

```bash
cd /tmp    # /root는 읽기 전용이라 gst가 LLVM profraw 경고를 냄
SPK=alsa_output.usb-Seeed_Studio_ReSpeaker_Lite_0000000001-00.analog-stereo
MIC=alsa_input.usb-Seeed_Studio_ReSpeaker_Lite_0000000001-00.analog-stereo
# 스피커: 440Hz 2.3초
gst-launch-1.0 audiotestsrc freq=440 num-buffers=100 ! audioconvert ! audioresample ! pulsesink device=$SPK
# 마이크: 5초 raw 녹음 (wavenc 없음)
gst-launch-1.0 pulsesrc device=$MIC num-buffers=500 ! audio/x-raw,format=S16LE,rate=16000,channels=2 ! filesink location=/tmp/mic.raw
```

- 녹음 파일 `320000` B = 16 kHz × 2ch × 2 B × **5.00초** — 녹음 경로 정상.
- 레벨(Mac에서 Python으로 분석): `peak=14743 (-6.9 dBFS)`, `rms=1294 (-28.1 dBFS)`. 0.5초 구간 RMS(dBFS) `-35 -37 -36 -36 -28 -22 -28 -24 -29 -37` → 발화 구간이 배경보다 약 8~14 dB 높음, 클리핑 없음.
- 두 채널 값이 완전히 같음 → USB 펌웨어가 처리된 단일 음성 스트림을 2채널로 복제해 내보내는 것으로 보임 [추정 — 확인 방법: Seeed ReSpeaker Lite wiki의 USB 펌웨어 채널 구성].
- Mac에서 WAV로 감싸 Pi5로 되돌려 ReSpeaker 스피커로 재생: `Execution ended after 0:00:05.001803571`.
- **기본 출력 장치가 HDMI** (`pactl get-default-sink` → `alsa_output.platform-107c701400.hdmi.hdmi-stereo`). 음성 앱은 출력 장치를 명시해야 함 → 음성 담당 전달.
- 스피커 기본 음량 46%(-20 dB)로는 잘 들리지 않을 수 있어 100%로 올림 (`pactl set-sink-volume $SPK 100%`, 재부팅 시 유지 여부 미확인).
- 배경 소음 약 -36 dBFS [추정: 박스 내 팬 소음 포함 — 확인 방법: 팬을 끈 상태와 켠 상태에서 무음 5초 녹음 비교]. **시연 중 후드 팬(fail-safe 시 3단)이 돌아가는 상태에서 음성 인식이 되는지**는 별도 검증 필요.

- Pi5에는 **3.5mm 잭이 없습니다** [문서: Raspberry Pi 5 공식 문서]. 스피커를 Pi5에 직접 꽂을 곳은 없습니다.
- ReSpeaker Lite의 I2S 핀·XIAO 소켓은 쓰지 않습니다 (USB 모드만).
- 확인:
  ```bash
  lsusb                      # Seeed / ReSpeaker 항목
  ls /dev/snd/
  cat /proc/asound/cards     # 카드 번호 확인
  ```
  ATLAS에 `aplay`/`arecord`가 있는지는 [추정] — `which aplay arecord`로 확인. 없으면 D-Bus 오디오 서비스 경로를 김병직 님 ATLAS baseline 문서에서 찾습니다.

### 2-E. Pi5 40핀 헤더

**아무것도 연결하지 않습니다.** 센서는 전부 Pi4, 액추에이터는 ESP32 담당입니다. 여기에 뭘 붙이면 P-009 경계를 깨는 것이니 붙이기 전에 팀에 먼저 말합니다.

---

## 3. 4CH 로직 레벨 컨버터 — 지금은 안 씁니다

| 후보 사용처 | 필요한가 | 이유 |
|---|---|---|
| SPS30 I2C | ❌ | 3.3V 직결로 `0x69` 응답 [실측 2026-09-20]. 이전 스펙도 「3.3V 직결」 |
| DS3231 | ❌ | VCC를 3.3V로 주면 버스 전체가 3.3V |
| ESP32 ↔ Pi5 | ❌ | USB CDC. GPIO 시리얼을 안 씀 |
| ESP32 → IRLZ44N | ❌ | 로직 레벨 MOSFET이라 3.3V 게이트로 구동 [문서: IRLZ44N Vgs(th) 1~2V] |
| **인덕션 차단 릴레이 모듈(5V 입력)** | 그때 가서 | P-009 항목 6은 **「데모까지 가상 유지」** [문서: `docs/decision/P-009-pi4-pi5-role-reallocation.md:204`]. 릴레이로 결정되면 ESP32 3.3V → 릴레이 5V 입력 사이에 이 컨버터를 씁니다 |

정전기 방지 봉투에 넣어 부품함에 보관. 배선도에 넣지 않습니다.

---

## 4. 켜는 순서 · 끄는 순서

### 켜기

1. 전원 전부 빠진 상태에서 **모든 배선 완료** → 사진 한 장.
2. 디스플레이 전원 ON, HDMI 연결.
3. **외부 전원 허브 ON** (ESP32 부팅 → 팬 3단으로 도는 게 정상).
4. Pi5 어댑터 연결 → ATLAS 로고 → 홈 화면.
5. Pi4 어댑터 연결.
6. 마스터 PC(<마스터 PC IP>)에서 `ping <Pi4 IP>`, `ping <Pi5 IP>`.

### 끄기

```bash
ssh root@<Pi4 IP> poweroff
ssh root@<Pi5 IP> poweroff
```
초록 ACT LED 소등 확인(약 10초) → 어댑터 분리 → 허브 전원 분리. **SD 카드는 주최측 봉인 상태**라 파일시스템이 깨지면 복구 수단이 없습니다. 절대 그냥 뽑지 않습니다.

---

## 5. 하지 말 것

| 금지 | 결과 |
|---|---|
| Pi 전원 켠 채 I2C 선 꽂기·빼기 | 센서 또는 GPIO 손상 |
| MLX90640 · DS3231을 5V에 | 즉사 또는 GPIO에 5V 인가 |
| SPS30 VDD를 3.3V에 | 팬 안 돔, 측정값 0 |
| SPS30 SEL 띄우기 | UART 모드, I2C에 안 잡힘 |
| 디스플레이 1의 `5V+Touch`를 Pi에 | USB 과전류 [실측] |
| 디스플레이 2 터치 USB를 Pi5 직결 | `HC died`, USB 전체 사망 [실측] |
| ESP32를 Pi5에 직결하고 팬 5V도 Pi5에서 끌기 | Pi5 다운 시 팬도 죽음 — fail-safe 무력화 |
| `/boot/config.txt`, `cmdline.txt` 수정 | 봉인 이미지. 0920 하루 날린 원인 |
| Pi5 40핀에 센서 연결 | 역할 경계 위반 (P-009) |

---

## 6. 이전 문서 수정 필요 사항

| 문서 | 현재 내용 | 고칠 내용 | 근거 |
|---|---|---|---|
| [하드웨어 스펙](2026-09-02-hardware-status-and-spec.md) §RTC, §I2C 버스 | DS3231이 `0x68`에 연결된 것처럼 기술 | 「계획 · 미연결」로 표기. 연결 후 [실측]으로 갱신 | `i2cdetect -y 1` 2026-09-20에 `0x68` 없음 + 담당자 확인 |
| [진행 현황](2026-09-02-progress-report.md) 부품표 | 「DS3231 · CR2032 · I2C `0x68`」 | 동일 | 동일 |
| [하드웨어 스펙](2026-09-02-hardware-status-and-spec.md) §ESP32 | 허브 사용만 언급 | 「외부 전원 허브 필수」로 격상, 디스플레이 2 터치·ReSpeaker도 같은 허브 | [실측 2026-09-20] `HC died` |
| [하드웨어 스펙](2026-09-02-hardware-status-and-spec.md) 전체 | 로직 레벨 컨버터·ReSpeaker·스피커 항목 없음 | 이 문서 §2-D, §3 반영 | — |

---

## 7. 재현 방법 (배선 후 검증 명령 모음)

```bash
# Pi4
ssh root@<Pi4 IP>
i2cdetect -y 1                          # 33 57 68 69
ls /dev/video*                          # /dev/video0
/data/mlx/sps30_diag                    # 시리얼 1C451D7CCF1D5D86 / FW 2.3
echo ds3231 0x68 > /sys/bus/i2c/devices/i2c-1/new_device && ls /sys/class/rtc/ && hwclock -r

# Pi5
ssh root@<Pi5 IP>
lsusb                                   # ESP32, ReSpeaker, (터치 27c0:0858), 허브
ls /dev/ttyACM* /dev/ttyUSB*            # ESP32 serial 경로
ls /dev/snd/ ; cat /proc/asound/cards   # ReSpeaker 오디오 카드
grep . /sys/class/drm/*/status          # HDMI-A-1 connected
wc -c /sys/class/drm/card*-HDMI-A-1/edid   # 256 (0이면 케이블)
hexdump -C /proc/device-tree/chosen/power/max_current   # 어댑터 협상 전류
```

---

## 8. 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| DS3231 모듈 종류(ZS-042 / Pi용 미니)와 충전 회로 유무 | 기판 실크 확인, 사진 첨부 | 정정인 |
| ATLAS Headless 커널에 `rtc-ds1307` 드라이버 존재 | `new_device` 후 `/sys/class/rtc/rtc0` 생성 여부 | 정정인 |
| DS3231 상시 적용(재부팅 후에도) 방법 — SD 수정 없이 | `/data` 스크립트 + 부팅 훅 가능 여부, 김병직 님 ATLAS baseline 참조 | 정정인·김병직 |
| ESP32 보드 USB 칩 (CDC / CP210x / CH340) → 시리얼 경로 | `lsusb`, `ls /dev/tty*` | 정정인 |
| ReSpeaker Lite 스피커 출력 커넥터 유무·규격 | 기판 실크, Seeed wiki 리비전 확인 | 정정인·임재범 |
| ATLAS에 `aplay`/`arecord` 또는 대응 D-Bus 오디오 서비스 | `which aplay arecord`, `abusctl list` | 임재범·김병직 |
| Pi5 어댑터 실제 협상 전류 | `hexdump -C /proc/device-tree/chosen/power/max_current` | 정정인 |
| 외부 전원 허브 역급전 여부 | Pi5 어댑터 뺀 채 허브만 연결 → PWR LED | 정정인 |
| Pi5 내장 RTC 배터리(J5) 보유 여부 | 부품함 확인 | 정정인 |
| 시연에 쓸 디스플레이 (1 vs 2) | **합의 필요** | 팀 |

---

## 8-A. 실제 배선 상태 (2026-09-26 시연 박스)

| 부품 | 연결 | 설계도 ②와 | 비고 |
|---|---|---|---|
| MLX90640 | Pi4 I2C1 | 일치 | |
| SPS30 | Pi4 I2C1 | 일치 | |
| DS3231 | **미연결 (연기)** | 보류 | Wi-Fi + 인터넷이라 NTP 동작 중이라는 전제. §8-B 확인 필요 |
| LifeCam HD-3000 | **Pi5** | **불일치 ★** | 설계도·현행 코드 경계는 Pi4. §8-C |
| ReSpeaker Lite + 스피커 | Pi5 (허브) | 일치 | |
| ESP32 + IRLZ44N + 팬 | 허브 | 일치 | |
| 디스플레이 · 마우스 | 미연결 (연기) | 보류 | 커넥터 결선이라 나중에 꽂아도 무방. 허브 포트 1개 예약 |

### 8-B. DS3231을 미루는 조건

미뤄도 되지만 **NTP가 실제로 동작한다는 것을 확인한 경우에만** 그렇습니다. Pi4 시계가 틀리면 payload가 전부 폐기되고 `offline`으로 떨어집니다.

```bash
ssh root@<Pi4 IP> date
ssh root@<Pi5 IP> date          # 두 보드 시각 차이가 1초 이내인가
ssh root@<Pi4 IP> 'timedatectl 2>/dev/null || cat /proc/uptime'
```
- Pi4를 **전원 차단 후 재부팅**했을 때도 시각이 맞는지가 진짜 시험입니다. 재부팅 직후 `date`가 과거로 가 있으면 NTP가 늦게 붙는 것이고, 그 창 동안 센서 데이터가 버려집니다.
- **폐쇄 LAN(설계도 ②)으로 전환하면 DS3231은 선택이 아니라 필수**가 됩니다. 함께 미루면 안 되는 항목: 마스터 PC를 LAN NTP 서버로 둘지 결정.

#### 확인 결과 — NTP 미동작 [실측 2026-09-26]

재부팅 후 두 보드 모두 시각이 틀렸습니다. **DS3231을 미뤄도 되는 조건(NTP 동작)이 성립하지 않습니다.**

| | `date` (KST) | 실제(Mac) 대비 | `timedatectl` RTC | 근거 |
|---|---|---|---|---|
| Pi4 | `Mon Sep  7 08:55:48 KST 2026` | 약 19일 늦음 | `RTC time: n/a` | RTC 없음 |
| Pi5 | `Thu Sep 24 13:11:29 KST 2026` | 약 2일 늦음 | `RTC time: Thu 1970-01-01 00:04:05` | 내장 RTC에 배터리 없음 → 전원 투입부터 계수 |

두 보드 공통 출력:
```
System clock synchronized: no
              NTP service: active
     Status: "Idle."
systemd-timesyncd[...]: System clock time unset or jumped backwards, restored from record...
  TimeUpdates = auto
  Timeservers = [  ]
  TimeserverSynced = False
```

- **원인: NTP 서버가 하나도 설정돼 있지 않음.** connman `Timeservers = [  ]`, timesyncd `Status: "Idle."`(질의할 서버 없음). 부팅 때마다 timesyncd가 마지막 저장 시각을 복원하고 그대로 흘러감 [실측].
- 두 보드 시각 차이 약 17일 → Pi4 payload가 Pi5 `maxAge` 5초에 전부 걸려 폐기 → `offline` 고정 [문서: `CLAUDE.md` 안전 불변식].
- 시간대(`Time zone: n/a (UTC)`)는 문제 아님. payload 시각은 코드에서 전부 UTC로 생성 [문서: `pi4_sensor_node/src/main.cpp:76` `gmtime_r`, `hood_seek_protocol.dart:89` `toUtc()`].

조치:
```bash
# 응급 (재부팅 시 소실) — Mac에서. PI4·PI5에 각 보드 IP를 넣어 둔다
for h in "$PI4" "$PI5"; do ssh root@$h "date -s '$(date '+%Y-%m-%d %H:%M:%S')'"; done

# 근본 — 각 보드에서. connman 서비스 설정에 NTP 서버 지정 (Wi-Fi 비밀번호와 같은 저장소)
connmanctl services                                  # * AO 표시된 wifi_..._managed_psk
connmanctl config <service> --timeservers kr.pool.ntp.org time.bora.net
connmanctl clock                                     # TimeserverSynced = True 기대
```
- **2차 원인: DNS 불통** [실측 2026-09-26 Pi4]. `ping -c1 8.8.8.8` 성공(인터넷 경로 정상)인데 `ping: bad address 'kr.pool.ntp.org'`, `cat: can't open '/etc/resolv.conf': No such file or directory`. timesyncd가 서버 이름을 풀지 못해 `Server: n/a (time.bora.net)` / `Packet count: 0`. connman 상태 `*AR`(online 아님)도 같은 원인으로 추정.
- **해결: NTP 서버를 IP로 지정** (DNS 수리 불필요 — 시스템은 MQTT를 IP로 연결). `/run` drop-in이라 봉인 이미지 무수정, 재부팅 시 소실.
  ```bash
  mkdir -p /run/systemd/timesyncd.conf.d
  printf '[Time]\nNTP=203.248.240.140 27.96.158.81\n' > /run/systemd/timesyncd.conf.d/ntp.conf
  systemctl restart systemd-timesyncd
  sleep 5; timedatectl timesync-status; date
  ```
  `203.248.240.140` = time.bora.net, `27.96.158.81` = KRISS (Mac에서 `dig +short`로 해석, 2026-09-26). Pi4 결과 [실측]:
  ```
         Server: 203.248.240.140 (203.248.240.140)
         Offset: +2w 5d 5h 12min 37.902584s
   Packet count: 1
  Sat Sep 26 14:16:48 KST 2026
  ```
- Pi5에도 같은 명령 적용 후 세 기기 비교 [실측 2026-09-26, Mac에서 `date; for h in "$PI4" "$PI5"; do ssh root@$h date; done`]:
  ```
  2026년  9월 26일 토요일 14시 20분 16초 KST     (Mac)
  Sat Sep 26 14:20:16 KST 2026                  (Pi4)
  Sat Sep 26 14:20:17 KST 2026                  (Pi5, ssh 순차 실행분 포함)
  ```
- **재부팅 후에는 `/data/ntp_sync.sh` 한 줄로 재적용.** 두 보드 `/data`에 설치하고 실행 확인 [실측 2026-09-26: Pi4 `Offset: +509us`, Pi5 `Offset: +222us`, 둘 다 `Packet count: 1`]. 원본은 [`ntp_sync.sh`](ntp_sync.sh). 부팅 시 자동 실행은 `/etc`(읽기 전용)에 서비스를 등록해야 해서 하지 않음.
- connman 설정이 재부팅 후에도 유지되는지는 [추정] — 확인 방법: 재부팅 후 `connmanctl clock`, `df /var/lib/connman`.
- **폐쇄 LAN 전환 시** 인터넷 NTP에 닿지 않으므로 timeservers를 마스터 PC(`<마스터 PC IP>`, NTP 서버 구동)로 바꾸고 **DS3231을 반드시 연결**합니다.

나중에 추가할 때를 위해 **지금 브레드보드에 3.3V · GND · SDA · SCL 한 가닥씩 여유 자리를 남겨 둡니다.** I2C는 활선 삽입이 안 되므로 추가 시 Pi4를 꺼야 합니다.

### 8-C. 웹캠을 Pi5에 붙인 것 — 배선이 아니라 설계 변경입니다 ★

현행 코드·문서 경계는 **Pi4가 카메라를 잡고 YOLO를 돌려 관측을 발행**, Pi5는 판단입니다 [문서: `docs/final-demo-architecture.md`, `CLAUDE.md` 아키텍처 표]. 카메라가 Pi5에 있으면 다음이 함께 움직입니다.

| 영향 | 내용 |
|---|---|
| `hoodseek/v1/pi4/cat/detection` | topic 이름과 `source`가 Pi4를 가리킴. 발행 주체가 바뀌면 계약 수정 |
| `person_present` | 현재 Pi4 관측 필드 [문서: `contracts/sensor-snapshot.schema.json`] |
| 안전 경계 | 「Pi4의 `consecutive_hits`를 신뢰하지 않고 Pi5가 재계산」이 **같은 보드 안에서는 무의미**해짐 [문서: `CLAUDE.md` 안전 불변식] |
| P-009 | 바로 이 항목. 현재 상태 「하드웨어 현장 검증 전」 [문서: `docs/decision/P-009-pi4-pi5-role-reallocation.md`] |
| USB 대역 | 웹캠(등시성 영상)과 ReSpeaker(등시성 오디오)가 같은 허브면 경합 가능 [추정 — 확인 방법: §8-D] |

**배선 권고**: 웹캠은 허브가 아니라 **Pi5 USB3 포트에 직결**하고, ReSpeaker는 허브에 둡니다. 두 등시성 장치를 분리하기 위함입니다.

**맥락 (2026-09-26 main 반영)**: P-009 구현 단계표는 비전 노드를 Pi5로 옮기는 ARC-005를 두고 있고, 김병직 님이 Pi5 + HD-3000 V4L2 라이브 추론(평균 313.7ms)을 실측했습니다 [문서: `docs/decision/P-009-pi4-pi5-role-reallocation.md`]. 카메라를 Pi5에 둔 배선은 이 방향과 일치하지만, **재배치 자체는 아직 승인 전**입니다.

**결정 필요**: 이건 정정인 단독으로 정할 수 없습니다. P-009 담당(현지·정인)과 계약 담당에게 알리고, 「하드웨어 편의상 Pi5」인지 「역할 재배치 확정」인지 명시해야 합니다. → **합의 필요**

### 8-D. 배선이 길어졌을 때 확인 방법

시연 박스 배치로 I2C 배선이 권장치(≤10cm)를 넘으면 선간 정전용량이 늘어 파형이 뭉개집니다. 증상은 「가끔 안 잡힘」이라 1회 확인으로는 안 보입니다.

```bash
# ① 반복 스캔 — 20회 중 한 번이라도 빠지면 불안정
for i in $(seq 20); do i2cdetect -y 1 | grep -c -E ' 33| 69'; done

# ② 지구력 시험 — 10분 로깅 후 행 수 확인 (초당 1행이면 약 600행)
/data/mlx/sps30_test log /data/mlx/soak.csv 600
wc -l /data/mlx/soak.csv

# ③ 흔들기 시험 — ②가 도는 동안 커넥터·점퍼를 하나씩 살살 흔든다
#    그 순간 행이 빠지거나 값이 튀면 그 접점이 범인
```

| 증상 | 원인 | 대응 |
|---|---|---|
| `i2cdetect`에서 주소가 들락날락 | I2C 선 길이·정전용량 | SDA·SCL을 **각각 GND선과 꼬아서** 배선, 길이 단축, 풀업 보강(2.2k~4.7k → 3.3V) |
| 값이 갑자기 0 또는 비정상 | 접점 불량 | 흔들기 시험으로 지점 특정, 커넥터 재체결 |
| SPS30만 이상 / 팬 소리 약함 | 5V 전압 강하 | 5V·GND는 **굵고 짧게**. 가는 점퍼 여러 개 직렬 금지 |
| 팬 돌 때만 값이 튐 | PWM 25kHz 노이즈 (09-25 당시 25 kHz. 10/22 #100부터 100 Hz — 재확인 필요) | I2C 배선을 **팬 전원선과 나란히 두지 않기**. 교차시키거나 이격 |

> I2C 클럭을 낮추는 방법(`dtparam=i2c_arm_baudrate`)은 **`config.txt` 수정이라 쓸 수 없습니다** (봉인 이미지). 배선 품질로 해결해야 합니다.

USB 쪽(웹캠·마이크):
```bash
ssh root@<Pi5 IP> 'dmesg | tail -n 40'   # disconnect, HC died, over-current 검색
ssh root@<Pi5 IP> lsusb -t               # 어느 장치가 어느 버스에 물렸는지
```

---

## 9. 설계도 ② (v4.0 · 2026-08-13) 대조

설계도는 부품 도착 **전**(08-13) 기준이라 이후 실측(09-02, 09-20)과 어긋나는 곳이 있습니다. 배선은 설계도를 따르되 아래 표의 「수정 필요」 항목은 이 문서를 우선합니다. 설계도 갱신은 원작성자와 상의.

| 설계도 항목 | 설계도 내용 | 현재 상태 | 판정 |
|---|---|---|---|
| Pi4·Pi5 ↔ 공유기 | **유선 · Wi-Fi OFF · 폐쇄 LAN** (이더넷) | 두 보드 모두 **Wi-Fi**로 가정용 공유기(<공유기 대역>)에 접속 [실측 2026-09-20]. 전원 재투입 후 두 보드 미응답, `<Pi4 IP>`를 다른 기기가 잠시 점유 | **수정 필요 ★** — 설계도대로 유선 복귀 권장. 최소한 MAC 기반 DHCP 고정 |
| 7인치 터치스크린 | HDMI + **터치 USB → Pi5 직결** | 7인치는 **터치 미탑재**, `5V+Touch` 연결 시 USB 과전류 [실측 2026-09-20]. 터치는 별도 15.8인치(디스플레이 2)로만 가능하며 **허브 경유 필수** | **수정 필요** — 디스플레이 선택은 `합의 필요` |
| 마우스 | 없음 | 디스플레이 1이면 필요 | 추가 |
| USB 허브 | ESP32·마이크 급전 | 여기에 **디스플레이 2 터치·마우스도** 허브로 | 확장 |
| Pi5 PSU 27W | ★라벨 확인 | 미확인 | 미확인 그대로 (§2-A 명령으로 확인) |
| DS3231 | 0x68 · Pi4 hwclock | **미연결** [실측 2026-09-20] | 배선 대기 (§1-C) |
| Pi5 LAN 시간동기 | LAN 동기 | Wi-Fi·인터넷 공유기라 NTP 자동. **폐쇄 LAN으로 돌아가면 NTP 서버(마스터 PC)가 필요** | 유선 복귀 시 함께 결정 |
| ReSpeaker Lite → 모노 스피커 | 스피커 단자 사용, USB DAC 예비 | 설계 의도 확인. 보드 실물 커넥터는 미확인 | §2-D [추정] 유지, 예비는 USB DAC |
| Pi4 케이스 팬 필수 | 5V 핀 급전 | `poweroff` 후에도 도는 팬이 이것 | 정상 (전원 분리해야 정지) |
| MLX90640 / SPS30 / ESP32 / IRLZ44N / 웹캠 | 표기대로 | 이 문서 §1, §2-C와 동일 | 일치 |
| 4CH 로직 레벨 컨버터 | **설계도에 없음** | 사용처 없음 (§3) | 일치 |
| 부저·LED | 삭제 | — | 일치 |

**Wi-Fi가 안전 동작에 미치는 영향** [추정 — 확인 방법 병기]: Pi4 heartbeat가 3초 끊기면 `offline` → 팬 3단 + 인덕션 잠금. Wi-Fi 지연·재접속이 이 3초를 넘기면 시연 중 오탐 `offline`이 납니다. 확인: 마스터 PC에서 `ping -i 0.2 <Pi4 IP>`을 10분 돌려 3초 이상 공백·손실 유무, 또는 gateway 로그의 `offline` 전이 횟수.

---

## 근거 목록

- Raspberry Pi 공식 문서 — GPIO pinout, Raspberry Pi 5 Power supply / RTC 절
- Sensirion SPS30 데이터시트 §3 Pin description, 전기 특성
- Melexis MLX90640 데이터시트 — 공급 전압
- Maxim DS3231 데이터시트 — VCC 범위; ZS-042 모듈 회로도(충전 회로)
- Infineon IRLZ44N 데이터시트 — 핀 배치, Vgs(th)
- Seeed ReSpeaker Lite wiki — USB 모드
- 저장소: `deploy/README.md:23`, `firmware/esp32_fan_controller/src/main.cpp:5`, `docs/final-demo-architecture.md`, `docs/decision/P-009-pi4-pi5-role-reallocation.md:204`
- 본인 실측: [디스플레이 검증](2026-09-20-display-touch-verification.md), [Pi5 화면 장애 조사](2026-09-20-pi5-display-troubleshooting.md), 2026-09-20 `i2cdetect -y 1`

*HOOD&SEEK 프로젝트 · 정정인 (임베디드·통신·fail-safe)*
