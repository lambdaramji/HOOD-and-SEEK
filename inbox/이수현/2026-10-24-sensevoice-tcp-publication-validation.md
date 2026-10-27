# SenseVoice 실제 TCP 송신 구현과 최신 실측 대조

- 작성자: 이수현 (작성 지원: Codex)
- 작성일: 2026-10-24 KST
- 상태: draft
- 카테고리: voice-node · TCP 송신 · 배포 · 정책 검토
- 관련: #62, Draft #107, 임재범 `d28e792` 재검토

## 변경 결과

[구현] 관측 전용이던 SenseVoice 분기에 기존 `VoiceTcpClient`를 연결했다. 명시적 수신기와 `--publish`가 있으면 하이엘지 판정 결과를 기존 TCP v1으로 보낸다. 실행 경로는 `ReSpeaker/PulseAudio → Silero VAD CPU1 → 상주 SenseVoice CPU4 → 호출어/명령 전체 판정 → 입력·기한 검사 → TCP voice → Core 판정`이다. 기본 발행 끔·재부팅 후 수동 기동은 유지한다. 실시간 웹 도구는 별도 관측 도구이며 실제 송신을 자동으로 켜지 않는다. 근거: `audio_observer.cpp`, `voice_tcp.cpp`, 배포 유닛/env.

| 항목 | 현재 구현 |
|---|---|
| 후드/하이엘지 | 기존 Whisper/Hood와 새 SenseVoice/HiLg를 명시적으로 분리. 후드 별칭 목록을 새 호출어 목록으로 재사용하지 않음 |
| DROP | 호출어 미확인·UTF-8/200자/제어 문자 검증 실패는 미전송 |
| NONE | 활성어가 있으나 부정·질문·인용·복합·미지원 문장인 경우 value 없이 송신 |
| 지원 명령 | FAN_SET(1..3), FAN_OFF, IND_OFF, IND_ON, IND_UP, IND_DOWN; Core가 허용·거부 |
| 풍량 없는 후드/팬 켜기 | 관측 후보는 FAN_SET(1), **송신에서는 NONE**. 이미2·3단인 팬을1단으로 낮추지 않도록 방향/켜기 계약 전 보류 |
| 목표 단수+올려/내려 | 기존 v1에 방향 필드가 없으므로 계속 NONE. 음성노드가 과거 state로 허용 여부를 판단하지 않음 |
| SenseVoice 점수 | ASR conf 미제공. wire conf=0은 미제공 표기 제안이며 자모 유사도·정답 확률 아님. Whisper 점수/fallback 옵션은 거부 |
| 전송 보장 범위 | 발화당1회·큐/실패 재전송 없음·최대250ms 쓰기·VAD 추정 종료+5초 만료·state 줄3초 무수신 차단·Linux TCP_USER_TIMEOUT 2초 |
| 로그 | 최초/최종 문장·역할 유사도·후보 intent·wire intent·sent/expired/disconnected·경과 시간 분리 |

[구현/문서] `CONTRIBUTING.md`의 voice_node CI 없음 문구를 System CI의 모델 없는 파서/VAD/TCP 검사 안내로 수정했다. 배포 유닛은 SenseVoice를 사용하며 `SENSEVOICE_MODEL_DIR`·`VAD_MODEL`·`LD_LIBRARY_PATH`를 요구한다. 이전 Whisper 유닛을 env만 그대로 둔 채 교체하지 않도록 모델/런타임 배치 → env 수정 → 관측 실행 확인 → 유닛 교체/verify/daemon-reload → 수동 start 순서와 복원을 적었다. 운영 파일·서비스를 자동 교체/시작하지 않았다.

## 최신 실발화·배경음 근거

[실측] 아래는 **송신 연결 전 관측 구성의 실제 입력 결과**다. 새 TCP 구현의 하드웨어 집행률로 바꾸지 않는다.

