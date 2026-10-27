# 하이엘지 음성 노드 — PM 완료 조건 시험 준비

- 작성자: 이수현, Codex 작성 지원
- 작성일: 2026-10-24 KST
- 상태: draft
- 카테고리: voice-node, #62, #107
- 진행 설명: 시험 도구·실시간 관측 실행본 준비 및 Pi5 입력 대기 확인. 공식 성적·실제 발행·PM 승인은 미완료

## 최신 PM 결정과 시험 게이트 — 2026-10-24

[문서] [PM 원문](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5637198979)에서 공식 구성·짧은 직접 질문·conf0·집계를 결정했다. 하이엘지8표기/근접0.60·대상0.60·동작0.70·차이0.10, Silero224/544ms, SenseVoiceSmall INT8을 시험 전에 head·바이너리/모델 SHA256과 함께 #62에 고정한다. 결과를 보고 바꾸면 새 시험이다. 팬 켜기는 FAN_SET(1), 대상 없는 꺼는 NONE이다. 허가/상태 질문·부정·인용·복합은 비실행이다.

[시험 순서] 고정 구성의 비명령36개와 실제 주변음300초를 관측 모드로 먼저 시행한다. 실행 후보0이면 남현지·팬 꺼짐·ReSpeaker/Pi5 필수5종×5회를 --publish로 시험하고, 한 건이라도 있으면 관측 전용을 유지한다. 첫 시도의 intent/value와 실제 원음 끝→intent≤3초, 발행 시 문항별 Core accepted=true를 성공 조건으로 삼는다. fan.level은 비교하지 않는다. 계약 담당 확인·CI·요청 리뷰어 전원 승인·운영 Core 시험 인계는 별도이며 기본 발행/자동 기동을 켜지 않는다.

[계획 수정] 이전 비명령 N36의 팬 켜줘는 PM 결정으로 지원 명령이 됐으므로 대상 생략 하이엘지 꺼로 교체했다. 기존 시험 결과는 소급 변경하지 않는다. field-test-plan.json의 기본 publish는false, 게이트 통과 뒤의25회만 조건부 발행이다. 발행 집계에는 검수 JSON의 core_result(source_id/last_intent/value/accepted/evidence)를 원문 Core state와 대조해 입력한다. 예전 마지막 결과를 재사용하지 않으며 자동 상관관계 판정은 아니다.

[기록 구분] 아래는 이 PM 답변 전936914d 준비 당시의 기록이다. 관측 전용/대상 생략/질문/팬 켜기 등에 관한 옛 표는 최신 결정을 대신하지 않는다. 현재 구현/시험은 위 결정·voice README·최신 송신 보고서를 따른다.

## 현재 구성과 과거 규칙의 역할

[문서] 현재 후보 경로는 `ReSpeaker/PulseAudio → Silero VAD → 상주 SenseVoiceSmall INT8 → 하이엘지 호출 확인 → 명령 해석 → 관측 로그`다. 전사를 시작하기 전에 소리로 호출어를 찾는 별도 wake-word 모델은 없다. 일반 발화도 VAD 뒤 전사하며, 전사 앞부분에서 호출어를 확인한다. 근거: `voice_node/src/audio_observer.cpp`, `hi_lg_classifier.h::classify`.

| 항목 | 이전 Whisper 경로 | 현재 SenseVoice 경로 |
|---|---|---|
| 활성 조건 | 문장 앞 후드·닫힌 별칭 | 하이엘지8표기 우선, 실패 시 앞부분 자모 유사도0.60 |
| ASR | Whisper ctx256, 조건부 원음 재전사 | SenseVoiceSmall INT8·ko·ITN·greedy·CPU4, 재전사 없음 |
| 명령 판단 | 전체 정규식·제한 치환 | 기존 문법 → 조합 문법 → 대상/동작의 제한 유사도 비교 |
| 품질 | Whisper 텍스트 토큰 평균과 no_speech | Whisper 점수 없음, `conf=unavailable quality=uncalibrated` |
| 실제 발행 | 명시적 옵션으로 가능한 경로, 변경 정책 검토 대기 | 발행 옵션을 모델·캡처 전에 거부 |
| VAD | `6ab3041` 에너지·20ms·종료800ms | 현재 공통 Silero·32ms·시작224ms·종료544ms·앞992ms. Whisper에도 적용 |

