# Q3 E2 안내 음원 교체 후보와 검사

- 작성자: 이수현
- 작성일: 2026-10-25 (KST)
- 상태: draft
- 카테고리: flutter-audio, generated-assets
- 관련: #133, #134, #121

## 결정과 범위

[문서] 사용자는 Qwen VoiceDesign의 Q3를 고른 뒤 말속도·‘복구됐어요’ 말끝을 비교해 E2를 선택했다. 안내 음성5개를 이 음색으로 제작해 PR에 포함하도록 요청했다. PM이 요청한 #134 출력 복구와는 별도 PR이며, **새 음원 승인·실기 확인 전 운영은 기존 Supertonic을 유지**한다. 파일명·PM 문구·alert 매핑·재생 로직·음성노드·계약·Core는 바꾸지 않는다. CAUTION·WARNING은 PM 제작 원본 그대로다.

[문서] **Typecast를 사용하지 않는 것으로 결정해 Typecast 무료 플랜의 이용 제한 문제는 이번 교체안에서 해소했다.** Typecast API·출력물·참조음을 사용하지 않았다. 업체 문의나10/27 회신 대기를 이 교체안의 조건으로 두지 않는다. 이전 Typecast 음색의 검사 결과도 이번 WAV의 검사로 취급하지 않는다.

## 제작과 라이선스

