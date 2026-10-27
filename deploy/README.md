# 배포

S10 배포는 [설계서 §8.2 빌드·배포 절차](../docs/2026-10-19-s3-s10-design-v0.1.md)를 따릅니다. ATLAS ipk(`arc build`) 설치가 기본이고, 재부팅 자동 기동이 안 되면 systemd 유닛으로 폴백합니다(Q-S10-1, #46).

| 파일 | 기기 | 상태 |
|---|---|---|
| [`systemd/hoodseek-safety-core.service`](systemd/hoodseek-safety-core.service) | Pi 4 | #76. root 실행 |
| [`systemd/hoodseek-vision-node.service`](systemd/hoodseek-vision-node.service), [`systemd/vision-node.env.example`](systemd/vision-node.env.example) | Pi 5 | #77. env 파일은 기기에서 복사해 실제 값으로 고침 |
| [`systemd/hoodseek-voice-node.service`](systemd/hoodseek-voice-node.service), [`systemd/voice-node.env.example`](systemd/voice-node.env.example) | Pi 5 | #62 후속 PR. 기본 관측 전용, 발행은 기기 env에서 명시 |
| [`pi5/edid-1920x1080.bin`](pi5/edid-1920x1080.bin) | Pi 5 | #64. 시연 화면 158TF3 1080p EDID, 부팅 뒤마다 적용 (아래 절) |
| vision 모델 v3e — **저장소 밖**(사람 사진으로 학습, #111 PM 결정 2) | Pi 5 `/data/ecoai/assets/models/object_detection/yolox.tflite` | sha256 `31a988c1116afbe5860e0ec77822e78069b8c0a322cbc0e87f6570ac11babbe1`. 사본 세 곳: 빌드 PC `~/hoodseek-train/models/hoodseek_s_v3e_int8.tflite`, Pi 5 `/data/hoodseek/models-test/hoodseek_s_v3e_int8.tflite`, 팀 공용 Google 드라이브 「임베디드SW경진대회_스마트가전_LG전자」 폴더 [실측 10/27: 빌드 PC·Pi 5 사본 sha256 일치, 드라이브는 같은 파일을 올리고 크기만 확인]. [v3e 문서](../inbox/김병직/2026-10-25-cat-model-v3e.md) |
| Pi 4·Pi 5 IP 표 | — | 사무실 확정 후 (설계서 §1.2) |

설치 위치와 기동 명령은 각 유닛 파일 머리 주석에 있습니다. 재부팅 뒤 자동 기동은 하지 않고 `systemctl start`를 수동으로 합니다(#46 결정 (4)).

기기 IP·계정 같은 환경값은 Git에 커밋하지 않습니다. Flutter는 `--dart-define`으로 주입합니다(설계서 §5.4).

## voice-node 수동 기동

마스터에서 ARM 바이너리를 빌드한 뒤 기존 파일/실행 중인 프로세스를 확인하고 `/data/hoodseek/bin/hoodseek-voice-observer`에 배포합니다. 현재 유닛은 SenseVoiceSmall INT8와 Silero를 사용합니다. 기존 모델·런타임을 재사용하며 빌드·해시 대조는 [voice README](../voice_node/README.md#sensevoice-하이엘지-실행과-선택적-송신)를 따릅니다.

[`systemd/voice-node.env.example`](systemd/voice-node.env.example)을 참고해 기기의 `SENSEVOICE_MODEL_DIR`·`VAD_MODEL`·`LD_LIBRARY_PATH`·`MIC`·`CORE_HOST`·`CORE_PORT`를 확인합니다. 모델 예시 경로가 실제로 없으면 기존 모델 경로로 바꿉니다. 실제 주소/계정/환경 파일은 커밋하지 않습니다.

### 기존 에너지 VAD 유닛에서 전환하는 순서

[문서] 유닛만 먼저 교체하면 이전 env에 `SENSEVOICE_MODEL_DIR`·`VAD_MODEL`·런타임 경로가 없어 기동/재시작을 반복할 수 있습니다. 기존 바이너리·env·유닛을 별도 이름으로 보존한 뒤 다음 순서를 지킵니다. 근거: [`hoodseek-voice-node.service`](systemd/hoodseek-voice-node.service), [`voice-node.env.example`](systemd/voice-node.env.example).

1. 실행 중인 voice 서비스를 담당자와 조정해 수동 stop합니다. 다른 노드는 종료하지 않습니다.
2. 기존 SenseVoice의 `model.int8.onnx`·`tokens.txt`, Silero 모델, sherpa/ONNX Runtime 라이브러리의 배치 경로·해시·읽기 권한을 확인합니다. 새 바이너리의 동적 라이브러리 의존성도 확인합니다. SDK/시스템 라이브러리는 교체하지 않습니다.
3. **기존 env에 `SENSEVOICE_MODEL_DIR=<model.int8.onnx·tokens.txt가 있는 경로>`와 `VAD_MODEL=<실제 Silero 모델 경로>`를 추가**하고 `VOICE_PUBLISH_ARGS=`를 비워 둡니다. 이전 `MODEL`·`SCORE_MIN`·`SCORE_RAISE`는 새 SenseVoice 유닛에서 사용하지 않습니다. 예시 env로 실제 주소/모델 경로를 덮어쓰지 않습니다.
4. sherpa 대응 바이너리를 쓰면 Whisper 모드에서도 동적 라이브러리가 필요합니다. 기기 env에 `LD_LIBRARY_PATH=<기기에 배치한 sherpa lib 디렉터리>`를 추가합니다. env 파일은 셸이 아니므로 `$SHERPA_ONNX_ROOT`·`$LD_LIBRARY_PATH` 확장식 대신 완성된 경로를 씁니다. Whisper 전용 바이너리는 이 추가 런타임이 필요 없습니다.
5. 서비스와 같은 env/사용자로 관측 명령을 먼저 실행해 모델 로딩·입력을 확인하고 종료합니다. 그 뒤 바이너리·유닛을 교체하고 `systemd-analyze verify` → `systemctl daemon-reload` → 수동 start합니다.
6. `journalctl`의 준비 로그·한 개 voice 프로세스·관측 전용을 확인합니다. 실패하면 stop한 상태에서 보존한 바이너리/env/유닛을 함께 복원하고 daemon-reload합니다. 자동 재시작을 반복시키며 시험하지 않습니다.

[문서/합의 필요] SenseVoice도 명시적 `--publish`로 기존 TCP v1을 송신하도록 구현했습니다. 활성어·유사도·미제공 conf의 의미는 PM·계약 검토 대상이며, 운영 발행은 검토 후 전환합니다. 기본 유닛/env는 수동 기동·발행 끔입니다. Whisper 실행은 voice README의 별도 명령을 따릅니다.

```bash
# Pi5의 저장소 루트에서 수행. 위 1~5의 모델/런타임·기기 env·관측 확인을 먼저 마친다.
# 최초 설치에만 예시를 복사해 실제 값을 채운다. 기존 env가 있으면 보존하고 필요한 항목만 수정한다.
# cp -i deploy/systemd/voice-node.env.example /data/hoodseek/voice-node.env
cp -i deploy/systemd/hoodseek-voice-node.service /data/share/usr/lib/systemd/system/
systemd-analyze verify /data/share/usr/lib/systemd/system/hoodseek-voice-node.service
systemctl daemon-reload
systemctl start hoodseek-voice-node
journalctl -u hoodseek-voice-node -f
# 로그 보기를 끝낼 때 Ctrl+C. 서비스 종료는 아래 명령으로 수행한다.
systemctl stop hoodseek-voice-node
```

`VOICE_PUBLISH_ARGS=`이면 hello/state와 ASR 후보만 관측하고 voice는 보내지 않습니다. 송신 시험은 대상과 정책을 확인한 뒤 `VOICE_PUBLISH_ARGS=--publish`로 바꾸고 수동 `systemctl restart hoodseek-voice-node`합니다. 최초 로그의 `mode=publish`, 이후 `sent`와 Core의 `state.voice.accepted/reason`을 구분해 확인합니다. `systemctl stop` 뒤 중복 프로세스와 추가 전송이 없는지 확인합니다. 자동 기동을 위해 `enable`하는 절차는 이번 범위에 넣지 않습니다.

### SenseVoice 송신 전환과 기존 Whisper 실행

[문서] `hoodseek-voice-node.service`는 SenseVoice 수동 실행 경로입니다. 기존 `VAD_DBFS`나 Whisper 점수 문턱은 사용하지 않습니다. [voice_node 실행 안내](../voice_node/README.md#sensevoice-하이엘지-실행과-선택적-송신)의 `--publish`에서만 voice를 보냅니다. 현재 상태를 해석해 명령을 바꾸지 않으며 팬 단수+방향은 계속 NONE입니다. 기본 풍량 없는 `후드/팬 켜`는 PM10/24 결정에 따라 FAN_SET(1)로 보내며 Core가 안전 하한을 적용합니다. 실제 발행은 같은 고정 구성의 부정36개·실제 주변음300초가 실행 후보0건인 것을 확인한 뒤 시험에서만 켭니다. 이번 PR은 기기의 기존 유닛·env를 자동 교체하거나 서비스를 시작하지 않습니다.

## vision-node 카메라 카드 영상 켜기 (#144)

카메라 카드 영상(UI 설계 v1 §3.2, 앱 수신 #145)은 **바이너리·유닛·env 세 가지가 모두 새것이어야** 켜집니다. 기기에 깔린 유닛은 `ExecStart` 끝에 `$PREVIEW_ARGS`가 없어서, 바이너리와 env만 바꾸면 영상이 나오지 않습니다 [실측 10/25 정인, #144 리뷰]. 형식은 [vision README 「카메라 카드 영상」](../vision_node/README.md#카메라-카드-영상-ui-설계-v1-32)에 있습니다.

restart할 때마다 vision이 몇 초 끊깁니다. 그동안 presence가 unknown(`VISION_STALE`)이라, 중불 이상이면 팬이 3단이 되고 인덕션 켜기를 거부합니다. **리허설·측정 중에는 하지 않고, 같은 박스를 쓰는 사람에게 먼저 말합니다.** 10/25 22:3x 영상 시험이 Must M12–M13 사이에 vision을 1분쯤 끊었습니다 [실측 정인, #144 리뷰].

| # | 할 것 | 바로 볼 것 |
|---|---|---|
| 1 | 옛 바이너리 백업 | 백업 sha256 = 지금 바이너리 |
| 2 | 새 바이너리로 바꾸고 restart (유닛·env는 그대로) | `active`, Core 로그 `hello (vision)`, 조리대 앞에서 presence present — **영상 없이 옛날처럼 도는지 먼저** |
| 3 | 유닛 백업 → 새 유닛 복사 → `daemon-reload` → restart | 그대로 present (`PREVIEW_ARGS`가 아직 없어 영상 꺼짐) |
| 4 | env에 `PREVIEW_ARGS=--preview-port 7431` → restart | `journalctl`에 `preview off` 없음, 아래 확인 줄이 1 이상, presence present |
| 5 | #67 L0에 바이너리·유닛 sha256 | |

```bash
# Pi 5에서. 새 바이너리는 미리 /data/hoodseek/bin/hoodseek_vision.new로 올리고 sha256을 #144와 대조한다
cd /data/hoodseek
sha256sum bin/hoodseek_vision bin/hoodseek_vision.new
cp -p bin/hoodseek_vision bin/hoodseek_vision.<옛 sha256 앞 8자>                       # 1
mv bin/hoodseek_vision.new bin/hoodseek_vision && systemctl restart hoodseek-vision-node  # 2 (실행 중 파일은 cp로 덮지 않고 mv로 바꾼다)
U=/data/share/usr/lib/systemd/system/hoodseek-vision-node.service
cp -p $U /data/hoodseek/hoodseek-vision-node.service.bak                                # 3
cp <저장소>/deploy/systemd/hoodseek-vision-node.service $U && systemctl daemon-reload && systemctl restart hoodseek-vision-node
echo 'PREVIEW_ARGS=--preview-port 7431' >> vision-node.env && systemctl restart hoodseek-vision-node   # 4
sleep 10   # 모델 불러오기
wget -T 5 -q -O - http://127.0.0.1:7431/ | dd bs=1k count=100 2>/dev/null | grep -c 'image/jpeg'   # 1 이상 = JPEG 파트가 실제로 옴
```

- Pi 5 BusyBox의 `nc`에는 `-w`가 없고 `head`에는 `-c`가 없어서 `dd`로 앞부분만 읽습니다. `curl`은 없습니다 [실측 10/25 `nc --help`·`head -c`].
- `wget -T 5`: 서버가 머리만 보내고 프레임을 안 보내면 BusyBox `wget`은 기본 대기 시간 동안 멈춰서 0을 보여 주지 못합니다. 그래서 5초에 끊습니다. Pi 5 BusyBox에 `timeout` 명령은 없고 `wget -T`는 있습니다 [실측 10/26 Pi 5, #159 정인 리뷰: 이 줄로 `image/jpeg` 5개, 없는 포트는 0].
- **확인은 `nc`가 아니라 `wget`으로 합니다.** BusyBox `nc`는 요청을 다 보내면 쓰기 쪽을 닫고, 서버는 그것을 「보는 쪽이 떠남」으로 보고 끊어 HTTP 머리(128바이트)만 옵니다. 머리에도 `boundary=hoodseekframe`이 있어서, 10/25까지 쓰던 `nc … | grep -c hoodseekframe`은 프레임이 안 나와도 1이었습니다 [실측 10/26 Pi 5, `nc`는 128바이트만, `wget`은 JPEG 파트가 옴(5초 18장, 새 바이너리 75장)]. `image/jpeg`는 파트 머리에만 있습니다.
- **되돌리기는 거꾸로 합니다.** env의 `PREVIEW_ARGS` 줄을 먼저 지우고, 옛 바이너리로 바꾼 뒤 restart합니다. 옛 바이너리는 `--preview-port`를 몰라 시작을 거부하므로, env 줄이 남아 있으면 1 s마다 재시작하는 동안 presence가 unknown으로 남습니다. 새 유닛은 `PREVIEW_ARGS`가 없으면 인자를 붙이지 않으니 그대로 두어도 됩니다.

```bash
cd /data/hoodseek
sed -i '/^PREVIEW_ARGS=/d' vision-node.env
cp -p bin/hoodseek_vision.<옛 sha256 앞 8자> bin/hoodseek_vision.tmp && mv bin/hoodseek_vision.tmp bin/hoodseek_vision
systemctl restart hoodseek-vision-node && sleep 3 && systemctl is-active hoodseek-vision-node
```

## Pi 5 ReSpeaker 출력 확인 (#133)

[실측 2026-10-25] ATLAS `com.atlas.AudioManager1.ActiveOutputDevice`가 HDMI인 상태에서는 ReSpeaker를 지정한 GStreamer·paplay·client-name 변경 재생도 HDMI로 연결됐다. AudioManager의 정규 메서드로 USB를 선택하자 기본 `pulsesink`도 ReSpeaker로 재생됐다. 허용받은 재부팅 1회 후에도 USB 선택과 재생 3종이 유지됐다. [검토 보고서](../inbox/이수현/2026-10-25-respeaker-output-recovery.md)에 전환 전후·재부팅 유지·실제 앱 확인 범위를 구분했다.

[문서] 아래는 **시스템의 활성 출력을 바꾸는 수동 절차**다. 담당자와 공유 사용 상태를 확인하고 진행한다. 입력 장치·마이크·Core·음성 발행 설정은 바꾸지 않는다. 실제로 노출된 USB 장치를 먼저 확인하고, 이미 USB이면 전환 호출을 건너뛴다.

```bash
# Pi5에서 실행. OutputDevices에 usb_speaker가 있는지 확인한다.
busctl get-property com.atlas.AudioManager1 /com/atlas/AudioManager1 com.atlas.AudioManager1 OutputDevices
busctl get-property com.atlas.AudioManager1 /com/atlas/AudioManager1 com.atlas.AudioManager1 ActiveOutputDevice
# 다른 재생이 없는지 확인. 실제 현장 출력에 영향을 주므로 사용 중이면 담당자와 조정한다.
PULSE_SERVER=unix:/run/pulse/native pactl list short sink-inputs
# USB 장치가 확인됐고 활성 출력이 HDMI일 때만 전환한다.
busctl call com.atlas.AudioManager1 /com/atlas/AudioManager1 com.atlas.AudioManager1 SetActiveOutputDevice o /com/atlas/AudioManager1/Device/usb_speaker
busctl get-property com.atlas.AudioManager1 /com/atlas/AudioManager1 com.atlas.AudioManager1 ActiveOutputDevice
# 기대값: o "/com/atlas/AudioManager1/Device/usb_speaker"
```

[문서] 전환 후에는 PM 승인 차임으로 실제 출력을 확인한다. 다음 예시는 저장소 루트 기준이며 기본 `pulsesink`를 쓴다. 별도 창의 `pactl list sink-inputs`에서 ReSpeaker로 연결됐는지와 현장 청취를 함께 확인한다. `AUDIO_SINK`만 지정하거나 종료코드0인 것만으로 통과시키지 않는다.

```bash
PULSE_SERVER=unix:/run/pulse/native gst-launch-1.0 -q filesrc location=flutter_app/assets/audio/CAUTION.wav ! wavparse ! audioconvert ! audioresample ! pulsesink
```

[문서] HDMI 복원은 담당자와 조정해 같은 메서드에 `/com/atlas/AudioManager1/Device/hdmi_speaker`를 전달한다. 실제 AppManager 앱 계정에서 alert→asset 추출→ReSpeaker 재생과 새 경고로 이전 소리가 중단되는 흐름을 추가 확인했다. 시험 음원은 #137 후보이며 운영 음원을 교체한 결과와 구분한다. [출력 복구 보고서](../inbox/이수현/2026-10-25-respeaker-output-recovery.md#2-실제-flutter-앱-확인과-종료-지점)를 근거로 Flutter 담당자가 최종 확인한다. 이번 검증에서는 설정이 유지돼 부팅 서비스나 앱 시작 시 자동 호출은 추가하지 않았다. 다른 이미지·USB 재연결·재부팅 후에는 속성을 다시 확인한다.

[실측 2026-10-27 E2E 3회차, 정인] **앱을 다시 설치한 뒤에도 확인한다.** 오전 재부팅 뒤 확인 때는 `usb_speaker`였는데, 앱을 세 번 다시 깐 뒤 13:34 결과 안내가 들리지 않았고 `ActiveOutputDevice`가 `hdmi_speaker`였다. 위 `SetActiveOutputDevice usb_speaker`로 되돌렸다. 원인은 앱 설치·실행 때 ATLAS가 기본 출력을 HDMI로 되돌리는 것으로 보인다 [추정 — 확인: 다음 앱 설치 직후 `ActiveOutputDevice` 값]. 앱을 깐 뒤에는 이 값부터 본다.

[문서] **재부팅 뒤 수동 노드도 확인한다.** `systemctl is-active hoodseek-vision-node`가 inactive이면 담당자와 공유 사용 상태·시연 준비를 확인한 뒤 `systemctl start hoodseek-vision-node`로 수동 기동한다. vision이 꺼져 presence unknown이면 Core가 인덕션 켜기를 허용하지 않을 수 있다(#67). 안전 허가를 우회하거나 자동 기동을 추가하지 않는다.

## Pi 5 화면 1080p EDID (#64)

시연 화면 smartmon 158TF3(15.8", PM 10/23 확정)는 Pi 5에 EDID를 보내지 않아(`/sys/class/drm/card0-HDMI-A-1/edid` 0 bytes) 커널 기본값 `1024x768`로 늘어나 보입니다 [실측 10/23]. [`pi5/edid-1920x1080.bin`](pi5/edid-1920x1080.bin)을 런타임 EDID로 넣으면 1920×1080 @ 60 Hz로 나옵니다. debugfs 설정이라 **재부팅하면 사라지므로 Pi 5 부팅 뒤마다** 넣습니다. 같은 모니터가 9/20에는 1920×1080으로 잡혔으므로(`docs/hardware-facts.md` §8), 넣기 전에 EDID 바이트 수를 먼저 보고 0이면 micro-HDMI 케이블부터 바꿔 봅니다 [추정: 0 bytes가 케이블 탓일 수 있음 — 확인: 아래 첫 명령을 다른 케이블로 다시 실행]. 케이블을 바꿔 128 이상이 나오면 EDID 단계는 건너뜁니다.

| 파일 | sha256 | 내용 |
|---|---|---|
| `pi5/edid-1920x1080.bin` (128 B) | `b83c4a1a8d5c12f0002f5424041c9de7a9b6b4af9896bf0890610922477c8718` | EDID 1.3. DTD = CEA-861 1080p60(148.5 MHz, H 1920/2008/2052/2200, V 1080/1084/1089/1125, +H +V), 범위 제한 56–62 Hz · 30–70 kHz · 최대 150 MHz, 이름 `1920x1080`, 텍스트 `HOODSEEK`, 344×194 mm(대각 약 15.6", 실제 15.8"와 조금 다름 — 해상도·타이밍과는 무관). 9/20 Pi 5 `/data/edid5.bin`(1280×720)에서 타이밍·이름·크기만 바꾸고 체크섬을 다시 계산 |

```bash
# 처음 한 번 (또는 Pi 5 사본 sha256이 위 표와 다를 때)
scp deploy/pi5/edid-1920x1080.bin root@$PI5:/data/edid-1920x1080.bin
ssh root@$PI5 'sha256sum /data/edid-1920x1080.bin'
# Pi 5 부팅 뒤마다: EDID 바이트 확인 → (0이면) EDID 적용 → window-manager 재시작 → Wi-Fi 절전 끄기 → 그다음 노드·앱 기동
ssh root@$PI5 'wc -c /sys/class/drm/card*-HDMI-A-*/edid'    # 0이면 케이블 교체 후 다시, 그래도 0이면 다음 줄
ssh root@$PI5 'mount | grep -q debugfs || mount -t debugfs none /sys/kernel/debug; cat /data/edid-1920x1080.bin > /sys/kernel/debug/dri/0/HDMI-A-1/edid_override; systemctl restart window-manager.service'
ssh root@$PI5 'iw dev wlan0 set power_save off; iw dev wlan0 get power_save'    # Power save: off (재부팅하면 다시 on)
ssh root@$PI5 'head -n 1 /sys/class/drm/card0-HDMI-A-1/modes'    # 1920x1080
```

- 확인 [실측 10/23, Pi 5]: `/sys/kernel/debug/dri/0/state` → `mode: "1920x1080": 60 148500 1920 2008 2052 2200 1080 1084 1089 1125`, ATLAS 설정 앱 표시 또렷, 터치 위치 정상
- Wi-Fi 절전: 기본 on이고 재부팅하면 다시 켜집니다. 켜진 상태에서 Pi 5가 네트워크에서 한 번 사라졌습니다 [실측 10/23, `docs/hardware-facts.md` §11, 절전과의 관계는 미확인]
- 되돌리기: `echo reset > /sys/kernel/debug/dri/0/HDMI-A-1/edid_override` 뒤 window-manager 재시작
- 터치 USB는 **외부 전원 USB 허브**를 거쳐 연결합니다. Pi 5에 직결하면 USB 과전류로 카메라·마이크까지 끊깁니다 [실측 9/20·10/23, `docs/hardware-facts.md` §8]

## 현장 보정 — 온도 경계 3개 (#65)

`T_metric`(= `max − median`)은 실내 온도에 따라 흔들립니다. 같은 손난로 단계의 metric이 10/24 → 10/25에 0.86~1.90 낮아졌고, 손난로 최고 온도(`max`)는 −0.63~+0.71로 거의 같았습니다 [실측 #65 정인 10/25 T3-2 중간, 실내 21.8 → 23.0 °C]. 그래서 **리허설·시연 직전, 시연 자리에서** 손난로 단계를 다시 재고 `P_T_mid`·`P_T_high`·`P_T_danger` 세 값만 기기 profile에서 고칩니다. Core는 SIGHUP으로 profile을 다시 읽으므로 재시작하지 않습니다(BRD TR-POL-10, `safety_core/src/main.cpp`).

- 바꾸는 것은 이 세 값뿐입니다. 다른 키·코드·손난로 단계 온도(약불 45 · 중불 50 · 강불 55 · 화재위험 65 °C)는 바꾸지 않습니다.
- 저장소의 `profiles/default.json`은 그대로 둡니다. 현장 값은 그날 그 자리의 값이라 기록으로만 남깁니다.
- G7 코드 프리즈 뒤(시연 당일 포함)에도 이 절차는 허용합니다 (PM 제안 2026-10-25, #138 리뷰로 합의). 조건: 세 키만 · 그날 그 자리에서 잰 값과 아래 계산식, 여유 > 0일 때만 · 전·후 sha256과 값 기록 · **시연 중이 아니라 리허설 직전에** (정인 #138 리뷰).

### 1. 측정

준비: 손난로 충전(10/25 65 °C 단계가 배터리 방전으로 무효), 박스·카메라·인덕션은 고정 상태 그대로, Core·vision 켬, 실내 온도 기록. 사람은 조리대 앞에 있습니다 — presence가 absent·unknown이면 중불부터 팬이 3단이 됩니다.

| 단계 | 손난로 | 팬 |
|---|---|---|
| 약불 | 45 °C | 1 |
| 중불 | 50 °C | 2 |
| 강불 | 55 °C | 3 |
| 화재위험 | 65 °C | 3 |

손난로를 냄비 자리에 넣고 최근 30 s metric 변동이 0.6 이하가 되면(「안정」) 그때부터 180 s를 셉니다. 기록·요약은 #122 T3와 같습니다.

```bash
# Mac에서. PI4 = Pi 4 주소(저장소에 적지 않음)
ssh root@$PI4 'journalctl -u hoodseek-safety-core -o short-unix -f' > core.log   # 측정 내내
# events.csv: 「안정 45」「끝 45」처럼 Pi 시각(date +%s)으로 한 줄씩
python3 inbox/정정인/2026-10-24-incense-matrix/tools/t3_summary.py <측정 폴더>
```

### 0. 키 이름 옮기기 + 고양이 감시 키 더하기 (D-059·D-060·D-065, 새 Core를 처음 올릴 때 한 번)

state v 3 Core는 프로파일 키를 새 이름으로 읽고, 고양이 감시 `P_cat_absent_s`(D-060)도 읽습니다. 옛 이름이 남았거나 새 키가 없는 `profile.json`이면 키 누락으로 로드를 거부하고 fault(팬 3·인덕션 0)로 시작합니다. **새 Core 바이너리를 바꾸기 전에** 값은 그대로 두고 키 이름만 옮긴 뒤 `P_cat_absent_s` 한 줄을 더하고, D-065 두 값(`P_cat_absent_s` 5.0 · `P_person_score` 0.5)을 맞춥니다. 이미 새 Core인 기기에서 다시 돌려도 이 두 값만 바뀝니다.

```bash
ssh root@$PI4 'cd /data/hoodseek && cp profile.json profile.json.bak-$(sha256sum profile.json | cut -c1-8) && sed -i \
  -e "s/\"P_T_caution\"/\"P_T_mid\"/" -e "s/\"P_T_warn\"/\"P_T_high\"/" \
  -e "s/\"P_fan_min_caution\"/\"P_fan_min_mid\"/" -e "s/\"P_fan_min_warning\"/\"P_fan_min_high\"/" \
  -e "s/\"P_ind_max_warning\"/\"P_ind_max_absent\"/" profile.json && \
  { grep -q "\"P_cat_absent_s\"" profile.json || sed -i "s/\"P_absent_s\": *\([0-9.]*\),/\"P_absent_s\": \1, \"P_cat_absent_s\": 5.0,/" profile.json; } && \
  sed -i -e "s/\"P_cat_absent_s\": *[0-9.]*/\"P_cat_absent_s\": 5.0/" -e "s/\"P_person_score\": *[0-9.]*/\"P_person_score\": 0.5/" profile.json && \
  grep -c "\"P_T_caution\"\|\"P_T_warn\"\|\"P_fan_min_caution\"\|\"P_fan_min_warning\"\|\"P_ind_max_warning\"" profile.json; grep -o "\"P_cat_absent_s\": *[0-9.]*\|\"P_person_score\": *[0-9.]*" profile.json; sha256sum profile.json'
```

- `grep -c`(옛 키 5개를 셈)는 `0`, 그다음은 `"P_cat_absent_s": 5.0`·`"P_person_score": 0.5` 두 줄이어야 합니다. 두 번 돌려도 키가 겹치지 않고, 앞서 3·4.4로 넣었던 profile도 5.0으로 바뀝니다. 값은 D-065(PM 10/27)입니다: `P_person_score` 0.5는 빈 박스 바닥 사람 오탐(최고 0.457)을 없애는 값, `P_cat_absent_s` 5.0 s는 0.5에서 사람·인형 같이 박스 안을 다시 잰 값(설계서 §3.4 — 규칙값 7.0과 다른 이유도 거기). 그 전 4.4 s는 0.35에서 2.36 s + 2 s였습니다(D-060). 옛 Core는 새 키를 모르므로, **되돌릴 때는 바이너리와 함께 `profile.json.bak-…`도 되돌립니다.**
- 값(오늘 현장 보정 16.47 · 21.51 · 25.36 등)은 바뀌지 않습니다. 옮긴 뒤 sha256을 #67에 남깁니다.

### 2. 계산

경계 = (아래 단계 metric 최대 + 위 단계 metric 최소) / 2, 소수 둘째 자리까지 남기고 내림(예: 19.185 → 19.18). 여유 = (위 단계 최소 − 아래 단계 최대) / 2.

| 키 | 아래 단계 최대 | 위 단계 최소 |
|---|---|---|
| `P_T_mid` (중불) | 45 °C | 50 °C |
| `P_T_high` (강불) | 50 °C | 55 °C |
| `P_T_danger` | 55 °C | 65 °C |

- 여유가 0 이하(두 단계 범위가 겹침)면 그 경계로는 단계를 가를 수 없습니다. 고치지 말고 기존 값을 그대로 두고 PM에게 알립니다.
- `P_T_mid`는 빈 상태 + 손 뻗기 최대 9.36(#64 S1)보다 커야 합니다.

### 3. 적용과 확인

```bash
ssh root@$PI4 'cd /data/hoodseek && cp profile.json profile.json.bak-$(sha256sum profile.json | cut -c1-8)'
scp root@$PI4:/data/hoodseek/profile.json .       # Mac에서 세 값만 고침
python3 -m json.tool profile.json >/dev/null      # JSON 형식 확인
scp profile.json root@$PI4:/data/hoodseek/profile.json
ssh root@$PI4 'systemctl kill -s HUP hoodseek-safety-core && sha256sum /data/hoodseek/profile.json'
```

- Core 로그에 `[core] SIGHUP: profile 재로드`가 찍히고 state `thresholds`의 `t_mid`·`t_high`·`t_danger`가 새 값이면 적용된 것입니다. 화면 지표 카드의 기준값도 state 값을 그대로 씁니다.
- `[core] SIGHUP: profile 재로드 실패 (…) — 이전 값 유지`가 나오면 Core는 이전 값으로 계속 돕니다. 순서(`P_T_mid < P_T_high < P_T_danger`)와 숫자 형식을 확인합니다.
- 되돌리기: `profile.json.bak-…`을 `profile.json`으로 복사하고 다시 SIGHUP.
- 기록(#67 또는 시연 기록): 실내 온도, 단계별 metric 평균(범위), 새 값 3개와 여유, profile sha256 전·후.

### 4. 현장 보정 기록

방 온도에 따라 같은 손난로의 metric(최고 − 중앙)이 달라져서 경계가 날마다 바뀐다. 저장소 `profiles/default.json`(16.99 · 21.86 · 27.07, D-056)은 기준값으로 두고, 시연장에서는 이 절차로 다시 잰다(PM 10/27). 장면 median은 실내 온도계 대신 적은 열화상 중앙값이다.

| 언제 | `P_T_mid` · `P_T_high` · `P_T_danger` | 여유(mid · high) | 장면 median | profile sha256 | 근거 |
|---|---|---|---|---|---|
| 10/26 오전(M2 1차 뒤)–21:20 | 16.47 · 21.51 · 25.36 | — · 0.73 | — | `e24e6b17…` → `ad4ef948…`(18:39 §0 키 이름 옮기기, 값 같음) | [실측 정인] 10/26 오전 보정(#67), E2E 1회차 머리말 |
| 10/26 21:20–22:26 | 15.34 · 18.83 · 25.36 | 1.41 · 1.61 | 23.94–24.66 | `13cacf94…` | [실측 정인] 보정 기록(#67) |
| 10/27 13:26–13:51 | 17.94 · 22.22 · 25.36 | 2.02 · 0.88 | 22.38–23.40 | `abac4294…` | [실측 정인] 보정 기록(#67) — 강불 여유 0.88로 세 번 중 가장 작아 50 °C·55 °C가 가까움. 55 °C 최대 23.86과 `P_T_danger` 사이가 1.50으로 줄어, 방이 더 시원하면 55 °C에서 과열 warning이 날 수 있음 [추정 — 확인: M3·M8에서 `TEMP_DANGER`가 안 뜨는지] |

## 인덕션 음성 목표 단수 후보 배포 (#182, 배포 순서)

이 절은 새 후보의 배포 순서이며 운영 적용 완료를 뜻하지 않는다. `voice IND_SET`은 v2, Core state는 v4로 바뀐다. 기존 실행본의 필수5종 성적을 새 실행본 승인으로 재사용하지 않는다.

1. #186의 PM 계약·문구 승인과 요청 리뷰어 전원 승인을 확인한다. 운영 발행은 아래 현장 확인 뒤 허용한다.
2. state v1·2·3·4를 받는 Flutter를 먼저 배포하고 기존 Core(state v3) 연결을 확인한다.
3. 기존 파일을 백업한 뒤 voice v1·2 수신·state v4 Core를 배포한다. 앱 수신과 기존 터치·필수5종 회귀를 확인한다.
4. 새 voice-node는 기본 발행 끔으로 배포한다. 수동 기동·ReSpeaker 입력 검증·모델/바이너리 해시 고정은 기존 절차를 따른다.
5. 새 구성의 현장 부정 시험(비명령·실제 주변음)과 새 음원의 실제 스피커→마이크 재유입·PM 청취를 확인한다. 이후 승인한 범위에서만 발행한다. 이 PR의 문자열·단위 검사는 현장 시험을 대신하지 않는다.

되돌릴 때는 voice를 먼저 멈추고 이전 voice/Core 실행본·설정을 복구한다. 새 앱은 이전 state v3를 계속 받는다. 앱·Core·voice의 수동 기동 정책과 안전 허가·만료는 바꾸지 않는다.
