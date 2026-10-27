# SafetyCore (Pi 4)

센서 읽기·**유일한 판정**·ESP32 집행·TCP state 발행을 하는 Pi 4 네이티브 서비스입니다 (설계서 §5.1). arc `native-service` 템플릿 구조이고 서비스 ID는 `com.atlas.HoodSeekSafetyCore1`입니다.

| 파일 | 역할 | 담당 |
|---|---|---|
| `src/main.cpp` | policy 루프(500 ms + 명령 즉시), profile 로드·SIGHUP, 스레드 시작 | 정정인 #47 |
| `src/tcp_server.*` | 계약 v1 TCP :7420 (poll 단일 스레드) | 정정인 #47 |
| `src/message.*` | 수신 검증(`contracts/*.schema.json`과 같은 규칙)·state 직렬화 | 정정인 #47 |
| `src/profile_loader.*` | `/data/hoodseek/profile.json` → `Profile`, 하나라도 틀리면 전체 거부 | 정정인 #47 |
| `src/dbus_service.*` | 버스 이름 소유 + `GetState()`. 실패해도 안전 루프는 계속 | 정정인 #47 |
| `src/sensors.*` | sensor 스레드: `/dev/i2c-1`로 MLX90640(0x33, 서브페이지 2 Hz → 프레임 1 Hz)·SPS30(0x69, 1 Hz). 정상 읽기만 Snapshot에 씀(`pm_seq`는 1부터) → 끊기면 3 s 뒤 fault(F4). SPS30은 3 s 동안 샘플이 없으면 측정 시작을 다시 보냄 | 정정인 #47 |
| `third_party/mlx90640/` | Melexis MLX90640 API (Apache-2.0, pimoroni 포크 `a92229a`). 출처·가져온 파일은 그 폴더 README | — |
| `src/uart.*` | ESP32 링크(팬 `/dev/ttyAMA3`·쿡탑 `/dev/ttyAMA5`): 목표 바뀌면 즉시 송신, 1 s 재전송, 일치 ACK 2 s → `fan.ack`·`induction.ack`. 판정 루프가 2 s 멈추면 송신 중단(ESP32 fail-safe) | 정정인 #56 |
| `src/policy.h`·`src/policy.cpp` | 판정(`Policy::tick`, 12단계). 입출력·스레드·시계 없음 | 남현지 #57 |
| `src/snapshot.h` | 스레드가 함께 쓰는 최신 값(뮤텍스 하나, 단조 시계) | 정정인 #47 |
| `src/csv_log.*` | 판정 CSV 로그: tick마다 입력·상태·출력·지연 한 줄 (아래 「판정 CSV 로그」) | 정정인 #88 |

## 호스트(Mac·Linux)에서 빌드·시험

sdbus-c++가 없으므로 D-Bus를 끄고 빌드합니다.

```bash
cmake -S safety_core -B safety_core/build -DHOODSEEK_WITH_DBUS=OFF
cmake --build safety_core/build
ctest --test-dir safety_core/build --output-on-failure
./safety_core/build/safetycore --port 7420 --profile <profile.json> --fan-uart none --cook-uart none --i2c none --log-dir none
```

`ctest`는 `message_test`(수신 검증·직렬화), `uart_test`(가상 터미널로 ESP32 흉내, 약 4 s), `sensor_test`(SPS30 샘플 분류), `csv_log_test`(CSV 열·파일 이름)를 돌립니다. 판정 시험 `policy_test`는 `safety_core/tests/`에 따로 있습니다. 장치 기본값은 UART `/dev/ttyAMA3`·`/dev/ttyAMA5`, I2C `/dev/i2c-1`이고, 호스트에서는 `none`으로 끕니다. CSV 로그는 `--log-dir none`으로 끄거나 다른 폴더를 줍니다. 센서 읽기는 Linux에서만 컴파일되고(Mac은 센서 무효로 동작), MLX90640 API도 Linux 빌드에만 들어갑니다.

다른 노드(vision·voice·Flutter)는 이 호스트 실행본에 붙여 클라이언트를 시험할 수 있습니다.

## 기기 빌드·설치 (마스터 PC atlas-dev → Pi 4)

```bash
docker exec -it atlas-dev bash -ic "cd /app/<경로> && arc build ./safety_core"   # bash -ic: SDK 환경
docker exec -it atlas-dev bash -ic "cd /app/<경로>/safety_core && arc install build/arm64/ipk/com.atlas.HoodSeekSafetyCore1.ipk -d rpi4"
# Pi 4 (최초 1회)
mkdir -p /data/hoodseek && cp <profile.json> /data/hoodseek/profile.json
cp deploy/systemd/hoodseek-safety-core.service /data/share/usr/lib/systemd/system/
# Pi 4 (재부팅 후 매번 — 부팅 자동 기동은 #46 결정 대기)
systemctl daemon-reload && systemctl start hoodseek-safety-core
```