- [문서] 음성5개는 **AI 합성 음성**이다. 사람이 녹음한 팀원 음성이나 기존 상용 TTS 음성을 복제하지 않았다.
- [문서] Q3의 원 음색은 [Qwen3-TTS 1.7B VoiceDesign](https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign)으로 직접 생성했다. 선택된 E2의 복구 안내를 보존하고 DANGER 두 문장은 같은 합성 샘플에서 분리했다. CAT_LOCK·CMD_REJECTED·OFFLINE은 [Qwen3-TTS 1.7B Base](https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-Base)에 자체 생성 참조음을 주어 만들었다. ‘확인해 주세요’ 누락 가능성을 줄이기 위해 거절 안내의 두 번째 문장을 별도로 합성해 연결했다.
- [문서] 공식 코드·모델은 Apache-2.0이다. [공식 코드 라이선스](https://github.com/QwenLM/Qwen3-TTS/blob/main/LICENSE)를 `flutter_app/assets/audio/MODEL-LICENSE.txt`로 보존한다. 이전 Supertonic 모델 라이선스가 새 Qwen 음원에 적용되는 것으로 남기지 않는다. 원 모델과 Mac 변환 모델의 출처·revision을 `manifest.json`에 기록한다.
- [문서] Mac 제작에는 mlx-audio0.6.1(MIT)·MLX8bit 변환 모델을 썼다. 모델·실행 도구는 앱에 넣지 않으며 Pi5는 PCM WAV만 재생한다. 기존 앱 런타임 의존성은 늘지 않는다.
- [실측] OFFLINE_RECOVER는 선택 E2의 첫 문장을 그대로 사용했다. 다른 문장은 음절을 잘라 강제로 늦추는 대신 자체 합성 참조와 약한 말끝 세기 조정을 사용했다. 파일별 제작 방법과 원본 참조 해시는 manifest에 남긴다.

## 파일과 형식

[실측] 전부48kHz·mono·PCM16 WAV, 유한 샘플·클리핑0, SHA256 대조를 통과했다. 차임2개의 해시는 기존 main 파일과 같다. 음성은 −19 LUFS 목표로 맞췄으며 선택 E2 보존본과 차임의 음량은 유지했다.

| 파일 | 길이 | SHA256 |
|---|---:|---|
| `DANGER.wav` | 6.47 s | 2c0332f28ab6bb8cfd8154ca8ba665fa33111ff2681a0ce26c35757aba5e39c9 |
| `CAT_LOCK.wav` | 4.88 s | 89032ca343ba8de047affd2257fbe11ea5fc77cbba697ae3adf21ae5d2f1dc94 |
| `OFFLINE_RECOVER.wav` | 2.40 s | 3fc04335eaac6d00e4c9d1be816ff9c1f825a6d79397bb0b3f13ecf2d5bd3420 |
| `CMD_REJECTED.wav` | 5.98 s | 07f9656943075c56db75746b8b559c0e028a838c7f100e0cb9076eafcd62c7ee |
| `OFFLINE.wav` | 6.32 s | a31ed3ee7c6e96b1086ae95d00957c00ea6bb95ce7ef97a73979da795369935a |
| `CAUTION.wav` | 0.96 s | e19d3deb7ccfd757bc98680dd76502523279fa63b50a2a96881513c620994b30 |
| `WARNING.wav` | 1.04 s | 3e9121f2cd0022c9f357d2aba4ebfc5d97b9587c5e59573221bb5cdc91facfa2 |

## 자동 검사와 한계

[실측] 새 최종 WAV7개 전체를 Mac의 기존 SenseVoiceSmall INT8·ko·ITN·CPU4로 전사하고, 이번 PR 기준의 실제 C++ 하이엘지 분류기에 입력했다. **7/7 DROP, 실행 intent 후보0**이었다. 이 과정은 파일 검사이며 ReSpeaker 재생·실시간 VAD·Core 송신을 실행하지 않았다.

| 파일 | 자동 전사 | 분류 |
|---|---|---|
| `DANGER` | 화재 위험 을 미리 감 지에 인덕션 을 꿨 어요 후드 를 최 대로 돌 리고 있요. | DROP |
| `CAT_LOCK` | 반려동 물이 감지돼 안전 잠 금이 설정 됐 어요. | DROP |
| `CMD_REJECTED` | 지 금은 요청을 처리 할 수 없어요 화의 안내를 확인 주세요. | DROP |
| `OFFLINE` | 안전 장치치에 응답이 없어요 연결 상태를 확인해 주세요. | DROP |
| `OFFLINE_RECOVER` | 장치 상 태가 복구 됐 어요. | DROP |
| `CAUTION` | 그. | DROP |
| `WARNING` | 그. | DROP |

[문서] 자동 전사는 원문 발음의 정답 판정이 아니다. 조사·어미 표기 차이와 일부 누락이 있어 사람의 청취 확인이 남는다. 차임도 짧은 음절로 환각됐지만 활성어가 없어 DROP이었다. 파일 전체 입력0건이 실제 스피커→마이크0건을 보장하지 않으며, 이전 Typecast70회 결과를 소급하지 않는다.

## 앱 검사와 종료 조건

- [실측] WAV7개 형식·해시·차임 불변·클리핑, 새 자동 전사/실제 분류기 입력 검사를 완료했다.
- [실측] 기존 Flutter 3.47.7 / Dart 3.13.5로 pub get, format(26 files/0 changed), analyze(No issues found), 기존 test85개, Web build를 통과했다. Web bundle에 들어간 WAV7개도 현재 파일 해시와 일치했다. 기존 CupertinoIcons 글꼴 경고는 별개이며 음원 파일 외 앱 실행 로직·의존성은 바꾸지 않았다.
- [실측] [PM 청취 승인](https://github.com/lambdaramji/HOOD-SEEK/pull/137#issuecomment-6396473963)으로 음성 5개의 음량·발음·끝맺음을 확인했다. 이후 WAV는 변경하지 않았다. 아래 Pi5 재유입·실제 앱 시험 결과는 담당자의 최종 검토를 요청한다.
- [문서] #134는 출력 복구 전용이다. 새 음원은 별도로 검토하며 승인 전 Pi5 운영 파일을 교체하지 않는다. 자동 병합·기기 배포·음성 발행은 하지 않는다.

## Pi5 ReSpeaker 재유입 70회 — 2026-10-25

[실측] 음원 기준은 이 PR `6ede7238a9f2ac03f36a291d3aef264d607ad8b6`의 WAV 7개이며 위 SHA256 표와 같다. Pi5에 이미 배치된 `55254b7` 음성 실행본을 관측 전용으로 사용했다. `--publish`·Core 접속 없이 실제 ReSpeaker 스피커 출력→같은 장치 마이크 입력→실시간 VAD→ASR→분류기로 평가했다. 음원 앞뒤에 시험용 무음을 붙이거나 파일 자체를 ASR에 직접 넣은 결과가 아니다. 기기 음량 100%·기본 `pulsesink`·USB 활성 출력을 유지했다.

| 고정 항목 | SHA256 |
|---|---|
| voice 실행본 | `1c38613a7bf12c612c32a85b8c434863b2b4bf1f08e75f166beff4526ac8f1fc` |
| SenseVoiceSmall INT8 모델 | `c71f0ce00bec95b07744e116345e33d8cbbe08cef896382cf907bf4b51a2cd51` |
| tokens | `f449eb28dc567533d7fa59be34e2abca8784f771850c78a47fb731a31429a1dc` |
| Silero 모델 | `2aa269b785eeb53a82983a20501ddf7c1d9c48e33ab63a41391ac6c9f7fb6987` |

[실측] 실시간 설정은 SenseVoice ko/ITN/greedy·ASR CPU4·VAD CPU1·시작 224 ms·종료 544 ms·최대 발화 4 s, 호출어 0.60·대상 0.60·동작 0.70·후보 차이 0.10으로 유지했다. 파서·별칭·문턱·실행본은 바꾸지 않았다.

| 집계 | 결과 |
|---|---:|
| 완전 재생 | 70회, 각 파일 10회 |
| 실행 intent | **0** |
| 실제 voice 송신 | **0** |
| ASR 결과 | 36건: DROP 35, NONE 1 |
| 노드 원래 candidates 카운터 | 1 — NONE도 포함하는 값 |
| VAD 최대 4 s 초과 폐기 | 13건 |
| 완전 재생마다 USB Playback Running·ReSpeaker 입력 확인 | 70/70 |
| 출력 스트림 수동 이동·음량 변경 | 0 |

[실측/범위] 70회는 연속 한 번이 아니라 **62회 완료 + 추가 8회 완료**다. 첫 구간 63번째 OFFLINE은 라우팅 확인 단계에서 중단돼 완료 마커가 없어 제외했고 원래 증거를 보존했다. 예비 재생도 분모에 넣지 않았다. 이 중단 원인을 특정 외부 프로세스 탓으로 확정하지 않는다. NONE 1건은 활성어만 있고 대상·동작이 없어 실행 intent가 아니었다. 원래 카운터를 0으로 고쳐 쓰지 않는다.

| 녹음 구간 | 길이 | 원음 SHA256 |
|---|---:|---|
| 첫 구간, 완료 62회 | 392.72 s | `160fe864737b46d9e066591719000e001726f7d8b137536b3b3aafeb82b409b2` |
| 추가 구간, 완료 8회 | 57.67 s | `759b424752884c259b4f2846e463cfcb4928e6b9da08c9e8914c0219c63401e9` |

[실측/제한] 기본 입력 이름과 음성노드의 ReSpeaker 지정이 있어도 실제 Pulse 녹음 스트림은 다른 USB 웹캠 마이크에 연결됐다. 두 USB 입력이 AudioManager의 같은 논리 입력으로 묶이는 현상을 관측했다. **시험 소유 녹음 스트림만 ReSpeaker로 옮긴 뒤** 매 재생 경로를 확인했다. 운영 서비스·전역 기본 입력은 수정하지 않았다. 따라서 이는 확인된 ReSpeaker 경로의 재유입 성적이며 **운영 노드의 자동 입력 라우팅이 해결됐다는 근거는 아니다.** AudioManager 입력 선택 원인은 정인·음성 담당 후속 검토로 남긴다. 시험 중 음원 본체 RMS는 −22.60~−12.42 dBFS였다.

[문서] 원음·개별 전사·기기 식별자·주소·개인 경로는 저장소에 추가하지 않는다. 집계·해시만 기록하며, 이전 음색의 70회 결과로 소급한 것이 아니다.

## 실제 Flutter 앱 재생·중단 — 2026-10-25

[실측] main `2cb1a6f88c267214604aa961845a3d630cae0bf4`의 기존 Flutter 소스·재생기를 그대로 재사용했다(#121·#136 포함). ATLAS Flutter 3.27.4 / Dart 3.6.2의 analyze·98개 test·ARM Release 빌드를 통과했다. 공식 ATLAS runner는 저장소 밖에서 생성했으며 앱 로직·계약·Core·프로파일을 변경하지 않았다.

[실측] AppManager가 Core 미연결 데모 앱을 일반 앱 계정으로 실행했다. 그 데모 앱의 음원만 임시 교체해 위 7개 해시와 대조했다. 기본 `pulsesink`의 실제 스트림이 ReSpeaker에 연결되고, 앱이 asset에서 푼 CAUTION·DANGER·OFFLINE 해시도 후보와 일치했다. 사용자가 다음 세 소리를 현장에서 모두 확인했다.

| 리허설 입력 | 실제 재생 | 확인 |
|---|---|---|
| 중불 조리 | CAUTION 차임 | 사용자 청취·USB 스트림 |
| 화재 위험 차단 | DANGER 음성 | 사용자 청취·USB 스트림 |
| 연결 끊김 | OFFLINE 음성 | 사용자 청취·USB 스트림 |

[실측] 별도 시험에서 DANGER가 말하는 도중 중불 조리를 누르자 위험 안내가 끝까지 나오지 않고 끊긴 뒤 차임이 들렸다는 사용자 확인을 받았다. 프로세스 관측에서도 DANGER는 약 2.1초에 종료되고 CAUTION으로 전환됐다(원 파일 6.47초). 표본 간격 약 0.15초이므로 정밀한 소리 지연 측정으로 쓰지 않는다. 실제 앱 경로를 사용했으며 별도 터미널 재생 명령의 성공으로 대체하지 않았다.

[문서] ‘잠금 중 음성 거부’ 데모 버튼은 `state.voice`만 바꾸고 `alert=CMD_REJECTED`를 주지 않아 소리가 안 난다. 이를 누르라는 최초 시험 안내를 정정했다. 실제 Core 거부 이벤트까지 시험했다거나 위험 안내가 항상 끝까지 재생된다는 주장은 하지 않는다. 기존 규칙은 새 클립이 이전 클립을 끊는 방식이다.

[실측] 데모 앱을 종료하고 원래 음원·권한을 복원해 기존 SHA256SUMS 7개를 다시 대조했다. 운영 앱의 Supertonic 음원과 voice 발행은 그대로다. 새 음원 운영 교체는 CI·담당 리뷰·PM의 범위 확인 뒤 별도 배포로 진행한다.

[문서] **남은 종료 조건**: 재범의 실제 앱 결과 검토(#121·#136), 정인의 출력/입력 경계 확인, PM의 62+8회·임시 입력 경로 조건을 포함한 시험 수용과 운영 교체 판단, 새 head CI·요청 리뷰어 전원 승인. 10/26 E2E 3회(#71) 전에 이 조건이 끝나지 않으면 PM 결정대로 Supertonic을 유지한다.

## Ponytail

[문서] 기존 WAV 파일명·PM 문구·차임·GStreamer 재생기·Flutter 검사를 재사용했다. 새 런타임 의존성·추상화·설정은 없다. 새 manifest1개는 모델·제작·해시 추적에 필요하고 보고서1개는 출력 PR과 구분해 검증 범위와 승인을 남기기 위해 필요하다. 안전·보정·입력 검증은 줄이지 않았다.
