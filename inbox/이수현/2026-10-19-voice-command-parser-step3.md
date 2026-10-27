# #62 STEP 3 — 텍스트 명령 파서와 거부 규칙

- [문서] 공개용 경로 표기: `<작업 PC>`·`<작업 PC 임시 경로>`·`<마스터 작업 경로>`는 실제 경로로 치환한다. 아래 명령은 당시 실행 기록이며 원래 경로와 로그는 저장소 밖에 보존한다.
- 작성자: 이수현 (@suuuhululu), AI 작성 지원: Codex
- 작성일: 2026-10-19 (KST)
- 상태: 설계서 축약형·제한 표현 보완, Mac 검사 및 ARM64 크로스 빌드·Pi5 검사 완료. #62 공유 댓글 2건 게시 완료(Pi5 추가 결과 포함). STEP 4 미진입, Push·PR 미실행
- 카테고리: voice-interface / test-calibration-demo
- [문서] 범위: 설계서 §5.3의 전사 → 정규식 intent. STEP 4 오디오·STEP 5 TCP는 제외

[문서] 후속 진행: 사용자 승인으로 STEP 4 관측 파이프라인을 구현·검증했다. 최신 오디오·실제 발화 결과는 [STEP 4 보고서](2026-10-19-voice-audio-pipeline-step4.md)를 따른다. 아래 STEP 4 미진입 표현은 STEP 3 당시 기록이다.

## 최신 공유 상태 (2026-10-19 21:03 KST 확인)