[문서] 하이엘지 뒤 명령문은 먼저 정확한 기존 문법을 재사용하므로 모든 단어를 유사도 계산으로만 판정하지 않는다. 실패하면 대상0.60·동작 형태0.70·다른 명령과의 차이0.10을 적용한다. 부정·인용·복합·상태·허가 질문은 먼저 검사한다. `팬 켜줘`는 단수 부족, `바람 바꿔줘`는 목표 부족으로 NONE이며 정확한 동작을 다른 동작으로 바꾸지 않는다. `맞춰죠`도 방향 없는 설정이므로 DOWN으로 승격하지 않는다. 근거: `hi_lg_classifier.h::semantic/fallback/classify_body`, `hi_lg_classifier_test.cpp`.

[문서] `호다`는 하이엘지 별칭이 아니다. 기존 Whisper에서는 후드 호출 별칭이고, SenseVoice에서는 하이엘지를 먼저 통과한 명령문 앞의 가전 이름 교정에만 남아 있다. 후자는 `hi_lg_rules.h::hoods`와 기존 `parse_command` 재사용으로 처리한다. 호출어 별칭8개, 본문 가전명 별칭, 동작 요청 어미는 서로 다른 역할이다. 이 구분 없이 기존 후드 별칭 승인을 새 호출어 정책 승인으로 옮기지 않는다.

| 코드에 직접 넣은 전사 | 현재 결과 |
|---|---|
| `호다 약풍` / `후드 약풍` | `DROP activation_similarity_below_threshold` |
| `하이엘지 약풍` / `하이엘지 호다 약풍` | `FAN_SET(1) existing_whole_command` |
| `하이에일지 팬 꺼줘` | `FAN_OFF existing_whole_command` |
| `하이엘지 팬 켜줘` / `하이엘지 켜` | `NONE target_too_different` |
| `하이엘지 인덕션 꺼?` | `NONE question_or_permission` |
| `하이엘지 인덕션 꺼.` | `IND_OFF existing_whole_command` |
| `하이엘지 꺼` | `FAN_OFF existing_whole_command` — 기존 후드 문법 재사용에 따른 현재 동작, 대상 생략 수용 여부 합의 필요 |
| `하이엘지 인덕션 켜지 마` | `NONE negation_quote_compound_or_relative` |

[실측] 위 표는 2026-10-24 Mac C++ 후단 입력 검사다. 원음 ASR 정확도나 공식 시험 성적이 아니다. 재현은 동일 문자열로 `hi_lg::classify`를 호출하며 `hi_lg_classifier_test`의 호출어·질문·부정 검사를 함께 실행한다. 근거: 해당 헤더/테스트, [60건 보고서](2026-10-24-hi-lg-respeaker60-pi5-validation.md).

## 시험 순서와 정답표 고정

[문서/준비] [기계 판독 계획](../../voice_node/tools/field-test-plan.json)에 부정36개·실제 주변음2개·필수25개·정책 진단2개를 고정했다. 부정36개의 직접 텍스트는 비실행, 필수25개는 지정 intent/value로 해석되는 후단 검사61개를 통과했다. 이 검사는 새 녹음 ASR 통과가 아니다.

1. PM·계약 담당에게 아래 정책안을 제시하고 답변 링크를 계획에 기록한다. 하이엘지·유사도·종료544ms·SenseVoice 품질/conf·직접동작 질문 및 대상 생략을 분리해 결정한다.
2. 실행 head·ARM 바이너리/모델 해시·Pulse 입력·화자·거리·팬/소음·문턱·정답표·집계법을 고정한다. `#62`에 「공식 구성」을 먼저 게시한다. 결과를 본 뒤 설정을 바꾸면 새 시험으로 분리한다.
3. 비명령36개와 실제 무음/소음 각150초를 녹음·관측한다. 30개 이상, 실제 합계300초 이상, 실행 후보0을 확인한다. 주변음 종류를 실제 조건으로 검수한다. 조용한 녹음을 소음 시험으로 재분류하지 않는다.
4. 남현지·ReSpeaker·Pi5·팬 꺼짐으로 강/중/약·팬 끄기·인덕션 끄기 각5회, 총25회를 시행한다. 첫 시도만 공식 분모로 유지하고 VAD 누락·ASR·품질·시간 실패를 제거하지 않는다. 재녹음은 진단용 추가 기록이다.
5. 후보 정확도와 실제 발화 끝→intent3초를 별도 계산한다. 현지 기존11건 재사용/부족14건 보충은 **승인된 대체안이 아니므로 기본 계획은 새25회**다.
6. 원문·정답·성공/누락·지연·부정/주변음 결과를 `#62`에 남기고 PM이 시연/병합 범위를 판단한다. 실제 발행·화면의 마지막 음성 명령 갱신은 별도 승인/구현 확인 전 완료로 표시하지 않는다.

