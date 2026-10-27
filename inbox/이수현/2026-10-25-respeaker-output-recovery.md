# ReSpeaker 출력 복구·재부팅 유지 검증

- 작성자: 이수현
- 작성일: 2026-10-25 (KST)
- 상태: draft
- 카테고리: deployment, flutter-audio
- 관련 이슈/PR: #133, #134, #121, #136
- 협업: 정정인(AudioManager 단서·전환 메서드), 임재범(Flutter 재생기)

## 현재 결론과 범위

- [실측] ATLAS 활성 출력이 HDMI였으며, 정규 메서드로 USB를 선택하자 장치 미지정 GStreamer도 ReSpeaker로 재생됐다. 사용자 허용 재부팅 1회 뒤에도 USB 선택과 재생 3종이 유지됐다.
- [문서] #134는 출력 진단·수동 복구 절차만 다룬다. 음원 교체·새 음원 사용 승인은 별도 PR이다. 이 PR의 WAV7개는 기존 Supertonic 음성5개·PM 차임2개와 바이트 단위로 같다.
- [문서] **Typecast를 사용하지 않는 것으로 결정해 Typecast 무료 플랜의 이용 제한 문제는 이번 교체안에서 해소했다.** Typecast 파일·API·참조 음성을 반입하지 않는다. 별도 Qwen Q3 E2 후보는 공개 모델 라이선스와 새 파일 검증으로 검토한다. 운영 음원은 새 교체안 승인 전까지 Supertonic을 유지한다.
- [실측] 실제 AppManager 앱 계정의 Flutter 데모에서 경고 이벤트→asset 추출→ReSpeaker 재생과 이전 소리 중단을 추가 확인했다(§2). 사용자가 현장 청취를 확인했다. #121·#136 담당자의 최종 검토는 별도이며 앱 시작 시 자동 D-Bus 호출은 추가하지 않았다.

## 1. 출력 문제의 관측과 수정

[실측] 같은 PM 제작 `CAUTION.wav`로 비교했다. 아래는 재생 프로세스의 종료코드만이 아니라 실제 sink-input과 USB Playback 상태를 확인한 결과다.

| 조건 | 재생 명령 | 실제 출력 | USB Playback |
|---|---|---|---|
| 전환 전 | ReSpeaker 지정 `pulsesink` | HDMI | Running 없음 |
| 전환 전 | ReSpeaker 지정 `paplay -d` | HDMI | Running 없음 |
| 전환 전 | ReSpeaker 지정 + `client-name=hoodseek-alert` | HDMI | Running 없음 |
| USB 전환 후 | 장치 미지정 `pulsesink` | ReSpeaker | Running 확인 |
| USB 전환 후 | ReSpeaker 지정 `pulsesink` | ReSpeaker | Running 확인 |
| USB 전환 후 | 기본 `paplay` | ReSpeaker | Running 확인 |
| 재부팅 후, 추가 전환 호출 없음 | 위 재생 3종 | 모두 ReSpeaker | 모두 Running 확인 |

[실측] 비교 명령은 모두 종료코드0이었다. 전환 전에는 처음 관측 가능한 스트림부터 HDMI였다. 프로그램 이름 변경은 `application.name`과 restore ID에도 반영됐다. restore 저장 항목 뒤에 ReSpeaker 문자열이 보여, 단순한 프로그램별 HDMI 저장값 가설은 확정하지 않았다. 이때는 audio-manager의 장치 선택 원인도 미확정이었다.

[실측] 정인님 제안을 실제 D-Bus introspection으로 대조했다. `OutputDevices`에 USB 스피커가 있고, `SetActiveOutputDevice` 메서드의 입력 시그니처가 `o`임을 확인했다. 전환 전후 속성 출력은 다음과 같다.

```text
o "/com/atlas/AudioManager1/Device/hdmi_speaker"
o "/com/atlas/AudioManager1/Device/usb_speaker"
```

[실측] 위 메서드로 USB를 선택한 뒤, 개별 `move-sink-input` 없이 일반 재생이 USB로 바뀌었다. Pi5 부팅 식별자가 바뀐 것을 로컬에서 확인했고, 재부팅 후에도 두 번째 속성 값이 유지됐다. 기기 식별자는 Git에 넣지 않는다.

[추정] 이번 환경의 원인은 ATLAS 활성 출력 선택 계층으로 좁혀졌다. 변경 직후와 재부팅 뒤의 재현 결과가 근거다. audio-manager 내부 코드의 모든 라우팅 분기까지 검증한 것은 아니다. 다른 이미지에서는 같은 절차로 재현·전환 전후를 다시 확인한다.

