# #62 STEP 4 — ReSpeaker·VAD·상주 Whisper 관측 검증

- [문서] 공개용 경로 표기: `<작업 PC>`·`<작업 PC 임시 경로>`·`<마스터 작업 경로>`는 실제 경로로 치환한다. 아래 명령은 당시 실행 기록이며 원래 경로와 로그는 저장소 밖에 보존한다.
- 작성자: 이수현 (@suuuhululu), 구현·검증 지원: Codex
- 작성·검증일: 2026-10-19 KST (작업 PC 기준)
- 카테고리: voice-interface / audio-pipeline-verification
- 상태: **오디오→VAD→ASR→명령 후보 연결 검증 완료. 실제 음성 인식 품질·지연 기준은 미충족/미확정. STEP 5 미진입**
- 범위: #62, 설계서 §5.3. 관측 프로그램만 구현하며 TCP·팬·인덕션·systemd 부팅 등록은 제외
- 표기: [실측] 실행 결과, [문서] 코드·합의·자료, [추정] 확인 방법을 붙인 가설

- [문서] 이 문서의 STEP 5 미진입은 STEP 4 당시 상태다. 이후 TCP 부분 구현과 PR #93·리뷰 반영은 [현재 검증 기록](2026-10-20-voice-acceptance-policy-design.md)을 따른다.

- [실측] 후속: [2026-10-20 1차 성능 시험](2026-10-20-voice-first-performance.md)에서 화자 1명의 기본 8종 후보 일치 `0/8`을 확인했다. 연결 검증 완료와 성능 검증 결과를 구분한다.

## 사전 점검과 승인 판단

| 항목 | 결과 |
|---|---|
| 범위·승인 | [문서] 사용자가 STEP 4 사전 검토 후 추가 승인 필요가 없으면 진행하도록 요청. 기존 도구·권한으로 가능해 진행 |
| 작업 위치 | [실측] `feat/suhyun-62-command-parser`, `<작업 PC 임시 경로>/hoodseek-62-parser-h1Wvyk`. 기존 #50 문서 브랜치·커밋과 `.vscode/`는 보존 |
| 계약 | [문서] #75 head `56209b931f438b2c0dbe6b400d1c1326f0992560` 유지, `merged_at=2026-10-19T12:08:33Z`(21:08:33 KST). intent·필드·NONE 기준 변경 없음 |
| Pi5 서버 | [실측] PulseAudio 17.0, 서버 `/var/run/pulse/native`, 사용자 `pulse`. root SSH 세션에서 기존 권한으로 접근 |
| 마이크 | [실측] `alsa_input.usb-Seeed_Studio_ReSpeaker_Lite_0000000001-00.analog-stereo`, s16le 2ch 16000Hz. LifeCam 및 monitor 소스도 있어 명시 선택 |
| 도구·라이브러리 | [실측] Pi5의 pulsesrc·appsink·audioconvert·audioresample plugin과 SDK의 gstreamer-app-1.0 헤더·pkg-config 파일이 기존에 존재 |
| 추가 승인 | [실측] 설치·서비스·장치 볼륨·권한·rootfs 변경 불필요. 검사 파일에만 실행 권한 부여 |
| 기동 합의 | [문서] #46 PM 결정대로 G2까지 재부팅 후 수동 start. 자동 기동은 PM 10/22 결정 이후. 이번에는 systemd 유닛을 설치하지 않음 |
| GitHub | [실측] 전문 제시 후 사용자 `진행` 승인으로 #62 결과 댓글 1회 게시. 본문 일치·이슈 Open 확인(2026-10-19 21:42:50 KST). Push·PR 없음 |

[문서] 사용자가 제공한 하드웨어 안내와 `inbox/정정인/2026-09-25-wiring-guide.md` §2-D에서 USB ReSpeaker 연결을 참고했다. 입력 소스명은 현재 Pi5 실측과 일치했다. 9월 문서의 push-to-talk·TTS 등 구 설계는 채택하지 않았으며 최신 §5.3·계약을 우선했다. 스피커 커넥터·실제 배선 리비전은 이번에 검사하지 않았다.

## 구현