[문서/합의 필요] 두 사람의 기존 `인덕션 꺼?`는 당시 질문 정답과 오탐2건을 보존한다. 「하이엘지+대상+직접 동작」을 명령으로 받는 안은 새 정책 진단2개로 분리했다. 부정36개에는 `켜도 돼?`·`꺼져 있어?` 같은 명확한 질문을 남겼다. 새 목록을 통과해도 과거 결과를 오탐0으로 덮어쓰지 않는다. 근거: 계획의 `policy`/`negative`, PM10/22 리뷰, 사용자10/24 제안.

## 실제 상주 입력과 실시간 확인

[문서] 현장 확인의 기본 경로는 [`live_observer.py`](../../voice_node/tools/live_observer.py)로 바꿨다. 실제 ARM 관측기를 수동 시작한 뒤 ReSpeaker 입력을 계속 처리하며 모델은 한 번만 로딩한다. 사용자에게 문장별 녹음 버튼이나 파일 업로드를 요구하지 않는다. localhost 화면은 입력 레벨·음성 확률·모델 로딩/전사 횟수·최초 전사·활성어·명령문·판정/사유·처리 시간을 표시한다. 실행 후보·NONE·DROP·발화4초 초과·기한5초 만료를 구분하고 실제 송신은0으로 표시한다. 새 의존성 없이 Python 표준 라이브러리와 기존 SSH/ARM 관측기를 재사용한다.

[실측] 2026-10-24 새 ARM 실행본의 SHA256 대조 후 Pi5에서 `model_loaded`·`recognition_config model=sensevoice`·`capture_ready`와 연속 `audio_level`을 확인했다. Safari 화면도 `상주 · 입력 대기`, 모델 로딩1회·ASR0회·송신0을 표시했다. 이는 입력 기동 확인이며 새 발화의 인식 성공이나 PM 공식 시험 통과가 아니다. 원문 근거는 작업 PC의 `live-console/run-1.log`와 `page.json`에 보관한다. 실제 주소·토큰·로컬 경로는 저장소에 넣지 않는다.

[문서] 현재 호출어는 VAD 뒤 ASR 텍스트에서 확인한다. “하이엘지 + 명령”을 한 발화로 말해야 하며, 호출어만 먼저 말한 후 다음 발화를 허용하는 세션은 없다. 이 제한을 화면과 README에 표시했다. `--publish`·Core 주소·원음 저장 옵션은 주지 않았다. 기존 서비스/다른 관측기가 있으면 시작을 거부하고, 종료 시 자기 PID의 실행 파일·작업 경로를 대조한다. 중복 실행과 다른 팀원의 프로세스 종료를 피한다.

## 원음·실시간 지연 기록

[문서] 현장 실행 옵션 `--capture-raw NEW_PATH`는 기존 파일을 거부하고 같은16kHz·mono·S16LE 입력을 GStreamer tee로 보존한다. ASR 중 앱 입력이 버려져도 녹음 분기는 원음을 유지한다. 실패/유실은 로그·원음과 함께 실패로 남긴다. 파일은 기기 밖으로 자동 전송하지 않는다. 기본 실시간 화면에서는 이 옵션을 끈다. 공식 원음/지연 검수 때만 별도 유한 실행으로 보존하며, 기존 문항별 녹음 도구는 선택적 정밀 검사로 남긴다. 근거: `audio_observer.cpp::observe_microphone`.

[문서] 첫 PCM sample의 `sample0_monotonic_ns`와 후보의 `intent_monotonic_ns`는 같은 기기 clock이다. 원음의 마지막 발음 끝을 `speech_end_s`로 청취 지정한 뒤 다음 식으로 계산한다. 원점이 없거나 원음·발음이 불명확하면 공식 지연은 미확인으로 둔다. `end_to_intent_ms`는 기존 VAD 추정치로 별도 기록한다.

```text
실제 발화 끝 → intent(ms)
  = (intent_monotonic_ns - sample0_monotonic_ns) / 1,000,000
    - speech_end_s * 1,000
```

