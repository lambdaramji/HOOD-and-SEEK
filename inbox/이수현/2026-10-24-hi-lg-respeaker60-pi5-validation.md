# 하이엘지 ReSpeaker 60건 Pi5 관측 검증

- 작성자: 이수현, Codex 작성 지원
- 작성일: 2026-10-24 KST
- 상태: draft
- 진행 설명: 관측 코드 검토용, 실제 voice 발행·병합 조건 미충족
- 카테고리: #62 음성 입력, #107 SenseVoice 관측 경로
- 대상: 수현·현지 ReSpeaker 원본 각30개, 합계60개

## 결론과 시험 범위

[실측] 수현·현지의 ReSpeaker60건에서 지원40건 중 **VAD37건**, 전체 WAV36건이 명령과 일치했다. 비명령20건에서는 두 경로 모두 **2건의 실행 후보**가 나왔다. 같은 설정으로 반복한3회는 전사·판정·구간 경계가 같았다. 전체 녹음 반복이므로 독립 표본180건으로 해석하지 않는다. 근거: [원문·해시·3회 결과](measurements/2026-10-24-hi-lg-respeaker60-pi5.json)의 `items`, `runs`, `trials`.

[문서] 포함 기준은 입력 장치이며 두 화자의30개 전부를 포함했다. 다른 입력 장치의 음원은 이 성능 집계와 첨부 결과에서 제외했다. 기존 개발 음원을 다시 사용한 회귀 시험으로, 화자 독립 인식률이나 공식 NFR-08 성적이 아니다. 두 화자의 거리·발음·주변 대화 조건도 완전히 같지 않아 차이를 목소리만의 원인으로 단정하지 않는다. 근거: 같은 JSON의 `scope`, `limitations`, `included_speakers`.

| 포함 화자 | 지원 문장 | 전체 WAV 일치 | VAD 일치 | 비명령 실행 후보 |
|---|---:|---:|---:|---:|
| 수현 |20|20/20|20/20|1/10|
| 현지 |20|16/20|17/20|1/10|
| 합계 |40|36/40|37/40|2/20|

[실측] 위 표는 Pi5 benchmark의3회 모두 같은 결과다. 선택한60개에는 지원40·비명령20이 있으므로 전체60개를 모두 실행해야 하는 시험이 아니다. `NONE`과 `DROP`은 모두 비실행 후보이며 실제 송신은0건이다. 근거: JSON의 `runs[*].metrics`, `execution_intent_published`.

## 관측 코드 구성

[문서] `PulseAudio/GStreamer → Silero 스트림 VAD → 상주 SenseVoiceSmall INT8 → 하이엘지·대상·동작 분류 → 관측 로그`로 연결했다. SenseVoice는 ko·ITN·greedy·CPU4, 모델1회 로딩이다. VAD는32ms 프레임·시작224ms·종료544ms·앞부분992ms·발화 구간4초·총 버퍼6초이며 EOF로 미완료 발화를 확정하거나 가상 무음을 붙이지 않는다. 원음·모델·SDK·시스템 라이브러리는 바꾸지 않았다. 근거: [`audio_observer.cpp`](../../voice_node/src/audio_observer.cpp), [`audio_pipeline.h`](../../voice_node/src/audio_pipeline.h), JSON의 `VAD`, `models`.

[문서/제안] 관측 분류기는 활성어·대상 자모 편집 유사도0.60, 동작 요청형0.70, 다른 명령과의 점수 차이0.10을 사용한다. 유사도는 정답 확률이 아니다. 정확한 동작을 우선하고 부정·질문·인용·복합·상태 표현을 먼저 검사한다. `맞춰죠`처럼 방향이 없는 기존 동작의 요청 어미 변형은 내림 명령으로 추정하지 않는다. 근거: [`hi_lg_classifier.h`](../../voice_node/src/hi_lg_classifier.h), [`hi_lg_classifier_test.cpp`](../../voice_node/tests/hi_lg_classifier_test.cpp).