| 실제 시험 | 결과·범위 |
|---|---|
| 수현 필수5종×5회 | 최초24/25=96%, 사람 재시도1건 제외 |
| 비명령30개 | 실행 후보0/30, DROP15·NONE15 |
| 전체55문항 일치 | 54/55=98.18%, 전사 WER/CER가 아님 |
| 필수 VAD 추정 종료→판정 | 765~1087ms, 평균957.24ms. 실제 원음 끝 시각·송신 시각 시험과 구분 |
| 실제 배경300초 | 조용150초·팬/유튜브150초에서 ASR0·실행 후보0. 별도 두 실행에서150초씩 선택 |
| 배경 입력 레벨 | 각각-45.7977547/-27.2678797dBFS, 비영 샘플·clipping 확인. 디지털0 입력을 성공으로 세지 않음 |

근거: [실시간55문항](2026-10-24-hi-lg-live55-validation.md), [원문 문항별 대조](measurements/2026-10-24-hi-lg-live55-trials.jsonl), [300초 실측](2026-10-24-voice-ambient300-validation.md). 이전의 “5분 생략 승인 전달”은 이후 실제 측정으로 대체한 경위이며 현재 결과의 승인 근거로 쓰지 않는다. 단일 연속300초 시험이나 남현지 지정 화자 공식 NFR-08 통과를 주장하지 않는다.

## 새 송신 구현 검증

[실측] [구조화 결과](measurements/2026-10-24-sensevoice-publication.json)와 [이벤트 로그](measurements/2026-10-24-sensevoice-publication-events.log)에 새 실행본·입력/소스 해시·wire·Core state를 남겼다. 개인 경로만 일반 표기로 치환했으며 원본 로그·WAV·모델·빌드 산출물은 저장소 밖에 보존한다.

| 검사 | 결과 |
|---|---|
| Mac Release CTest (ASR 포함) | 8/8 통과, TCP loopback32.35초 |
| 직렬화 경계/HiLg 실제 수신 | 50개 검사 실패0, 기존 연결/무수신/만료/미재전송+신규 HiLg 시나리오 통과 |
| ASR 옵션 | 수신기 필수·SenseVoice 점수/fallback 거부 등6건 실패0 |
| 기존55건 전사의 실제 TCP 재입력 | 실행 intent24·NONE16·DROP15. 비명령30개 실행 intent0, 사람 재시도 제외 |
| 실제 Mac SenseVoice WAV→격리 Core | FAN_SET(1)·IND_OFF 각1회 수신, Core accepted=true. hello1회/voice2회 |
| 동일3개 WAV의 기본/송신 모드 대조 | 각각 ASR3회·DROP1; 기본 voice0, --publish voice2 |
| wire/수신 state 스키마 | jsonschema4.23.0·계약 v1 검사 실패0 |
| ATLAS SDK ARM/Pulse 빌드·Pi5 실행 | parser8206·audio1559·hi-lg98·TCP50, 실패0. 새 바이너리 해시 대조 |

[실측/미완료] 새 ARM 실행본 SHA256은 `bb7edf7d26222dd4c5436aaaf9981367628c9e6382378b38114c2327a9f4a77a`이며 Pi5 별도 시험 폴더에 배치해 위 단위 검사를 실행했다. 이어진 Pi5 WAV→격리 Core 추가 시험은 SSH 직접/마스터 경유 연결이 끊겨 완료하지 못했다. **Mac WAV→Core 성공을 Pi5 WAV 송신 성공으로 바꾸지 않는다.** Pi5 운영 유닛·마이크·실제 발행은 시작하지 않았다.

[범위] 보관 ReSpeaker WAV2개는 VAD 종료가 가능한 전체 경로 시험을 위해 **원음 샘플은 유지하고 끝에 디지털 무음1초를 추가한 시험본**을 사용했다. 원본 첫 WAV는 EOF 전 종료 무음이 부족해 ASR로 넘어가지 않았고, 둘째만 IND_OFF로 송신됐다. EOF 강제 확정이 없는 현재 정책대로이며 원본2/2 성공으로 세지 않는다. 무음 추가 시험은 송수신 구현 검증이고 실시간 성능 정확도 표본에 합산하지 않는다.

[범위] 격리 SafetyCore는 `--i2c none --fan-uart none --cook-uart none`으로 실행했다. Core 수락과 실제 하드웨어 집행은 다르다. FAN_SET(1) 수락 후에도 안전 하한/ACK 부재로 최종 fan.level은3이며, 이를1단 집행 성공으로 표시하지 않는다. 파일 재생 지연을 공식 발화 종료→intent 지연으로 집계하지 않는다. 기존 서비스·Flutter 화면·실제 하드웨어 집행은 이 시험에서 검증하지 않았다.