[문서] `field_evaluation.py`는 각 문항의 **첫 attempts/metadata.json**을 사용한다. 첫 녹음 실패 후 재시도 성공으로 분모를 바꾸지 않는다. 한 문항에서 다른 명령·중복 실행 후보가 나오면 실패이며 NONE/DROP/무검출은 지원 문항 실패다. 비명령에서 무검출은 캡처 정상·청취 검수가 있어야만 비실행 성공이다. PM 승인·환경·공식 선언은 수치와 별개이므로 출력의 `official_status`는 자동 PASS가 아니다. 근거: 도구의 `first_attempts/score/summarize`와 `self-test`.

```bash
python3 voice_node/tools/field_evaluation.py self-test
python3 voice_node/tools/field_evaluation.py review --session "$SESSION_DIR" --out "$SESSION_DIR/review.html"
# review.html에서 원음·말끝·실제 주변음·발화 끝을 확인해 다운로드한 파일 지정
python3 voice_node/tools/field_evaluation.py summarize --session "$SESSION_DIR" \
  --annotations "$ANNOTATIONS_JSON" --out "$NEW_RESULT_DIR"
```

## 재범 리뷰 반영 범위

| 항목 | 처리·남은 조건 |
|---|---|
| Whisper 재전사 확대 누락 | README에 네 사유·모델/ctx/beam·품질·기한 조건을 명시. PM 검토 전 송신에 사용하지 않음 |
| 모호 전원 재전사가 IND_ON 허용 | 후보를 IND_OFF로 고정해 같은 명령 비교로 제한. 파서 검사에 방향 불변식을 남김 |
| `호다`·동일 문턱 | 기존 Whisper/본문 교정 역할과 동일값 허용을 명시. 기존 조건부 승인을 새 정책에 자동 적용하지 않음 |
| 공통 Silero·544ms | Whisper에도 적용됨을 명시. 종료 추정−544ms+5초 deadline과 이전4/25의 조건 차이를 기록 |
| 삭제된 `--vad-dbfs` 예제 | 실행 예제에서 제거, 필수 옵션은 `--vad-model`로 정정 |
| env/유닛 이행 | Silero 배치→env VAD_MODEL/필요시 LD_LIBRARY_PATH→관측 실행→유닛 교체→verify/daemon-reload 순서와 복원을 deploy 안내에 추가 |
| #109 충돌 | `git merge-tree --write-tree HEAD origin/pr-109`에서 deploy/README 충돌 확인. #109 화면 절과 voice 절을 모두 보존하는 해결본을 별도 준비, 미병합 타 PR을 이 PR에 합치지 않음 |
| inbox 상태 | 60건 보고서의 상태를 draft로 정정 |
| 경고음 클립 | 승인된 호출어와 근접 발음으로 시작하는 제어 문장을 피하고, 실제 ReSpeaker 재입력 시 실행 후보0인지 별도 검사. 문구 회피만으로 무오탐을 단정하지 않음 |

## CI·수신기·기동·승인

[문서/준비] System CI에 모델 없는 voice_node job을 추가하고 `system_ci_result`의 needs에 포함했다. 파서·하이엘지·VAD·TCP 직렬화/loopback·집계 도구 검사를 실행한다. ASR/GStreamer ARM 빌드·현장 음성 인식률까지 CI가 검증하는 것은 아니다. 근거: `.github/workflows/system.yml`, `voice_node/CMakeLists.txt`.

[실측] Mac Release에서 모델 없는 CTest7/7(34.08초), SenseVoice/Whisper 통합 CTest8/8(33.10초)을 통과했다. 통합 빌드에는 기존 Whisper 중복 라이브러리 링크 경고가 남았다. ATLAS SDK ARM64/Pulse 빌드와 Pi5 파서8206·오디오1559·하이엘지54검사를 통과했고, 옵션 경계는 원음 저장의 WAV 입력 거부와 기존 파일 덮어쓰기 금지를 포함한다. 실시간 화면의 분류·상태 전이·접근 검사도 별도 확인했다. 근거: 작업 기록의 `mac-final-check.log`, ARM 빌드/검사 로그, 도구 self-test. 이는 PM 음성 인식 성적이 아니다.

