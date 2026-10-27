# HOOD&SEEK 🍳🔥

> 제24회 임베디드SW경진대회 스마트 가전 부문 · Team Ro92n

인덕션과 후드를 독립 가전이 아닌 하나의 협력형 주방 안전 시스템으로 동작시키는 온디바이스 기반 스마트 키친 솔루션입니다.

## 공개본 안내

- 공개 저장소 [`lambdaramji/HOOD-and-SEEK`](https://github.com/lambdaramji/HOOD-and-SEEK)는 비공개 개발 저장소의 `v1.0.0-submission` 태그(2026-10-27 코드 프리즈)를 한 커밋으로 옮긴 제출본입니다(D-064).
- 문서 속 `#번호`(이슈·PR) 링크와 커밋 해시는 비공개 개발 저장소 기준이라 공개 저장소에서는 열리지 않습니다. 사용을 마친 문서를 모아 둔 아카이브(`docs/archive/`·`inbox/archive/`)는 공개본에 넣지 않아, 그쪽을 가리키는 링크도 열리지 않습니다. 팀원별 기여는 [CONTRIBUTORS.md](CONTRIBUTORS.md)와 커밋의 공동 작성자 표기로 확인할 수 있습니다.
- Team Ro92n이 작성한 코드와 문서는 [MIT 라이선스](LICENSE)로 공개합니다. 저장소에 포함된 외부 코드·글꼴·음원은 각자의 라이선스를 따르며 [THIRD_PARTY.md](THIRD_PARTY.md)에 정리했습니다.

## 시스템 경로

```text
Pi 4 SafetyCore (C++, 유일한 판정) ◀── TCP :7420 line-JSON ──▶ Pi 5 vision-node · voice-node · Flutter
  MLX90640 / SPS30 (I2C)
  ├ UART ─▶ 팬 ESP32 (GPIO26 PWM, 3 s fail-safe → 팬 3단)
  └ UART ─▶ 쿡탑 ESP32 (LED 판 시뮬레이터, 3 s fail-safe → OFF+잠금 표시)
```

하드웨어 없이 Flutter demo feed, 계약 예제 검증(`contracts/validate.py`), ESP32 firmware build를 확인할 수 있습니다. SafetyCore 판정 테스트는 `safety_core/tests`(CI `safety_core_policy`)로 확인합니다.

### Flutter 데모

```bash
cd flutter_app
flutter pub get
flutter run -d chrome
```

SafetyCore 연결 방식(TCP :7420, `CORE_HOST`)은 [설계서 §5.4](docs/2026-10-19-s3-s10-design-v0.1.md)를 따릅니다.

현재 화면 범위:

- 정상·주의·경고·위험·연결 끊김 단계와 근거
- 조리면 온도, PM2.5, 사용자 재실, 인덕션 잠금 상태
- 안전 최소 풍량보다 낮출 수 없는 후드 팬 요청
- SafetyCore 연결 상태 (state 3초 무수신 → 연결 끊김)

## 시스템 구성

| 노드 | 언어 | 책임 |
|---|---|---|
| Raspberry Pi 4 · SafetyCore | C++ | MLX90640·SPS30 읽기, **유일한 판정**, ESP32 UART 집행, TCP state 발행 |
| Raspberry Pi 5 | C++ · Flutter | vision-node(ECO.AI YOLOX)·voice-node(SenseVoice, 호출어 「하이엘지」)는 관측·의도 보고만, Flutter는 상태 표시와 요청 전송 |
| 팬 ESP32 · 쿡탑 ESP32 | Arduino C++ | 팬 PWM·쿡탑 LED 판 집행과 3초 무명령 fail-safe |

코드 기준 구조·링크·고장 동작은 [시스템 구조 한 장](docs/architecture.md), 세부 설계는 [S3~S10 설계서](docs/2026-10-19-s3-s10-design-v0.1.md)(단일 기준), 요구사항은 [BRD](docs/2026-10-19-brd-v0.1.md), 메시지 계약은 [`contracts/README.md`](contracts/README.md)를 확인합니다.

## Atlas AI 통합 상태

비전은 Pi 5에서 ECO.AI YOLOX로 직접 추론하고(D-022), 결과는 관측으로만 SafetyCore에 보냅니다. 연속 판정과 잠금은 SafetyCore만 합니다.

- [Atlas AI 통합 의사결정 대장](docs/ai-integration-decisions.md)
- 팀원별 요청은 담당자를 지정한 GitHub 이슈로 관리합니다 — [Projects 운영 안내](docs/github-projects-guide.md)

통합에 사용을 마친 원문과 완료 리뷰는 아카이브에 보존하며, 에이전트는 별도 요청이 없는 한 이를 기본 콘텍스트로 읽지 않습니다. 기준은 [`ARCHIVE_POLICY.md`](ARCHIVE_POLICY.md)를 따릅니다.

## 팀 역할

| 이름 | 담당 |
|---|---|
| 남현지 | 팀장 · PM · 개발계획서 · 아키텍처 · 판정 상태기계·프로파일(T2) · TCP 계약 · 결정 대장 · UI 설계 · 작품소개서 최종본 · 발표 |
| 임재범 | Flutter 사용자 디스플레이(T6) · 통합 계획 · 저장소 구조·CI 초기 구축 |
| 정정인 | Pi 4 SafetyCore(T1 — 센서·UART·TCP·D-Bus·판정 로그) · ESP32 펌웨어 2종(T3) · fail-safe · 연기·온도 실측 · 시연 운영(공동) |
| 이수현 | 음성 노드(T5 — SenseVoice·명령 파서·ReSpeaker) · 안내 음성·조작음 · 시연 지정 화자 · 시연 영상 · 시연 운영(공동) |
| 김병직 | 비전 노드(T4 — V4L2·ECO.AI YOLOX·고양이 모델·ROI·카메라 영상) · 박스(T3) · 측정·보정 · 시연 운영(공동) · 작품소개서 초안 |

## 저장소 구조

```text
HOOD-SEEK/
├── safety_core/   # Pi 4 SafetyCore — 유일한 판정 (C++17)
├── profiles/      # 판정 파라미터 default.json (코드에 숫자를 박지 않음)
├── vision_node/   # Pi 5 카메라 관측 보고 (C++)
├── voice_node/    # Pi 5 음성 명령 → intent 보고 (C++, 발행은 기본 끔)
├── flutter_app/   # Pi 5 대시보드(15.8인치 158TF3, 1920×1080) (state 표시 + ui_cmd 전송)
├── firmware/      # ESP32 집행과 독립 fail-safe
├── contracts/     # TCP 계약 스키마 5종(state v4 · ui_cmd v2 · voice v1·2 · vision·hello v1) + examples + validate.py
├── deploy/        # 배포 안내, systemd 폴백 유닛
├── scripts/       # GitHub 저장소 설정 스크립트
├── inbox/         # 팀원별 원문과 작업 메모
└── docs/          # 설계서·BRD·결정 대장·실측 기록
```

폴더마다 README가 있습니다: [safety_core](safety_core/README.md) · [flutter_app](flutter_app/README.md) · [vision_node](vision_node/README.md) · [voice_node](voice_node/README.md) · [firmware](firmware/) · [deploy](deploy/README.md) · [contracts](contracts/README.md).

## 개발 규칙

`main`에 직접 푸시하지 않고 `<종류>/<이름>-<이슈번호>-<topic>` 브랜치에서 작업한 뒤 Pull Request와 리뷰를 거쳐 병합합니다([CONTRIBUTING.md](CONTRIBUTING.md)).

- [팀 의사결정 문서](docs/decision/README.md)
- [Pi4 · Pi5 역할, 설계가 맞는가 (검토)](docs/2026-08-30-pi4-pi5-role-design-review.md)
- [S3~S10 설계서 (단일 기준)](docs/2026-10-19-s3-s10-design-v0.1.md)
- [비즈니스 요구사항 정의서 (BRD)](docs/2026-10-19-brd-v0.1.md)
- [S10 전환 체크리스트](docs/s10-transition-checklist.md)
- [하드웨어 실측 사실 (SoT)](docs/hardware-facts.md)
- [팀원별 역할과 대표 기여](CONTRIBUTORS.md)
- [팀원별 작업 자료와 AI 콘텍스트](inbox/README.md)
- [Git 시작 가이드](docs/git-start-guide.md)
- [기여 기록 및 협업 가이드](docs/contribution-guide.md)
- [AI 작업 규칙 · Ponytail full](AGENTS.md)
