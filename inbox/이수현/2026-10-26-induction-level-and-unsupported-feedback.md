# 인덕션 목표 단수와 미지원 안내 후보

- 작성자: 이수현
- 작성일: 2026-10-26
- 상태: draft
- 카테고리: 구현·검증
- 관련: #182, #176, #173

## 요청과 범위

[문서] 사용자 요청으로 인덕션 1·2·3단 및 약불·중불·강불을 목표 단수로 받는 코드를 준비했다. 관련 Core·계약·Flutter 변경을 별도 PR에 함께 준비하는 범위를 사용자가 확인했다. 추가 문구 「지원하지 않는 명령입니다.」로 결과/미지원 안내는 기존11개+신규1개=12개다. 기존 경고 안내5개와 효과음4개는 이 수에 포함하지 않는다.

| 한 문장 발화 | 목표 후보 |
|---|---|
| 하이엘지 인덕션 1단으로 해줘 / 한 단계로 설정해줘 / 약불로 해줘 | IND_SET(1) |
| 하이엘지 인덕션 2단으로 해줘 / 두 단계로 설정해줘 / 중불로 해줘 | IND_SET(2) |
| 하이엘지 인덕션 3단으로 해줘 / 세 단계로 설정해줘 / 강불로 해줘 | IND_SET(3) |

[문서] 숫자에는 설정 동사를 요구한다. 단·단계와 1/일/한·2/이/두·3/삼/세를 받으며, 인덕션 대상 생략·0/4/11/음수/소수·목표+방향·부정·허가/상태 질문·인용·복합을 실행하지 않는다. 불 세기 이름을 팬 풍량 오인식으로 승격하지 않도록 대상 경계를 둔다. 약불·중불·강불은 요청 단수 이름이며 센서 hazard를 지정하지 않는다.

## 경계와 재사용

[문서] voice v2에 IND_SET·value1..3을 추가하고 기존 메시지는 v1을 유지한다. Core는 두 버전을 받되 v1 IND_SET은 거부한다. state v4로 결과 intent·value를 돌려주며 Flutter는 state1·2·3·4를 받는다. 계약 문서·예제와 양쪽 파서가 같은 PR에 있다. 버전·운영 범위는 PM·계약 담당 검토 전이다.

[문서] Core의 기존 kIndSet 집행을 재사용해 최신 적용 시점 단수보다 높일 때만 allowed_on을 검사한다. 잠금·위험·재실·ACK·5초 만료·자동 하향·팬 하한을 완화하지 않았다. 실제 쿡탑은 ESP32 시뮬레이터다. 음성노드가 IND_UP/DOWN 여러 번으로 절대 단수를 흉내 내거나 ui 역할로 우회하지 않는다.

[문서] 기존 RESULT_IND_1·2·3을 재사용한다. accepted·목표와 Core 최종 단수 일치·ACK를 확인하고, 맞지 않으면 최대3초 뒤 생략한다. ACK는 장치 링크 생존 정보이며 요청별 물리 목표 완료를 보장하지 않는다. 요청 ID가 없어 동시 제어·동일 결과 반복을 정확히 구분하지 못하는 기존 한계는 남는다.

[문서] 새 NO_INTENT 거부 결과(last_intent=NONE, accepted=false)에만 RESULT_UNSUPPORTED를 재생한다. 호출어 없는 DROP·VAD/ASR 실패·과거 결과·다른 거부 사유·동일 결과 반복에는 새로 재생하지 않는다. 같은 state의 경고 또는 재생 중인 경고를 끊지 않는다.

## 음원

[실측] 기존 자체 합성 Q3 E2 참조와 Qwen3-TTS Base 8bit로 「지원하지 않는 명령입니다.」를 생성했다. 48kHz·mono·PCM16·2.01초이며 WAV는 fmt/data만 갖는다. 모델·참조·seed·파일 해시는 results/manifest.json·SHA256SUMS에 기록했다. 기존11개 WAV는 바이트를 보존했다. 실제 사람 녹음·Typecast를 사용하지 않은 AI 합성 음성이다. 기존 Apache-2.0 전문을 재사용한다.

[실측] 12개 형식·해시 일치, 12개 문구는 새 hi_lg 호출어 게이트에서 모두 DROP이다. 이는 문자열 검사이고 새 파일의 ASR/마이크 시험이나 PM 청취 승인이 아니다. #173의 기존110회 결과는 당시 실행본·원본11개에 한정한다.

## 검사

[실측] Mac에서 voice 모델 없는 CMake/CTest, Core 메시지·UART·센서 검사와 정책 검사를 실행했다. jsonschema4.23.0으로 새 버전·정수 범위·값 누락·v1 거부 예제를 검사했다. 기존 경계와 새 절대 단수·부정·질문·복합을 함께 검사한다.

[실측] 마스터 atlas-dev의 별도 작업본에서 Flutter format·analyze·test·Web build를 실행했다. 팀원 작업본·공용 SDK 권한·운영 앱은 바꾸지 않았다. 최종 명령별 결과는 PR 검증란에 기록한다. 운영 DB가 없는 테스트 환경의 DB 열기 메시지는 기존 위젯 시험의 메모리 경로에서 나오는 로그다.

```bash
cmake -S voice_node -B voice_node/build -DCMAKE_BUILD_TYPE=Release
cmake --build voice_node/build
ctest --test-dir voice_node/build --output-on-failure
cmake -S safety_core -B safety_core/build -DHOODSEEK_WITH_DBUS=OFF
cmake --build safety_core/build
ctest --test-dir safety_core/build --output-on-failure
cmake -S safety_core/tests -B safety_core/tests/build
cmake --build safety_core/tests/build
ctest --test-dir safety_core/tests/build --output-on-failure
python contracts/validate.py
cd flutter_app
flutter pub get
dart format lib test
flutter analyze
flutter test
flutter build web
```

[실측] 최종 출력: `hi_lg_checks=389 failures=0`, voice CTest7/7, Core CTest3/3, 정책 `36 cases, 80958 checks, 0 failed`. 최신 main UI 변경을 보존한 Flutter 검사에서도 `No issues found!`, `+173: All tests passed!`, `Built build/web`를 확인했다. format된 파일을 반영했다.

## 남은 확인과 배포

[문서] 새 계약·문구는 PM/정인/재범 리뷰 대상이다. 새 ARM 실행본·실제 Core 수락·현장 부정 시험·새 음원 청취/마이크 재유입·새 구성 지연은 미실시다. 운영 Pi5/Core를 교체하거나 마이크를 새로 켜지 않았다. 기존 시연5종 승인과 새 명령 승인을 구분한다.

[문서] 배포 순서는 새 Flutter → 새 Core → 발행 끔 voice 후 현장 검사다(deploy/README). #176 병합 뒤 최신 main의 동시 UI 보완도 보존했다. 기능 동결 뒤의 적용 시점·범위는 PM이 결정한다.

## #185(D-061) main 합본 — 2026-10-26

[문서] PM이 #186 리뷰에서 voice v2·state v4 계약과 새 음원 청취를 승인했다. #185가 먼저 병합돼 두 시험 등록을 모두 보존하고, voice_absolute_induction의 hot 블록을 재실·3단 수락으로 바꿨다. 기존 main의 allowed_on ≤ warning과 사용자 잠금·danger/fault·부재 보호를 그대로 유지한다.

[실측] 합본 정책: `37 cases, 99332 checks, 0 failed`. Core 호스트 검사3/3, 계약 검사 exit0. Flutter 합본 최종 결과는 PR 본문에 남긴다. 이는 실기 재배포나 현장 부정 시험을 완료했다는 의미가 아니다.