## 현지·정인·재범 검토 요청

[제안/합의 필요] 의미 변경은 아직 PM/Core/계약 승인 완료가 아니다. 현재 PR은 실제 송신 구현을 포함하지만 기본 발행은 끄고 Draft를 유지한다.

1. 현지(PM): 최신24/25·0/30·배경300초의 실측 범위, 하이엘지 근접 발음·VAD544ms·질문/미제공 conf의 발행 정책을 검토한다. 과거 짧은 질문 오탐2건은 이번 다른30문장/배경음 결과로 해결됐다고 하지 않는다.
2. 정인(Core/계약): 최종 `fan.level`을 적용 직전에 비교하고, 안전 하한 미달 요청은 기존처럼 requested를 보존한 뒤 max 보정하는 안을 검토한다. 동단수 NO_CHANGE·반대 방향 WRONG_DIRECTION, 전원이 켜진 팬의 켜기 요청을 어떻게 표현할지 정한다.
3. 재범(수신/화면): `up + 목표1`만으로는 현재2·3단에서 “켜기 유지”를 표현하지 못하므로 FAN_ON/켜기 의미 또는 하한 보장 의미를 포함해 합의한다. 방향/결과 reason과 D-051의 v 이행·송수신 동시 배포에 묶을지는 계약 PR에서 확정한다. UI는 Core 최종 상태/수락을 인식 후보와 구분한다.
4. 공통: `conf=0` 미제공 표기·최종 인식 문장·하이엘지 활성어의 README 의미 변경은 구조가 같은 v1 제안이다. 새 방향 필드/intent를 추가할 때는 스키마·예제·v·송신/수신/Flutter를 함께 변경한다. 이번 PR에는 임의 방향 필드나 안전 정책 변경을 넣지 않았다.

## 이전 설명과 현재 상태

| 이전 | 현재 |
|---|---|
| SenseVoice --publish를 거부하는 관측 전용 코드 | 명시적 --publish에서 기존 TCP v1 송신, 기본 발행 끔 |
| 5분 무음/소음 생략 전달 | 조용150초·팬/유튜브150초 실제 측정 자료 제출 |
| 기본 팬 켜기 후보1단을 그대로 발행할 수 있다는 가정 | 관측은 보존, 송신은 NONE; 이미 도는 팬을 낮추는 문제를 계약 검토 |
| voice_node CI 없음 | System CI 모델 없는 검사 존재, 실제 ASR/ARM/현장 시험은 별도 |
| Whisper systemd/env | SenseVoice/Silero/런타임 env와 수동 전환 안내 |

[남은 완료 조건] 지정 화자 공식 NFR-08, 짧은 질문 오탐 처리/허용 범위 합의, PM·계약 승인, 새 head의 실제 ReSpeaker→운영 수신기/화면/장치 확인, CI와 요청 리뷰어 전원 승인이다. 실제 송신 코드가 있다는 사실과 이 완료 조건 충족은 구분한다. #62는 열어 둔다.

## PR 갱신 전 코드·문서 재검토 — 2026-10-24

[문서] 원격 #107 `936914d`와 로컬 검토 소스의 전체 Git tree는 `514032140635c21416690e7677f12e93b841e9fe`로 같음을 확인했다. 송신 옵션·ASR별 활성어·conf 검사·팬 기본 켜기 NONE·기한·연결 감시를 코드와 대조했다. 이번 재검토에서 C++ 동작·모델·VAD 수치·안전 정책·스키마를 추가 변경하지 않았다. 근거: `audio_observer.cpp`, `voice_tcp.cpp`, 테스트 및 아래 재실행.