- [문서] `src/audio_pipeline.{h,cpp}`: 20 ms RMS 에너지 VAD와 일반 텍스트 토큰 점수 평균. 0.3 s 시작 확인·0.8 s 종료 무음·4 s 상한. 잘린 발화는 분할 실행하지 않고 폐기한다. 짧은 활성 단어를 위해 시작 확인 전 1 s를 보존한다(검증용 보정값).
- [문서] `src/audio_observer.cpp`: 기존 GStreamer appsink로 ReSpeaker를 명시 선택, 16 kHz mono PCM16 변환. queue 2 buffers, 100 ms 제한 대기로 종료 요청을 확인한다. 추론 동안 쌓인 입력은 버리고 0.8 s 무음 이후 재개한다.
- [문서] 기존 whisper.cpp·모델을 재사용해 context 1회 로딩. CPU 4 threads, audio_ctx 256, beam 5, ko, no_context, no_timestamps, 기존 단어 문맥 사용. WAV 디코딩은 whisper의 기존 common 라이브러리를 재사용한다.
- [문서] `conf`는 특수 토큰을 제외한 `p`의 평균. 빈 일반 토큰·NaN·무한대·범위 밖 지표는 폐기. 점수가 높아도 정확도 확률로 간주하지 않는다. 품질 미보정 후보는 `quality=uncalibrated`, `observation_only`로만 출력한다.
- [문서] SIGINT/SIGTERM 시 캡처·추론을 중단하고 context·GStreamer 자원을 정리한다. TCP 구현·계약 JSON·장치 제어 경로는 없다. `transmitted=0` 로그는 전송 경로 미구현을 나타내며 패킷 계측 결과가 아니다.
- [문서] 새 파일은 VAD 상태·점수 검증을 ASR 의존성 없이 검사하고 관측 실행 파일을 만들기 위해 필요하다. 새 패키지·모델·외부 서비스는 추가하지 않았다.

## 환경·증거 보존

| 항목 | [실측] 경로·해시 |
|---|---|
| 로컬 증거 | `<작업 PC 임시 경로>/hoodseek-62-step4-UEMdvJ` |
| 마스터 빌드 | `<마스터 작업 경로>/hoodseek-work/issue-62/step4-audio-20261019-cOyobQ` |
| Pi5 실행·로그 | `/data/hoodseek-issue62-audio-20261019-GR6TsI` |
| SDK 이미지 | `sha256:0a6fa7930de27d0ab57acb2748ce348b95da2e92f2bf89f00a5d91d0786b3eba`, Clang 20.1.1 |
| whisper 커밋 | `d1be6fde11ac6e0407606b4e42fe72d34add8037` — 기존 소스를 read-only bind mount |
| base 모델 | `/data/hoodseek-issue50-20261019-qHyd66/ggml-base.bin`, SHA-256 `60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe` 유지 |
| 최종 관측 실행 파일 | `hoodseek-voice-observer-preroll1s`, SHA-256 `cddef1073321fdcb30e8237f908d9c484819fd90ae1be2ef43d945cf12af1fb3` Mac↔Pi5 일치 |
| 최종 ARM 오디오 검사 | `audio_pipeline_test-preroll1s`, SHA-256 `3b9bf56e1440e62d053ceec10453c7293fc69f410e01a76537a34e4f1c961eb0` |

[실측] 소스 8개는 `source.sha256`·`source-verification.log`로 로컬↔마스터 일치를 확인했다. `cross-build*.log`, `whisper-commit.txt`, Pi5의 `runtime.log`, `parser-test.log`, `audio-test-preroll1s.log`, `wav-preroll1s.log`, `mic-*.log`, `lifecycle.log`를 로컬에도 회수했다. 녹음 파일을 저장하지 않고 전사·입력 레벨·상태 로그만 남겼다. 검사 후 관측 프로세스는 종료됐고 마스터 SSH 재사용 연결도 닫았다.

## 검증 결과

