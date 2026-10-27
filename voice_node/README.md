# voice-node — 오디오 관측 파이프라인과 TCP 송신

## SenseVoice 하이엘지 실행과 선택적 송신

[문서] 현재 구성은 ReSpeaker/PulseAudio → 스트림 Silero VAD(CPU1) → 상주 SenseVoiceSmall INT8(CPU4) → 하이엘지·대상·동작 분류 → 선택적 TCP v1 voice 송신입니다. 기본은 발행 끔이며 **명시적 수신 주소와 `--publish`에서만 송신**합니다. 최초 전사·최종 문장·역할별 유사도·후보·wire intent·송신 결과를 로그로 남깁니다. Core의 안전 판정·스키마·intent는 바꾸지 않습니다. 아래 정책의 PM·계약 리뷰와 실제 장치 전체 경로 확인은 별도 완료 조건입니다.

### 실제 PulseAudio 입력 확인·고정

[문서] `--pulse-source`에는 `pactl list short sources`에서 확인한 실제 source 이름을 쓴다. ATLAS에서는 요청한 ReSpeaker가 아닌 다른 USB 마이크에 녹음 스트림이 연결되는 현상을 확인했다. 장치 설정값이나 과거 `capture_ready source=...`만으로 실제 입력을 판정하지 않는다.

[문서] Pulse 빌드는 기존 SDK의 `libpulse` 개발 파일도 사용한다(`pkg-config libpulse`, ATLAS 17.0 확인). 노드는 자기 `pulsesrc.source-output-index`에 대해서만 요청 장치로 이동하고 `current-device`를 대조한다. 확인 전 PCM은 버려 원음 저장·VAD·ASR로 보내지 않는다. 확인 뒤 `capture_source_verified` → `capture_ready` 순서로 기록한다. 3초 안에 확인하지 못하면 오류로 종료하며 다른 입력으로 대체하지 않는다. 실행 중 입력이 바뀌면 새 PCM을 버리고 기존 추론 중단 플래그를 올려 후속 처리를 중단한다. 시스템 기본 입력·AudioManager 설정·다른 프로세스의 스트림은 변경하지 않는다.

[문서] 수동 기동·기본 발행 끔은 유지한다. 시작 때 실제 로그와 `pactl list source-outputs`의 해당 프로세스/Source를 함께 확인한다. 오류로 재시작을 반복하면 서비스를 stop하고 입력 연결을 점검한다. 이 변경은 모델·파서·VAD 문턱을 바꾸지 않지만 새 바이너리이므로, 현장 발행 전 PM의 부정 시험/시연 범위 확인을 다시 받는다. 기존 C 24/25를 이 수정본의 실발화 성적으로 쓰지 않는다.

[문서] 다음 **수동 기기 검사**는 다른 녹음이 없을 때만 실행한다. 두 정상 기동과 자기 스트림을 다른 입력으로 강제 이동했을 때 중단되는지, 없는 입력에 대체 연결하지 않는지를 확인한다. Core 주소·`--publish`·원음 저장 옵션은 쓰지 않는다.

```sh
# 기존 모델/런타임을 사용하는 별도 시험 바이너리와 실제 입력 이름을 지정한다.
export VOICE_TEST_BINARY=/path/to/candidate/hoodseek-voice-observer
export VOICE_TEST_MODEL_DIR="$SENSEVOICE_MODEL_DIR"
export VOICE_TEST_VAD_MODEL="$VAD_MODEL"
export VOICE_TEST_SOURCE="$MIC"
export VOICE_TEST_OTHER_SOURCE='<다른 사용 가능한 마이크의 PulseAudio source 이름>'
sh voice_node/tests/pulse_source_guard_test.sh
```

[실측/한계] [입력 고정 검증 보고서](../inbox/이수현/2026-10-25-voice-input-routing-fix.md)에 현재 운영본의 잘못된 연결 재현과 별도 후보의 검사를 구분했다. 과거 C·부정 시험은 ReSpeaker 지정 기록만 남아 실제 마이크 귀속은 미확인이다.

### 2026-10-25 이수현 지정 시연 조건부 승인

[실측/PM 결정] 남현지 완주2회19/25(76%,120cm)·18/25(72%,150cm)와 이전 공식14/25는 지정 화자 NFR-08 미달로 확정했다. 이수현150cm 추가25회는 정답intent/value·Core수락24/25(96%)이며 PM이 원음 신호 끝→판정 최대1.48초(보수적1.73초)를 시연 근거로 수용했다. 일반화 성능이나 남현지 공식 합격으로 표시하지 않는다.