profile.json을 부팅 때 못 읽으면 fail-safe 출력(팬 3·인덕션 0·`locked_danger`)을 내며 기다리고, SIGHUP으로 처음 읽히면 그때 `Policy`가 생겨 **부팅 유예부터** 시작합니다(#57 동작 규칙 3).

root로 실행해야 합니다. ipk 서비스 계정(`u0_a…`)은 `/dev/i2c-1`·`/dev/ttyAMA3·5`를 열 수 없어서, 기기 빌드는 root가 아니면 바로 종료합니다 [실측 #46].

## 확인

```bash
systemctl is-active hoodseek-safety-core
busctl list | grep -i hoodseek          # PID 열이 '-'면 안 떠 있음
busctl call com.atlas.HoodSeekSafetyCore1 /com/atlas/HoodSeekSafetyCore1 com.atlas.HoodSeekSafetyCore1 GetState
kill -HUP $(systemctl show hoodseek-safety-core -p MainPID --value)   # profile 재로드
# Pi 5
(printf '%s\n' '{"v":1,"type":"hello","src":"ui","seq":0,"t_ms":0,"p":{"node":"nc","ver":"0.1.0"}}'; sleep 2) | nc <Pi4> 7420
```

## 판정 CSV 로그 (#88)

policy 루프가 **tick마다 한 줄**(500 ms + 명령으로 일찍 깬 tick)을 `/data/hoodseek/log/`에 씁니다 (설계서 §5.1 「로그」). journal의 `[state]`는 판정이 바뀔 때만 남으니, 그래프·지연 분석은 이 파일로 합니다.

| 규칙 | 내용 |
|---|---|
| 파일 | Core가 뜰 때마다 새 파일 `YYYYMMDD-HHMMSS.csv`. 같은 초에 다시 뜨면 `-2`, `-3` … (덮어쓰지 않음) |
| 시각 | 파일 이름·`wall_ms`는 **Pi 벽시계라 틀릴 수 있습니다**(NTP 없음, 10/20 실측 `Sep 27`). 재부팅 뒤 Mac 시각으로 맞추면 맞습니다 [실측 10/27: 10:39에 맞춘 뒤 파일 이름 `20261027-144433` = Core 시작 14:44:33]. 분석은 `t_ms`(monotonic)로 하고, 실제 시각은 기준점 하나(폰 시각 = 어느 `seq`)를 따로 적어 맞춥니다 |
| 크기·회전 | 한 줄 평균 161 B × 2 Hz ≈ **하루 약 28 MB** [실측 10/27 Pi 4 `20261027-144433.csv` 2,374줄, 호스트 10/23은 평균 176 B]. 회전·삭제는 없습니다. Pi 4 `/data` 여유 5.3 GB(`df -h`, 10/27)로 약 190일 |
| 지연 | 정기 tick이 예정보다 늦은 `late_ms` 평균 0.11 · 최대 1 ms, 판정 한 번 `work_us` 평균 412 · 최대 1,008 µs [실측 10/27 Pi 4, 같은 파일, #203] — 파일 쓰기가 500 ms 주기를 밀지 않습니다 |
| 실패 | 폴더·파일을 못 열거나 쓰기가 실패하면 `[log] … CSV 로그 끔`을 한 번 남기고 그 뒤로 쓰지 않습니다. **판정·집행은 그대로** 돕니다 |
| 전원 차단 | 한 줄씩 커널로 넘기지만 fsync는 하지 않습니다(판정 루프를 SD 쓰기에 묶지 않으려고). 갑자기 전원이 끊기면(M11) 디스크에 아직 안 쓴 마지막 몇십 초는 잃을 수 있습니다 [추정 — 확인: M11 뒤 마지막 줄의 `t_ms`와 전원을 뺀 시각 비교] |
| 끄기 | `--log-dir none` |

열 (머리줄 그대로):

| 묶음 | 열 | 뜻 |
|---|---|---|
| tick | `seq` `t_ms` `uptime_s` `wall_ms` | `seq`·`t_ms`는 state와 같은 값 |
| 지연 | `wake` | `tick` = 500 ms 정기, `cmd` = voice·ui_cmd가 와서 바로 깸 |
| | `late_ms` | 정기 tick이 예정보다 늦게 깬 정도 (`cmd`는 0) |
| | `cmd_wait_ms` | 이번 tick에 집행한 명령 중 가장 오래 기다린 것의 TCP 수신 → tick 시간 |
| | `work_us` | 깬 뒤 입력 복사·`Policy::tick`·UART 목표·state 발행까지 (CSV 쓰기는 빠짐 — 늦어지면 다음 줄 `late_ms`에 보임) |
| | `rx_dropped` | 검증 실패·만료로 버린 메시지 누적 (F8) |
| 입력 | `t_max` `t_median` `pm25` `tsize` `pm_saturated` `pm_seq` | 마지막 정상 읽기 값. 지금 유효한지는 `thermal_valid`·`pm_valid`. `pm_saturated`는 SPS30 상한에서 잘린 샘플(연기형, D-054·D-056) |
| | `vision_msgs` `commands` | 이번 tick에 넘긴 vision 메시지 수, 명령 목록(`ui:FAN_SET=2\|voice:IND_ON`) |
| 판정 | `t_metric` `pm_baseline` `vision_fresh` `person` `cat` `presence` `hazard` `lock` `lock_release_left_s` `reasons` `alert` | state와 같은 뜻(`lock_release_left_s`는 state v2, #116). 목록은 `\|`로 잇고, 없으면 빈 칸 |
| 출력 | `fan_level` `fan_min` `fan_requested` `fan_ack` `induction` `allowed_on` `ind_ack` | 이번 tick 집행 목표와 ESP32 ACK |
| 음성 | `voice_intent` `voice_value` `voice_accepted` `voice_reason` | 마지막 voice 결과 (없으면 빈 칸). `voice_value`는 `FAN_SET`·`IND_SET` 단수 (D-062) |

```bash
ls -l /data/hoodseek/log/ && tail -3 "$(ls -t /data/hoodseek/log/*.csv | head -1)"
# 최악 지연 (열 번호는 머리줄 순서: 6 late_ms, 7 cmd_wait_ms, 8 work_us)
awk -F, 'NR>1{if($6>a)a=$6; if($7>b)b=$7; if($8>c)c=$8} END{print "late_ms",a,"cmd_wait_ms",b,"work_us",c}' <파일>
```