| 검사 | 결과와 한계 |
|---|---|
| Mac·Pi5 파서 | [실측] 각각 `checks=299 failures=0` |
| Mac·Pi5 오디오 처리 | [실측] 각각 `audio_checks=1517 failures=0`. 시작 보관·짧은 활성 단어 후 휴지·0.8 s 종료·무음·4 s 초과 폐기·재진입·PCM/점수 경계 |
| Mac sanitizer | [실측] 두 검사 모두 통과. ASan/UBSan 진단 없음. Pi5 sanitizer는 미수행 |
| Pi5 WAV | [실측] context 1회 로딩·전사 3회. 아래 원문 참조. 동일 Yuna 합성 명령 재사용이며 독립 실제 화자 3명이 아님 |
| 무음·시간 초과·EOF | [실측] 무음 4 s 호출 0, 6 s tone 1구간 폐기 후 호출 0, 불완전 EOF 호출 0. 다음 정상 발화 처리 가능 |
| 캡처 | [실측] ReSpeaker의 16 kHz mono 프레임 수신. 3 s 구간 150 frames, ASR 0회 |
| 종료 | [실측] 캡처 중 SIGTERM, 추론 중 SIGINT 각각 exit 0. 자원 정리 후 프로세스 종료 |
| 잘못된 source | [실측] `event=error message="Failed to connect stream: No such entity"`, exit 1 |
| CLI 경계 | [실측] 누락 옵션·NaN 임계값·알 수 없는 옵션·없는 파일을 오류 exit 1로 처리 |

[실측] 최종 Pi5 WAV 출력(발췌 원문):

```text
audio_checks=1517 failures=0
event=model_loaded count=1 load_ms=204 vad_dbfs=-50 mode=observation_only
event=vad_drop reason=max4s
event=asr index=1 samples=47040 asr_ms=2196 text=" 후드 강풍으로 바꿔줘." conf=0.769628 no_speech_max=1.92877e-05 disposition=observation_only quality=uncalibrated intent=FAN_SET value=3
event=asr index=2 samples=47040 asr_ms=2163 text=" 후드 강풍으로 바꿔줘." conf=0.769628 no_speech_max=1.10434e-06 disposition=observation_only quality=uncalibrated intent=FAN_SET value=3
event=asr index=3 samples=44480 asr_ms=2028 text=" 후드 강풍으로 바꿔줘." conf=0.803878 no_speech_max=1.10435e-06 disposition=observation_only quality=uncalibrated intent=FAN_SET value=3
event=stopped frames=1096 peak_dbfs=-12.575 asr_calls=3 overlong_drops=1 candidates=3 transmitted=0
capture_sigterm_exit=0
asr_sigint_exit=0
invalid_source_exit=1
```

[문서] `asr_ms`는 모델 로딩·VAD 종료 대기·전송을 제외한 whisper_full 시간이다. 위 값은 padded WAV·상주 API 조건이다. #50의 원본 WAV·CLI 전체 시간과 같은 지표로 비교하지 않는다. Mac은 CPU BLAS backend가 포함돼 Pi5·기존 Mac 비교와도 조건이 다르다. 실제 발화에서도 약 2 s 전체 목표를 달성했다고 주장하지 않는다.

## 실제 마이크·주변 대화 시험

- [실측] 사용자가 실제 발화 시험 가능, 10 s 사용자 무발화 측정 가능, 이후 앞뒤 주변 대화 소음이 있었다고 확인했다. **조용한 환경 R5 표본으로 사용하지 않는다.**
- [실측] 임시 -50 dBFS의 35 s 시험은 `asr_calls=0 overlong_drops=1`. -35 dBFS 3 s 관측은 peak `-24.0429`. -20 dBFS 10 s 사용자 무발화 관측은 대부분 약 -35~-37 dBFS였으나 peak `-15.2628`, `asr_calls=0`이었다. 완전한 디지털 무음 시험과 구분한다.
- [실측] -25 dBFS 시험에서 첫 전사 `강풍으로 해줘.`는 활성 단어가 없어 제외됐고, `후드, 인덕션, 켜지마.`는 NONE 후보였다. 처음의 ASR 시간은 9908 ms, 뒤 발화는 2385 ms였다.
- [추정] 첫 활성 단어 누락에는 시작 확인 지연·ASR 자체 오류·주변 대화가 영향을 줄 수 있다. 시작 보관을 0.3 s에서 1 s로 늘리고 짧은 활성 단어 뒤 휴지 회귀 검사를 추가했다. **개선 원인을 확정하지 않았다.** 확인 방법: 조용한 조건에서 동일 녹음·VAD 구간을 고정해 이전/수정 보관 길이와 ASR 출력을 분리 비교한다.
- [실측] 수정본 시험에서 사용자가 한 번 말한 문장이 `호대 광풍으로 해줘. 호대 광풍으로 해줘.`로 중복·오인식됐다. `conf=0.86508`, `asr_ms=7681`, 활성 단어 불일치로 제외됐다. 높은 conf가 정확도를 뜻하지 않음을 보여 주는 관측이다.
- [실측] 이어진 배경 대화 조건에서 다른 전사·4 s 초과 폐기도 발생했다. 정답 발화와 로그 구간을 독립적으로 대응시키지 못한 결과는 명령 정확도 계산에서 제외했다. 사용자가 제안한 문장별 2회 비교는 통제된 정답 표본으로 완성하지 못했다.
- [문서] 에너지 VAD는 주변 사람의 말과 사용자 명령을 구별하지 않는다. 긴 ASR 처리 중 입력을 버리는 현재 방식은 오래된 발화의 지연 실행을 피하지만 다음 명령을 놓칠 수 있다. 실제 사용성 검증이 남는다.