- [실측] 사용자 `진행` 승인 후 Pi5 추가 결과 전문 1건을 [#62 댓글](https://github.com/lambdaramji/HOOD-SEEK/issues/62#issuecomment-6266750459)로 게시했다. 재조회에서 승인 전문과 본문 일치를 확인했다.
- [실측] #62는 `open`, 댓글 2개다. 이번 요청의 GitHub 쓰기는 추가 댓글 1회뿐이다. STEP 4·Push·PR·이슈 종료는 수행하지 않았다. 아래 20:54 공유 기록은 첫 댓글 게시 당시 상태를 보존한다.

## Pi5 추가 검증 (2026-10-19 20:59 KST)

- [실측] 사용자 요청으로 STEP 3 파서를 기존 ATLAS SDK에서 ARM64 Release 크로스 빌드하고 Pi5에서 직접 실행했다. **`checks=299 failures=0`, `test_exit=0`**. 오디오·whisper 추론·TCP·장치 제어·systemd start/stop은 실행하지 않았다. STEP 4 미진입이다.
- [실측] 검사 대상 소스는 기능 보완 커밋 `66b74ca`와 같다. CMakeLists.txt·header·parser·test 4개 파일의 SHA-256을 Mac↔마스터에서 대조했고, 빌드 전후·검증 후 모두 일치했다. 기존 `atlas-dev` 컨테이너·SDK·시스템 라이브러리·#50 결과를 변경하지 않았다.
- [실측] 기존 SDK 이미지 `sha256:0a6fa7930de27d0ab57acb2748ce348b95da2e92f2bf89f00a5d91d0786b3eba`, Clang 20.1.1, SDK CMake 3.31.6, `OEToolchainConfig.cmake`를 재사용했다. 새 #62 디렉터리만 bind mount한 임시 컨테이너에 `--network none --read-only`를 적용했다. 새 패키지 설치·이미지 교체·실행 중인 기존 컨테이너 재생성 없음.

| 항목 | [실측] 확인 결과 |
|---|---|
| 마스터 빌드 경로 | `<마스터 작업 경로>/hoodseek-work/issue-62/step3-parser-20261019-UWASHR` |
| Pi5 실행 경로 | `/data/hoodseek-issue62-parser-20261019-TJcwdF/command_parser_test` |
| 로컬 증거 경로 | `<작업 PC 임시 경로>/hoodseek-62-pi5-verify-m5hP5E` |
| Pi5 아키텍처·커널 | `aarch64`, `Linux atlas 6.12.25-v8-16k #1 SMP PREEMPT Tue May 12 06:09:10 UTC 2026 aarch64 GNU/Linux` |
| ELF | `ELF 64-bit LSB pie executable, ARM aarch64`, loader `/lib/ld-linux-aarch64.so.1` |
| 실행 권한·크기 | `-rwxr-xr-x`, `1516648` byte |
| 실행 파일 SHA-256 | `02893157b23abc47a69fafd621b6e946d82fbdced5f4f1640c7318d3d608789a` — 마스터·Mac·Pi5 일치 |
| loader 검사 | `loader_verify_exit=0`, `loader_list_exit=0` |
| 파서 검사 | `checks=299 failures=0`, `test_exit=0` |

[실측] Pi5에는 CMake·C++ 컴파일러·file·ldd가 없었다. 도구를 추가 설치하지 않고 ELF 아키텍처는 마스터/Mac에서 확인했다. Pi5에서는 실제 glibc 동적 링커와 실행 결과를 확인했다. `ldd`·Pi5 CTest·Pi5 ASan/UBSan을 실행한 것으로 표시하지 않는다. Mac의 sanitizer 검사 299개 통과는 앞선 별도 결과다.

[실측] Pi5 동적 링커가 읽은 실제 경로 및 검사 출력 원문:

```text
command_parser_test: OK
Linux atlas 6.12.25-v8-16k #1 SMP PREEMPT Tue May 12 06:09:10 UTC 2026 aarch64 GNU/Linux
6733.31 25926.23
0.00 0.00 0.00 2/332 88605
02893157b23abc47a69fafd621b6e946d82fbdced5f4f1640c7318d3d608789a  command_parser_test
	linux-vdso.so.1 (0x0000ffffa718c000)
	libstdc++.so.6 => /usr/lib/libstdc++.so.6 (0x0000ffffa6e80000)
	libgcc_s.so.1 => /usr/lib/libgcc_s.so.1 (0x0000ffffa6e40000)
	libc.so.6 => /usr/lib/libc.so.6 (0x0000ffffa6c80000)
	/lib/ld-linux-aarch64.so.1 (0x0000ffffa7140000)
	libm.so.6 => /usr/lib/libm.so.6 (0x0000ffffa6bd0000)
checks=299 failures=0
loader_verify_exit=0
loader_list_exit=0
test_exit=0
```

[실측] 빌드 전 tar 압축 해제에서 macOS 확장 속성 경고가 4회 나왔다. 원문: `tar: Ignoring unknown extended header keyword 'LIBARCHIVE.xattr.com.apple.provenance'`. 소스 4개 파일의 해시는 일치했고 빌드·Pi5 검사는 종료 코드 0이었다. 경고를 없애기 위한 SDK·소스 변경은 하지 않았다.

[실측] Pi5 파일 시각은 `Sep 26 23:29`로 표시됐으나 시스템 시각을 변경하지 않았다. 검증일은 작업 PC의 KST를 기준으로 기록했다. 재부팅도 수행하지 않았다.

### 재현 방법과 증거

[실측] 마스터의 `run-arm-build.sh`는 기존 SDK environment-setup을 source하고 아래 CMake 명령을 실행했다. 빌드 전체는 `cross-build.log`, ELF는 `binary-elf.txt`, 소스 일치는 `source-verification.log`에 보존했다.

```bash
cmake -S /work/voice_node -B /work/build-arm \
  -DCMAKE_TOOLCHAIN_FILE=/opt/atlas-sdk-x86_64/sysroots/x86_64-oesdk-linux/usr/share/cmake/OEToolchainConfig.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=OFF
cmake --build /work/build-arm --parallel 2
```

[실측] Pi5의 `run-pi5-parser-test.sh`는 바이너리 SHA-256을 기록하고 loader `--verify`·`--list` 뒤 검사 프로그램을 1회 실행했다. `environment.log`, `binary.sha256`, `loader-verify.log`, `loader-list.log`, `test.log`, `result.log`를 Pi5와 로컬 증거 경로에 보존했다. 검사 핵심 명령은 다음과 같다(재실행하지 않음).

```sh
cd /data/hoodseek-issue62-parser-20261019-TJcwdF
/lib/ld-linux-aarch64.so.1 --verify ./command_parser_test
/lib/ld-linux-aarch64.so.1 --list ./command_parser_test
./command_parser_test
```

[문서] 설치 구분: CMake·컴파일러는 **빌드용**으로 마스터 서버에서 필요하다. Pi5에는 **실행 파일·공유 런타임**이 필요하며, 이번 파서는 기존 libstdc++·libgcc_s·libc·libm으로 실행했다. 최종 음성 노드의 whisper 모델·PulseAudio·오디오 관련 런타임은 STEP 4에서 별도 점검한다. 이번 파서 성공으로 전체 음성 의존성 검증을 대체하지 않는다.

| 이전 기록 | [실측/문서] 갱신 |
|---|---|
| ARM 크로스 빌드·Pi5 실행 미검증 | 이번 사용자 요청으로 크로스 빌드·직접 실행 299개 검사 통과 |
| Mac 성공만 확인 | Mac과 Pi5 텍스트 검사를 각각 확인. 실제 음성·오디오·TCP는 여전히 미검증 |
| #62 첫 공유 댓글의 ARM·Pi5 검증 대기 | 첫 게시 당시 상태를 보존. 이번 Pi5 추가 결과는 별도 사용자 승인 후 두 번째 댓글로 게시 완료 |

### Pi5 추가 결과 공유 전문 (승인 후 1회 게시 완료)

```markdown
## STEP 3 Pi5 추가 검증 (수현, 2026-10-19)

- [실측] 같은 파서 소스를 기존 ATLAS SDK Clang 20.1.1로 Release ARM64 크로스 빌드하고 Pi5에서 직접 실행했습니다. checks=299 failures=0, test_exit=0입니다. Mac과 같은 검사이며, Pi5 CTest·ASan/UBSan은 사용하지 않았습니다.
- [실측] 실행 파일의 마스터·Mac·Pi5 SHA-256이 일치했고, Pi5 동적 링커의 --verify·--list도 각각 exit 0입니다. 필요한 공유 런타임은 기존 /usr/lib에서 로딩됐으며 새 패키지·SDK·시스템 라이브러리 설치나 변경은 없었습니다.
- [문서] 이번 검증은 텍스트 파서 범위입니다. 오디오·ASR 진단 점수 계산·TCP·수동 기동·실제 음성 인식률은 미검증이며 STEP 4는 시작하지 않았습니다. #62는 Open으로 유지합니다.
```

## 공유 상태 (2026-10-19 20:54 KST 확인)

- [실측] 사용자 `진행해주세요` 승인 후 아래 전문 1건을 [#62 댓글](https://github.com/lambdaramji/HOOD-SEEK/issues/62#issuecomment-6266588521)로 게시했다. GitHub 재조회에서 승인 전문과 본문 일치를 확인했다.
- [실측] #62는 `open`, 댓글 1개다. GitHub 쓰기는 이 댓글 1회뿐이며 Push·PR·이슈 종료는 수행하지 않았다. STEP 4와 Pi5 접속도 시작하지 않았다.
- [문서] 아래의 공유 초안·전문은 검토 및 승인 이력을 보존한다. 최신 게시 상태는 이 절을 기준으로 한다.

표기: [실측] 직접 실행 결과, [문서] 코드·계약·운영 규칙, [추정] 확인 방법을 붙인 미검증 해석. 단위 검사 통과를 음성 인식률·실기기 동작 성공으로 보지 않는다.

## 사전 점검

| 항목 | 확인 |
|---|---|
| 파일·소유 | [문서] AGENTS.md·CONTRIBUTING.md·README.md·CLAUDE.md·ARCHIVE_POLICY.md, 설계서 §5.3, CODEOWNERS 확인. voice_node는 수현·현지 공동 소유 |
| 계약 | [문서] PR #75 head `56209b931f438b2c0dbe6b400d1c1326f0992560`, Open. voice.schema.json과 README의 NONE 기준 확인. 로컬 main의 MQTT 스키마를 정본으로 쓰지 않음 |
| 기존 코드 | [실측] 현재 main `2c629528827a0026d09b8b2ff292c88cd97b346c`에 voice_node 없음. pi4_sensor_node/CMakeLists.txt의 독립 CMake·CTest 패턴 재사용 |
| 도구 | [실측] Mac arm64, Apple clang version 21.0.0 (clang-2100.1.1.101), CMake 4.3.4. 새 패키지 설치 없음 |
| 실기기 | [실측] 최초 텍스트 검사는 Mac에서 수행. 이후 사용자 요청으로 마스터 크로스 빌드·Pi5 직접 실행 완료(상단 추가 검증 절). 장치 제어는 없음 |
| 팀원 의존 | [문서] #75 병합 뒤 계약 재대조 필요. 현재 head 기준으로 텍스트 구현 가능. Core #47이나 Flutter 수정 없음 |
| PM 결정 | [문서] G2 수동 start 합의 반영. 수현 확인: 절대 풍량+제한 표현, 상대 팬 NONE, conf는 ASR 진단 점수. 품질 임계값은 실측 후 결정 |
| GitHub 쓰기 | [실측] 초기 공유 1회·Pi5 추가 결과 1회, 각각 사용자 승인 후 누적 댓글 2건 게시 완료. PR·Push·이슈 종료 없음 |

## 구현·검증 결과

- [문서] `voice_node/src/command_parser.{h,cpp}`: `parse_command`와 독립 `is_valid_command`. 외부 라이브러리·모델·추상 계층 없이 C++17 표준 기능만 사용.
- [문서] ASR 성공·품질 검사 통과가 호출 전제. 유효 UTF-8과 원문 200자 상한을 확인한 뒤 공백만 정리. 원문을 자르거나 "푸드/후들"을 "후드"로 고치지 않음.
- [문서] "후드" 없음·빈 전사·잘못된 UTF-8·200자 초과는 전송 제외. "후드"가 있으나 문장 전체 allowlist 불일치는 NONE. 단일 지원 명령은 그대로 Core에 전달할 후보이며, 노드에 위험도·재실·잠금 판단 없음.
- [문서] FAN_SET만 value 1..3. 다른 intent는 value 없음. fan 상대 명령은 NONE이며, 내부·외부에 새 intent를 추가하지 않음.
- [실측] 초기 커밋 `e7a618b`의 Mac 검사 238개 실패 0. 이번 요구사항 대조에서 축약형 누락을 발견해 보완했고, 최종 Mac Release·AddressSanitizer·UndefinedBehaviorSanitizer 검사는 각각 299개 실패 0, 진단 없음. 검사 수를 음성 인식률의 표본 수로 사용하지 않음.

| 입력 | [실측] 파서 결과 |
|---|---|
| 후드 약풍으로 해줘 | FAN_SET(1) |
| 후드 중풍으로 해줘 | FAN_SET(2) |
| 후드 강풍으로 바꿔 줘. | FAN_SET(3) |
| 후드, 강풍으로 바꿔줘. | FAN_SET(3) |
| 후드 약 / 중 / 강 | FAN_SET(1) / FAN_SET(2) / FAN_SET(3) |
| 후드 팬 좀 꺼줘 | FAN_OFF, value 없음 |
| 후드 팬 꺼 | FAN_OFF, value 없음 |
| 후드 인덕션 켜 | IND_ON, value 없음 — 집행 여부는 검사하지 않음 |
| 후드 화력 올려 / 불 줄여 주세요 | IND_UP / IND_DOWN, value 없음 |
| 후드 켜지 마 / 바람 좀 바꿔줘 / 세게 했다가 꺼줘 | NONE, value 없음 |
| 후드 한 단계 올려줘 / 바람 좀 줄여줘 | NONE, value 없음 |
| 요리 냄새가 심하네 / 푸드 강풍으로 바꿔줘 / 후들 강풍으로 바꿔줘 | 전송 제외 |

[실측] 경계: UTF-8 연속 byte 오류·잘림·과장 인코딩·surrogate·U+10FFFF 초과, NUL·제어 문자, 한글·4-byte 문자 포함 200/201자, FAN_SET value 누락·0·4, 다른 intent value 포함을 검사했다. 허용 문장에 부정·취소·복합 명령·질문을 덧붙인 회귀 검사도 포함한다.

## 실행 기록·재현

[실측] 작업 폴더 `<작업 PC 임시 경로>/hoodseek-62-parser-h1Wvyk`, 브랜치 `feat/suhyun-62-command-parser`. 기존 `docs/suhyun-50-whisper-native-spike`와 문서 커밋 `b3869e3`는 보존했다. 기본 IDE 작업 폴더의 `.vscode/`는 제외했다.

```bash
<작업 PC>/miniforge3/envs/ros2_jazzy/bin/cmake -S voice_node -B <작업 PC 임시 경로>/hoodseek-62-parser-h1Wvyk-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++
<작업 PC>/miniforge3/envs/ros2_jazzy/bin/cmake --build <작업 PC 임시 경로>/hoodseek-62-parser-h1Wvyk-build --parallel 2
<작업 PC>/miniforge3/envs/ros2_jazzy/bin/ctest --test-dir <작업 PC 임시 경로>/hoodseek-62-parser-h1Wvyk-build --output-on-failure -V
```

[실측] 종료 코드 모두 0. CTest 출력 원문:

```text
1: checks=299 failures=0
1/1 Test #1: command-parser ...................   Passed    0.58 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.58 sec
```

```bash
<작업 PC>/miniforge3/envs/ros2_jazzy/bin/cmake -S voice_node -B <작업 PC 임시 경로>/hoodseek-62-parser-h1Wvyk-sanitize -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/clang++ '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
<작업 PC>/miniforge3/envs/ros2_jazzy/bin/cmake --build <작업 PC 임시 경로>/hoodseek-62-parser-h1Wvyk-sanitize --parallel 2
<작업 PC>/miniforge3/envs/ros2_jazzy/bin/ctest --test-dir <작업 PC 임시 경로>/hoodseek-62-parser-h1Wvyk-sanitize --output-on-failure -V
```

[실측] 종료 코드 모두 0. CTest 출력 원문:

```text
1: checks=299 failures=0
1/1 Test #1: command-parser ...................   Passed    0.45 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.45 sec
```

[문서] Mac 원본 CTest 로그는 각 build 폴더의 `Testing/Temporary/LastTest.log`. 초기 Mac 검증 당시 `CONTRIBUTING.md`의 atlas-dev 크로스 빌드는 미실행이었다. 이후 사용자 요청으로 별도 경로에서 파서 크로스 빌드와 Pi5 실행을 수행했고, 상단 추가 검증 절에 구분해 기록했다. #50에서 whisper를 크로스 빌드한 결과로 새 파서 검증을 대체하지 않았다.

[실측] 검사 추가 중 공백 경계 fixture를 201자 대신 200자로 만든 산술 오류로 `FAIL: 201 codepoints with whitespace`, `checks=238 failures=1`을 확인했다. fixture 공백 수를 196/197로 수정한 뒤 위 최종 결과를 얻었다. 제품의 200자 제한은 변경하지 않았다.

## 한계·후속 작업

- [문서] 초기에는 `후드 약/중/강`, `후드 팬 좀 꺼줘`가 NONE이었다. 설계서 §5.3·부록 A R5와 대조해 누락을 확인하고 사용자 선택에 따라 보완했다. `응 후드 강풍`·질문은 여전히 NONE이다. 정확한 지원 형태는 voice_node/README.md와 실행 검사를 기준으로 한다.
- [문서] `conf`를 1.0이나 인식 확률로 임의 지정하지 않았다. ASR 진단 점수 정의는 수현 확인을 받았고, 계산·품질 게이트·발화당 최대 1회 발행·TCP 직렬화는 아직 구현하지 않았다.
- [문서] 현재 구조는 ASR가 부정어를 누락하거나 무음에서 완전한 허용 문장을 환각하는 경우를 텍스트만으로 구분하지 못한다. VAD·품질 게이트와 실제 음성 평가가 필요하다.
- [문서] STEP 4: PulseAudio/VAD/상주 whisper 연결. STEP 5: TCP hello·seq·재접속·envelope·text/conf·4096 byte·단일 발행 검사. **이번에 진입하지 않는다.**
- [문서] R5 조용한 환경 ≥80%는 미측정. 실제 화자·잡음·무음·부정문으로 ASR 오류와 파서 오류를 분리 기록한다. #62 전체 완료나 2초 목표 달성으로 보고하지 않는다.

## 공유 초안·문서 갱신 거리

[문서] 지금 PR 생성은 필수가 아니며, main 병합 전 PR·승인·CI가 필요하다(CONTRIBUTING.md §1). #50 문서와 #62 코드를 별도 브랜치에 보존하고, #62 구현 진척에 맞춰 PR을 만든다. #62 전체 완료 전 `Closes #62`로 병합해 이슈를 닫지 않는다.

[문서] 🔔 공유 대상: 현지·정인·재범 / Slack·#62 / 지원 문장 범위·NONE·후속 통합 상태. 아래 Slack 초안은 미게시이고, GitHub #62 공유 댓글은 사용자 승인 후 게시했다.

> 수현 / #62: G2까지 재부팅 후 수동 start 방침을 반영했습니다. 절대 풍량·강/중/약 축약형·제한 표현을 지원하고 상대 팬은 NONE으로 유지합니다. Mac 파서 검사 299개 통과이며, conf는 일반 텍스트 토큰 평균의 ASR 진단 점수로 정의했습니다(계산 구현 대기). 자세한 기준은 #62 댓글로 공유했습니다.

| 문서 | [문서] PM에게 요청할 갱신 |
|---|---|
| 설계서 §5.3 매칭 예시 | 단일 키워드 예시와 실제 문장 전체 allowlist를 구분. `약/중/강` 축약형은 코드를 보완해 기존 요구사항을 유지 |
| 설계서 §5.3 활성 단어 | "후드" 포함은 후보 조건이고, 실행 명령은 지원 문장 전체가 맞아야 함을 구분 |
| 계약 README | #75 최신 NONE 규칙과 일치. intent·필드·스키마 변경 요청 없음 |
| 결정 대장 | D-040 ⑦ 절대 풍량 유지. 상대 팬은 이번 MVP 미지원, conf는 담당 구현 기준으로 #62 공유. 새 계약 intent 없음 |
| 설계서 §8.2·시연 체크리스트 | G2까지 재부팅 후 수동 start. 자동 기동은 PM의 10/22 결정 후 반영 요청. PM 소유 문서는 직접 수정하지 않음 |
| #50 기존 실측 보고 | 수정 없음. #62 파서 단위 검사와 기존 whisper 성능 실측을 분리 보존 |

[문서] 게이트 보고: G1 계약 변경 없음, G2 #62는 텍스트 단계까지 진행. 오디오·ASR 품질·conf·TCP·R5·실기기 통합이 남아 있고, G2 일정은 변경하지 않는다. 다음 STEP은 사용자 확인 후 진행한다.

## STEP 4 진입 전 재검토 (2026-10-19)

### PM·수현 합의 반영

- [문서] 사용자가 전달한 PM·수현 대화 및 [#46 PM 결정 (2)](https://github.com/lambdaramji/HOOD-SEEK/issues/46#issuecomment-6266289483): G2(10/22)까지 voice도 재부팅 후 수동 start. 자동 기동은 LG·주최측 회신을 보고 PM이 10/22까지 결정한다. 이 합의를 다시 사용자에게 결정 요청하지 않는다.
- [문서] #46의 직접 실측 대상은 Pi4 템플릿 서비스다. Pi5 적용은 같은 이미지라는 PM 판단·운영 합의이며, voice 유닛을 Pi5에서 재부팅 검증했다고 기록하지 않는다.
- [문서] voice_node/README.md에 재부팅 후 수동 start → PID·오디오 준비 → TCP 구현 후 hello/state 확인 → 수동 stop·정상 종료 → 재시작 절차를 추가했다. 실제 유닛·실행 파일·TCP는 아직 구현되지 않았으므로 수행 완료로 표시하지 않는다. 자동 기동용 rootfs·부팅 등록 변경은 이번 범위에서 제외한다.
- [문서] systemd의 실행 중 프로세스 재시작과 재부팅 자동 기동을 구분한다. 전자는 별도 장애 시험 대상이며, 자동 기동 성공을 뜻하지 않는다. SafetyCore의 root 실행 전제를 voice에 그대로 복사하지 않고 PulseAudio 서버·소스·실행 계정을 확인한다.

### 요구사항 대조와 사용자 결정

| 항목 | 현재 결론 |
|---|---|
| 설계서 축약형 | [문서] §5.3·부록 A R5의 강/중/약을 초기 구현에서 누락했다. 요구사항을 축소하는 문서 변경 대신 코드·검사를 보완했다 |
| MVP 표현 범위 | [문서] 수현 선택: 절대 풍량+제한 동의어(팬 좀 꺼줘 등), 상대 팬은 NONE. 실제 문장 전체 allowlist는 유지 |
| 활성 단어 | [문서] "후드" 포함이 후보 조건. 명령 실행 후보는 문장 앞 "후드"부터 전체 규칙 일치 필요. "푸드/후들" 자동 보정 없음 |
| conf 의미 | [문서] 수현 선택: 일반 텍스트 토큰 점수 평균의 ASR 진단 점수. 명령 정답 확률·Intent Confidence가 아님. Core 정책 판단에 사용하지 않음 |
| 품질 임계값 | [문서] 아직 실측 근거 없음. 고정 임계값을 임의 확정하지 않고 STEP 4에서 음성·잡음·무음 로그를 수집한 뒤 제안 |
| 추가 사용자 결정 | [문서] 이번에 MVP 표현 범위·conf 의미 선택을 받았다. 모델별 상세 임계값·오디오 API·계정은 먼저 사전 점검·실측으로 좁히며, 설치·권한·환경 변경이 필요할 때만 원인·대안을 제시한다 |

[문서] conf 계산 제안의 구현 기준: 모든 segment의 일반 텍스트 토큰 `p`를 같은 가중치로 평균하고, 종료·타임스탬프·언어 등 특수 토큰은 제외한다. 지원 명령과 NONE에 같은 방법을 사용한다. 일반 토큰 0개·NaN·무한대·범위 밖 점수면 품질 탈락으로 전송하지 않는다. 규칙 일치 여부는 별도 내부 판정이며, 일치했다는 이유로 conf=1.0을 부여하지 않는다. 이 정의는 담당 구현 기준이고, 계약 스키마·필드 변경은 아니다.

[문서] 고정 whisper.cpp `d1be6fde11ac6e0407606b4e42fe72d34add8037`의 `include/whisper.h`에서 `whisper_token_data.p`, `whisper_full_get_token_p`, `whisper_full_get_token_id`, `whisper_token_eot`를 확인했고, examples/cli/cli.cpp의 특수 토큰 제외 패턴을 확인했다. 점수는 토큰화·문맥에 영향받으므로 정답 확률로 보정됐다고 주장하지 않는다. conf의 높은 값만으로 환각·부정어 누락을 차단할 수 없다.

### 남은 검증과 STEP 4의 시작 범위

| 순서 | 확인할 내용 | 진입 판단 |
|---|---|---|
| STEP 3 텍스트 | [실측] 축약형·제한 표현 및 새 부정·복합 회귀 검사 포함 299개 통과. ASan/UBSan도 통과 | 보완 완료 |
| ARM 파서 | [실측] 후속 요청으로 ATLAS SDK 크로스 빌드·Pi5 검사 프로그램 직접 실행 299개 통과. Pi5 CTest는 미설치로 사용하지 않음 | 파서 ARM 검증 완료. 오디오·TCP 통합은 별도 |
| 오디오 사전 점검 | [문서] 실제 PulseAudio 소스명·서버·실행 계정·SDK 헤더/라이브러리·기존 캡처 API 확인 | STEP 4 첫 작업. 없는 패키지를 자동 설치하지 않음 |
| VAD 구간 보존 | [문서] 설계서 0.3 s 시작 확인·0.8 s 종료·최대 4 s 유지. 시작 확인 중의 원본 음성을 보존해 "후드" 앞부분을 잃지 않도록 시험 | 에너지 기준은 실제 음성과 팬 소음으로 점검 |
| 4 s 초과 | [문서] 문장 끝이 잘려 부정어·복합 절을 잃지 않도록 제한 초과 발화를 명령에서 제외하는 방안을 우선 검증 | 부분 전사로 즉시 명령을 보내지 않음 |
| whisper 연결 | [문서] #50의 base·문맥·4 threads·audio_ctx 256·beam 5를 시험 기준 후보로 재사용. 모델 1회 로딩·발화별 재사용 검증 | 제품 최종값이나 전체 2초 달성으로 표현하지 않음 |
| 품질·인식률 | [문서] 실제 화자 2명, 조용한 환경·팬 소음·무음·부정문·복합 발화로 정답 문장/ASR 전사/파서 결과를 분리 기록 | R5 조용한 환경 ≥80%는 아직 미측정. 허위 실행 후보도 별도 집계 |
| 수동 운용 | [문서] STEP 4에서 수동 실행·정상 종료, STEP 5에서 TCP hello/state·재연결·중복 발행 없음 확인 | 자동 기동 미해결은 G2 수동 운용의 블로커가 아님 |

[문서] 단계 판정: STEP 3 텍스트 보완은 완료했다. 사용자 확인 후 STEP 4의 **명령 전송 없는 오디오·ASR 관측 시험**으로 진입할 수 있다. 품질 기준과 R5를 충족했다고 간주하거나 STEP 5 TCP·자동 기동을 함께 구현하지 않는다. 이번 응답에서는 STEP 4를 시작하지 않는다.

### 이전 보고 수정 사항

| 이전 표현 | 수정 |
|---|---|
| [문서] 검사 238개 통과이므로 STEP 3 완료 | 현재 구현 검사는 통과했으나 요구사항 축약형 대조가 누락됐음. 보완 후 299개 통과로 갱신 |
| [문서] 약/중/강 미지원으로 설계서 변경 검토 | 기존 R5 요구사항을 유지하고 구현을 보완 |
| [문서] conf 정의 미확정 | 수현이 ASR 진단 점수 정의 선택. #62 공유 완료, 계산·임계값은 후속 작업 |
| [문서] systemd 배포라 재부팅 자동 시작 기대 | G2 수동 start를 명시하고 자동 기동은 PM의 10/22 결정 후 반영 |

### #62 공유 댓글 전문 (승인 후 1회 게시 완료)

```markdown
## #62 STEP 3 보완 및 G2 운용 기준 (수현, 2026-10-19)

- [문서] #46 PM 결정과 공유받은 합의에 따라 G2(10/22)까지 voice 노드도 재부팅 후 수동 start로 진행합니다. 자동 기동은 PM의 10/22 결정 이후 반영합니다. 수동 실행·정상 종료는 오디오 단계에서, TCP hello→state 및 재연결 확인은 TCP 단계에서 구현·검증하겠습니다.
- [문서] MVP는 절대 풍량과 제한된 표현을 지원합니다. 설계서의 "후드 강/중/약" 축약형과 "후드 팬 좀 꺼줘" 등을 보완했습니다. 상대 팬·부정·모호·복합 명령은 NONE, "후드" 없는 발화는 전송 제외입니다. 지원 명령의 실행 허용·거부는 Core에 맡깁니다.
- [문서] voice.p.conf는 Whisper 일반 텍스트 토큰 점수 평균의 ASR 진단 점수로 정의합니다(특수 토큰 제외). 명령 정답 확률·Intent Confidence가 아니며 Core 정책 판단에 사용하지 않습니다. 지원 intent와 NONE에 같은 산출법을 적용하고, 유효 토큰 없음·비정상 점수는 품질 탈락으로 전송하지 않습니다. 품질 차단 임계값은 실제 음성·잡음·무음 시험 후 정하겠습니다. 계약 필드·intent·스키마 변경은 없습니다.
- [실측] Mac 텍스트 파서 검사: checks=299 failures=0. ASan/UBSan에서도 같은 검사 통과했습니다. 단위 검사 결과이며 음성 인식률·Pi5 동작 성공을 의미하지 않습니다.
- [문서] 남은 작업은 ASR 진단 점수 계산·품질 게이트·오디오/상주 whisper·TCP·ARM 크로스 빌드·Pi5 및 R5 검증입니다. STEP 4는 아직 시작하지 않았고 #62는 Open으로 유지합니다. PR·Push는 후속 구현에 맞춰 준비하겠습니다.
```

[문서] 게시 게이트 근거: 사용자가 전달한 작업 규칙 9-나 — "올릴 본문 전문을 보여 준 뒤 내가 '진행'이라고 하면 실행". 위 전문을 제시하고 사용자 승인을 받아 1건만 게시했다. 이슈 종료·Push·PR은 포함하지 않았다.

## 근거

- [문서] AGENTS.md·CONTRIBUTING.md §1~4·CLAUDE.md 문서화 규칙·ARCHIVE_POLICY.md.
- [문서] docs/2026-10-19-s3-s10-design-v0.1.md §5.3, D-040 ⑦.
- [문서] [#62](https://github.com/lambdaramji/HOOD-SEEK/issues/62).
- [문서] [#75 계약 README `56209b9`](https://github.com/lambdaramji/HOOD-SEEK/blob/56209b931f438b2c0dbe6b400d1c1326f0992560/contracts/README.md), [voice schema](https://github.com/lambdaramji/HOOD-SEEK/blob/56209b931f438b2c0dbe6b400d1c1326f0992560/contracts/voice.schema.json).
- [실측] voice_node/tests/command_parser_test.cpp와 위 CTest 로그.