[문서] 수신기 검증은 localhost loopback의 연결/침묵/재접속/만료/중복/프레이밍과 실제 Core의 수락 여부를 구분한다. SenseVoice는 현재 발행하지 않으므로 hello/state 확인으로 `state.voice`나 Flutter 카드 갱신 검증을 대신하지 않는다. conf/발행 정책 승인 뒤에 실제 voice 연동을 구현·검증해야 한다. 임의 conf=1 또는 유사도를 ASR 확률로 보내지 않는다. 근거: `voice_tcp_receiver_test.py`, SenseVoice 발행 옵션 경계 검사.

[문서/준비] 현장 담당자와 조정한 뒤 관측 전용 수동 start→준비 로그/단일 PID/hello-state→stop/프로세스 정리→다시 start를 확인한다. 기존 운영 프로세스나 다른 팀원 세션을 자동 종료하지 않는다. 기존 서비스·env·모델을 일괄 덮어쓰거나 자동 기동을 설정하지 않는다. 근거: `deploy/README.md`.

[문서] 새 head는 PM/voice 담당 남현지·계약 정정인·코드/CI 임재범 검토와 `all_reviewers_approved`, `system_ci_result`의 실제 상태를 확인한다. 에이전트가 승인·스레드 해결·Ready 전환·병합·#62 종료를 대신하지 않는다. 근거: `.github/CODEOWNERS`, `.github/workflows/review-guard.yml`, PM10/22 리뷰.

## PM·계약 담당에게 제시할 결정 문안

> 현재 후보는 ReSpeaker→Silero(시작224ms/종료544ms)→SenseVoiceSmall INT8→하이엘지 확인→명령 문법/역할별 유사도이며 관측 전용입니다. 하이엘지8표기와 근접 발음0.60, 대상0.60·동작0.70·후보 차이0.10을 공식 시험 구성으로 사용할지 확인 부탁드립니다. 후드/호다 목록은 하이엘지 호출을 대체하지 않으며 본문 가전명 교정과 기존 Whisper 호환 경로에 남아 있습니다. 짧은 「인덕션 꺼?」를 직접 명령으로 수용할지, 대상 생략 「하이엘지 꺼」를 팬 끄기로 받을지도 별도로 확정하고 싶습니다. 부정·허가/상태 질문·인용·복합은 비실행으로 유지합니다. SenseVoice에는 Whisper 토큰 점수가 없어 현재 conf를 전송하지 않습니다. 실제 발행 전 conf 의미/품질 게이트를 별도로 합의하고, 그전까지 공식 시험은 관측 후보·실제 지연으로 집계하는 것을 허용할지 확인 부탁드립니다. 부정36개+실제 주변음300초, 남현지 필수25회를 고정 설정으로 시행하며 과거 오탐/실패는 보존합니다.

## 이전 설명의 정정

| 이전 설명 | 현재 코드와 시험에서 구분할 내용 |
|---|---|
| 호다 규칙은 더 이상 쓰지 않는다 | 현재 하이엘지 호출 별칭은 아니지만 본문 가전명 교정과 Whisper 호환 경로에는 남아 있음 |
| 모든 단어가60%이면 명령 | 정확한 문법을 우선하며 대상/동작 문턱·후보 차이·거부 규칙을 각각 적용 |
| 하이엘지 꺼는 대상 불명으로 NONE | 현재는 FAN_OFF. 정책 변경 시 별도 구현/시험 필요 |
| 하이엘지를 듣고 전사를 시작 | 현재는 VAD→전사→텍스트 호출 확인, 별도 음향 wake-word 아님 |
| 질문2건을 새 정책으로 오탐0 처리 | 기존 질문 정답·오탐2 기록 보존, 변경 정책/진단 문항과 분리 |
| WAV 처리 시간 또는 VAD 종료 추정으로 공식3초 판정 | 실제 원음 끝·같은 기기 시간 원점으로 검수 후 계산, 미확인은 PASS 금지 |

## 종료 지점

[문서/준비] 이번 준비의 종료는 실시간 상주 화면·시험 목록·선택적 원음/시간 기록 실행본·첫 시도 집계·복원 가능한 실행 절차·리뷰 수정·CI 구성이 준비된 상태다. Pi5 ReSpeaker 입력 대기는 확인했지만 새 발화 시험, 공식 NFR-08, PM 정책 승인과 실제 송신은 완료로 보고하지 않는다. 실제 결과가 모이면 별도 보고서로 판정한다. 근거: 본 계획과 PM10/22 조건.