[문서] 판정: 파이프라인·진단 점수 계산·종료 경로의 기술 검증은 완료했다. 실제 한국어 R5 ≥80%·전체 약 2 s·품질 임계값은 미충족/미측정으로 남긴다. 현 상태를 바로 TCP 명령 송신에 연결하지 않는다. 모델 교체·독립 웨이크워드·VAD 모델 도입은 이번에 하지 않았다.

## 재현 방법

[실측] 로컬 CMake는 `<작업 PC>/miniforge3/envs/ros2_jazzy/bin/cmake` 4.3.4, Apple Clang 21.0.0을 사용했다. Mac 단위 검사 원문은 `mac-unit-final.log`, sanitizer는 `mac-sanitize-final.log`다.

```bash
cmake -S voice_node -B <작업 PC 임시 경로>/hoodseek-62-step4-UEMdvJ/mac-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DWHISPER_SOURCE_DIR=<작업 PC 임시 경로>/hoodseek-mac-asr-20261019-ENEFih/whisper.cpp \
  -DGGML_METAL=OFF -DGGML_ACCELERATE=OFF -DGGML_OPENMP=OFF
cmake --build <작업 PC 임시 경로>/hoodseek-62-step4-UEMdvJ/mac-build \
  --target command_parser_test audio_pipeline_test hoodseek-voice-observer --parallel 2
ctest --test-dir <작업 PC 임시 경로>/hoodseek-62-step4-UEMdvJ/mac-build --output-on-failure -V
```

[실측] ARM 명령은 마스터 `run-arm-build.sh`에 보존했다. SDK 환경을 source한 뒤 기존 OEToolchainConfig를 사용했다. 임시 컨테이너는 network none·read-only이고 새 작업 폴더만 쓰기 가능, 기존 whisper 소스는 read-only였다.

```bash
cmake -S /work/voice_node -B /work/build-arm \
  -DCMAKE_TOOLCHAIN_FILE=/opt/atlas-sdk-x86_64/sysroots/x86_64-oesdk-linux/usr/share/cmake/OEToolchainConfig.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=OFF \
  -DWHISPER_SOURCE_DIR=/whisper -DHOOD_SEEK_WITH_PULSE=ON \
  -DGGML_CPU_ARM_ARCH=armv8-a+crc -DGGML_OPENMP=OFF -DGGML_BLAS=OFF \
  -DGGML_CCACHE=OFF -DWHISPER_BUILD_IS_DEV=OFF
cmake --build /work/build-arm --target command_parser_test audio_pipeline_test hoodseek-voice-observer --parallel 2
```

## 문제와 남은 확인

