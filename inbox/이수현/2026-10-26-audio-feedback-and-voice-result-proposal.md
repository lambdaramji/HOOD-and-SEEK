# 기동음·조작 확인음·4회 경고음과 음성 수락 안내 검토

- 작성자: 이수현
- 작성일: 2026-10-26
- 상태: draft
- 카테고리: flutter-ui, audio
- 관련 이슈: [#167](https://github.com/lambdaramji/HOOD-SEEK/issues/167)
- 검토 기준: main `316385a`(#162·#163 병합 뒤) 추가 병합, Flutter 팝업·ClipPlayer·사용자 선택 음원
- 상태 설명: 최신 main 병합·필수 Flutter 재검사·새 차임 20회 재유입 완료. 새 head CI·재검토와 운영 앱 반영은 별도.

## 작업 범위와 담당 경계

- [문서·사용자 요청] 기존 WARNING 차임을 앱 기동음으로 사용하고, 새 4회 경고음과 사용자가 확정한 V5 조작음을 적용한다. 음성 명령 안내는 현재 팝업 문구를 확인해 사전 제작 가능 여부를 검토한다.
- [문서·구현] 음원 제작·해시·재현 기록은 이수현 작업이다. Flutter의 재생 시점·중단 우선순위·터치 연동 수정안은 재범 담당 영역이므로 별도 브랜치에서 제안하고 담당 리뷰를 받는다. 이번 작업으로 현장 앱을 교체하지 않았다.
- [문서·구현] 기존 `GstClipPlayer`, Core state·ACK, 대시보드 타이머를 재사용한다. 새 패키지·계약·안전 판정·음성노드 설정을 추가하지 않는다. 새 WAV 2개는 기존 WARNING의 기동음 보존과 새 조작음에 필요하다.

- [문서·main 반영] 게시 중 병합된 `f15b204`의 state v3·D-059 변경을 보존해 충돌을 정리했다. 정상 중불·강불은 무음이고 과열·과한 연기만 WARNING을 낸다. CAUTION 파일·계약 ID는 보존하며 경고 발생 정책은 이 PR에서 새로 판단하지 않는다.

## 기존과 달라지는 동작

| 항목 | 기존 | #167 수정안 |
|---|---|---|
| 앱 기동 | 소리 없음 | 대시보드 생성 시 APP_START 1회; 기존 WARNING 차임을 그대로 사용 |
| WARNING | G5 → C6 → E6 상행 차임 | 같은 720 Hz 전자음 4회, 1.31 s |
| 기기 조작 | 소리 없음 | 최신 터치 요청의 목표 단수·ACK가 확인된 상태 변화에 V5 탭 1회 |
| CAUTION·안내 음성 5개 | 기존 파일 | 파일·문구·Core alert 매핑 유지 |
| 재생 중단 | 새 클립이 앞 클립을 중단 | 새 경고는 중단 가능, 기동음·조작음은 경고 로딩/시작/재생 중 생략 |
| 음성 명령 수락 | 팝업·현재 단수 표시 | 이번 수정안에서 음성 수락 WAV는 추가하지 않음; 아래 문구 제안 |

[문서·근거] `flutter_app/lib/screens/dashboard_screen.dart`, `lib/audio/gst_clip_player.dart`, `assets/audio/manifest.json`.

### 조작 확인음 조건

- [문서·구현] 버튼 접촉 자체에는 소리를 내지 않는다. 로컬 `FAN_SET`·`IND_SET` 최신 요청 하나만 추적한다. 요청한 값이 기존 단수와 다르고, 3초 안에 수신한 Core state의 목표 단수·ACK가 일치해야 한다. FAN_SET은 `fan.requested`도 일치해야 한다.
- [문서·구현] 같은 단수, 전송 실패, Core alert, state 끊김, 3초 만료는 대기를 지운다. 안전 최소 단수 때문에 목표와 다른 값이 되면 확인음을 내지 않는다. 탭 이동·시트 닫기·USER_LOCK·ACK_ALERT는 대상이 아니다.
- [문서·제약] `ui_cmd`에는 개별 요청 ID·수락 응답이 없다. 동시 음성/자동 제어가 같은 단수를 만들면 정확한 인과관계를 구별할 수 없다. 이 소리는 **목표 상태·ACK 일치 확인**이며 요청별 Core 수락이나 물리 동작 완료를 증명하지 않는다. 정확한 요청별 통지가 필요하면 후속 계약 합의가 필요하다.
- [문서·구현] 피드백 대기 3초는 화면 피드백의 만료 시간이다. Core 메시지 5초 기한·안전 하한·집행 규칙을 바꾸지 않는다.

## 음원과 재현

| 파일 | 길이 | sha256 |
|---|---:|---|
| APP_START.wav | 1.04 s | `3e9121f2cd0022c9f357d2aba4ebfc5d97b9587c5e59573221bb5cdc91facfa2` |
| WARNING.wav | 1.31 s | `f54ff680df8d3d84a49b350c70c86c99a18e1d234cf6bd68815100804b2ad6b2` |
| TOUCH_FEEDBACK.wav | 0.06 s | `ce6b3fa264e41d08b4637677447e3423e077aae54fed43cb630db72d45210a24` |

- [실측·2026-10-26 Mac] 9개 WAV 모두 manifest의 해시·길이 일치, 48 kHz·mono·PCM16. 새 파일 포함 클리핑 0. `python3 flutter_app/assets/audio/make_chimes.py` 전후 9개 파일 해시가 모두 같았다.
- [문서·제작] APP_START는 기존 PM 차임을 바이트 그대로 재사용했다. WARNING·TOUCH_FEEDBACK은 Python 표준 라이브러리로 합성한 팀 제작음이며 외부 음원·Typecast·음성 모델을 쓰지 않는다. Q3 E2 안내 음성 5개와 기존 라이선스 기록은 보존한다.
- [문서·검증 범위] #137의 7개×10회 재유입 결과는 당시 해시에 대한 기록이다. 새 WARNING·TOUCH_FEEDBACK의 재유입 합격으로 옮겨 적지 않는다.

## 음성 결과 안내 — #173 후속 범위

- [문서·사용자 추가 요청 2026-10-26] 「요청을 받았어요」를 음성으로 안내하지 않고 최종 처리 결과를 말한다. 이 문서 초안의 수락형 8문구 제안은 대체했다.
- [문서] [#173](https://github.com/lambdaramji/HOOD-SEEK/pull/173)에 「후드를 강풍으로 설정했어요」「인덕션이 꺼졌어요」 같은 결과 8개와 후드 안전 하한 유지 3개의 WAV·조건표를 준비했다. 최종 단수·장치 ACK 확인 후 실제 값으로 선택하는 방향이다. 결과 재생 코드는 아직 연결하지 않았다.
- [문서·범위] 이 #169의 조작음은 터치 FAN_SET·IND_SET에 한정한다. USER_LOCK·ACK_ALERT·음성 명령은 터치 조작음 범위 밖이며 #173 음원은 이 PR에 포함하지 않는다.
- [문서·제약] `state.voice` 요청 식별자 부재로 같은 결과 연속·동시 제어를 정확히 대응시키는 한계는 남는다. 후속 연동도 경고 우선순위·유효 상태·만료를 확인해야 한다.

## 이전 소프트웨어 검증 — `4f58df2` 참고

[실측·2026-10-26 마스터 PC atlas-dev] Flutter 3.27.4 / Dart 3.6.2, 이 작업 전용 복사본에서 실행했다.

```text
dart format lib test
Formatted 39 files (0 changed) in 1.08 seconds.
flutter analyze
No issues found! (ran in 3.7s)
flutter test --reporter expanded
00:14 +154: All tests passed!
flutter build web
✓ Built build/web
```

- [실측] 경고 로딩 중·프로세스 시작 중·재생 중 피드백 억제, 경고가 기동음 중단, 완료 뒤 피드백 복귀를 가짜 프로세스로 검사했다.
- [실측] 기동 1회, 터치 즉시 무음, 단수·ACK 일치 시 1회, 같은 값·불일치·안전 하한·거부·전송 실패·3초 만료 시 무음을 위젯 검사로 확인했다.
- [문서·한계] Web은 원래 무음 stub이다. Web 빌드 성공은 스피커 출력 시험이 아니다. 실제 ACK·물리 팬 변화·터치 조작음의 청취 결과는 아직 없다.

### 이전 ARM 패키지 — `4f58df2` 검토용, 최신 main 재검사 패키지가 아님

- [실측·2026-10-26 마스터 PC atlas-dev] 저장소 README의 ATLAS 플랫폼 생성 뒤 ARM64 release IPK 빌드 성공. 아래 명령의 환경값은 저장소 밖에서 주입했다.

```bash
source <ATLAS SDK environment-setup>
flutter-atlas create --no-pub --platforms atlas --project-name hood_seek flutter_app
# 생성된 기본 widget_test.dart만 제거한 뒤
cd flutter_app
flutter-atlas build atlas --ipk --release --target-sysroot="$SDKTARGETSYSROOT" \
  --dart-define=CORE_HOST=<Core 주소> --dart-define=FONT_DIR=<기기 글꼴 폴더>
```

| 산출물 | 크기(byte) | sha256 |
|---|---:|---|
| libapp.so | 6988976 | `44a500e5e364d508c67ced83b8efe0e9107b97e084221521daf065f81f3fc938` |
| com.atlas.app.hood_seek.ipk | 10110654 | `5f95c7b85e2242a34c2a0bd9a722857ba53b4090d5ca8277c3230e956e935bed` |

- [실측] 번들·패키징 폴더의 음원 9/9 해시가 manifest와 일치했다. 이 후보 패키지는 현장 기기에 설치하지 않았다. 새 ARM 앱의 현장 재생·재유입 결과는 미확인이다.
- [실측·검사 경계] Web 빌드는 성공했으며 `CupertinoIcons` 폰트 탐색 경고가 출력됐다. 최종 검사 입력에서 Mac 전송 보조 파일을 제외한 뒤 format·analyze·154개 시험·Web 빌드를 완료했다.

## PM 조건 재검증 — #162·#163 병합 뒤

- [실측·2026-10-26 GitHub] #162와 #163의 병합을 확인한 뒤 main `316385add869f8458bf2f3b26a2932e981f70ba1`을 이 작업본에 합쳤다. 충돌한 `initState`에는 `widget.clips.play('APP_START');`와 `_loadHistory();`를 모두 남겼다. sqlite3·기록 DB·반려동물 문구 등 main 변경을 보존했다.
- [실측·마스터 PC atlas-dev, Flutter 3.27.4 / Dart 3.6.2] 최종 실행:

```text
flutter pub get
dart format lib test
Formatted 42 files (0 changed) in 1.84 seconds.
flutter analyze
No issues found! (ran in 6.8s)
flutter test --reporter expanded
00:17 +161: All tests passed!
flutter build web
✓ Built build/web
```

- [실측·검사 환경] 첫 관리자(root) 실행은 #163의 읽기 전용 파일 시험 1개가 실패했다. root가 0444 DB에 쓸 수 있어 `.old` 경로가 생기지 않은 원인이다. 동일 코드를 `setpriv --bounding-set=-dac_override,-dac_read_search` 검사 프로세스에서 실행해 해당 1/1 통과를 확인하고, 이후 위 필수 검사 전체를 같은 환경에서 완료했다. 제품 코드·해당 시험·공유 SDK 권한은 수정하지 않았다. Web의 CupertinoIcons 탐색 경고는 비실패 출력으로 남았다.

## 새 WARNING·TOUCH_FEEDBACK 실제 재유입 20회

[실측·2026-10-26 Pi5, 사용자 사용 가능 확인 뒤] 운영 앱의 파일을 바꾸지 않고 별도 시험 폴더에서 새 WAV를 GStreamer 기본 pulsesink로 재생했다. 자체 음성 서비스만 잠시 발행 끔으로 바꾸고 원래 설정·발행 모드로 복구했다. 원음 PCM 파일은 저장하지 않았다.

| 항목 | 결과 |
|---|---|
| 새 WARNING | 10회 완전 재생 |
| TOUCH_FEEDBACK V5 | 10회 완전 재생 |
| 재생 완료·USB 출력 경로·USB Playback Running | 20/20 |
| 매 재생 전 자체 PID의 ReSpeaker 입력 확인 | 20/20, 입력 강제 이동 없음 |
| ASR | 6회, 모두 DROP |
| 실행 intent 후보 / 노드 candidates | 0 / 0 |
| voice 송신 | 0 |
| 최대 발화 초과 / 비정상 재시작 | 0 / 0 |
| 현장 청취 | 사용자: 두 소리 모두 들렸음 |
| 출력 장치·음량 | 시험 전후 동일 |
| 시험 뒤 | 원래 발행 켜짐 복구, ReSpeaker 확인·capture_ready, 무기한 실행 유지 |

- [실측] 현행 voice 실행본은 `70839f9c9f096f21fed7a213525a550b077ee1a1451ce6a624569f53264d43a1`이다. 모델·tokens·VAD 해시 및 두 WAV 해시는 [공개 집계 JSON](2026-10-26-pr169-reentry20.json)에 기록하고 [sha256](2026-10-26-pr169-reentry20.sha256)로 대조한다.
- [문서·범위] 실제 스피커→마이크 재유입 시험이며 Flutter 앱의 새 터치·기동 트리거를 직접 조작한 실기는 아니다. 6개 전사 원문·문항 시각·기기 식별자·주소·개인 경로는 게시하지 않는다. 비공개 로그와 집계만 분리 보존했다.

### APP_START 과거 결과 재사용 근거와 한계

- [실측] APP_START는 과거 WARNING 및 현장 앱의 기존 WARNING과 바이트가 동일하다: `3e9121f2cd0022c9f357d2aba4ebfc5d97b9587c5e59573221bb5cdc91facfa2`.
- [문서·과거 실측] #137의 2026-10-25 동일 WARNING 재유입은 10회 완전 재생·실행 후보 0이었다. 당시 ASR 모델·tokens·VAD 해시와 문턱은 현행과 동일하다. 단, 당시 voice 실행본은 `1c38613a...`이고 현행은 입력 검증이 추가된 `70839f9c...`이다.
- [제안·재사용 범위] PCM·모델·문턱이 같은 **변경 없는 음원의 기존 증거**로 참조할 수 있다. 실행본이 다르다는 한계를 함께 남기며, 현행 실행본으로 새 APP_START 10회를 한 것처럼 집계하지 않는다. 금번 신규 시험 성적은 WARNING 10 + TOUCH_FEEDBACK 10만이다. 앱 기동 트리거의 실제 재생 확인은 앱 반영 뒤 별도로 한다.

## 적용 전 남은 확인

1. [완료·실측] #162·#163 병합 뒤 main 통합·필수 검사, 새 WARNING·TOUCH_FEEDBACK 각 10회 실행 후보 0, 현장 청취.
2. [검토 요청·재범/PM] 새 head에서 main 초기화 보존·시험 결과·APP_START 기존 증거 참조 범위를 확인한다.
3. [미확인] 새 head CI·요청 리뷰어 전원 승인 및 새 앱 묶음 반영. 기동음·기기 상태 반영 후 조작음·경고 중단 실기는 새 앱 설치 뒤 확인한다.
4. [문서·운영] 음성노드 실행본·모델·문턱·별칭·시연 화자 범위는 바꾸지 않았다. 현장 앱도 교체하지 않았다.

## 재현 명령

```bash
python3 flutter_app/assets/audio/make_chimes.py
(cd flutter_app/assets/audio && shasum -a 256 -c SHA256SUMS)
cd flutter_app
flutter pub get
dart format lib test
flutter analyze
flutter test
flutter build web
```