| 이번 #107의 마무리 범위 | 후속 PR 범위(합의 필요) |
|---|---|
| 현재 한 문장 발화·SenseVoice/Silero·선택적 TCP v1 송신 | Pi5 음성노드 → Flutter 표시용 로컬 이벤트 |
| 모델/런타임·env 선행 준비와 수동 start/stop·복원 안내 | 호출어 확인 후 요청 상태·미송신/만료 안내, 진단 화면 분리 |
| 입력/기한/연결/미재전송 검사, 기존 Core 결과 표시 재사용 | 동일 Core 결과 반복을 구분하는 계약 식별자 |
| PM·계약 판단·공식 시험·운영 전체 경로와 새 head 승인 확인 | 팬 켜기/방향 wire, 새 intent/reason 및 관련 송수신 변경 |

[문서] 기존 Flutter 결과 표시가 있으므로 새 로컬 UI 연동은 #107의 송수신 시험 선행 조건이 아니다. #113에는 주변 대화에 반응하지 않고 호출어 확인 후 요청부터 보여 주는 안을 제안했다. 별도 호출어 검출기·호출 후 대기·부분 전사·TTS는 이번 구현 범위가 아니다. #116에서 state만 v2·Core 입력 v1 유지로 확정되면 voice 송신을 함께 올릴 필요는 없으며, 공통 입력/voice 계약까지 바뀌면 T5를 포함한다. 근거: [수정한 T5 리뷰](https://github.com/lambdaramji/HOOD-SEEK/pull/113#pullrequestreview-5636978134), [재범 송신 리뷰](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5636822111). 해당 제안들은 팀 결정 완료로 승격하지 않는다.

[실측] 같은 소스의 Mac 실행본으로 아래를 재실행했다. 원문 결과는 기존 [구조화 결과](measurements/2026-10-24-sensevoice-publication.json)의 `pre_pr_review_20261024`에 추가했다. 기존 시험 결과·ARM 해시는 보존했다.

| 재검증 | 출력/결과 |
|---|---|
| ASR 포함 Release CTest | `100% tests passed, 0 tests failed out of 8`, loopback32.36초, 전체33.95초 |
| 최초 제한 환경의 TCP 루프백 | localhost bind가 `PermissionError: [Errno 1] Operation not permitted`로 차단. 소켓 허용 환경의 위8/8과 구분 |
| jsonschema4.23.0 계약 검사 | 정상14개·거부23개가 모두 기대대로 통과 |
| 기존55건 최초 전사 → 실제 TCP | 실행24·NONE16·DROP15, 비명령 실행0/30, 스키마 실패0 |
| 동일2WAV → 실제 SenseVoice → 격리 SafetyCore | hello1·voice2, FAN_SET(1)·IND_OFF 각각 accepted=true, 스키마 실패0 |
| 동일3WAV 모드 대조 | 기본 ASR3·voice0·DROP1, publish ASR3·voice2·DROP1 |

[범위] 두 WAV는 앞서 사용한 원음+종료 무음1초 시험본이다. 새 녹음·새 독립 정확도·Pi5 실시간 시험으로 합산하지 않는다. 격리 Core의 I2C·팬/쿡탑 UART는 모두 none이고 운영 장치를 제어하지 않았다. Mac 링크에는 기존 `ld: warning: ignoring duplicate libraries: 'whisper/src/libwhisper.a'`가 남아 있어 경고0으로 보고하지 않는다. 이번 추가 문서/서비스 주석은 이전 ARM 바이너리의 기능을 바꾸지 않는다.

[문서] 재현은 기존 sherpa/Whisper 소스를 지정한 ASR 빌드에서 `cmake --build "$ASR_BUILD"`, `ctest --test-dir "$ASR_BUILD" --output-on-failure`, jsonschema4.23.0 환경에서 `python contracts/validate.py`를 실행한다. 55전사 TCP 재입력은 `voice_tcp_test_client`·`tests/voice_tcp_receiver_test.py`와 기존 trials JSONL을 재사용했고, WAV/격리 Core/모드 대조의 재현 스크립트·전체 로그·WAV는 저장소 밖 시험 산출물에 보존한다.

[문서] 실제 배포 전에는 기존 env에 SenseVoice/Silero/런타임을 먼저 맞추고 관측 실행을 확인한 뒤 유닛을 교체한다. 저장소 루트 기준 `deploy/systemd/` 복사 경로, 현재 `quality=asr_unscored` 로그, 활성어 모드별 TCP API 설명을 고쳤다. 배경300초의 ASR0은 파서의 말소리 거절 검증이 아니며 유튜브 대사 여부도 미기록임을 후속 보고서에 명시했다.