[조건부 승인] [PM 판단2026-10-25](https://github.com/lambdaramji/HOOD-SEEK/issues/62#issuecomment-6393359736)에 따라 **이수현·C시험150cm자리·검증된 필수5종**으로 시연한다. 발행 전 source55254b7·실행본1c38613a… 그대로 **이수현/남현지가 나눠 말하는 새 현장 비명령36개+실제 주변음300초(조용150·팬/대화150)에서 실행후보0**을 확인해야 한다. 보관 음원 재전사0은 새현장 조건을 대체하지 않는다. 자리·대본은 [시연 대본](../inbox/이수현/2026-10-25-demo-speaker-approval-request.md#시연-대본과-자리-150-cm)과#67에 기록한다. 오작동 시 터치로 전환한다. 기본 발행 끔·수동 기동을 유지한다.

### #107 완료 범위와 후속 PR 경계

[문서] #107은 **현재 한 문장 발화 방식의 음성노드를 기존 TCP v1 수신 경로에 연결하고 수동 배포할 수 있게 하는 작업**이다. “하이엘지 + 명령”을 한 발화로 말하며, 발화 종료 후 전체 전사에서 호출어를 확인한다. 기존 Flutter의 Core 결과 표시는 재사용한다. 근거: `src/audio_observer.cpp`, `src/voice_tcp.cpp`, [배포 안내](../deploy/README.md#voice-node-수동-기동), [T6 리뷰](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5636822111).

| #107에서 마무리 | 후속 PR에서 다룰 것 |
|---|---|
| SenseVoice·Silero·현재 호출어/명령 판정 → 선택적 TCP 송신 | 호출어 확인 후의 요청·미송신/만료 상태를 Flutter에 전달하는 Pi5 로컬 경로 |
| 기본 발행 끔·수동 start/stop, 환경 전환·복원 안내 | T5/T6 이벤트 형식·끊김·요청 식별자 합의 및 Flutter 수신/표시 |
| UTF-8/길이/intent 검증, 발화당1회·기한·연결 감시·미재전송 검사 | 동일 명령의 Core 결과를 건별로 구분하는 계약 식별자 |
| 기존 Core 수락/거부·현재 단수·장치 결과의 통합 확인 | 목표 단수+방향·새 intent/reason 계약(기본 팬 켜기는 PM 결정대로1단) |
| PM·계약 승인, 공식 NFR-08 및 부정 시험의 충족 여부 확인 | 별도 음향 호출어 검출기·호출 후 대기 세션·부분 전사·TTS가 필요하면 별도 범위 합의 |

[문서/합의 필요] UI 로컬 연동은 현재 송수신 검증의 선행 조건이 아니다. 일반 화면은 주변 대화에 반응하지 않고 전사 후 호출어가 확인된 요청부터 표시하는 안을 [#113 리뷰](https://github.com/lambdaramji/HOOD-SEEK/pull/113#pullrequestreview-5636978134)에 제안했다. 입력 레벨·전사 원문·처리 시간은 진단 화면용이며 현재 진단 웹 페이지가 Flutter IPC를 제공하는 것은 아니다. 로컬 이벤트 형식·전송·끊김·요청 식별자와 UI 표시는 T5/T6가 별도 합의·구현한다.

[문서] #113 `b3a1127`의 PM 결정은 **state만 v2, Core 입력 hello/vision/voice/ui_cmd는 v1 유지**다. 현재 voice 송신은 v1을 유지하며 #116 때문에 송신자의 v를 함께 올리지 않는다. `VoiceTcpClient`는 state의 줄 프레이밍/수신 시각만 확인하고 JSON 본문을 해석하지 않는다. 실제 계약·Core·Flutter의 혼합 버전 이행 검증은 #116에서 수행한다. 향후 voice 입력 계약까지 바뀔 때는 T5도 이행 범위에 넣는다. 근거: [UI 설계 §4.5.1·§7](https://github.com/lambdaramji/HOOD-SEEK/blob/b3a112760545eb27bd965c7897d8126c47a3c99c/docs/2026-10-24-flutter-ui-design-v1.md#L217), [D-051](https://github.com/lambdaramji/HOOD-SEEK/blob/b3a112760545eb27bd965c7897d8126c47a3c99c/docs/ai-integration-decisions.md#L84).

[문서/제안] 일반 화면의 기본 예시는 `하이엘지, 후드 약풍`처럼 한 문장으로 표시한다. 부분 전사·음성 인식률·말하는 중 호출 감지·TTS/호출 응답음은 현재 제공하지 않는다. Core의 `accepted=true`는 요청 수락이며 실제 단수/장치 동작 완료와 구분한다. 호출어 미확인 발화에는 일반 화면을 유지하고, NONE의 중립 안내·전달/만료 상태는 로컬 경로가 생긴 뒤 제공한다. X·바깥 탭·시트 닫기는 표시만 닫으며 상주 노드를 종료하거나 전달된 명령을 취소하지 않는다. UI 자동 닫힘 시간은 VAD 최대 발화4초와 별도다. 온도 조회·남은 환기 시간·후드 상태 조회는 현재 지원하지 않는다.

[문서] #107의 종료는 코드가 송신할 수 있다는 사실만으로 판정하지 않는다. 지정 화자·ReSpeaker/Pi5 공식25회(80% 이상·원음 종료→intent 3초 이내), 비명령30개 이상·실제 배경300초의 PM 수용 여부, 질문 오탐 정책·최종 문장/conf 의미 승인, 새 head의 수동 기동/종료·Core/화면/장치 확인, CI·요청 리뷰어 전원 승인이 남아 있다. 수현24/25와 두 실행의 배경150초씩을 공식 시험 승인으로 바꾸지 않는다. 근거: [PM 완료 조건](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5614157940), [송신 검증 보고서](../inbox/이수현/2026-10-24-sensevoice-tcp-publication-validation.md).

### 최신 실시간 55문항과 대상 표현 보완 — 2026-10-24

[실측] 수현·ReSpeaker/Pi5·팬 꺼짐의 실제 상주 입력에서 필수 5종×5회는 **최초 24/25(96%)**, 비명령은 **실행 후보 0/30(DROP15·NONE15)**이었다. 전체 문항 일치는54/55이며 전사 WER/CER나 하드웨어 집행 정확도가 아니다. 거리는 약30cm에서 중간에1m 이상으로 바뀌었고, 남현지 지정 화자의 공식 NFR-08 결과로 대체하지 않는다. 유일한 실패 `인덕션 꺼 → 인덕션 거`는 NONE이었으며 사람의 재시도 성공을 최초 성적에 넣지 않았다. 근거: [원문·집계·검토 요청](../inbox/이수현/2026-10-24-hi-lg-live55-validation.md).

[문서/실측] 시험에 사용한 **VAD CPU1**을 공통 관측기에 반영했다(Whisper에도 적용). ASR CPU4·시작224ms·종료544ms·앞992ms·4초 제한·송신 기한은 유지한다. CPU4 VAD의 긴 프레임 처리가 입력 소비를 지연시키던 문제를 줄이기 위한 변경이며 전력 실측 결과는 아니다. `event=vad_model_loaded`에 `vad_threads`를 남긴다. 근거: 같은 보고서의 VAD 대조와 `src/audio_observer.cpp`.

[문서] SenseVoice에서 `하이엘지 후드 켜/팬 켜줘/후드 팬을 켜주세요`는 사용자 지정 기본 풍량 **FAN_SET(1)**이다. 풍량을 명시한 요청은 그 값을 사용하며 인덕션 켜기는 `인덕션`을 명시한다. [PM 결정 2026-10-24] **실제 송신도 FAN_SET(1)**이다. 이미2·3단인 팬의 requested가1로 낮아질 수 있고 실제 단수는 Core가 `max(fan_min, requested)`로 보정한다. 현재 단수를 유지하는 FAN_ON 의미로 바꾸지 않는다. 목표 단수+방향은 계속 별도 합의 대상이다. 아래의 기존 Whisper 파서는 이 기본 풍량 확장을 적용하지 않는다. 근거: `src/hi_lg_rules.h`, `tests/hi_lg_classifier_test.cpp`.

[문서/실측] `불 세기`를 화력 대상 후보로 등록했다. 호출어와 올려/내려/줄여 동작을 확인한 뒤 기존 토큰 유사도로 `색이·세이·새기·쎄기·세기이·새이·색기`를 비교하며, 전역 치환이나 문턱 완화는 하지 않는다. 최근 실패 전사7건의 텍스트 재입력은0/7→7/7이고, 이전55문항과 재시도1건의 intent/value는 그대로다. 음성 재녹음·ASR 재실행 성적이 아니다. 근거: [구조화 결과](../inbox/이수현/measurements/2026-10-24-hi-lg-segi-text-replay.json).

[실측] 별도 무음·소음 시험은 생략안 이후 실제 수행했다. **조용150초·팬/유튜브150초 총300초에서 실행 후보0건·ASR 호출0건·송신0건**이다. 동일 구성의 두 수집에서150초씩 선택했으며 단일 연속300초 시험은 아니다. 입력 RMS도 확인했으므로 무입력0값을 소음으로 집계하지 않았다. [후속 보고서](../inbox/이수현/2026-10-24-voice-ambient300-validation.md)를 최신 근거로 제출한다. 이는 명령 성공·지연·발행 승인을 뜻하지 않는다.

[실측] 새 송신 구현의 Mac CTest8/8, 실제 TCP 전사55건 회귀·보관 WAV→격리 Core·Pi5 ARM 실행 검사 범위는 [송신 검증 보고서](../inbox/이수현/2026-10-24-sensevoice-tcp-publication-validation.md)를 따른다.


[PM 결정/시험 준비] 공식 후보 구성은 하이엘지8표기·근접 발음0.60·대상0.60·동작0.70·후보 차이0.10, Silero 시작224/종료544ms, SenseVoiceSmall INT8이다. 새 head·바이너리/모델 해시·파서 교정 범위를 #62에 고정한 뒤 **비명령36개와 실제 주변음300초에서 실행 후보0건**을 먼저 확인한다. 통과하면 남현지·팬 꺼짐·Pi5/ReSpeaker 필수5종×5회를 `--publish`로 평가하고, 미달이면 관측 모드로 intent·시간만 집계한다. 성공은 첫 시도의 intent/value 정답·실제 원음 끝→intent≤3초이며 발행 시 문항과 대조한 Core accepted=true도 필요하다. 실제 fan.level은 정답과 비교하지 않는다. `field_evaluation.py`는 발행 모드에서 문항별 Core 원문 근거가 없으면 성공으로 세지 않는다. 기본 발행은 계속 꺼짐이다. 근거: [PM 공식 시험 결정](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5637198979).

### 실제 TCP 송신의 처리 기준

[구현/합의 필요] 최초 전사 → 호출어·문장 전체 규칙/토큰 유사도 판정 → 송신 직전 같은 분류와 UTF-8·200자·intent/value 검증 → TCP v1 순서다. 활성어 없음/잘못된 입력은 DROP(미전송), 활성어가 있으나 부정·허가/상태 질문·인용·복합·미지원 문장은 NONE(value 없음), 지원 문장은 기존 intent를 보낸다. 대상 없는 꺼/멈춰와 팬 단수+방향은 NONE이며, 대상이 명시된 풍량 없는 팬 켜기는 FAN_SET(1)이다.

[구현/사용자 요청 2026-10-24] **하이엘지가 확인된 뒤 지원 명령 끝의 `?`·`？`는 문장부호로 취급**한다. “하이엘지 인덕션 꺼?” → IND_OFF, “하이엘지 인덕션 켜?” → IND_ON이다. 끝의 연속 물음표도 같은 기준이며 문장 중간의 물음표는 제거하지 않는다. 최초 전사는 보존하고 실행 문장의 최종 text에서는 끝 물음표를 제거한다. “켜도 돼?”·“끄지 마?”·상태/인용/복합·호출어 없음은 그대로 비실행이며, 미지원 문장을 무조건 실행시키지 않는다. 기존 Whisper/후드 모드는 유지한다. [PM 결정](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5637198979)에서 직접 질문 수용을 확인했으며 계약 담당 확인·새 head의 리뷰 승인은 별도로 남는다. 근거: `hi_lg_classifier.h::classify`, 분류/직렬화/실제 TCP 경계 시험.

[범위] 이 변경은 ASR의 질문 억양 구별 능력을 개선한 것이 아니라 명령 수용 정책 변경이다. 과거60건에서 비실행 정답으로 두었던 짧은 “인덕션 꺼?” 사례의 결과·라벨은 당시 정책 기록으로 보존한다. 새 정책으로 비교할 때는 정책 버전과 기대값 변경을 명시하고 과거 오탐을 음성 모델 개선 성적으로 지우지 않는다.

[구현/합의 필요] SenseVoice는 Whisper의 token p/no_speech 점수를 제공하지 않는다. **wire `conf=0`은 점수 미제공 표기**이고 확률0이나 자모 유사도가 아니다. Core는 현재 이 값을 판정하지 않고0..1 형식만 검사한다. `--min-score`·`--min-score-raise`·`--fallback-model`은 SenseVoice에서 거부한다. Whisper의0.7/0.8 품질 문턱을 통과했다고 표시하지 않는다. 이 의미와 하이엘지 활성어/최종 문장 정책은 `contract-change`·PM/계약 검토 대상이다.

[구현] TCP 연결마다 hello(seq0) → voice(seq1부터), 발화당1회, 큐·실패 재전송 없음, 3초 state 줄 무수신 시 연결 폐기, Linux TCP_USER_TIMEOUT 2초, 쓰기 대기 최대250ms를 재사용한다. ASR 작업 중에도 주 스레드가 TCP를 처리한다. 기한은 VAD 추정 발화 종료+5초이며 만료·연결 끊김이면 보내지 않는다. `sent`는 소켓 쓰기 완료이고 Core 수락/실제 집행은 `state.voice`와 장치로 따로 확인한다.

```bash
# 먼저 --publish 없이 입력·모델·hello/state를 확인한다.
hoodseek-voice-observer --sensevoice-dir "$SENSEVOICE_MODEL_DIR" \
  --vad-model "$VAD_MODEL" --pulse-source "$MIC" \
  --core-host "$CORE_HOST" --core-port "$CORE_PORT"
# 승인된 수신 대상에서 실제 송신 시험할 때 위 명령에 --publish를 추가한다.
```

[문서] 기본 Whisper 빌드에는 새 ASR 의존성이 없습니다. SenseVoice를 선택할 때만 기존 sherpa-onnx 1.13.8 CPU 배포본의 C API를 `SHERPA_ONNX_ROOT`로 지정합니다. 런타임·모델을 자동 다운로드하거나 저장소에 넣지 않습니다. Whisper 소스는 기존 #50 고정 커밋 `d1be6fde11ac6e0407606b4e42fe72d34add8037`을 사용하며 Silero와 WAV 읽기에 계속 필요합니다.

```bash
cmake -S voice_node -B build-voice-observe \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
  -DWHISPER_SOURCE_DIR="$WHISPER_SOURCE_DIR" \
  -DSHERPA_ONNX_ROOT="$SHERPA_ONNX_ROOT" \
  -DGGML_METAL=OFF -DGGML_ACCELERATE=OFF -DGGML_OPENMP=OFF
cmake --build build-voice-observe
ctest --test-dir build-voice-observe --output-on-failure

build-voice-observe/hoodseek-voice-observer \
  --sensevoice-dir "$SENSEVOICE_MODEL_DIR" \
  --vad-model "$VAD_MODEL" --wav "$TEST_WAV"
```

[문서] `--wav`는 여러 번 지정해 같은 모델로 파일을 순서대로 검사할 수 있습니다. `SENSEVOICE_MODEL_DIR`에는 `model.int8.onnx`·`tokens.txt`, `VAD_MODEL`에는 기존 `ggml-silero-v6.2.0.bin`을 지정합니다. 한국어·ITN·greedy·CPU4, 모델은 시작 때 한 번 로딩합니다. Pi5 빌드에는 기존 ATLAS SDK와 `-DHOOD_SEEK_WITH_PULSE=ON`을 사용합니다. Pi5에서는 해당 기기에 둔 런타임의 `lib`를 `LD_LIBRARY_PATH`에 추가합니다. SenseVoice 대응 바이너리는 Whisper 모드로 실행해도 이 런타임이 필요하며, Whisper 전용 빌드는 필요하지 않습니다. 기존 운영 바이너리를 덮어쓰지 말고 새 실행 경로를 사용합니다. ReSpeaker 앞 준비가 끝난 뒤 아래 명령으로 입력을 시작하고 Ctrl+C로 종료합니다.

```bash
export LD_LIBRARY_PATH="$SHERPA_ONNX_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
build-voice-observe/hoodseek-voice-observer \
  --sensevoice-dir "$SENSEVOICE_MODEL_DIR" --vad-model "$VAD_MODEL" \
  --pulse-source "$MIC" --seconds 300
```

[문서] `MIC`는 ReSpeaker의 실제 PulseAudio 입력 이름을 명시합니다. LifeCam/default로 자동 대체하지 않습니다. `conf=unavailable quality=asr_unscored`는 점수 미제공 표시이며, 자모 유사도는 ASR 확률이나 Whisper 품질 문턱과 다릅니다. SenseVoice의 `--min-score`, `--min-score-raise`, `--fallback-model`도 모델 로딩·입력·연결 전에 거부합니다. `--publish`를 생략하면 Core 주소를 지정해도 hello/state 관측만 하고 voice를 보내지 않습니다.

[문서] 주변 대화·속삭임을 분리/제거하는 화자 분리·소프트웨어 빔포밍은 구현하지 않았다. VAD의 noise_dbfs·relative_db는 진단용이며 음량 게이트나 대화 억제 필터가 아니다. 현재 파서 교정 결과를 소음 제거 성능으로 해석하지 않는다.

[문서] 공통 VAD는 시작224ms·종료544ms·앞부분992ms, 발화 구간4초·총 버퍼6초이며 EOF를 임의 확정하지 않습니다. Whisper의 ASR·품질·TCP 경로는 유지하지만 공통 VAD는 기존 에너지 방식에서 변경되었습니다. 기존 Whisper systemd 유닛도 `VAD_MODEL`을 채워야 합니다. 이번 코드 변경이 기기 서비스를 자동으로 바꾸지는 않습니다.

[문서] 현장 지연 검수에는 Pulse 입력에 `--capture-raw <새 파일>`을 추가할 수 있습니다. GStreamer tee가 같은 16 kHz·mono·S16LE 원음을 연속 저장하며 기존 파일은 거부합니다. `event=capture_audio_origin sample0_monotonic_ns`와 `event=asr intent_monotonic_ns`를 함께 보존합니다. 원음에서 확인한 마지막 발음 끝을 `speech_end_s`라 하면 실제 지연은 `(intent_ns − sample0_ns)/1e6 − speech_end_s×1000`입니다. 시간 원점·청취 검수가 없으면 공식 3초 통과로 집계하지 않습니다. `end_to_intent_ms`는 계속 VAD 종료 추정값으로 따로 남깁니다. 기록 옵션은 캡처/관측에만 쓰며 송신 허가를 추가하지 않습니다.

[문서] 보관 무음/소음 재생을 위해 WAV는 최대600초·20MiB까지 받습니다. 발화 구간4초 제한은 그대로입니다. 시험 계획·원음 검수·첫 시도 집계는 `tools/field_evaluation.py`와 [PM 시험 준비 기록](../inbox/이수현/2026-10-24-voice-pm-acceptance-preparation.md)을 따릅니다.

[문서/제안] [`hi_lg_classifier.h`](src/hi_lg_classifier.h)는 활성어·대상 자모 유사도0.60, 동작 요청형0.70, 다른 명령과의 차이0.10을 사용합니다. 명확한 동작은 우선하고, 부정·허가/상태 질문·인용·복합 표현을 검사합니다. 끝 물음표 수용은 위 정책을 따릅니다. 정확한8개 활성어 별칭 외의 근접 발음도 비교하므로 기존 닫힌 별칭 정책과 다른 **정책 변경 제안**이며 운영 발행은 PM·계약 검토 대상입니다. 별도 웨이크워드 모델이나 일반 언어 이해 모델을 추가한 것은 아닙니다.

[문서/제안] 남현지 녹음 회귀를 근거로 하이엘지 **확인 뒤 본문 처음**의 `호두·포두·푸두·푸도`도 후드 가전명 별칭으로 비교합니다. 이들은 호출어가 아니며 Whisper의 별칭 목록은 바꾸지 않습니다. 기존 가전명/어휘 교정을 전체 문법과 토큰 경로가 함께 사용합니다. 대상이 남는 두 음절 풍량 말미는 약풍·중풍·강풍과 유사도0.60·후보 차이0.10으로 비교하고, 단일 팬 요청이 성립할 때만 받습니다. 예: `하이엘지 호두 중궁` → `하이엘지, 후드 중풍`·FAN_SET(2). 한 음절 `준`·대상 없는 `약쿠`·후보가 동률인 `궁풍`·목표 풍량+올려/내려는 확정하지 않습니다. 동작 자체를 유사도로 고칠 때는 알려진 전체 대상 문법이 맞아야 하며 대상과 동작을 함께 추측하지 않습니다. `level_similarity`를 별도로 기록하며 ASR conf로 쓰지 않습니다. 같은 개발 음원의 교정·회귀 결과이므로 새 화자 독립 평가와 구분합니다. 근거: [남현지81건 재검토](../inbox/이수현/2026-10-24-hyeonji-parser-calibration.md).

[실측] 수현·현지 ReSpeaker60건의 Pi5 benchmark 3회 반복에서 지원40건 중 VAD37건·전체 WAV36건이 일치했고, 비명령20건 중2건은 실행 후보였습니다. 실제 송신은0건입니다. 질문 `인덕션 꺼?`가 ASR에서 `인덕션 꺼.`로 바뀌는 문제가 남아 PM 부정 시험 통과·제품 배포 완료로 보지 않습니다. [60건 결과와 통합 검사 기록](../inbox/이수현/2026-10-24-hi-lg-respeaker60-pi5-validation.md)을 확인합니다. 휴대폰 음원은 이 집계에서 제외했습니다.

### ReSpeaker 상주 입력과 실시간 화면

[문서] 호출어가 확인되고 전체 명령문이 `팬거`·`펜거`·`팬 거`·`펜 거`·`펜꺼`·`펜 꺼`·`페꺼`·`패꺼`·`페 꺼`·`패 꺼`이면 FAN_OFF로 받습니다. 기존 `펜→팬` 정규화와 팬 끄기 문법을 재사용하고, `거→꺼` 보정은 전체 문장이 `팬 거`인 경우에만 추가합니다. 인덕션·다른 단어의 `거`를 일괄 치환하지 않으며 부정·허가/상태 질문·인용·복합 문장 검사는 유지합니다. 끝 물음표는 기존 PM 정책대로 처리합니다. 동일한 실발화 원본 전사의 정정 평가는19/25→21/25, 팬 끄기3/5→5/5이며 실제 Core 수락·추가 재전사와 조건을 구분한 [검증 집계](../inbox/이수현/2026-10-25-fan-off-alias-validation.md)를 따릅니다.

[문서] 하이엘지 경로는 팬 목표1~3의 단위 `단`·`단계`를 같은 뜻으로 받습니다. 예: `하이엘지 후드 2단으로 해줘`, `하이엘지 후드 2단계로 해줘`는 모두 FAN_SET(2)입니다. 숫자·한글 수사(1/일/한, 2/이/두, 3/삼/세)를 기존 전체 문법으로 검사하며 `한 단계로 해줘`는 팬 대상을 명시했을 때 절대 목표1, `한 단계 올려줘`·`3단계로 올려줘`는 기존대로 NONE입니다. 이 표현 확장은 `tests/hi_lg_classifier_test.cpp`로 검사하며 새 ASR 정확도 측정을 뜻하지 않습니다.

[문서] `tools/live_observer.py`는 문장마다 녹음·파일 평가를 시작하는 도구가 아닙니다. 실제 Pi5 관측기를 SSH로 한 번 실행하고, 같은 모델을 유지하며 ReSpeaker 입력·최초 전사·활성어·명령 판정을 Mac의 localhost 화면에 표시합니다. 원음 저장은 기본 꺼짐이며 stdout 로그만 남깁니다. 기존 서비스가 실행 중이거나 다른 관측기가 있으면 중복 시작을 거부합니다. 기기 환경·기동 절차는 [배포 안내](../deploy/README.md#voice-node-수동-기동)를 따르며 OS·기존 유닛·자동 기동은 변경하지 않습니다.

```bash
# 실제 기기별 값은 저장소 밖에서 설정합니다. OUTPUT_DIR는 없는 새 경로여야 합니다.
python3 voice_node/tools/live_observer.py \
  --ssh "$PI_SSH" --observer "$PI_OBSERVER" --expected-sha "$BINARY_SHA256" \
  --model-dir "$PI_SENSEVOICE_DIR" --vad-model "$PI_VAD_MODEL" \
  --runtime-lib "$PI_RUNTIME_LIB" --remote-root "$PI_NEW_RUN_DIR" \
  --output "$OUTPUT_DIR" --seconds 0
```

[문서] 출력된 `LIVE_PAGE` 주소를 열고 **음성노드 수동 시작**을 누릅니다. `상주 · 입력 대기`·모델 로딩1회를 확인한 뒤 **“하이엘지 + 명령”을 한 발화로** 말합니다. 별도 음향 wake-word나 호출 이후의 대기 세션은 없으므로 호출어와 명령을 서로 다른 발화로 나누지 않습니다. 중간에 모델을 다시 로딩하거나 녹음 버튼을 누르지 않습니다.

[문서] 화면의 실행 후보는 실제 장치 집행이 아닙니다. NONE·DROP·발화4초 초과·기한5초 만료·처리3초 초과를 구분하며, 지연은 VAD 종료 추정값입니다. 관측기는 Core에 연결하지 않고 `--publish`도 주지 않으므로 voice 송신은0입니다. **음성노드 종료** 또는 서버의 Ctrl+C는 해당 도구가 만든 PID·실행 파일·작업 경로를 대조해 자기 노드만 종료합니다. 토큰 주소는 로컬 접근용이며 공유하지 않습니다. 유한 원음 검수가 필요할 때만 `--capture-raw --seconds 300`처럼 저장 시간과 옵션을 명시합니다.

## 기존 Whisper 및 TCP 경로

[문서] 아래는 기존 Whisper 파서·송신 동작과 이전 시험 이력입니다. 위 SenseVoice 관측 경로의 결과로 해석하지 않습니다.

### `6ab3041` 이후 Whisper 경로 변경 — PM 검토 대기

[문서/합의 필요] 이번 변경은 SenseVoice 분기에만 한정되지 않는다. 아래 항목은 `--publish`가 가능한 Whisper 경로에도 적용되므로 PM 검토 전 송신 설정으로 사용하지 않는다. 기존 `6ab3041`의 4/25 실측은 에너지 VAD·이전 재전사 조건의 결과이며 아래 구성의 성적으로 대체하지 않는다. 근거: `src/audio_observer.cpp`, `src/audio_pipeline.h`, `tests/command_parser_test.cpp`.

| 항목 | 현재 구현과 채택 조건 |
|---|---|
| `supported_below_threshold` | 지원 명령이 최초 품질 미달이면 같은 모델 ctx512·beam5로 원음 재전사. 같은 intent/value·새 품질·기한을 모두 통과할 때만 채택 |
| `equivalent_repeated_command` | 글자 일치 대신 문장 두 개의 intent/value 일치로 재전사 후보를 생성. ctx512·beam3 결과가 같은 명령이고 품질/기한 통과 시 채택. 반복 원문 자체는 실행하지 않음 |
| `ambiguous_power_text` | `후드 인덕션 강`은 ctx512·beam5로 재전사하되 **IND_OFF만** 채택. IND_ON은 거부하며 최초 NONE을 유지. `f95b934`에서 ON/OFF를 모두 받던 동작은 재범 리뷰 후 제한 |
| `damaged_fan_original` | 명시적 `--fallback-model`이 있을 때만 `팬 고`를 다른 모델 ctx256·beam5로 재전사. FAN_OFF 또는 FAN_SET(1..3), 품질/기한 통과 시 채택. systemd는 옵션을 주지 않아 비활성 |
| 활성어 `호다` | 사용자 요청으로 닫힌 별칭 목록에 추가된 변경. 제품 정책 확정은 PM 검토 대상 |
| 점수 문턱 | 올림 ≥ 일반을 허용하므로 동일값도 실행 옵션상 유효. 동일값 허용은 통계적 교정·PM 승인과 별개이며 기존 Whisper 시험 예시는 일반0.7·올림0.8이며 현재 SenseVoice env에는 점수 문턱 없음 |
| 공통 VAD·deadline | Whisper와 SenseVoice 모두 Silero·32ms 프레임·종료544ms 사용. 발화 종료 추정은 `종료 확정 − 544ms`, 송신 기한은 그 시점 +5초. 원음 종료 정답을 대신하지 않음 |

[문서/합의 필요] SenseVoice의 현재 선택적 송신 기준은 이 문서 첫 절을 따른다. 위 Whisper 옵션·VAD, 하이엘지 역할 유사도, 짧은 `꺼?` 수용안과 SenseVoice conf/품질은 [PM 시험 준비 기록](../inbox/이수현/2026-10-24-voice-pm-acceptance-preparation.md)의 승인 항목을 먼저 확인한다.


오디오 프로그램은 ReSpeaker/PulseAudio → 스트리밍 Silero VAD → 상주 whisper.cpp → ASR 진단 점수 → 명령 파서 → TCP v1 송신으로 구성된다. **기본 모드는 관측 전용**이며, 명시적 수신 주소·품질 문턱과 `--publish`를 함께 지정할 때만 voice를 보낸다. Pi5 WAV와 격리 수신기에서 연결을 검증했으며 실제 ReSpeaker의 인식 품질과 전체 지연 목표는 미충족·미확정이다.

기준: 설계서 §5.3, [main의 TCP 계약 v1 정본](https://github.com/lambdaramji/HOOD-SEEK/blob/main/contracts/README.md).
`#75`는 같은 head로 2026-10-19 21:08:33 KST 병합됐다. 기존 main의 MQTT 계약을 사용하지 않는다. 2026-10-21 PR 준비에서 main `56ca4f1`을 기능 브랜치에 충돌 없이 통합했다. 당시 검증은 [PR 준비 기록](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#16-pr-준비와-main-통합--2026-10-21)을 따른다. 현재 동작·최신 Pi5 실행 범위는 아래 「선택적 송신·발화 종료 기한」과 이 문서의 최신 날짜 기록을 우선한다.

## 입력·출력

`parse_command(text)`는 **ASR 성공·품질 검사를 통과한 전사**를 입력받고, 알려진 오인식을 명령 문맥에서 정규화한 뒤 분류한다. 무음·ASR 실패·품질 검사 탈락은 호출자가 먼저 제외한다. 이 함수는 ASR 품질을 판단하지 않는다.

관측 프로그램은 미보정 ASR 결과의 분류도 확인하기 위해 `quality=uncalibrated` 후보를 출력한다. 이는 품질 통과나 전송 허가를 뜻하지 않는다. 유효한 일반 토큰이 없거나 확률·no_speech 지표가 비정상이면 후보를 만들지 않는다.

| 결과 | 의미 | 후속 TCP 단계에서 할 일 |
|---|---|---|
| `std::nullopt` | 빈 전사, 활성 단어/지원 교정 없음, 잘못된 UTF-8·제어 문자·200자 초과 | 보내지 않음 |
| `Command{NONE, nullopt}` | "후드"는 있지만 지원 명령 하나로 확정 불가 | 발화당 최대 1회 NONE 후보, `text`·`conf` 필요 |
| 지원 `Command` | 정규화한 문장 전체가 allowlist와 일치 | 최종 문장의 활성어·계약·품질 검사 후 intent 전달, 실행 허가·거부는 Core |

`std::optional`은 C++에서 "값이 있음/없음"을 표현한다. 바깥 optional의 값 없음은 전송 제외이고, `Command.value`의 값 없음은 FAN_SET 이외 intent의 value 생략을 뜻한다.

`is_valid_command(command)`는 FAN_SET의 value 1..3 필수, 다른 intent의 value 금지를 독립적으로 검사하며 TCP 직렬화 직전에도 사용한다. `state.p.voice.value`의 null 규칙과 송신 `voice.p.value` 생략 규칙을 혼동하지 않는다.

## 허용 문장

파서는 문장 앞의 `후드` 또는 아래에 명시한 별칭을 해석한다. 단어 사이 ASCII 쉼표와 `마침표+공백`은 구분자로 정규화하고 마지막 `.` 또는 `!` 1개와 ASCII 공백 차이를 허용한다. 물음표·말줄임표·시작/끝/반복 쉼표는 무조건 삭제하지 않는다. 인용·복합 문장을 추출하지 않으며 "후드"가 문장 중간에만 있으면 NONE이다. 지원 명령의 TCP `text`는 정규화한 최종 문장이고 최초 전사는 관측 로그에 보존한다. 최종 문장/별칭의 계약 문구 변경안은 `contract-change`·PM 리뷰 대상이다.

2026-10-21 사용자 진행 요청에 따라 조사·존댓말·팬 숫자 단수·명확한 전원 표현과 직접 요청형을 확장했다. `줘/주세요/줄래/줄래요/세요`로 끝나는 명시적 행동 요청에는 마지막 `?` 1개도 허용한다. `켜?`, `꺼?`, `켜도 돼?`, `켜져 있어?`는 NONE이다. 물음표를 먼저 지워 실행 여부를 정하지 않는다.

| intent/value | 기본 표현 | 허용 확장 |
|---|---|---|
| FAN_SET(1/2/3) | 후드 약풍·약 / 중풍·중 / 강풍·강 | 대상 `팬/풍량/바람/팬 풍량/팬 바람`과 조사 선택. `(으로/로) (좀) 해줘·바꿔줘·설정해줘·맞춰줘`와 `주세요/줄래/줄래요` 변형. 풍량을 명시하면 `켜·켜줘·켜주세요·켜줄래(요)·켜세요`도 허용. 예: 후드 팬을 약풍으로 켜줘 |
| FAN_SET(1/2/3) 숫자 | 후드 팬을 1/2/3단으로 설정해줘 | 대상과 설정/켜기 동사 필수. 1·일·한, 2·이·두, 3·삼·세 + 단. 숫자 단수에 한해 `대상 → 단수 → 동작`과 `단수 → 대상 → 동작` 허용. 예: 후드 3단으로 팬을 켜줘 |
| FAN_OFF | 후드 꺼 / 팬 꺼 / 정지 | 팬 조사·`팬 전원`·동사 앞 `좀` 선택. 꺼줘·꺼주세요·꺼줄래(요)·끄세요·정지해줘/주세요/줄래(요) |
| IND_OFF | 후드 인덕션 꺼 | 인덕션 조사·`전원`·`좀` 선택. 꺼줘·꺼주세요·꺼줄래(요)·끄세요 |
| IND_ON | 후드 인덕션 켜 | 인덕션 조사·`전원`·`좀` 선택. 켜줘·켜주세요·켜줄래(요)·켜세요 |
| IND_UP | 후드 화력 올려 / 불 높여 | `인덕션`·`좀` 선택, 대상/속성 조사. 올려/높여 + 줘·주세요·줄래(요), 올리세요·높이세요 |
| IND_DOWN | 후드 화력 내려 / 불 줄여 | `인덕션`·`좀` 선택, 대상/속성 조사. 내려/줄여 + 줘·주세요·줄래(요), 내리세요·줄이세요 |

띄어쓰기 변형은 [정규식](src/command_parser.cpp)과 [실행 검사](tests/command_parser_test.cpp)가 기준이다. 한국어를 byte 단위 문자 집합(`[강중약]` 등)으로 매칭하지 않는다.

현재 대상 명사는 모두 받침으로 끝나므로 조사는 `을/은`을 지원한다. `후드 팬 일단 꺼줘`의 ‘일단’을 숫자로 바꾸지 않는다. `후드 팬 3단`은 설정 동사가 없어 NONE이고, `후드 3단`은 대상도 없어 NONE이다. `후드 전원 꺼`는 대상을 추정하지 않아 NONE이다. `후드 켜/팬 켜`에 기본 풍량을 정하지 않으며 인덕션 절대 단수도 지원하지 않는다.

문장 일부의 키워드만 추출하지 않으므로 부정문·복합 명령·허가/상태 질문은 명령으로 승격되지 않는다. `응 후드 강풍`, `후드 좀 팬 꺼줘`, `후드 강풍으로 풍량을 바꿔줘`처럼 허용 위치 밖 표현은 현재 NONE이다. 자연어 전반의 이해를 보장하는 파서는 아니다. 이 확장은 텍스트 문법이며 ASR가 실제로 해당 문장을 정확히 인식한다는 보장이 아니다.

상대 팬 명령은 현재 상태·경쟁 조건을 해결하기 전까지 NONE이다. `IND_UP`·`IND_DOWN`은 기존 계약의 인덕션 명령이고, 새로운 팬 상대 intent와 독립 웨이크워드 엔진은 추가하지 않는다. 알려진 오인식의 명령 문맥 정규화는 다음 절을 따른다.

### 알려진 전사 오인식 정규화 — 2026-10-22

사용자의 구현 요청에 따라 `normalize_command_text(text)`를 추가했다. 먼저 **원문**의 UTF-8·제어 문자·200자 경계를 검사하고, 임시 교정문 전체가 단일 지원 명령으로 매칭될 때만 교정본을 반환한다. 교정해도 부정·허가/상태 질문·인용·복합·미지원 문장이면 원래 표현을 유지한다. 공백·내부 쉼표 정규화는 기존 규칙과 같다. 입력을 잘라내거나 누락된 동작을 채우지 않는다.

| 위치 | 알려진 오인식 | 내부 표현 |
|---|---|---|
| 문장 처음 | 호대·호등·후등·푸드·후도·후들·호드·포드·후둘·호둬·푸대·부드·부대·후다·호다 | 후드 |
| 팬 대상 | 펜 | 팬 |
| 풍량 | 약품·약풍프 / 종풍 / 감풍 | 약풍 / 중풍 / 강풍 |
| 인덕션 대상 | 임덕션·인덕장·인덕전 | 인덕션 |
| 문장 끝 요청 어미 (`.`/`!` 1개 선택) | 꺼져·꺼죠 / 켜져·켜죠 | 꺼줘 / 켜줘 |

[문서] 2026-10-22 사용자 요청으로 풍량을 명시한 켜기 표현과 위 요청 어미 교정을 추가했다. `후드 팬 꺼져 → 후드 팬 꺼줘`, `후드 팬 중풍으로 켜죠 → 후드 팬 중풍으로 켜줘`처럼 최종 문장 전체가 지원 명령일 때만 적용한다. 어미 교정에 물음표를 허용하지 않고 `켜져 있어`, `꺼져 있니`, `안 켜져`, 부정·인용·복합은 보존한다. `후드 팬 켜져`는 풍량이 없어 여전히 NONE이다. 단독 `켜져/꺼져`가 상태 설명 의도인지 원음만으로 구분됐다는 뜻은 아니며, 명시한 단일 제어 문맥에서 요청 별칭으로 수용하는 정책이다.

[실측] 이 확장 후 Mac Release CTest4/4, 파서 `checks=7655 failures=0`, 오디오 `audio_checks=1513 failures=0`, TCP `tcp_checks=40 failures=0`, 기존 loopback27시나리오를 통과했다. 직접 시험 녹음24개 재처리에서 최초 전사·conf는 모두 같고, 기존 NONE4개가 FAN_OFF2개·FAN_SET(2)2개로 바뀌었다. 이 중3개는 기존 시험 문턱 통과,1개는 conf0.762215가 FAN_SET 문턱0.8 미달이었다. 실제 송신은0건이며 독립 평가 인식률이 아니다. 이 어미 확장본은 아직 ARM 크로스 빌드·Pi5 재실행을 하지 않았다.

예: `호대 종풍 → 후드 중풍`, `호등 강풍 → 후드 강풍`, `후도 펜을 약품으로 해주세요 → 후드 팬을 약풍으로 해주세요`, `후들 약품 → 후드 약풍`. 새로운 표현을 임의의 유사도 점수로 끼워 맞추지 않고 이 표의 명시한 치환만 사용한다. 팬·풍량 자리는 기존 전체 문법이 결정하므로 `후드 약품 설명해줘`, `후드 펜션 예약해줘`는 명령이 되지 않는다.

`거·커·버` 같은 동작어, `강풍`을 `꺼`로 바꾸는 해석, 반복 문장 삭제는 하지 않는다. `모두·구두·우즈` 등 문장/고유명사 수준으로 달라진 결과도 추측으로 활성어에 넣지 않는다. 이들 때문에 남는 실패는 별도 ASR 개선 대상으로 기록한다.

관측 로그의 `text`는 최초 전사이고 `normalized_text`는 정규화문이다. `normalized_text`는 단어 교정뿐 아니라 공백/쉼표/내부 마침표 차이에도 나타날 수 있다. ASR 진단 conf는 원래 토큰에서 계산하며 점수를 더하지 않는다. 기본 모드는 미송신이며 명시적 문턱과 `--publish`를 지정해야 아래 선택적 송신이 작동한다.

`voice_line`은 원문 UTF-8·제어 문자·200자부터 검사한다. 지원 명령이면 제한된 정규화문으로 intent/text를 일치시키고, NONE이면 원문 text를 보존한다. `푸드 강풍`은 최종 `후드 강풍`과 FAN_SET(3), `후드 펜을 약품으로 해주세요`는 최종 `후드 팬을 약풍으로 해주세요`와 FAN_SET(1)이다. 별칭이 없는 일반 대화, 부정/질문/복합을 교정해 지원 명령으로 만들지 않는다. 실제로 푸드라고 발음했어도 해당 단일 명령 문맥이면 별칭으로 허용하는 정책이다.

최초 정규화 검사는 [§23 기록](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#23-알려진-전사-오인식-정규화-구현--2026-10-22)을 따른다. 이후 내부 마침표·송신 연결까지 반영한 Mac ASan/UBSan CTest는 4/4, 파서 `checks=6754 failures=0`, 오디오 `audio_checks=1513 failures=0`, TCP 직렬화 `tcp_checks=34 failures=0`, loopback 27시나리오를 통과했다. 기존 SDK로 만든 ARM64 실행 파일도 Pi5에서 같은 세 단위 검사를 통과했다. 실제 Pi5 WAV→시험 수신기 결과와 인식/지연의 한계는 아래 최신 기록을 따른다.

## 빌드·검사

파서·오디오 처리는 C++17 표준 기능, TCP는 macOS/Linux의 기본 POSIX socket을 사용한다. CMake는 빌드 절차를 구성하고, CTest는 발화·거부·경계·로컬 TCP 검사를 실행한다. 수신기 검사는 기존 Python 3 표준 라이브러리를 사용하며 새 패키지를 설치하지 않는다. `-DBUILD_TESTING=OFF`이면 Python 없이 라이브러리만 빌드한다.

```bash
cmake -S voice_node -B /tmp/hoodseek-voice-parser-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/hoodseek-voice-parser-build
ctest --test-dir /tmp/hoodseek-voice-parser-build --output-on-failure -V
```

2026-10-19 보완 후 Mac Apple Clang 21.0.0 / CMake 4.3.4: `checks=299 failures=0`. AddressSanitizer·UndefinedBehaviorSanitizer에서도 같은 검사 통과.

2026-10-20 내부 쉼표 보완 후 Mac Release·ASan/UBSan과 ATLAS SDK 크로스 빌드→Pi5 실제 실행에서 `checks=352 failures=0`, `audio_checks=1517 failures=0`을 확인했다. 이는 문법·오디오 처리 검사이며 실제 음성 인식률은 아니다. [원음 보관·컨텍스트 비교·음성 템플릿 검토](../inbox/이수현/2026-10-20-voice-first-performance.md#원음-보관과-동일-입력-비교--2026-10-20-추가)를 참고한다.

2026-10-21 표현 확장 후 Mac Release에서 `checks=1190 failures=0`, `audio_checks=1517 failures=0`, 파서 ASan/UBSan에서 `checks=1190 failures=0`을 확인했다. 숫자는 독립 녹음 수가 아닌 코드 검사 횟수다. 이번 변경은 Pi5 크로스 빌드·실행, ASR 인식률·지연, 원음 재검증과 TCP 전송을 새로 검사하지 않았다. [지원 표현 결정·검증 보고](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#12-표현-확장-텍스트-구현과-검증--2026-10-21)를 참고한다.

같은 날 추가 검증에서 기존 ATLAS SDK Clang 20.1.1로 Release ARM64 크로스 빌드한 `command_parser_test`를 Pi5에서 직접 실행해 `checks=299 failures=0`, 종료 코드 0을 확인했다. Pi5에는 CMake·컴파일러·ldd가 없어 설치하지 않고, 동적 링커 `--verify`·`--list`와 실제 실행으로 기존 공유 런타임 로딩을 확인했다. ASan/UBSan은 Mac 검사 결과이며 Pi5에서는 수행하지 않았다. 경로·해시·원문 로그는 [STEP 3 보고서](../inbox/이수현/2026-10-19-voice-command-parser-step3.md)에 기록했다.

크로스 빌드 방식에서는 CMake·컴파일러가 마스터 빌드 서버에 필요하고, Pi5에는 ARM 실행 파일과 필요한 런타임이 필요하다. 이번 파서 런타임은 기존 라이브러리로 충족했다. 최종 음성 노드의 모델·PulseAudio 접근·오디오 라이브러리 필요 여부는 STEP 4 사전 점검에서 따로 확인한다.

## G2 기동 방침 — PM·수현 합의 (2026-10-19)

G2(10/22)까지는 재부팅 후 **수동 start**로 운용한다. [#46 PM 결정](https://github.com/lambdaramji/HOOD-SEEK/issues/46#issuecomment-6266289483)과 사용자가 전달한 PM·수현 대화가 근거다. `/data` 유닛의 재부팅 자동 기동을 보장하지 않는다. 자동 기동은 PM이 LG·주최측 회신을 보고 10/22까지 결정한 뒤 반영한다.

관측 프로그램의 수동 실행·SIGINT/SIGTERM 정상 종료는 Pi5에서 확인했다. 이후 hello/state 연결과 명시적 옵션의 voice 송신을 추가해 Pi5 WAV·격리 수신기로 검사했다. 서비스 유닛·재부팅 자동 기동과 새 송신 경로의 ReSpeaker 실발화 시험은 미완료다.

1. 재부팅 후 PulseAudio 소스·실행 계정과 Core 상태를 확인하고, 중복 voice 프로세스가 없는지 확인한다.
2. [voice systemd 유닛·환경 예제](../deploy/README.md#voice-node-수동-기동)를 기기에 배포해 수동 start하거나 아래 실행 파일을 직접 시작한다. 이번 점검에서는 기기의 voice 유닛을 등록/기동하지 않았다. 파일 존재만으로 기동 성공으로 보지 않는다.
3. voice PID·오디오 캡처·ASR 준비 로그를 확인한다. TCP 구현 뒤에는 hello→state 응답·접속 로그까지 확인한다. 포트 접속 성공만으로 계약 연결 성공으로 보지 않는다.
4. 수동 stop 또는 SIGINT/SIGTERM으로 정상 종료하고, 캡처·작업 스레드·소켓을 정리해 추가 명령이 전송되지 않는지 확인한다. 다시 start해 중복 프로세스·오래된 명령 재전송이 없는지 확인한다.

운영 절차의 수동 start, 실행 중 프로세스 재시작, 재부팅 자동 기동은 서로 다른 검사다. SafetyCore의 root 실행 결론을 voice에 그대로 적용하지 않고, PulseAudio 서버 접근 계정을 먼저 확인한다. 이번 합의로 rootfs·부팅 등록을 변경하지 않는다.

## conf 정의 — 계산 구현 완료, 품질 임계값 미확정

`voice.p.conf`는 Whisper 일반 텍스트 토큰의 `p`를 평균한 **ASR 진단 점수**로 정의한다. 종료·타임스탬프·언어 등 특수 토큰은 제외한다. 명령 정답 확률이나 Intent Confidence가 아니며, Core의 실행 판단에도 사용하지 않는다. 지원 명령과 NONE에 같은 산출법을 적용한다.

근거 API는 고정 whisper.cpp의 `whisper_full_get_token_p` / `whisper_full_get_token_id` / `whisper_token_eot`다. 유효한 일반 토큰이 없거나 점수가 NaN·무한대·0..1 밖이면 임의 점수를 채우지 않고 품질 탈락으로 처리한다. 토큰 점수 평균이 높은 환각도 가능하므로 이것만으로 품질 통과를 결정하지 않는다. 품질 차단 임계값은 실제 음성·잡음·무음 시험 후 정한다.

텍스트 파서는 conf를 계산하지 않고, 관측 프로그램이 Whisper의 일반 텍스트 토큰만 수집해 `mean_token_score`로 평균한다. [#62 댓글](https://github.com/lambdaramji/HOOD-SEEK/issues/62#issuecomment-6266588521)에 의미·한계를 공유했다. 스키마·필드·intent는 변경하지 않는다. 실제 오인식에도 높은 점수가 나왔으므로 점수만으로 전송을 허가하지 않는다.

[문서] 현재 Mac 직접 시험의 일반 문턱0.7·올림 문턱0.8·최대 no_speech0.6은 개발용으로 명시한 값이며, 독립 교정 자료에서 오명령/누락 비율을 최적화한 수치가 아니다. `conf = 유효한 일반 텍스트 토큰 p의 합 / 토큰 수`로 계산하고, 해당 conf 이상과 `no_speech <= 0.6`을 함께 요구한다. IND_ON·IND_UP·모든 FAN_SET은0.8, FAN_OFF·IND_OFF·IND_DOWN·NONE은0.7이다. 현재 팬 상태를 해석하지 않으므로 FAN_SET(1)도0.8을 적용한다. 정규화로 conf를 올리지 않으며 이번 어미/풍량 문법 수정에서도 이 문턱은 유지한다. 제품 수치는 장치별 별도 교정·평가 음원으로 결정한다.

## 반복 발생 시 원음 재전사 1회 — 2026-10-22

[문서] 아래는 최초 반복 재전사 구현·실측 기록이다. 현재 Whisper의 조건별 재전사와 intent/value 동치 비교는 위 「`6ab3041` 이후 Whisper 경로 변경」 표를 따른다. SenseVoice에는 이 재전사를 적용하지 않는다.

[문서] `repeated_command_candidate(text)`는 원문 경계 검사를 통과하고 동일한 지원 명령이 정확히 두 번 나온 경우만 재전사 대상으로 표시한다. 두 문장을 축약해 실행하지 않는다. 서로 다른 명령·부정·질문·인용·상태 설명·세 번 반복은 대상에서 제외한다. 최초 결과가 그 단일 명령의 품질 문턱도 통과하고 기한이 남아 있으면 같은 PCM·모델로 `audio_ctx=512` 추론을 한 번 더 한다. 기본256·문맥·나머지 설정과 시험 문턱은 유지한다.

[문서] 재전사 전체가 최초 반복 후보와 같은 intent/value이고 새 토큰 점수·no_speech가 품질 조건을 통과할 때만 그 결과를 선택한다. 최초와 재전사의 점수를 합치거나 최댓값으로 바꾸지 않는다. 실패·불일치·품질 탈락이면 최초 NONE/미전송 결과를 유지한다. 두 추론은 한 발화 번호로 처리하고 `publish`는 최종 단계에서 한 번만 호출한다. 큐·지연 재전송은 없다.

[문서] 기한은 처음 정한 발화 종료+5초이며 재전사로 갱신하지 않는다. 재전사에는 같은 기한의 abort callback을 설정하고, TCP 연결 관리도 계속한다. callback 호출 간격 때문에 계산 종료가 기한을 조금 넘을 수 있지만 송신 직전 기한 검사로 만료 메시지는 버린다. 첫 추론은 기존처럼 5초를 넘길 수 있으며 그런 발화는 재전사를 시작하지 않고 송신 단계에서 만료 처리한다.

[문서] `event=asr`는 발화당 최종 결과 하나이며 `initial_text`·선택된 `text`·`retry_status`·`retry_ms`·두 추론 합계 `asr_ms`를 기록한다. `asr_retry`와 `asr_retry_result`에는 각 전사·점수·선택 이유가 남는다. `asr_calls`는 실제 추론 횟수, `utterances`는 처리한 VAD 구간 수, `retry_calls`는 두 번째 추론 횟수다. 최종 TCP text/conf는 선택된 결과만 사용하며 스키마·intent·v를 변경하지 않는다. 최종 문장/별칭의 계약 README 변경안은 기존과 같이 PM 리뷰 대상이다.

[실측] Mac Release CTest4/4, 파서 `checks=7678 failures=0`, 오디오 `audio_checks=1513 failures=0`, TCP `tcp_checks=40 failures=0`, loopback27시나리오를 통과했다. 기존369개+직접 시험28개=397입력의 같은 조건 비교에서 기존 라벨의 지원178개 중 품질 통과가65→67, 비실행191개 실행 후보0이었다. 변경4개 중2개는 직접 시험 입력으로 독립 정답률에 포함하지 않았다. 실제 음성을 두 번 이어 붙인 합성 대조3개는 재전사 후에도 NONE이었다. 잘라 붙인 대조의 경계는 별도 청취하지 않았으므로 독립 실제 반복 발화 검증이 아니다.

[실측] 기존 SDK로 크로스 빌드한 ARM64판은 Pi5에서 같은 단위 검사3개를 통과했다. WAV40개→격리 수신기에서25건(실행 intent15·NONE10)을 수신했고 같은 발화의 중복 송신은0이었다. 수신된 메시지의 종료→송신 추정 중앙2344ms·최대3377ms였다. 만료7·품질 탈락2·text 제외2이며 무발화4개는 ASR을 호출하지 않았다. 이 시간은 WAV 처리 시계와 VAD 끝 무음800ms를 포함한 추정이며 ReSpeaker 실시간 응답이 아니다.

[실측] 최근 강풍 반복 음원의 Pi5 첫 추론은5444ms로 재시도를 시작하지 못했다. 합성 반복 대조1개는 재전사를2145ms 수행하다 같은5초 기한으로 중단(rc=-9), 최종5044ms 시점에 만료로 미송신했다. 따라서 Pi5에서 기한 차단·연결 유지·다음 WAV 처리는 확인했지만 반복 복구 성능을 완료로 판정하지 않는다. 실제 Core·팬·쿡탑에는 연결하지 않았다. 상세 조건과 후속은 [§31 기록](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#31-조건부-재전사구현mac-시험pi5-wav-검증--2026-10-22)을 따른다.

## STEP 4 빌드·실행

새 패키지를 설치하지 않고 #50의 고정 whisper.cpp `d1be6fde11ac6e0407606b4e42fe72d34add8037`를 외부 소스 경로로 재사용한다. WAV 디코딩도 whisper의 기존 `common` 라이브러리를 사용한다. 소스·모델을 자동 다운로드하지 않는다.

```bash
cmake -S voice_node -B /tmp/hoodseek-voice-observer-build \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
  -DWHISPER_SOURCE_DIR=/path/to/existing/whisper.cpp \
  -DGGML_METAL=OFF -DGGML_ACCELERATE=OFF -DGGML_OPENMP=OFF
cmake --build /tmp/hoodseek-voice-observer-build \
  --target command_parser_test audio_pipeline_test voice_tcp_test_client hoodseek-voice-observer
ctest --test-dir /tmp/hoodseek-voice-observer-build --output-on-failure -V
```

Pi5 크로스 빌드에는 ATLAS SDK toolchain과 `-DHOOD_SEEK_WITH_PULSE=ON`을 추가한다. 기존 `gstreamer-app-1.0` 개발 파일과 런타임을 사용한다. Mac에서는 이 옵션 없이 WAV 모드를 검사했다. 실제 실행한 명령·환경은 [STEP 4 보고서](../inbox/이수현/2026-10-19-voice-audio-pipeline-step4.md)에 있다.

```bash
hoodseek-voice-observer --model /path/to/ggml-base.bin --vad-model "$VAD_MODEL" \
  --wav /path/to/command-with-trailing-silence.wav \
  --wav /path/to/another-command.wav

hoodseek-voice-observer --model /path/to/ggml-base.bin --vad-model "$VAD_MODEL" \
  --pulse-source alsa_input.usb-Seeed_Studio_ReSpeaker_Lite_0000000001-00.analog-stereo \
  --seconds 30
```

`--vad-model`은 필수이며 기존 Silero 모델 파일을 지정한다. 제거된 `--vad-dbfs` 옵션은 사용하지 않는다. 입력 장치를 명시해 LifeCam·monitor 소스의 자동 선택을 피한다. 16 kHz·2채널 장치 입력은 GStreamer가 16 kHz·모노 PCM16으로 변환한다.

현재 VAD는 32 ms 프레임, 시작224ms, 종료544ms, 발화 구간4초·총 버퍼6초다. 시작 확인 전992ms를 보관한다(주변 대화 유입 가능). 4 s 초과 발화는 분할하지 않고 폐기 후 무음을 기다린다. EOF의 미완성 발화도 폐기한다. WAV 모드가 자동으로 뒤 무음을 붙이지는 않는다.

모델은 한 번 로딩하고 CPU 4 threads·audio_ctx 256·beam 5·고정 단어 문맥·no_context로 재사용한다. GStreamer queue는 2 buffers로 제한하고, 추론 중 캡처를 쌓아 나중에 실행하지 않는다. 추론 뒤544ms 무음부터 다시 받으므로 바쁜 동안 말한 명령을 놓칠 수 있다.

Mac·Pi5에서 파서 `checks=299 failures=0`, 오디오 처리 `audio_checks=1517 failures=0`. Mac sanitizer도 통과했다. Pi5 WAV는 한 모델 로딩으로 전사 3회를 수행했고, 무음·4 s 초과·불완전 EOF에서 ASR을 호출하지 않았다. 실제 발화에서는 주변 대화와 ASR 오인식·문장 중복·긴 추론이 관측됐다. R5 인식률·약 2 s 응답·고정 품질 임계값을 통과한 것으로 보지 않는다.

## STEP 5 — TCP 송신 모듈·로컬 수신기 검증

`src/voice_tcp.{h,cpp}`는 기존 파서와 `Command` 검사를 재사용한다. 모델·VAD를 링크하지 않으며 TCP v1의 필드·intent는 변경하지 않는다. 최종 문장/별칭의 의미는 계약 README 변경안으로 함께 검토한다. 별도 파일은 송신 구현을 오디오 추론과 독립적으로 검사하기 위해 필요하다. 수신기 테스트 두 파일 외 새 의존성·설정 파일·전송 큐·추상 인터페이스는 없다.

- `voice_line(text, conf, seq, t_ms, activation)`: 원문 UTF-8·200자와 유한한0..1 conf부터 검사한다. 기본 `Activation::Hood`는 기존 전체 문장 파서와 후드 활성어를 사용하며 지원 명령은 정규화문, NONE은 원문 text다. `Activation::HiLg`는 하이엘지 분류·최종 문장과 conf=0을 사용한다. 두 모드를 자동 혼용하지 않는다. FAN_SET에만 value1..3을 넣고 LF 포함4096byte를 넘으면 생성하지 않는다.
- `VoiceTcpClient(ipv4, port, activation)`: 숫자 IPv4·포트와 활성어 모드(기본 Hood)를 명시한다. `service()`를 단일 스레드에서 주기적으로 호출하면 비동기 접속·실패 시 1초 backoff·hello 선행·재접속을 처리한다. 연결 완료 확인이 늦어져도 POLLOUT·SO_ERROR 성공을 우선하고 아직 연결 중일 때만 1초 기한을 적용한다. hello seq=0, voice seq=1부터 연결별 증가다. 쓰기 대기는 최대 250 ms이며 명령을 큐에 보관하거나 실패 후 재전송하지 않는다.
- `publish(id, text, conf, quality_passed, deadline)`: 프로세스 내 단조 증가하는 발화 번호·ASR별 입력 통과 여부·**발화 종료 기준으로 정한 deadline**을 호출자가 제공한다. 품질/입력 탈락·만료·선택한 모드의 활성어 없음·끊김은 보내지 않는다. 생략·실패한 번호도 소비하므로 같은 발화를 나중에 재전송하지 않는다. 새 발화의 같은 문장은 허용한다. SenseVoice의 호출 통과 여부를 보정된 ASR 품질 점수로 해석하지 않는다.
- `Sent`는 socket 쓰기 완료이고 Core 수락·장치 집행 확인이 아니다. 유효 전사의 부정·모호·복합·미지원 문장은 기존대로 NONE이다. 내부 수용 정책의 HOLD/REJECT를 임의로 NONE으로 매핑하지 않는다.
- Linux는 vision-node와 같은 `TCP_USER_TIMEOUT=2000 ms`를 필수 설정해 확인되지 않은 TCP 데이터의 장기 재전송을 제한한다. 설정 실패 시 연결을 버린다. 모든 플랫폼에서 완성된 수신 줄이 3초간 없으면 연결을 닫고 1초 뒤 다시 접속한다. 미완성 줄은 감시 시간을 연장하지 않는다. `publish()`도 이 검사를 수행해 긴 추론 뒤 오래된 연결에 쓰지 않는다.
- `wait_with_service(task)`는 `std::launch::async`로 시작한 `std::future<int>` 작업을 기다리며 기존 `service()`를 계속 호출한다. 10 ms 단위로 완료를 확인하며, TCP는 호출 스레드에서만 다룬다. 비어 있거나 지연 실행(deferred)인 작업은 거부하고 추론 예외는 호출자에게 전달한다. 오래된 수신 버퍼를 읽기 전 만료 검사와 기존 2/3초 제한은 유지한다. 10 ms는 운영체제 스케줄링까지 보장하는 실시간 상한이 아니다.
- conf는 전송 시 소수점 뒤 4자리가 아닌 **유효숫자 4자리**로 표현한다. 0.72는 0.72로, 1e-9 같은 작은 점수는 0이 아닌 지수 표기로 보존한다. ASR 원점수 계산이나 품질 통과 기준을 바꾸지 않는다.
- 서버 수신은 매 호출 최대 16 KiB, 한 줄 LF 포함 4096 byte까지만 프레이밍을 확인하고 본문을 보관하지 않는다. state JSON의 의미 검증·안전 판단·결과 피드백은 수행하지 않는다. Flutter가 Core의 `state.voice`를 표시하는 기존 역할을 유지한다. `received_lines()`는 유효 state 개수가 아닌 프레이밍된 줄 수다.

`tests/voice_tcp_test_client.cpp`는 CTest용 송신 실행 파일이며 주소는 127.0.0.1로 고정한다. `tests/voice_tcp_receiver_test.py`가 임시 포트를 열고 실제 C++ 송신 JSON의 envelope·seq·payload를 기대값과 대조한다. 모델 conf가 아닌 명시적 시험값을 사용하며 실제 마이크·Core·장치 제어는 없다. 수신기 테스트를 실행하려면 로컬 loopback socket 권한이 필요하다.

기본 모드는 `quality=uncalibrated`, `transmitted=0`이다. 아래 명시적 송신 옵션을 사용한 격리 수신기 검증을 실제 Core/장치 집행 완료로 보고하지 않는다. #62 부분 PR은 `Refs #62`로 연결하고 이슈를 닫지 않는다.

### 긴 추론 중 연결 관측 — 2026-10-22

`--core-host IPV4 --core-port PORT`를 함께 지정하면 관측 프로그램이 hello를 보내고 수신 줄을 비운다. 지정하지 않으면 기존 오디오 관측 동작을 사용한다. 주소·포트는 실행할 때 제공하며 자동 접속 대상은 없다. 이 옵션은 **voice 발행을 활성화하지 않는다.** `transmitted=0`은 voice 메시지 수이며, hello를 포함한 전체 TCP 패킷 수가 아니다.

```bash
hoodseek-voice-observer --model /path/to/ggml-base.bin --vad-model "$VAD_MODEL" \
  --wav /path/to/command-with-trailing-silence.wav \
  --core-host "$CORE_HOST" --core-port "$CORE_PORT"
```

연결 관측 시에는 한 발화의 `whisper_full`만 별도 작업 스레드에서 실행하고, 호출 스레드는 `wait_with_service`로 TCP를 처리한다. 동시에 추론하는 발화는 하나이며, `get()`이 끝날 때까지 context·samples를 보존한다. 모델 상주·CPU 4 threads·audio_ctx 256·문맥·VAD·파서·품질 기준은 바꾸지 않았다. 추론 중 입력을 버리는 기존 마이크 방침도 같다. 종료 플래그는 신호 처리 함수와 추론 스레드가 안전하게 공유하도록 lock-free atomic을 사용한다.

`event=asr`의 `asr_ms`는 작업 스레드 안의 Whisper 호출 시간, `wait_ms`는 작업 시작·TCP 처리·완료 대기를 포함한 호출자 경과 시간이다. 모델 로딩·발화 종료 대기·voice 송신 시간은 둘 다 포함하지 않는다. `link_connected`·`link_lines`는 연결 여부·프레이밍한 줄 수이며, 유효 state 수나 Core 수락 여부가 아니다.

Mac Release·ASan/UBSan CTest 각각 4/4, Mac·Linux loopback 각각 27개 시나리오를 통과했다. 4.2초 모의 추론 중 정상 연결 유지, 무수신 중 차단, 작업 예외 전달, 추론 중 재접속을 검사했다. 이는 실제 Pi5 Whisper 지연 측정이 아니다. 기존 WAV의 실제 Mac Whisper 실행에서는 연결 관측 전후 전사·conf·intent/value가 같았고 hello 이외 송신이 없었다. 상세 범위와 ARM 빌드는 [후속 2번 검증 기록](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#18-후속-2번--긴-추론-중-tcp-처리--2026-10-22)을 따른다.

## 남은 연결

실제 ReSpeaker·대화 소음에서의 VAD/ASR 품질 보정, 최종 문장/별칭의 계약 문구 PM 리뷰와 현장 전체 경로 검증이 남아 있다. 호스트 시험용 Core·Flutter 수신/음성 카드와 Pi5 WAV 송신 연결은 [§30 오프라인 통합 기록](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#30-전체-개발-음원-회귀호스트-coreflutterpi5-병목-분석--2026-10-22)에서 확인했다. 실제 센서·UART 집행은 끄고 검사했다. 현재 코드의 송신 전5초 기한과 Core의 수신 후5초 유효 기간은 별개다. WAV/운영자 문턱 시험을 ReSpeaker 성능 합격으로 보고하지 않는다.

## 선택적 송신·발화 종료 기한 — 2026-10-22

[문서] 현재 ASR는 `base`·CPU4·audio_ctx256·beam5와 기존 어휘 뒤 `꺼, 켜.`를 사용하고 timestamp 토큰을 켠다. 무조건 반복 삭제나 앞0.5초 무음 추가는 하지 않는다. 현재 공통 VAD는 위 SenseVoice 절의 Silero224/544ms·발화4초·총6초 구성을 사용한다. 이전 Energy VAD의 개발 기록은 현재 음성/비음성 판정 기준으로 쓰지 않는다.

[문서] `--core-host`와 `--core-port`만 지정하면 기존처럼 hello/state만 처리한다. `--publish`는 두 점수 문턱 `--min-score`·`--min-score-raise`와 수신 주소가 모두 있어야 한다. 값은 유한한0..1, 올림 문턱은 일반 문턱 이상이어야 한다(동일값 허용). 누락·역전·비정상 값은 모델을 로딩하기 전에 거부한다. score는 일반 텍스트 토큰의 평균 진단값이며 명령 정답 확률이 아니다. 동일값 허용과 새 재전사/VAD의 제품 적용은 PM 검토 대기다. 문턱을 지정한 사실을 통계적으로 보정 완료됐다는 뜻으로 해석하지 않는다.

[문서] FAN_SET은 현재 상태를 해석하지 않으므로 전부 올림 문턱을 적용하며 IND_ON·IND_UP도 같다. 나머지는 일반 문턱을 적용한다. 유효한 ASR 지표·전체 문장 문법과 해당 문턱, `no_speech <= 0.6`을 함께 검사하고 같은 conf를 그대로 전송한다. 품질/만료/끊김은 재전송하지 않는다. 실행 허가는 Core에 있다.

[문서] deadline은 VAD가 끝을 확정한 프레임의 시각에서544ms를 뺀 추정 발화 종료+5초다. 마이크 모드는 GStreamer PTS와 파이프라인 시계로 버퍼 나이를 빼며 송신 모드에서 시각을 확인할 수 없으면 중단한다. 오디오 불연속은 이어 붙이지 않고 무음까지 버린다. WAV 모드는 처리 시계로 계산하므로 로그 `end_to_send_ms`를 실제 실발화 지연으로 부르지 않는다. 송신 결과가 sent인 값만 송신 측 경과 시간으로 집계한다.

```bash
# SCORE_MIN / SCORE_RAISE는 해당 입력 환경의 시험으로 정한 운영자 값이다.
hoodseek-voice-observer --model "$MODEL" --vad-model "$VAD_MODEL" \
  --wav "$TEST_WAV" --core-host "$TEST_RECEIVER_HOST" --core-port "$TEST_RECEIVER_PORT" \
  --min-score "$SCORE_MIN" --min-score-raise "$SCORE_RAISE" --publish
```

[문서] 실측 범위·시험용 수치·만료/품질 탈락·Pi5 실행은 [오늘의 연결 검증 기록](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#29-치환후처리선택적-송신과-pi5-wav-검증--2026-10-22)을 따른다. ReSpeaker 실발화 시험은2026-10-23 18:00 KST에 별도로 진행하며 자동 시작/녹음은 예약하지 않았다.

ASR가 부정어를 누락하거나 무음에서 완전한 허용 문장을 환각하면 텍스트 파서만으로 원래 음성을 복원할 수 없다. 실제 음성·잡음·무음·부정문을 별도로 평가해야 한다.

## 개발용 원음 후보 대조 도구 — 2026-10-22

`tools/candidate_probe.cpp`는 기존 전체 prefix 재계산과 중복 prefix 결과 재사용을 비교하는 **오프라인 진단 도구**다. 같은 음원에서 같은 앞부분을 공유하는 후보는 한 번만 계산하고, 다음 음원에서는 캐시를 새로 만든다. 모든 후보·종료 토큰 EOT를 유지하며 원래와 다른 점수가 나왔던 KV 증분 경로는 사용하지 않는다. 점수 가산·전사 치환·품질 판정·TCP 발행은 하지 않는다. 모델·추가 패키지를 다운로드하지 않는다.

이 소스가 필요한 이유는 76후보 대조의 중복 계산 비용을 줄이면서 원래 점수가 보존되는지 재현하기 위해서다. `WHISPER_SOURCE_DIR`가 있을 때만 타깃이 정의되며 `EXCLUDE_FROM_ALL`로 기본 빌드에서는 제외한다. 기존 오디오 관측 프로그램도 이 도구를 호출하지 않는다.

```bash
cmake -S voice_node -B /tmp/hoodseek-candidate-probe \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
  -DWHISPER_SOURCE_DIR="$WHISPER_SOURCE_DIR" \
  -DGGML_CPU_ARM_ARCH=armv8-a+crc \
  -DGGML_METAL=OFF -DGGML_ACCELERATE=OFF -DGGML_BLAS=OFF -DGGML_OPENMP=OFF
cmake --build /tmp/hoodseek-candidate-probe --target voice_candidate_probe --parallel 2
/tmp/hoodseek-candidate-probe/voice_candidate_probe \
  "$MODEL" "$CANDIDATES_TSV" "$INPUTS_TSV" compare > scores.jsonl
```

후보 TSV는 `ID<TAB>점수화할 문장`, 입력 TSV는 `ID<TAB>PCM 파일 경로`다. 기존 실험은 후보 문장 앞 ASCII 공백 1개·끝 문장부호 제외를 고정했다. ID는 영문·숫자·밑줄 1..64자, 중복 금지이며 TSV는 1 MiB·512행·행당 4096 byte 이하다. 후보 텍스트 토큰은 1..200개이며 입력은 16 kHz mono PCM16LE 원시 파일이다. WAV 헤더가 있는 파일을 PCM으로 넣지 않는다. 파일 크기를 먼저 확인해 빈 파일·홀수 byte·30초 초과 PCM을 거부한다. 이는 오프라인 도구의 입력 상한이며 제품 VAD의 4초 상한은 그대로다.

`reference`는 원래 계산, `cached`는 중복 prefix 재사용, `compare`는 두 경로와 후보 순서 반전을 대조한다. `compare`는 토큰 logp 최대 차이 `1e-6` 초과 시 실패한다. 이 값은 수치 동등성 오차이며 명령 품질 문턱이 아니다. 점수·시간·조건은 JSONL로 출력한다. 같은 intent의 다른 문장이 1위라는 사실을 실제 전사 복원이나 wire text/conf로 사용하지 않는다.

기존 중립 ARM 플래그의 Mac 라이브러리에서 16음원×76후보의 점수 차이는 0이었고 3회 반복 점수도 같았다. 대조 연산 중앙값은 7271.544062499999 ms → 3997.1802079999998 ms, 약 45% 단축이다. 가산 0의 지원 후보 일치는 그대로 0/8이므로 전사 정확도 개선으로 해석하지 않는다. 별도 83녹음·6전사 설정 비교에서도 검수된 지원 후보의 향상은 확인하지 못해 **제품 기본값은 유지했다.**

아키텍처 플래그가 다른 두 Mac 라이브러리 사이에는 최대 약 0.03의 토큰 logp 차이가 있었다. 각 빌드 안에서는 원래/최적화 계산이 같았으나 서로 다른 빌드의 점수·문턱은 섞지 않는다. 위 시간은 CLI 프로세스 또는 오프라인 도구의 Mac 측정이며 Pi5 응답 시간이 아니다. 상세 자료·빌드별 대조·별도 교정 필요 사항은 [3번 개발 검증 기록](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#19-후속-3번--개발용-연산-최적화와-전사-설정-비교--2026-10-22)을 따른다.

## 병합 후 빌드·배포·수동 실행 — 2026-10-22

[실측] 마스터 SDK 환경을 적용하면 CMake3.31.6·ARM64 Clang·GStreamer 개발 파일을 사용할 수 있다. Pi5에는 실행 라이브러리·base 모델·ReSpeaker가 있지만 Git·CMake·g++는 없다. **마스터에서 빌드하고 Pi5로 복사한다.** GitHub 병합은 Pi5 배포나 프로세스 재시작을 수행하지 않는다. 최신 준비 점검과 근거는 [§32](../inbox/이수현/2026-10-20-voice-acceptance-policy-design.md#32-병합-후-실행-환경과-재현-빌드-점검--2026-10-22)를 따른다.

[실측] 마스터의 대문자 `HOOD-SEEK`가 실제 Git 저장소이며 소문자 `hoodseek`는 Git 저장소가 아니었다. 기존 `atlas-dev`는 후자를 `/app`으로 마운트한다. 그 컨테이너 안에서 단순히 `git pull`·빌드하지 않는다. 아래 절차는 Git의 병합 커밋을 별도 소스 폴더에 추출하고, 같은 SDK 이미지로 일회성 빌드한다. 기존 컨테이너·SDK·정상 산출물은 보존한다.

### 1. 마스터에서 병합 커밋 빌드

[문서] `REPO`는 실제 Git 저장소, `WHISPER_SOURCE`는 #50 고정 커밋의 기존 whisper.cpp 경로다. `git fetch`는 원격 정보를 갱신하고 `git archive`는 해당 커밋만 추출하므로 현재 작업 브랜치/미커밋 파일을 덮어쓰지 않는다. 병합 전에는 이 절차가 오늘의 미커밋 코드를 포함하지 않는다.

```bash
REPO=/path/to/HOOD-SEEK
WHISPER_SOURCE=/path/to/existing/whisper.cpp
SDK_IMAGE=sha256:0a6fa7930de27d0ab57acb2748ce348b95da2e92f2bf89f00a5d91d0786b3eba
git -C "$WHISPER_SOURCE" rev-parse HEAD
# d1be6fde11ac6e0407606b4e42fe72d34add8037인지 확인한다.
git -C "$REPO" fetch origin main
VOICE_REV=$(git -C "$REPO" rev-parse origin/main)
VOICE_JOB=$(mktemp -d "$HOME/hoodseek-work/issue-62/postmerge-XXXXXXXX")
mkdir "$VOICE_JOB/source"
git -C "$REPO" archive "$VOICE_REV" voice_node | tar -x -C "$VOICE_JOB/source"
printf '%s\n' "$VOICE_REV" > "$VOICE_JOB/source-commit.txt"

docker run --rm -i --read-only --network none --cpus 2 --memory 2g --tmpfs /tmp \
  -v "$VOICE_JOB:/work" -v "$VOICE_JOB/source:/source:ro" \
  -v "$WHISPER_SOURCE:/whisper:ro" "$SDK_IMAGE" /bin/bash -s <<'BUILD'
set -e
. /opt/atlas-sdk-x86_64/environment-setup-armv8a-atlas-linux
# SDK 파일 대신 이번 Clang 호출의 GCC 전용 옵션만 제외한다.
export LDFLAGS="${LDFLAGS//-fuse-linker-plugin/}"
cmake -S /source/voice_node -B /work/build-arm \
  -DCMAKE_TOOLCHAIN_FILE=/opt/atlas-sdk-x86_64/sysroots/x86_64-oesdk-linux/usr/share/cmake/OEToolchainConfig.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=OFF \
  -DWHISPER_SOURCE_DIR=/whisper -DHOOD_SEEK_WITH_PULSE=ON \
  -DGGML_CPU_ARM_ARCH=armv8-a+crc -DGGML_OPENMP=OFF -DGGML_BLAS=OFF \
  -DGGML_CCACHE=OFF -DWHISPER_BUILD_IS_DEV=OFF
cmake --build /work/build-arm \
  --target command_parser_test audio_pipeline_test voice_tcp_test_client hoodseek-voice-observer \
  --parallel 2
BUILD
```

[문서] 각 단계가 성공한 뒤 다음 단계로 진행한다. ARM 실행 파일은 x86 마스터에서 CTest로 실행하지 않고 Pi5에서 검사한다. whisper 공통 소스의 기존 `stb_vorbis.c` 경고1건은 이번 전체 빌드에서도 발생했으므로 경고0으로 보고하지 않는다.

### 2. 마스터에서 Pi5 새 폴더로 복사

```bash
PI_SSH='root@<Pi5주소>'
VOICE_DEPLOY="/data/hoodseek-voice-$(printf '%.12s' "$VOICE_REV")"
ssh "$PI_SSH" "mkdir '$VOICE_DEPLOY'"
# 폴더가 이미 있으면 중단하고 기존 내용/커밋을 확인한다. 덮어쓰지 않는다.
scp "$VOICE_JOB/build-arm/command_parser_test" "$VOICE_JOB/build-arm/audio_pipeline_test" \
  "$VOICE_JOB/build-arm/voice_tcp_test_client" "$VOICE_JOB/build-arm/hoodseek-voice-observer" \
  "$PI_SSH:$VOICE_DEPLOY/"
sha256sum "$VOICE_JOB/build-arm/hoodseek-voice-observer"
ssh "$PI_SSH" "sha256sum '$VOICE_DEPLOY/hoodseek-voice-observer'"
```

[문서] 전송 전후 해시가 같은지 확인한다. 기존 모델은 재사용하고 실행 파일만 새 폴더에 둔다. 병합 결과의 소스가 오늘 검증한 소스와 다르면 이전 실행 파일로 최신 변경 검증을 대신하지 않는다.

### 3. Pi5에서 검사 후 관측 전용으로 수동 실행

```bash
cd '/data/hoodseek-voice-<배포한-커밋12자리>'
./command_parser_test
./audio_pipeline_test
./voice_tcp_test_client --self-test
/lib/ld-linux-aarch64.so.1 --list ./hoodseek-voice-observer
pactl list short sources

MODEL=/data/hoodseek-issue50-20261019-qHyd66/ggml-base.bin
MIC=alsa_input.usb-Seeed_Studio_ReSpeaker_Lite_0000000001-00.analog-stereo
VAD_MODEL=/path/to/ggml-silero-v6.2.0.bin
# VAD 모델은 별도 준비한다. Whisper 0.7/0.8은 개발용 시험값이다.
./hoodseek-voice-observer --model "$MODEL" --vad-model "$VAD_MODEL" \
  --pulse-source "$MIC" --seconds 60 --min-score 0.7 --min-score-raise 0.8
```

[문서] `model_loaded` → `capture_ready`를 확인한 후 말한다. `audio_level`은 입력 레벨, `asr`는 최초/최종 전사·품질·명령 후보다. 60초 후 끝나거나 `Ctrl+C`로 종료하며 `event=stopped`를 확인한다. 위 명령은 Core에 연결하지 않고 `transmitted=0`이다. 현재 프로그램은 마이크 원음 WAV를 자동 저장하지 않으므로 평가 원음은 기존 별도 녹음 절차로 보관한다.

[문서] Core 연결만 확인하려면 위 실행 옵션에 `--core-host "$CORE_HOST" --core-port "$CORE_PORT"`를 추가하되 `--publish`는 붙이지 않는다. Core의 주기적 수신 줄·연결 상태를 확인할 수 있고 voice는 전송하지 않는다. 실제 발행 시험은 수신 대상과 시험 조건을 확인한 뒤 `--publish`를 추가한다. 이때 `sent`는 소켓 쓰기 완료이며 Core의 `state.voice.accepted/reason`과 실제 집행 여부는 따로 확인한다.

[실측] 이번 점검에서는 PulseAudio17.0의 ReSpeaker16kHz·2채널 소스와 필요한 plugin을 확인했다. 소스의 `SUSPENDED`는 읽는 클라이언트가 없는 상태였으며 접근 실패가 아니었다. root의 Pulse cookie/설정 경고는 있었지만 `pactl info`와 소스 조회는 성공했다. 이 경고를 없애려고 전역 권한·서버 설정을 변경하지 않았다. Pi5 시스템 날짜도 작업 PC와 달랐다. 현재 `t_ms`·송신 기한은 단조 시계를 쓰지만 운영 기록의 날짜 대조에는 주의가 필요하며, 시각 설정은 변경하지 않았다.

[문서] 최신 바이너리의 ReSpeaker 실발화·거리/소음 보정·실제 장치 집행과 재부팅 후 수동 실행은 현장 확인으로 남는다. voice systemd 유닛 설치나 자동 기동은 이번 준비 점검에 포함하지 않는다. 이후 저장소 운영 방식에 맞춘 [유닛·기기별 env 예제](../deploy/README.md#voice-node-수동-기동)를 추가했다. 기본 env는 관측 전용이며 수동 start/stop과 발행 전환 절차는 해당 안내를 따른다.

[문서] 공식 완료 지표는 BRD `NFR-08`의 **필수5종×5회=25회 중80% 이상, 발화 종료→intent3초 이내, 지정 화자 현장 측정**이다. 앞선 약2초는 개발 중 지향한 목표이며 공식3초 지표와 구분한다. 현재 코드는 기술 통합을 검토할 수 있지만 이 현장 지표와 최종 문장/별칭 정책 승인까지 완료한 상태는 아니다. #62는 `Refs #62`로 연결하고 완료 근거가 생긴 뒤 종료한다.



## 인덕션 단수·불 세기 지정 (#182, PM 계약 승인·현장 확인 대기)

SenseVoice/하이엘지 경로에만 다음 표현을 추가한다. 호출어와 명령을 이어 말한다.

| 표현 | 후보 |
|---|---|
| 하이엘지 인덕션 1단으로 해줘 / 한 단계로 설정해줘 / 약불로 해줘 | IND_SET(1) |
| 하이엘지 인덕션 2단으로 해줘 / 두 단계로 설정해줘 / 중불로 해줘 | IND_SET(2) |
| 하이엘지 인덕션 3단으로 해줘 / 세 단계로 설정해줘 / 강불로 해줘 | IND_SET(3) |

인덕션 대상은 생략하지 않는다. 1·일·한 / 2·이·두 / 3·삼·세와 단·단계를 받되 숫자에는 설정 동사를 요구한다. 약불·중불·강불은 목표 단수 이름이다. 온도 측정에 따른 hazard low·mid·high를 지정하는 말이 아니다. 목표+올려/내려, 0·4·11·음수·소수, 부정·허가/상태 질문·인용·복합은 기존 비실행 경계로 남긴다.

새 후보만 voice v2·IND_SET·value 1..3으로 보내고 기존 7종/hello는 v1이다. Core 수신과 state v4·Flutter 처리를 같은 PR에 준비했다. **이 문단은 운영 적용 완료가 아니다.** PM 계약 승인은 #186에 있으며, 요청 리뷰어 승인과 Flutter→Core→voice 순서 배포·새 현장 부정 시험이 필요하다. 기존 지정 화자 필수5종 성적을 이 새 구성의 성적으로 재사용하지 않는다.
