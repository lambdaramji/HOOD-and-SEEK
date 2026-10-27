# Pi4 안전 권한 시험 — 2026-09-02 실행 환경 진단

- 작성자: 머지 통합 재작성 (원 로그는 #22. 정정인 본인 기록이 아님)
- 작성일: 2026-09-02
- 갱신: 2026-09-20
- 상태: **판정 불가 / 미실행**
- 카테고리: embedded-communication
- 관련: [P-009](../../docs/decision/P-009-pi4-pi5-role-reallocation.md) ARC-001

## 표기 규칙

| 표기 | 뜻 |
|---|---|
| **[실측]** | 아래 원문 로그를 실행해 확인 |
| **[문서]** | 저장소 문서·Dart 도구 설명 |
| **[추정]** | 확인하지 못한 해석. 확인 방법 병기 |

## 요약

1. **[실측]** 2026-09-02T15:25:21Z에 시험을 시도한 환경은 Pi4가 아니라 **x86_64 컨테이너**였다. `dart` 없음, systemd/mosquitto 없음, `/dev/ttyACM*`·`/dev/i2c-*`·RTC 없음.
2. 대상 보드의 Dart AOT·MQTT·ESP32·I2C를 **실패로 적을 수 없다.** 실행하지 못했다.
3. 독립 배포 명제를 검증하려면 `dart compile exe --target-os=linux --target-arch=arm64` 산출물을 **Pi4에서** 실행해야 한다. `aot-snapshot`과 `dart run`은 그 명제를 증명하지 못한다. [문서]

## 실행 환경과 원문 로그 [실측]

```text
$ date -u '+%Y-%m-%dT%H:%M:%SZ'
2026-09-02T15:25:21Z
$ uname -a
Linux localhost 6.18.35 #1 SMP Mon Aug 31 18:10:37 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
$ dart --version
/bin/bash: line 1: dart: command not found
$ systemctl is-active mosquitto
System has not been booted with systemd as init system (PID 1). Can't operate.
Failed to connect to bus: Host is down
$ find /dev -maxdepth 1 -type c \( -name 'ttyACM*' -o -name 'ttyUSB*' \) -printf '%f %G %U:%G %m\\n'
# no output
$ find /dev -maxdepth 1 -type c -name 'i2c-*' -printf '%f %G %U:%G %m\\n'
# no output
$ cat /sys/class/rtc/rtc0/name
cat: /sys/class/rtc/rtc0/name: No such file or directory
$ hwclock --show
/bin/bash: line 1: hwclock: command not found
```

| 확인 항목 | 결과 |
|---|---|
| Dart arm64 독립 실행 | 미실행 |
| MQTT localhost | 미실행 |
| ESP32 serial identity | 미확인 (`ttyACM*`/`ttyUSB*` 없음) |
| MLX90640+SPS30 I2C | 미실행 (`i2c-*` 없음) |
| RTC·시간 동기 | 미실행 |

## 실제 Pi4에서 재실행할 명령

자리표시자 `ttyACM0`·I2C bus `1`은 탐지한 경로로 바꾼다. `rg` 대신 `grep -Ei`를 쓴다. ATLAS 이미지에 `rg`가 있는지는 미확인이다. [추정] 확인 방법: Pi4에서 `command -v rg`.

```bash
date -u '+%Y-%m-%dT%H:%M:%SZ'; uname -m
dart compile exe \
  --target-os=linux \
  --target-arch=arm64 \
  pi5_gateway/bin/hood_seek_gateway.dart \
  -o hood-seek-safety-arm64
# 산출물을 Pi4로 복사한 뒤
./hood-seek-safety-arm64

systemctl is-active mosquitto
udevadm info --query=all --name=/dev/ttyACM0
stat -c '%n %U:%G %a' /dev/ttyACM0
i2cdetect -y 1
journalctl -k -b --since '<START_UTC>' --until '<END_UTC>' | grep -Ei 'i2c|nack|timeout|mlx|sps'
date -u '+%Y-%m-%dT%H:%M:%SZ'; timedatectl status; hwclock --show
```

## 결정과의 관계

이 기록은 P-009 선행 확인의 Dart AOT 칸을 **비우지 않고 판정 불가로 채우는 근거**다. 역할 재배치를 부결하지 않는다.
