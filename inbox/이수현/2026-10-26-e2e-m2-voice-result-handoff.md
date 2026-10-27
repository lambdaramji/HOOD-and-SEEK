# E2E 2회차 M2 결과 안내 누락·음성 담당 인계

작성자: 이수현
작성일: 2026-10-26
상태: draft
카테고리: 실측·검토 요청
대상: #67 E2E 2회차, 배포 main dec8005

## 결론

- [실측] M2 약 22:51:17의 인덕션 1 → 2단 변경은 Core에서 확인된다. 이전 음성 결과(22:39:31)와 last_intent=IND_UP·value=null·accepted=true·reason=null이 같았고, 앱 기록 DB에는 두 번째 음성 결과 이벤트가 없다. [정인 현장 확인](https://github.com/lambdaramji/HOOD-SEEK/issues/67#issuecomment-6418061358)도 결과 안내 누락 1건을 보고했다.
- [문서] 앱은 lastIntent·value·accepted·reason만 비교한다. 요청 식별자가 없으므로 시간이 오래 지나거나 중간에 터치 조작이 있어도 같은 voice 결과면 새 시트·결과 안내를 만들지 않는다. 음성노드만 고쳐 해결할 수 있는 범위가 아니다.
- [실측] 배포 소스 dec8005의 기존 TCP 격리 시험은 PASS다. 같은 문장도 새 발화면 전송하고, 같은 연결에서는 seq가 증가하며 재연결 때 hello 0부터 다시 시작한다. 이번 E2E 모든 패킷의 원문 seq가 보존된 것은 아니므로 현장 패킷 전수 확인으로 표현하지 않는다.
- [범위] 이번 PR은 수현 담당 문서만 추가한다. Core·Flutter·계약·음원·음성노드 실행 코드는 수정하지 않고 운영 장비를 재빌드·재배포하지 않는다. M2 누락을 수정 완료로 처리하지 않는다.

## 실행본과 분석 범위

[실측] 세 실행본은 모두 main dec8005f459b5843599a9b85c46f5b0d85af4015에서 마스터 PC atlas-dev로 빌드했다.

| 대상 | sha256 앞 16자 | 확인 |
|---|---|---|
| Pi 5 앱 libapp.so | 2d951149884cb764 | state v1~4 수신, 결과 안내 12개 포함 |
| Pi 4 Core safetycore | 55cc7af6a1489ad8 | state v4, 기존 안전 집행 경로 |
| Pi 5 voice-node | 7ab0f2b384a9006c | ReSpeaker 실입력, 22:29:19부터 publish |

[실측] Core 상태·앱 기록 DB·남아 있는 Pi 5 재생/음성 journal을 읽기 전용으로 수집했다. 운영 DB의 사본은 integrity_check=ok였다. IP·기기 식별정보·개인 경로·원음·개별 전사·Core 원문은 이 PR에 넣지 않는다.

- 공식 E2E 2회차: 22:31:34–22:57:59. 정인님의 #67 기록과 대조한다.
- 전체 확인 범위: 새 voice 발행 이후부터 23:05:30까지. 22:58 이후 추가 조작은 공식 E2E 성적과 분리한다.
- 보존 한계: 분석 때 Pi 5 journal에는 약 22:58:26 이후만 남았다. 따라서 그 이전 스피커 재생을 journal만으로 전수 증명할 수 없다. Core·앱 DB·현장 청취 기록을 구분한다.

## 음원이 나와야 할 때와 실제 근거

| 구간 | 기대·조건 | 확인 결과와 근거 |
|---|---|---|
| M4·M5 22:34:46–22:37:01 | 후드 3·2·1단, 팬 끄기, 인덕션 켜기·올리기·끄기·켜기·끄기 | [실측] 9개 Core 수락·설정 변경과 앱 음성 이벤트 확인. [현장] 정인님이 9개 결과 안내 정상 보고 |
| 22:36:05 비명령 | NONE·NO_INTENT·alert 없음 → RESULT_UNSUPPORTED | [실측] Core/앱 기록 일치. [현장] 미지원 안내 정상 보고 |
| 22:34:07·22:37:35 잠금 중 켜기 | LOCKED 거부·CMD_REJECTED | [실측] 거부/alert 확인. [현장] 거부 안내 확인 |
| M2 약 22:51:17 | IND_UP으로 1 → 2단 뒤 결과 안내 | [실측] Core 단수 변경, voice 네 필드 불변, 새 앱 음성 이벤트 없음. [현장] 누락 확인 |
| 22:43:50·22:53:41·22:56:43 | CAT_LOCK·WARNING·DANGER 전이 | [실측] Core alert 각각 1회. 초반 재생 journal이 남지 않아 전체 청취/완주를 독립 확정하지 않는다. 정인님은 WARNING 차임을 현장 확인 |
| M13 22:45 | state 끊김 뒤 OFFLINE | [실측] 앱 DB 연결 끊김 22:45:29, 복구 22:45:42. [현장] OFFLINE 정상 보고 |
| 23:01–23:03 추가 조작 | 수락 9개·NONE 2개에 결과/미지원 안내 | [실측] 전송 11개와 대응 WAV 11개 실행·파일 읽기 흔적이 모두 맞는다. 결과 준비→재생 시작은 약 0.08~0.15초. 실제 청취·끝까지 재생 보장은 별개 |

[문서] M1/M2/M3의 정상 조리 low·mid·high는 alert=NONE이므로 주의/경고음을 내지 않는다. WARNING은 warning 진입 때다. M8 자리 비움 보호와 M10 위험 잠금 해제에도 전용 결과 TTS가 연결되어 있지 않다. 이때 조용한 것을 재생 실패로 세지 않는다. OFFLINE_RECOVER도 Core의 해당 alert가 있을 때이며 모든 잠금 해제·UI 재접속에 자동으로 내는 소리가 아니다.

[실측] 추가 조작에서 팬 끄기가 안전 하한 2단에 걸린 때 RESULT_FAN_MIN_2가, 중풍 요청이 안전 하한 3단에 걸린 때 RESULT_FAN_MIN_3가 실행됐다. 실제로 꺼지지 않은 팬을 꺼졌다고 안내하는 패턴은 발견하지 못했다.

## M2 원인과 음성노드 seq 확인

[문서] [dashboard_screen.dart](../../flutter_app/lib/screens/dashboard_screen.dart)의 _sameVoice는 네 필드만 비교한다. _compare가 새 voice로 판단해야 pending 결과와 시트가 만들어진다. 단수 변화만으로 음성 요청을 추정하면 터치·자동 안전 제어까지 음성 결과로 오인할 수 있어 이 보고서에서 보완 코드를 넣지 않았다.

[문서] [voice_tcp.cpp](../../voice_node/src/voice_tcp.cpp)는 성공한 send_line 뒤 next_seq_를 증가시키며, 새 TCP 연결의 hello 뒤에는 1부터 시작한다. [voice_tcp_receiver_test.py](../../voice_node/tests/voice_tcp_receiver_test.py)가 수신 seq 순서를 직접 검사한다.

[실측 10/26, Mac 격리 빌드·루프백] 배포 당시 승인 소스를 별도 추출해 기존 TCP 시험 실행본만 빌드했다. 33개 시나리오 PASS, voice 51개·hello 15개를 수신했다. new-utterance-same-text·reconnect-new-seq·재전송 방지 포함이다. 운영 Core·마이크에는 시험 명령을 보내지 않았다.

~~~sh
c++ -std=c++17 -O2 -pthread -I voice_node/src \
  voice_node/tests/voice_tcp_test_client.cpp \
  voice_node/src/voice_tcp.cpp voice_node/src/command_parser.cpp \
  -o voice_tcp_test_client
python3 voice_node/tests/voice_tcp_receiver_test.py ./voice_tcp_test_client
~~~

[한계] 현재 음성 로그에는 모든 송신 wire seq가 남지 않는다. 이번 확인은 코드·격리 수신 시험·Core 집행 결과를 근거로 하며, 실제 E2E 패킷별 seq 목록을 확보했다는 뜻이 아니다.

## 정인·재범·PM께 요청

- 정인(Core·계약): state.voice에 요청을 구분할 값을 보존하는 계약안을 검토해 주세요. 수락·거부 모두 새 요청을 구분할 필요가 있습니다. 수신 voice.seq는 연결이 바뀌면 초기화되므로, 그 값만 추가할 경우 연결 구분도 함께 검토해야 합니다.
- 재범(앱): 합의된 식별자를 _sameVoice와 pending 결과 안내 판별에 반영하는 후속 PR을 검토해 주세요. 기존 최종 단수·ACK·경고 우선·3초 만료 조건은 유지합니다.
- PM: state v4 → v5 등 계약 버전과 동결 뒤 적용 시점을 결정해 주세요. 위 항목은 제안이며 이번 문서 PR이 계약을 확정하지 않습니다.
- 당장 시연: 직전 음성 명령과 같은 명령을 다시 써야 하면 사이에 다른 음성 명령을 넣는 대본으로 구성합니다. 터치 조작이나 긴 대기만으로 last voice가 바뀌지는 않습니다.

## 추가 관측과 미확인

- [실측] 남아 있는 22:58–23:05 음성 로그에는 ASR 30건·4초 최대 발화 초과 5건이 있다. 대화 포함 구간이며, 이를 명령 실패 5건이나 공식 인식률로 집계하지 않는다.
- [문서/실측] 숫자 단수는 설정 동사가 필요하다. 추가 조작의 숫자 단수만 있는 전사는 NONE이었고 미지원 음원이 실행됐다. 새 IND_SET의 정상 설정 문장 실발화 합격을 이번 기록으로 주장하지 않는다.
- [실측] TOUCH_FEEDBACK 실행 흔적은 남은 구간에서 4회다. 파일은 60 ms, peak 약 -24.4 dBFS·RMS 약 -37.1 dBFS로 짧고 작다. 현장에서 안 들렸다는 체감이 있으면 호출 누락과 가청성은 별도 확인해야 한다. 요청 없이 확정 음원의 gain은 바꾸지 않았다.
- [실측] 새 음성노드의 NRestarts=0과 ReSpeaker 실행본 해시를 확인했다. 22:45 연결 재접속은 #67의 의도된 네트워크 차단 시험과 일치한다.
- [미확인] 회전된 초반 재생 journal과 스피커의 실제 음압·모든 클립의 완주 여부는 확인하지 못했다. 추가로 확정한 결과 안내 누락은 없지만, 전 구간 무누락으로 표현하지 않는다.

## 근거

- [정인 E2E 2회차·현장 청취·M2 분석](https://github.com/lambdaramji/HOOD-SEEK/issues/67#issuecomment-6418061358)
- [수현 배포·백업·env·프로파일 실측 기록](https://github.com/lambdaramji/HOOD-SEEK/issues/67#issuecomment-6418289933)
- [#176 기존 결과 안내와 요청 식별자 한계](https://github.com/lambdaramji/HOOD-SEEK/pull/176)
- [음원 조건표](../../flutter_app/assets/audio/results/README.md)