[실측] 기기 음량·모듈·서비스 설정과 앱 코드는 바꾸지 않았다. 재부팅 전후 voice 서비스는 inactive·disabled였다. Core·마이크는 이 출력 비교에서 사용하지 않았다. 재부팅으로 켜진 Wi-Fi 절전만 원래 off 상태로 복원했고 화면은 1920×1080이었다.

## 2. 실제 Flutter 앱 확인과 종료 지점

- [실측 2026-10-25] main `2cb1a6f88c267214604aa961845a3d630cae0bf4`의 Flutter 코드를 ATLAS Flutter 3.27.4 / Dart 3.6.2로 ARM Release 빌드했다. analyze·기존 test 98개를 통과했다. Core 미연결 데모 앱을 AppManager로 실행해 앱 계정에서 재생했다. 운영 Core·프로파일·voice 발행은 바꾸지 않았다.
- [실측] 데모 앱에만 #137 `6ede723`의 WAV 7개를 임시 배치하고 해시를 대조했다. 중불 조리→CAUTION, 화재 위험 차단→DANGER, 연결 끊김→OFFLINE이 실제 ReSpeaker에서 들렸다는 사용자 확인과 기본 `pulsesink`의 USB 연결을 함께 확인했다. 앱이 풀어 낸 CAUTION·DANGER·OFFLINE의 해시도 후보 파일과 같았다. 운영 앱의 기존 Supertonic 파일은 교체하지 않았다.
- [실측] 별도 중단 시험에서 DANGER 재생 도중 중불 조리를 누르자 DANGER 자식 프로세스가 약 2.1초에 종료되고 CAUTION으로 바뀌었다. 원 파일 6.47초보다 짧고, 사용자가 실제 음성 중단·차임 전환을 확인했다. 약 0.15초 간격 프로세스 관측에 따른 근삿값이며 정밀 오디오 지연값으로 쓰지 않는다. 먼저 진행한 순차 재생만으로 중단 통과를 주장하지 않았다.
- [문서] ‘잠금 중 음성 거부’ 데모는 voice 거부 상태만 주고 `alert=CMD_REJECTED`를 발생시키지 않아 소리가 나지 않는다. 이 버튼을 소리 중단 시험에 쓰라는 최초 안내를 정정했다. Core의 실제 CMD_REJECTED 연동 시험을 마쳤다는 뜻은 아니다.
- [실측] 시험 뒤 데모 앱을 종료하고 원래 음원·권한을 복원했으며 원래 SHA256SUMS 7개를 모두 대조했다. 실제 운영 앱 음원은 그대로다.
- [문서] 재범: 위 실측 근거와 #121·#136 동작 범위를 최종 검토한다. 새 경고는 위험도와 관계없이 이전 소리를 끊는 기존 규칙이다. ‘위험 안내를 항상 끝까지 재생’하는 우선순위를 새로 추가한 것은 아니다.
- [문서] 정인: AudioManager 조회·USB 전환·되돌리기 절차와 재부팅 결과를 검토한다.
- [문서] 이수현: 출력 복구와 음원 교체를 별도 PR로 유지한다. #134는 `Refs #133`으로 연결하며 이슈를 자동 종료하지 않는다.
- [문서] 새 음원을 승인·배포하기 전까지 기존 Supertonic/PM 파일을 운영한다. 새 후보의 과거 검사나 다른 음색의 7개×10회 검사를 현재 운영 파일의 검사로 소급하지 않는다.
- [문서] 요청 리뷰어·CI를 확인한 뒤 병합을 판단하며 자동 병합은 켜지 않는다. 이번 작업으로 Core·voice 발행을 켜지 않는다.

[문서] 재부팅 뒤 `hoodseek-vision-node`의 active 상태도 확인한다. inactive이면 담당자와 사용 상태를 조정해 수동 start한다. presence unknown으로 인덕션 켜기가 거절되는 경우 안전 허가를 우회하지 않는다. 배포 안내에 반영했다(#67).

## 3. 검증

- [실측] 기존 Flutter 3.47.7 / Dart 3.13.5로 pub get, format(26 files / 0 changed), analyze(`No issues found!`), test85개, Web build를 통과했다. Web build의 기존 CupertinoIcons 글꼴 경고는 이 문서 변경과 별개다.
- [실측] 현재 폴더 WAV7개의 SHA256·48 kHz·mono·PCM16 형식을 대조했다. 이번 PR의 WAV 변경은0개이며 재생 로직·의존성도 변경하지 않았다.
- [문서] 후속 수정은 범위 분리와 문서만 변경했다. 상대 링크·민감정보 제외·`git diff --check`를 확인한다. 새 head CI는 별도 확인한다.