[미완료] PM의 질문 오탐·근접 활성어/conf/VAD 정책 판단, 계약 승인, 지정 화자 공식25회와 원음 끝3초 검증, 배경300초 수용 여부, 새 head의 Pi5 운영 입력→Core→화면/장치 확인, 원격 CI·전원 승인은 여전히 남는다. 사람 시험·승인을 완료한 것으로 표시하거나 후속 UI PR로 옮겨 생략하지 않는다. 원격 PR 갱신 전에는 위 경계와 남은 조건을 사용자에게 먼저 제시한다.

[문서/조회] 원격 `936914d`의 전원 승인 검사 `all_reviewers_approved`는 `failure`다. PM·계약 담당의 리뷰는 COMMENTED이며 코드 검토 의견을 승인으로 바꾸지 않는다. 이번 로컬 문서 갱신의 원격 CI는 아직 실행하지 않았다. 근거: [전원 승인 검사](https://github.com/lambdaramji/HOOD-SEEK/actions/runs/40062349944), #107 리뷰 목록(2026-10-24 조회).

## 후속 Pi5 운영 경로 점검과 로그 표시 수정 — 2026-10-24

[실측] 위의 미완료였던 Pi5 WAV 송신을 추가 수행했다. 기존 운영 바이너리·env·유닛을 백업하고, 검증된 ARM 실행본(SHA256 `bb7edf7d26222dd4c5436aaaf9981367628c9e6382378b38114c2327a9f4a77a`)을 실제 `/data/hoodseek/bin/hoodseek-voice-observer` 경로에 배치했다. SenseVoice/Silero/런타임 env를 채웠고 발행 옵션은 비워 두었다. 서비스는 수동 기동 방침을 유지하며 enable하지 않았다.

[실측] 해당 Pi5 운영 바이너리에서 기존 종료 무음1초 추가 WAV2개를 전사해 SSH 경유의 격리 Mac Core로 전송했다. hello1·voice2, FAN_SET(1)·IND_OFF 각각 accepted=true, 스키마 실패0이다. 실제 Pi5 ASR/TCP 경로 검증이며 실시간 마이크·현장 Core·Flutter·장치 집행 성적은 아니다. 최초 시도는 기기에 시험 WAV가 없어 실패했고, 해당 로그를 보존한 뒤 파일을 별도 시험 폴더에 복사해 재실행했다. 근거: 구조화 결과의 `pi5_operating_followup_20261024`.

[실측/미완료] 실제 운영 Core 포트는 연결을 거부했고, 담당 기기의 SafetyCore 서비스가 inactive인 것을 확인했다. 사용자가 다른 시험 중이라 기동 보류를 요청해 Core를 시작하지 않았다. 따라서 현장 Core 수신·화면/장치 전체 경로는 계속 미완료다. Pi5와 마스터의 장치 wall clock 차이가 있어 시험 기록은 작업 PC KST, 지연은 Pi5 원음 시간 원점·steady clock으로 구분한다.

[구현/실측] systemd 상주 입력의 PulseAudio XDG 경고에 포함된 작은따옴표가 `live_observer.fields()`에서 parse_error로 처리돼, 입력은 정상인데 화면에 오류가 남는 문제를 확인했다. `event=`로 시작하는 노드 이벤트만 파싱하고 그 밖의 journal/PulseAudio 문구는 원문 로그에 보존하도록 공통 원인을 고쳤다. 잘못된 `event=` 문장은 계속 parse_error다. 실제 경고문·오류 미발생·원문 보존 검사를 추가해 `live_observer_checks=11 failures=0`, CTest8/8(전체32.64초, loopback32.31초)을 확인했다. ASR·VAD·ARM 바이너리나 PulseAudio 권한을 바꾼 수정은 아니다.

[준비/미완료] 남현지의 다음25회는 사용자 선택에 따라 팬 켜짐 소음 조건으로 준비했다. 앞선 사전 전사2개와 0/25 상태의 원음은 보존했으며 최초25회에 넣지 않는다. systemd 서비스에 원음 저장·최대600초·재시작 끔의 시험용 runtime drop-in만 적용하고 종료 시 제거하는 별도 도구를 사용한다. Core·실제 voice 발행은 꺼 둔다. 팬 꺼짐 공식 NFR-08 완료로 대체하지 않으며 최종 문항·원음 검수 결과는 시험 후 추가한다.

## 하이엘지 뒤 문장 끝 물음표 정책·ARM 재검증 — 2026-10-24

[구현/합의 필요] 사용자가 PM·Core 담당자의 동의를 전달하고 변경을 요청해, 하이엘지 활성어 확인 뒤 **지원 명령 끝의 `?`·`？`만** 명령 구두점으로 처리한다. `하이엘지 인덕션 꺼?`와 `켜?`는 각각 IND_OFF·IND_ON이다. `켜도 돼?`·`켜져 있어?`·`끄지 마?` 같은 허가·상태 질문/부정, 인용·복합은 계속 NONE이고 활성어가 없으면 DROP이다. 풍량 없는 팬 켜기는 끝 물음표 여부와 관계없이 송신 NONE 보류를 유지한다. 기존 Whisper/후드 경로는 바꾸지 않았다. 최초 전사는 보존하며 실행 후보의 최종 문장만 끝 물음표를 제거한다. 근거: `voice_node/src/hi_lg_classifier.h`, `audio_observer.cpp`, 해당 분류/TCP 시험과 `contracts/README.md`의 리뷰 대기 문구. 사용자의 동의 전달을 원격 PR 승인 완료로 표시하지 않는다.

| 이전 정책/기록 | 이번 변경/해석 |
|---|---|
| 하이엘지 뒤 짧은 명령도 끝 `?` 때문에 NONE | 활성어·지원 명령이 성립하면 끝 물음표만 무시 |
| 과거60건의 짧은 질문2건을 비명령으로 평가 | 과거 정답·성적은 보존. 이번 정책 변경을 억양 인식 개선이나 오탐 해소로 바꾸지 않음 |
| 이전 ARM `bb7edf…` | 새 ARM `4c7d2327e38dae55b0a604b6c31c0066b6ecdd1fae5c18ca18ba3c0baf0eb29a`, 운영 경로에 백업 후 배치 |

[실측] 2026-10-24 새 정책 소스의 Mac Release CTest8/8(전체33.72초·loopback32.35초), HiLg145건·TCP55건 실패0, jsonschema4.23.0 정상14/거부23 예제를 확인했다. Pi5에서도 parser8206·audio1559·HiLg145·TCP55건 실패0이다. 최초 ARM 묶음에서 TCP 시험 실행본이 빠진 패키징 실패를 보존한 뒤 파일을 보완해 Pi5에서 다시 실행했다. 근거: 아래 구조화 결과 `terminal_question_policy_20261024`의 소스/원문 해시 및 `voice_node/tests/` 실행 검사.

[실측/범위] 새 ARM의 보관2WAV → SenseVoice → 격리 Mac Core 송신은 hello1·voice2, FAN_SET(1)/IND_OFF 각각 accepted=true·스키마 실패0이다. Mac에서도 같은2WAV 송수신을 확인했고 기존55건 최초 전사의 TCP 재입력은 실행24/NONE16/DROP15·비명령 실행0/30이다. 두 WAV는 앞서 설명한 종료 무음1초 추가 시험본이며 실제 마이크·운영 Core·화면/장치 시험이 아니다. Pi5 로그의 VAD 추정 종료→송신815/810ms를 공식 원음 끝→intent 성적으로 합산하지 않는다. 격리 Core의 I2C·팬/쿡탑 UART는 모두 none이다.

[실측/준비] 재접속 시 운영 음성 서비스 inactive·MainPID0·runtime drop-in 없음, PulseAudio 녹음 스트림0, ReSpeaker 입력 존재와 새 실행본 해시를 확인했다. 다른 SSH 접속만으로 음성 충돌이라고 판단하지 않는다. 새25회 페이지는 실제 systemd 서비스를 수동 시작하며, 시작 직전 다른 observer/녹음 스트림이 있으면 거부한다. 이전16건도 사용자가 연습이라고 확인해 앞선2건과 함께 제외했다. 팬 켜짐 소음 조건의 새25회는 아직 성적이 없고 공식 팬 꺼짐 NFR-08과 구분한다. 운영 Core 기동 보류·기본 voice 발행 끔·수동 기동은 유지한다.

## #113 재검토 반영 — T5 문서 정리

[문서] #113 `b3a1127`에서 PM은 #116을 state만 v2·Core 입력 hello/vision/voice/ui_cmd는 v1 유지로 정리했다. voice README와 #107 초안을 이에 맞춰 갱신했다. 구현·스키마 배포 완료를 의미하지 않으며 실제 혼합 버전 시험은 #116에서 확인한다. 근거: [UI 설계 §4.5.1](https://github.com/lambdaramji/HOOD-SEEK/blob/b3a112760545eb27bd965c7897d8126c47a3c99c/docs/2026-10-24-flutter-ui-design-v1.md#L217).

[문서/제안] T5 쪽은 한 발화·전사 후 호출어 확인, 입력 레벨/원문의 진단용 구분, Core 수락과 실제 동작 완료의 구분, X/시트 닫기의 표시 전용 의미를 명시했다. 현재 Flutter IPC·부분 전사·TTS·호출 응답음을 제공한다고 쓰지 않는다. #113의 §6·음성 목업·미지원 예시·옛 송신 상태 설명은 PM에게 새 의견 리뷰로 수정 요청한다. 이번 정리는 문서 변경이며 ASR/VAD/intent/Core 실행 로직을 추가 변경하지 않았다. #107의 남은 공식 시험·정책 승인·운영 통합 조건은 유지한다.

## PM 최종 정책 반영·공식 시험 전 고정 준비 — 2026-10-24

[문서/구현] [PM 결정](https://github.com/lambdaramji/HOOD-SEEK/pull/107#pullrequestreview-5637198979)에 따라 풍량 없는 팬/후드 켜기를 실제 wire FAN_SET(1)로 연결했다. 기존의 NONE 보류를 해제하며 현재보다 낮은 requested1이 될 수 있고 실제 팬은 Core가 fan_min으로 보정한다. 대상 없는 하이엘지 꺼/꺼줘/멈춰줘는 NONE으로 막았다. 직접 동작 질문 수용·conf0 미제공·v1은 유지하고 허가/상태 질문·부정·인용·복합은 비실행이다. 새 방향 필드는 추가하지 않았다. 앞선 단계의 팬 켜기 보류 기록은 당시 결과로 보존하며 현재 정책은 이 절을 따른다.

[실측] Mac 전체 CTest8/8(33.17초·loopback32.34초), Pi5 parser8206·audio1559·HiLg179·TCP60건 실패0, 시험 계획 텍스트63건과 집계 도구19검사를 통과했다. 새 ARM SHA256은2beedc52474cffce629f5818c1cfe223f6ade56417baab9668aa57bb891728c8이다. 기존 중복 Whisper 링크 경고는 남아 있다. 실제 부정36개/주변음300초와 공식25회 성적은 아직 없다. 근거: 측정 JSON의 pm_policy_20261024.

[시험 게이트] head·바이너리/모델 SHA256·교정 범위를 #62에 고정한 뒤 비명령36개·실제 주변음300초를 관측 모드로 먼저 시험한다. 실행 후보0이면 팬 꺼짐·남현지·Pi5/ReSpeaker25회를 --publish로 평가하며, 실패하면 관측 전용을 유지한다. 성공은 최초 시도 intent/value·실제 원음 끝→intent≤3초·발행 시 문항별 Core accepted=true다. fan.level은 비교하지 않는다. 집계 도구에 문항과 대조한 Core 결과/원문 근거 검수를 추가했고 이전 마지막 결과나 sent만으로 성공 처리하지 않는다.

[범위/미완료] 소프트웨어의 대화/속삭임 분리·제거 기능을 새로 넣은 것은 아니다. 이번 추가 별칭/풍량 토큰 교정과 PM 정책 반영을 새 head에서 고정해 시험하며, 설정 변경 시 새 시험으로 분리한다. 정정인 계약 확인·새 head CI·리뷰어 전원 승인과 운영 Core 시험 인계는 별도로 남는다.
