# 앱 음원 9개 — Q3 E2 안내·기동음·조작음·경고음 (2026-10-26)

Q3 E2 안내 음성 5개와 CAUTION은 기존 파일을 유지한다. 사용자 선택(#167)에 따라 기존 WARNING 상행 차임을 APP_START로 옮기고, WARNING은 4회 반복음으로 바꿨다. 짧은 조작 확인음 TOUCH_FEEDBACK을 추가했다. **음원 선택은 완료됐고, Flutter 재생 연동은 재범·PM 검토와 현장 확인 뒤 적용한다.**

| 파일 | 언제 울리나 | 내용 | 길이 |
|---|---|---|---|
| `CAUTION.wav` | D-059부터 Core가 내지 않음; 계약 ID·기존 파일 보존 | 말 없는 차임 E5 → A5 | 0.96 s |
| `WARNING.wav` | 과열·과한 연기 WARNING alert; 정상 중불·강불에는 없음(D-059) | 720 Hz 전자음 4회 | 1.31 s |
| `APP_START.wav` | 대시보드 생성 때 한 번 | 기존 WARNING 차임 G5 → C6 → E6 | 1.04 s |
| `TOUCH_FEEDBACK.wav` | 최신 터치 요청의 목표 단수·ACK 확인 | 작고 부드러운 키보드형 탭 V5 | 0.06 s |
| `DANGER.wav` | 화재 위험 진입 | 화재 위험을 미리 감지해 인덕션을 껐어요. 후드를 최대로 돌리고 있어요. | 6.47 s |
| `CAT_LOCK.wav` | 반려동물 잠금 | 반려동물이 감지돼 안전 잠금이 설정됐어요. | 4.88 s |
| `OFFLINE_RECOVER.wav` | 고장·끊김에서 복구 | 장치 상태가 복구됐어요. | 2.40 s |
| `CMD_REJECTED.wav` | 명령 거부 | 지금은 요청을 처리할 수 없어요. 화면의 안내를 확인해 주세요. | 5.98 s |
| `OFFLINE.wav` | state 3초 무수신 | 안전 장치의 응답이 없어요. 연결 상태를 확인해 주세요. | 6.32 s |

## 제작·라이선스

- 음성5개는 **AI 합성 음성**이다. 직접 만든 Q3 E2 합성음을 재사용·참조했으며 실제 사람이나 팀원의 목소리를 녹음해 복제하지 않았다.
- **Typecast를 사용하지 않는 것으로 결정해 Typecast 무료 플랜의 이용 제한 문제는 이번 교체안에서 해소했다.** Typecast API·음원·참조 파일은 사용하지 않는다.
- 음색 설계: [Qwen3-TTS 1.7B VoiceDesign](https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign). 다른 문장 생성: [Qwen3-TTS 1.7B Base](https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-Base). 코드·모델은 Apache-2.0이며 라이선스 전문은 `MODEL-LICENSE.txt`에 보존한다.
- Mac 제작 도구: mlx-audio 0.6.1(MIT), MLX 8bit 변환 모델. 사용한 모델 revision·직접 만든 참조음 해시·파일 해시·후처리는 `manifest.json`에 남긴다. 모델 가중치·합성 참조음·제작 가상환경은 앱에 넣지 않는다. Pi5는 WAV만 재생한다.
- OFFLINE_RECOVER는 선택된 E2의 첫 문장을 그대로 보존했다. DANGER는 같은 기준 샘플의 두 문장을 재사용했다. 나머지3개는 자체 합성 참조음으로 생성했고 말끝 세기만 약하게 다듬었다. 음절별 시간·음높이 강제 편집으로 발음이 흐려지는 방법은 제외했다.
- CAUTION·APP_START는 남현지(PM)가 만든 기존 차임이며, WARNING·TOUCH_FEEDBACK은 이수현 작업의 Python 합성음이다. 모두 `make_chimes.py`에서 Python 표준 라이브러리만으로 재현하며 외부 음원·음성 모델을 사용하지 않는다.

## 형식과 검증 범위

- 현재 파일 9개는 모두 48 kHz·mono·PCM16 WAV, 클리핑 0이다. `manifest.json`의 파일별 길이·sha256과 `SHA256SUMS`를 함께 확인했다. `python3 make_chimes.py`를 실행해도 선택한 차임·조작음 4개의 바이트가 동일하다.
- 음성 5개는 −19 LUFS 목표로 조정한 기존 Q3 E2 파일이다. 선택 E2 보존 파일·CAUTION·APP_START의 음량은 원본을 유지한다. TOUCH_FEEDBACK은 사용자가 청취해 확정한 V5의 작은 음량을 보존했다.
- **과거 검증의 범위**: #137의 전체 파일 전사 7/7 DROP과 ReSpeaker 재유입 7개×10회(62+8회) 실행 intent 0·송신 0은 당시 7개 해시에 대한 결과다. 그 시험에는 이번 새 WARNING·TOUCH_FEEDBACK이 포함되지 않았다. 음성 5개와 CAUTION은 같은 파일이며 APP_START는 당시 WARNING과 같은 바이트다.
- 당시 재유입 ASR 결과는 DROP 35·NONE 1이고 NONE 포함 노드 candidates 카운터는 1이었다. 시험 입력을 ReSpeaker로 옮겨 매 재생 확인한 조건이며 운영 노드 자동 라우팅 전체의 증거로 확대하지 않는다. [#137 보고서](../../../inbox/이수현/2026-10-25-q3-e2-audio-replacement.md)에 조건·해시를 남겼다.
- 기존 음성 5개는 [PM 청취 승인](https://github.com/lambdaramji/HOOD-SEEK/pull/137#issuecomment-6396473963) 뒤 파일을 바꾸지 않았다. 10/26 새 WARNING·TOUCH_FEEDBACK은 Pi5·ReSpeaker 실제 재유입 각 10회에서 실행 후보 0·송신 0이며, USB 출력/입력 경로 20/20과 두 소리 청취를 확인했다. 새 Flutter 앱의 기동·터치 트리거 실기는 별도로 남아 있다.
- 이번 소프트웨어 검사와 음성 명령 안내 검토는 [#167 보고서](../../../inbox/이수현/2026-10-26-audio-feedback-and-voice-result-proposal.md)에 구분한다.

## 재생·운영

- `state.p.alert`가 NONE 아닌 새 값으로 바뀔 때 해당 WAV를 한 번 재생하고, state가 3초 없으면 OFFLINE을 낸다. 경고 발생 여부는 Core가 정한다. 새 경고는 앞 소리를 끊는다.
- APP_START는 대시보드 생성 때 한 번이며 재접속·탭 이동에는 반복하지 않는다. 기동음·조작음은 경고·끊김 안내의 파일 로딩/프로세스 시작/재생을 방해하지 않고 해당 피드백을 생략한다.
- TOUCH_FEEDBACK은 최신 로컬 FAN_SET·IND_SET 요청 뒤 3초 안에 목표 단수와 Core state·ACK가 일치할 때만 한 번 낸다. 같은 단수·반려·전송 실패·지연·안전 하한으로 목표와 다른 단수·단순 화면 터치는 조용하다. Core 계약에 요청별 수락 ID가 없어 개별 요청의 수락이나 물리 동작 완료를 보증하는 신호는 아니다.
- 음성 명령의 최종 결과 안내 WAV·조건표는 #173 후속 범위다. 이 PR의 터치 조작음은 음성 명령·USER_LOCK·ACK_ALERT에 적용하지 않는다. 기존 CMD_REJECTED 등은 Core alert대로 재생한다.
- 스피커 소리가 같은 마이크에 다시 들어오므로 호출어를 포함한 명령 문구로 시작하지 않는다. 음원 교체가 음성노드의 실제 발행을 켜는 것은 아니다.
- 형식: WAV(PCM). Pi5에서는 `gst-launch-1.0 … wavparse ! audioconvert ! audioresample ! <AUDIO_SINK>`로 재생한다. 기본 출력 요소는 장치 미지정 `pulsesink`다.
- [실측 2026-10-25] ATLAS `ActiveOutputDevice`가 HDMI면 `device=`를 지정해도 HDMI로 재생될 수 있다. [배포 안내의 USB 출력 확인](../../../deploy/README.md#pi-5-respeaker-출력-확인-133)을 먼저 수행한다. AudioManager에서 USB를 선택한 뒤 기본 `pulsesink`도 ReSpeaker로 나왔고 재부팅 1회 뒤에도 유지됐다.
- `--dart-define=AUDIO_SINK`로 GStreamer 출력 요소를 지정할 수 있지만, 그 값이나 종료코드 0만으로 실기 출력을 보장하지 않는다. 실제 sink·USB 재생 상태와 청취를 함께 확인하며 실제 장치 이름은 저장소에 적지 않는다.
- 클립 파일이나 GStreamer가 없으면 화면은 그대로 두고 로그만 남긴다.
- [#136](https://github.com/lambdaramji/HOOD-SEEK/pull/136)은 앱 환경에 `PULSE_SERVER`가 없고 시스템 PulseAudio 소켓이 있을 때만 그 소켓을 넘긴다. 기존 값은 덮어쓰지 않으며 출력 장치 선택은 AudioManager가 담당한다.

## 출력 복구와 음원 교체 범위

[#134](https://github.com/lambdaramji/HOOD-SEEK/pull/134)는 출력 복구 문서·주석만 다루고 당시 Supertonic/PM 음원을 바꾸지 않았다. Qwen Q3 E2 음원·라이선스·제작/검증 기록은 [#137](https://github.com/lambdaramji/HOOD-SEEK/pull/137)의 교체 범위다. 출력 복구·음원 교체는 음성노드를 자동 기동하거나 발행을 켜는 변경이 아니다.