[문서/합의 필요] 정확한8개 하이엘지 별칭 이외의 근접 발음과 단어 조합까지 보는 관측 제안이다. 기존 닫힌 별칭·최종 문장 정책의 PM 승인으로 자동 확정된 것으로 보지 않는다. 설계서·BRD·계약의 공식 의미를 이 기록에서 바꾸지 않는다. 근거: [#107 PM 리뷰](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5614157940), [`contracts/README.md`](../../contracts/README.md).

[문서] SenseVoice에는 Whisper 토큰 진단 점수가 없어 `conf=unavailable quality=uncalibrated`로 기록한다. `--publish`·Whisper 점수 문턱·fallback 옵션은 모델 로딩·입력·연결 전에 거부한다. 최초 전사·정규화문·유사도·후보·시간을 남기며 Core·Flutter·집행 코드와 wire schema를 변경하지 않는다. 근거: `audio_observer.cpp`, [`voice_asr_options_test.py`](../../voice_node/tests/voice_asr_options_test.py).

## PR 관측 실행본 통합 검사

[실측] 별도 benchmark에만 있던 분류기를 실제 관측 실행본의 SenseVoice 분기에 연결했다. ReSpeaker60개를 직접 읽어 VAD·ASR·분류한 **Pi5 관측기66개 출력의 전사·intent·value가 기존 Pi5 benchmark와66/66 같았다.** 수현20/20·현지17/20, 비명령 실행 후보2/20·실제 송신0건이었다. 기존 voice 서비스는 inactive 그대로였고 새 마이크 녹음은 하지 않았다. 근거: JSON의 `integrated_observer.pi5`, `verification`.

[실측] Mac의 같은 관측기 재생도 수현20/20·현지17/20·비명령 실행 후보2/20이었다. 전체 비율이 같아도 전사와 성공 항목은 일부 다르다. 예를 들어 현지H07은 Pi5에서 `화력 울`→NONE, Mac에서는 `화력 올려`→IND_UP이었다. H08은 Pi5가 일치하고 Mac이 미확정이었다. 기기별 결과를 서로 대체하지 않는다. 근거: JSON의 `integrated_observer.mac.benchmark_signature_differences`.

| 실제 실행 검사 | 결과 | 근거 |
|---|---|---|
| Mac Release CMake·CTest |6/6 통과|JSON `verification`|
| sherpa 없는 Whisper 전용 빌드 |성공, ASR 옵션 경계5개 통과|JSON `verification`|
| 기존 ATLAS SDK ARM64 빌드 |관측기·단위 검사 빌드 성공|JSON `build`|
| Pi5 단위 검사 |`checks=8206 failures=0`, `audio_checks=1559 failures=0`, `hi_lg_checks=54 failures=0`|JSON `verification.native_unit_output`|
| Pi5 공유 라이브러리 |동적 링커 로딩·실제 모델 실행 성공|JSON `verification.runtime_loader`|
| SenseVoice `--publish` |모델·캡처 전에 오류로 거부|JSON `verification.native_publish_rejection`|
| 원본·입력 WAV60 해시 |전후 동일|[입력 해시](measurements/2026-10-24-hi-lg-respeaker60.sha256), JSON `verification`|

[실측] Mac 링크에는 기존 Whisper 중복 라이브러리 경고1건이 있었다. voice-node 소스는 `-Wall -Wextra -Wpedantic -Werror`로 빌드했다. 단위 검사 횟수를 녹음 수로 세지 않는다. 근거: `voice_node/CMakeLists.txt`, JSON `verification`.

## 남은 실패와 종료 조건

[실측] 현지 지원 실패3개는 H07 `화력 울`, H13 `크랜 작동 멈 죠`, H20 `활용 맞춰줘`다. 두 화자의 H21은 사전 정답이 질문 `하이엘지, 인덕션 꺼?`→비실행인데 ASR에서 `인덕션 꺼.`로 나와 IND_OFF 후보가 됐다. 물음표를 지우거나 정답표를 사후 변경하지 않았다. 근거: JSON의 해당 `items`, `trials`, `integrated_observer.pi5`.

[문서] 비명령 실행 후보2건이 남고 비명령 분모도20개이므로 PM의 비명령30개 이상·실제 무음/소음300초 조건을 통과하지 않았다. 공식 NFR-08(지정 화자·ReSpeaker/Pi5·필수5종×5회·80% 이상·발화 종료→intent3초)은 별도다. 지원40개의 높은 비율로 대체하지 않는다. Draft·#62 Open·관측 전용을 유지한다. 근거: [#107 PM 리뷰](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5614157940).

[실측 범위] JSON의 `asr_ms`는 상주 모델의 계산 시간이다. 파일 재생의 `end_to_intent_ms`는 처리 시계와 VAD 추정치이며 실발화→TCP→집행 시간으로 보고하지 않는다. 모델 로딩·종료 대기·네트워크·집행과 구분한다. Pi5 시스템 날짜가 실제 날짜와 달랐으므로 시험 날짜는 작업 PC의 KST 기록을 따랐다. 장치 시간을 수정하지 않았다. 근거: `audio_observer.cpp`의 steady clock, JSON `limitations`.

| 이전 기록·표현 | 이번 정리 |
|---|---|
| Whisper head6ab3041 검사로 새 ASR도 검증됐다는 해석 | 새 SenseVoice 관측 소스의 빌드·통합 검사를 별도로 기록 |
| 실험 파서는 관측기에 미연결 | 이번 PR에서 관측 분기에 연결, 발행은 계속 차단 |
| 서로 다른 입력 장치의 결과 합산 | 이 기록은 ReSpeaker 두 화자60개만 포함 |
| 에너지 VAD·절대 dBFS 실행 예제 | 공통 Silero와 `--vad-model`로 정정, 기존 기기 설정은 자동 변경하지 않음 |

## 재현 방법

[문서] 기존 모델·런타임·고정 Whisper 소스를 준비해 [`voice_node/README.md`](../../voice_node/README.md#sensevoice-하이엘지-관측-실행)의 CMake·CTest 명령으로 빌드한다. 실제 기기 주소·계정·인증정보를 저장소에 넣지 않는다. SDK와 원본 모델을 교체하거나 패키지를 자동 설치하지 않는다.

[문서] 전용 `WAV_DIR`에60개 원본의 읽기 사본 또는 링크를 `<id>.wav` 이름으로 두고 아래 해시부터 확인한다. ID와 정답은 JSON의 `items`가 기준이다. 기존 결과 폴더는 보존하고 새 로그 이름을 사용한다. 아래 반복은 파일 재생이며 마이크를 켜지 않는다.

```bash
(cd "$WAV_DIR" && sha256sum -c "$RECORDING_SHA256SUMS")
export LD_LIBRARY_PATH="$SHERPA_ONNX_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
set -- "$VOICE_OBSERVER" --sensevoice-dir "$SENSEVOICE_MODEL_DIR" --vad-model "$VAD_MODEL"
for speaker in suhyun nam_hyeonji; do
  n=1
  while test "$n" -le 30; do
    name=$(printf '%s_H%02d.wav' "$speaker" "$n")
    set -- "$@" --wav "$WAV_DIR/$name"
    n=$((n + 1))
  done
done
for run in 1 2 3; do
  log="respeaker60-observer-$run.log"
  test ! -e "$log" || exit 1
  "$@" > "$log" 2> "$log.stderr"
done
```

[문서] `RECORDING_SHA256SUMS`에는 첨부 해시 파일의 절대 경로, `VOICE_OBSERVER`에는 실행할 관측기 경로를 지정한다. 녹음 WAV·모델·SDK·빌드 산출물은 PR에 포함하지 않는다. 첨부 JSON에는 원문·정답·입력/모델 해시·실측 시간을 남겼다.