| 항목 | 기록·대응 |
|---|---|
| 원본 보존 | [실측] 기존 source·모델·정상 산출물 불변. 새 경로에 초기·meter·preroll1s 실행 파일을 각각 보존 |
| 실행 권한 | [실측] 새 파일 전송 후 실행 권한을 빠뜨려 exit 126 발생. 해당 검사 파일만 `chmod u+x`한 뒤 캡처 정상 실행. 실패 stderr도 보존 |
| Pulse 경고 | [실측] `Failed to create secure directory (/root/.config/pulse): No such file or directory`, `Failed to load cookie file from cookie: No such file or directory`. 캡처는 성공. 전역 설정·권한을 바꾸지 않음 |
| LLVM profiling | [실측] plugin 검사에서 `LLVM Profile Error: Failed to write file "default.profraw": Read-only file system`. 이후 각 검사 프로세스에만 `LLVM_PROFILE_FILE=/dev/null` 적용. 시스템 설정 변경 없음 |
| upstream 경고 | [실측] 기존 whisper의 CMake deprecation·git metadata·stb_vorbis pointer comparison 경고. upstream 소스를 수정하지 않음 |
| 추가 TTS | [실측] Mac say/afconvert로 만든 새 부정·모호·복합·상대 문장의 source WAV가 0 sample이었다. 유효 발화 표본에서 제외. 기존 정상 Yuna WAV와 실제 마이크 결과만 근거로 사용 |
| 품질 보정 | [문서] 조용한 조건의 실제 화자·부정문·복합 명령·팬 소음 정답 집합을 따로 확보해야 함. 임계값 하나로 배경 대화를 제거할 수 있다고 가정하지 않음 |
| 지연 | [문서] 모델 상주로 로딩을 줄여도 추론·0.8 s 종료 대기·busy 구간 손실이 남음. VAD/ASR 변경은 속도와 거부 정확도를 함께 비교 |

## 공유·문서 갱신 거리

- [문서] 🔔 공유 대상: 현지·정인·재범 / #62·Slack. G2 수동 운용은 유지하되 실제 발화 품질과 지연 위험을 공유해야 한다. GitHub는 아래 댓글로 공유했고 Slack은 미게시다.
- [문서] PM 요청: 설계서 §5.3에 명시 입력 소스·관측 상태·품질 미확정·발화 보관과 지연 측정 범위를 구분해 기록하는 방안을 검토 요청한다. §8.2의 G2 수동 start 합의는 유지한다. PM 소유 문서·계약·대장·일정은 직접 수정하지 않았다.
- [문서] 다음 STEP은 TCP 구현이지만 사용자 승인 없이 진입하지 않는다. 현재 음성 후보를 바로 집행에 연결할 품질 기준은 확보하지 못했다. 우선 조용한 정답 집합으로 ASR/VAD 오류를 재현·분리하는 것을 권장한다.

### #62 공유 완료 (쓰기 1회)

- [실측] 사용자 승인 후 [STEP 4 결과 댓글](https://github.com/lambdaramji/HOOD-SEEK/issues/62#issuecomment-6267473814)을 게시했다. 재조회한 댓글 본문은 승인된 전문과 일치하며 #62는 Open이다(확인: 2026-10-19 21:42:50 KST).
- [문서] 연결 검증은 ReSpeaker→VAD→ASR→명령 후보까지다. 성능은 일부 측정했으나 실제 발화 정확도·오작동률·약 2 s 전체 지연 목표의 검증 완료는 후속 과제다. SafetyCore TCP 연결은 STEP 5 범위이며 이번 승인으로 진행하지 않았다.

```markdown
## STEP 4 관측 파이프라인 검증 (수현, 2026-10-19)

- [실측] 기존 GStreamer/PulseAudio와 고정 whisper.cpp/base 모델로 ReSpeaker→에너지 VAD→상주 ASR→진단 점수→명령 후보를 연결했습니다. 설치·SDK·시스템 권한 변경은 없고, TCP·장치 명령은 전송하지 않았습니다.
- [실측] Mac·Pi5 각각 파서 299개와 오디오 처리 1517개 검사 통과, Mac ASan/UBSan 통과입니다. Pi5에서는 모델 1회 로딩으로 기존 합성 명령을 3회 전사했고, 무음·4초 초과·불완전 EOF는 ASR에서 제외했습니다. 캡처 중 SIGTERM·추론 중 SIGINT 정상 종료, 잘못된 입력 소스 오류 종료도 확인했습니다.
- [실측] 실제 마이크 시험에는 앞뒤 주변 대화가 있었습니다. 활성 단어 오인식·문장 중복과 긴 추론이 관측됐고, conf가 높아도 오인식했습니다. 조용한 환경 R5·전체 약 2초 목표·품질 임계값은 통과로 보지 않습니다.
- [문서] G2까지 재부팅 후 수동 start 방침 유지입니다. STEP 5는 미진입이며, 실제 명령 전송 전에 조용한 정답 집합으로 VAD/ASR 품질·지연을 추가 검증하겠습니다. #62는 Open으로 유지합니다.
```
