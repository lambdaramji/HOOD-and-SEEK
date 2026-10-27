# LG 회신 기준 위배 서술 전수 정정 계획

- 작성자: 임재범
- 작성일: 2026-09-03
- 상태: draft — 담당자 확인 필요
- 카테고리: process, vision-ai
- 조사 대상: 2026-09-03 LG 회신으로 무효·약화된 저장소 활성 서술 전체
- 선행 문서: [LG 회신 검토와 반영 계획](2026-09-03-lg-answer-review.md), [열린 PR 11건 적대적 검토](2026-09-02-open-pr-adversarial-review.md)

## 표기 규칙

| 표기 | 뜻 |
|---|---|
| **[실측]** | 이 checkout에서 명령을 실행해 확인 (명령은 「재현 방법」에 있음) |
| **[회신]** | 2026-09-03 LG 회신. 출처는 선행 문서에 있다 |
| **[추정]** | 우리 해석. 확인 방법을 함께 적는다 |

## 요약

전수 조회로 **위배 서술 20곳**을 찾았다. 심각도는 셋으로 갈린다. [실측]

| 심각도 | 수 | 뜻 |
|---|---:|---|
| **무효** | 4 | 사실이 아니게 됐거나 실행 불가능한 절차가 됐다 |
| **근거 약화** | 5 | 수치는 유효하지만 결론의 근거로 쓸 수 없다 |
| **해석 정정** | 5 | 사실은 맞지만 잘못된 해석을 유도한다 |
| **보강** | 6 | 새로 확정된 사실을 반영해야 한다 |

**코드와 계약은 수정할 것이 하나도 없다.** `packages/`, `contracts/`, `pi5_gateway/`, `pi4_sensor_node/`, `flutter_app/lib/`, `firmware/`에서 `VisionService`·`ecoai`·`qnn`·`xnnpack`·`tflite`·`yolo` 문자열이 걸리는 곳은 `pi4_sensor_node/README.md` 산문 한 줄뿐이다. [실측] **`D-005`(backend 문자열을 allowlist로 제한하지 않는다)와 runtime 독립 계약 원칙 덕분에 이번 뒤집힘의 비용이 문서에만 남았다.** 이 사실 자체를 `D-005`의 유지 근거로 기록할 만하다.

**진행을 막는 제약이 하나 있다.** 위배 20곳 중 **9곳이 열린 PR과 같은 파일·같은 줄을 공유한다.** `docs/ai-integration-decisions.md`는 #19·#21·#24·#27이, `docs/decision/P-009-...md`는 #18·#19·#21·#22·#24가 이미 건드리고 있다. 지금 정정 PR을 올리면 충돌이 하나 더 늘어난다. 그래서 **무경합 11곳을 먼저 고치고(R1), 경합 9곳은 열린 PR 배치가 정리된 뒤에 한 PR로 묶는다(R2).**

## 정정 — 어제 문서의 xnnpack [추정] 철회

어제 [LG 회신 검토](2026-09-03-lg-answer-review.md) ①에서 "통합 API 백엔드 이름 4개에 xnnpack이 없다"를 미확인으로 올렸다. **저장소 안에 이미 대응 관계가 적혀 있다.** [실측]

> `gpu_cuda`는 NVIDIA, `htp_qnn`은 퀄컴 전용이라 **Pi 4에서는 `cpu`(XNNPACK)가 사실상 유일한 선택지**입니다.
> — [`inbox/김병직/2026-08-29-atlas-platform-baseline.md:82`](../김병직/2026-08-29-atlas-platform-baseline.md)

즉 **LG가 Pi4에 권장한 xnnpack은 통합 API의 `cpu` 백엔드를 가리킨다.** 새 백엔드 이름이 아니다. 재문의 항목에서 빼고, 남는 확인은 "`cpu` 백엔드가 실제로 XNNPACK delegate로 실행되는지"의 실행 로그 수준으로 낮춘다. 재문의는 11건에서 10건이 된다.

## 위배 서술 전수 목록

### 무효 — 사실이 아니거나 실행 불가능

| # | 위치 | 현재 서술 | 왜 무효인가 | 어떻게 | 경합 |
|---|---|---|---|---|---|
| N1 | `docs/ai-integration-decisions.md:52` | 하드웨어 검증 순서 2 — "내장 VisionService 원형 경로를 수정 없이 실행해 모델 존재와 lifecycle을 확인한다" | **실행할 수 없는 절차다.** ⑥으로 그 인터페이스가 Pi5 미지원이고, ④로 원본 모델은 제공 대상이 아니다 | 2번을 **"권장 경로(video profile 이미지 + OpenCV 직접 추론, 또는 nnstreamer)를 실행해 프레임 취득과 추론을 확인한다"**로 교체 | #19·#21·#24·#27 |
| N2 | `inbox/김병직/2026-08-30-device-survey-and-vision-test.md:487` | "**[추정]** 모델 파일만 확보되면 Baseline이 되살아납니다" | **두 번 반증됐다.** ④ 모델 파일은 별도 제공 대상이 아니고, ⑥ 인터페이스 자체가 Pi5 미지원이다 | 해당 [추정]을 **반증 기록으로 교체.** "Baseline은 되살아나지 않는다. Fallback이 유일한 경로다"로 결론 확정 | 없음 |
| N3 | `docs/final-demo-architecture.md:34` | "Cat AI \| **내장 VisionService 또는** 외부 runtime을 `cat-detection` 계약에 연결" | 내장 경로는 선택지가 아니다 | **"내장 VisionService 또는"을 삭제.** "권장 경로(video profile + 직접 추론 / nnstreamer)로 `cat-detection` 계약에 연결"로 | #20 |
| N4 | `docs/archive/to-정인.md:44` | "Atlas OS의 `VisionService`, `libai_vision_atlas_plugin.so`, ECO.AI/TFLite/QNN 라이브러리와 내장 모델이 보드에 실제 존재하는지 확인해 달라" | **존재 확인이 목적을 잃었다.** 있어도 Pi5에서 지원되지 않는다 | 요청을 **"권장 경로의 전제 확인"으로 교체** — OpenCV 런타임 유무, nnstreamer 설치 여부, video profile 프레임 취득 명령, 모델 설정의 `NPU_QNN_HTP` 잔존 여부 | #23 |

### 근거 약화 — 수치는 유효, 결론 근거로는 불가

| # | 위치 | 현재 서술 | 어떻게 | 경합 |
|---|---|---|---|---|
| W1 | `inbox/archive/category/vision-ai.md:10` | "[실측] Pi5의 Atlas `VisionService1`에서 GPU backend와 확보한 YOLOX를 사용하면 3.7–3.8 FPS로 동작한다" | 수치 유지 + **"LG는 이 인터페이스를 RPi5 미지원으로 회신(2026-09-03). 미지원 경로 측정값이며 권장 경로 재측정 전까지 아키텍처 확정 근거로 쓰지 않는다"** 병기 | 없음 |
| W2 | `inbox/김병직/2026-08-30-open-questions-and-decisions.md:40` | `P-006` 행 "후보 하나가 실측 완료됐습니다" | **"미지원 경로에서 완료. 권장 경로 재측정 필요"**로 조건 추가 | 없음 |
| W3 | 같은 파일 `:38` | `P-009` 행 "근거 보탬. **B안 쪽으로 기웁니다**" | **"A안은 ⑤로 불가 확정. 단 B안 근거는 ⑥으로 약화 — 재측정 뒤 판단"**으로 교체 | 없음 |
| W4 | `docs/2026-08-30-pi4-pi5-role-design-review.md:80` | "실측 보탬(확정 아님): … `VisionService1`은 Pi5에만 있고…" | ⑤·⑥ 회신을 각주로 붙여 **경로 자체가 미지원임**을 명시 | #21 |
| W5 | `inbox/김병직/2026-08-30-device-survey-and-vision-test.md:16~18, 74` | 요약 표의 "✅ 동작 · 3.7 FPS", "원인이었던 백엔드 설정 ✅ 해결" | 표 위에 **한 줄 단서**를 넣는다. 표 자체는 그대로 둔다 — 조사 당시 기록이므로 사후 편집하지 않는다 | 없음 |

### 해석 정정 — 사실은 맞지만 오해를 유도

| # | 위치 | 현재 서술 | 어떻게 | 경합 |
|---|---|---|---|---|
| I1 | `inbox/김병직/2026-08-29-atlas-platform-baseline.md:142` | "모델이 **설정만 있고 파일이 없습니다.** 또한 백엔드가 퀄컴 NPU 전용으로…" | ④(제공 대상 아님)·①(퀄컴 전용이라 Pi5 사용 불가) 회신 병기. **결함이 아니라 정상 상태**임을 명시 | 없음 |
| I2 | `inbox/archive/category/vision-ai.md:11` | "플랫폼 원본 YOLOv8·YOLO11 모델 파일은 없지만 YOLOX 대체 경로는 동작한다" | **"원본 파일은 배포 누락이 아니라 별도 제공 대상이 아니다(④)"**로 정정 | 없음 |
| I3 | `inbox/김병직/2026-08-30-device-survey-and-vision-test.md:159` | "**[실측]** 조회 메서드는 설정 파일만 보고 목록을 만듭니다" | ④로 의미 확정 — **LG 회신(9/3) 결론 — LG가 내부에서 시험한 모델 목록이고 사용 가능한 전체 목록은 아님** 추가 | 없음 |
| I4 | `inbox/김병직/2026-08-30-open-questions-and-decisions.md:62` | `B-2` "…어디서 받나요? 배포 누락인가요?" | **답변 도착으로 종료 처리.** "누락 아님(④)" | 없음 |
| I5 | `docs/ai-integration-decisions.md:40` | `P-002` 질문 "내장 YOLOv8 W8A8 모델·라벨·**VisionService plugin이 실제 존재하는가**" | **질문을 바꾼다.** 존재 여부가 아니라 **"권장 경로의 artifact와 실행 경로가 확보되는가"** | #19·#21·#24·#27 |

### 보강 — 새로 확정된 사실 반영

| # | 위치 | 어떻게 | 경합 |
|---|---|---|---|
| A1 | `docs/ai-integration-decisions.md:44` `P-006` | backend 권장 확정 반영: **Pi5 TFLite = GPU**(OpenCL 없음, OpenGL fallback), **Pi4 = `cpu`(XNNPACK)** — Pi4 GPU는 CPU 대비 약 5배 느림. 완료 근거에 **"권장 경로에서 측정"** 조건 추가 | #19·#21·#24·#27 |
| A2 | 같은 파일 `:45` `P-007` | 근거 추가: 공식 권장 모델 없음, LG도 상용 라이선스 제한으로 YOLOX 고려. **YOLOX 우선 후보로 기록.** 결정권자는 현지, 플랫폼 제공물 라이선스 표기 확인은 미완 유지 | 위와 같음 |
| A3 | 같은 파일 `:33` `D-005` | 결정 유지. 변경 조건에 **"권장 backend는 회신으로 확정됐으나 실행 경로 재측정 전까지 allowlist 도입 보류"** 추가. **이번 뒤집힘에서 코드 수정이 0건이었던 사실을 유지 근거로 기록** | 위와 같음 |
| A4 | 같은 파일 `:23` | "실제 카메라 캡처, 모델 로딩, Atlas `VisionService`, …는 구현 완료로 표시하지 않는다" — 문장은 유효하나 `VisionService`를 목표 경로처럼 나열한다. **목록에서 빼고 권장 경로로 대체** | 위와 같음 |
| A5 | `docs/decision/P-009-...md` A/B 비교표 「비전 성능」·「Pi4 부하」 | **논거 교체:** A안 불가 이유는 성능이 아니라 **Headless 이미지에 Vision AI 기능 부재**([문서] 확정). 동시에 **B안의 Pi5 비전도 `VisionService`가 아닌 직접 경로**임을 명시 | #18·#19·#21·#22·#24 |
| A6 | 같은 문서 「선행 확인」 | **권장 경로 비전 재측정**을 선행 확인 항목으로 추가 | 위와 같음 |

부수 항목: `inbox/김병직/2026-08-30-open-questions-and-decisions.md` B절에서 **`B-1′`·`B-2`·`B-4` 종료**, **`B-5`·`B-6` 부분 답변**으로 갱신하고, `B-3`·`B-12`는 미답 최우선을 유지한다. `inbox/김병직/2026-08-29-atlas-platform-baseline.md:82`의 XNNPACK 대응은 ①로 확정 표기한다.

## 경합 지도

정정 대상 파일이 열린 PR과 겹치는 관계다. [실측]

| 파일 | 건드리는 열린 PR | 정정 항목 |
|---|---|---|
| `docs/ai-integration-decisions.md` | #19 · #21 · #24 · #27 | N1, I5, A1~A4 |
| `docs/decision/P-009-...md` | #18 · #19 · #21 · #22 · #24 | A5, A6 |
| `docs/final-demo-architecture.md` | #20 | N3 |
| `docs/archive/to-정인.md` | #23 | N4 |
| `docs/2026-08-30-pi4-pi5-role-design-review.md` | #21 | W4 |
| `inbox/**` (4개 파일) | 없음 | N2, W1~W3, W5, I1~I4 |

## 실행 계획

### R1 — 지금 착수. 무경합 11곳, 1 PR

- **대상:** `inbox/archive/category/vision-ai.md`, `inbox/김병직/2026-08-29-atlas-platform-baseline.md`, `inbox/김병직/2026-08-30-device-survey-and-vision-test.md`, `inbox/김병직/2026-08-30-open-questions-and-decisions.md`
- **항목:** N2, W1·W2·W3·W5, I1·I2·I3·I4, B절 갱신, XNNPACK 대응 확정
- **주의:** 셋은 **김병직 담당 문서**다. `CLAUDE.md`는 "남의 담당 문서를 상의 없이 `deprecated`로 바꾸지 않는다"고 정한다. 따라서 **초안 PR로 올리고 병직·수현의 확인을 받은 뒤 병합한다.** 조사 당시 기록(요약 표, 시간 순 절)은 사후 편집하지 않고 **단서만 덧붙인다.**
- **검증:** 상대 링크 대상 존재 확인, `git diff --check`. 이 경로는 CI 트리거 밖이므로 사람 리뷰가 유일한 게이트다
- **담당:** 재범 초안 → 병직·수현 확인

### R2 — 열린 PR 배치 정리 후. 경합 9곳, 1 PR

- **선행 조건:** `docs/ai-integration-decisions.md`와 `docs/decision/P-009-...md`를 건드리는 열린 PR(#18·#19·#21·#22·#24·#27)이 **병합되거나 닫혀야 한다.** 순서는 [적대적 검토](2026-09-02-open-pr-adversarial-review.md)의 「병합 순서」를 따르되, 아래 두 가지가 LG 회신으로 바뀐다.
  - **#24는 재작성 또는 닫음.** 「카메라 USB 연결 = 승인」의 근거가 미지원 인터페이스 측정값이다. 대리 서명 문제(B3)와 별개로 내용 근거가 재측정 대기다
  - **#26은 병합 전 강화.** 결론 2를 "`VisionService` 경로는 후보에서 제외"로, 결론 6에 근거 약화 사유를 추가한다
- **항목:** N1·N3·N4, W4, I5, A1~A6
- **원칙:** **대장 표는 이 PR에서 한 번만 고친다.** 여러 PR이 같은 줄을 고치는 것이 이번 배치의 근본 문제였다
- **담당:** 재범 초안 → 현지(대장·P-009 결정권자) 승인

### R3 — 재측정 결과 도착 후

- 권장 경로 실측을 `inbox/`에 기록하고, 기존 3.7~3.8 FPS와 **같은 표에 나란히** 남긴다
- 그 결과로 대장 `P-002`·`P-006`의 완료 근거를 갱신하고, `P-009`의 비전 배치를 결정한다
- **담당:** 수현·병직 측정 → 재범 문서 반영 → 현지 결정

### R4 — 재문의 답변 후

- **Pi4 Video profile 이미지 제공 여부**에 따라 `P-009` A안을 완전히 닫을지 확정한다. 지금은 ⑤가 Headless 정의로만 답해 이미지 교체 선택지가 남는지 알 수 없다
- **`B-3`(읽기 전용 `/etc`의 정식 설정 변경 방법)** 답변이 오면, 모델 설정의 `NPU_QNN_HTP` 잔존 수정을 정식 절차로 처리한다. 지금은 remount 상태다
- **담당:** 병직 문의 → 정인 적용

## 재측정 계획

기존 3.7~3.8 FPS를 대체할 근거를 만든다. **합격선 2 FPS 기준은 그대로 유지한다.**

| # | 무엇 | 산출물 | 담당 |
|---|---|---|---|
| V-1 | Pi5에서 video profile 프레임 취득 경로를 확정한다. ⑥이 언급한 `start-process` 계열 CLI 또는 `Camera1` 경유 | 실행한 명령 원문, 프레임 포맷·해상도·fps, 실패 시 오류 원문 | 병직 |
| V-2 | V-1 프레임에 OpenCV + YOLOX로 직접 추론 | 사람·사물 FPS, p95 지연, RSS, 발열, 명령 원문. **기존 값과 같은 표에 배치** | 수현 |
| V-3 | nnstreamer 가용성 확인 후 동일 측정 | `gst-inspect-1.0 \| grep -i nnstreamer` 출력, 패키지 조회 결과, 가능하면 FPS | 정인 |

추가 확인: Pi5의 OpenCV 런타임 유무(`python3 -c "import cv2; print(cv2.__version__)"`, `pkg-config --modversion opencv4`). 기존 `B-10`은 Pi4만 대상이었다.

**탑뷰 고양이 검출(`B-12`)은 이 재측정과 별개로 남아 있다.** 경로를 바꿔도 각도 문제는 그대로다.

## 재문의 목록 (10건)

어제 11건에서 xnnpack 항목을 뺐다.

| # | 질문 | 왜 필요한가 |
|---|---|---|
| 1 | Pi4에도 Video profile 이미지를 제공하는가 | ⑤가 직접 답하지 않았다. `P-009` A안을 완전히 닫을 수 있는지가 여기 달렸다 |
| 2 | `start-process` 계열 CLI의 실제 명령·입출력 형식, ⑤가 언급한 inference tutorial의 문서명·절 | 권장 경로를 구현할 수 없으면 재측정이 막힌다 |
| 3 | 바이너리로만 제공된다는 것의 범위 — 우회가 필수인지, 설정으로 켤 수 있는지 | 우회 범위를 정해야 설계가 확정된다 |
| 4 | `deprecated` 이후 권장 backend 표기 | ①에서 미답 |
| 5 | 플랫폼 제공 검출 모델(YOLOv8·YOLO11)의 라이선스 표기 | `B-5` 잔여. `P-007` 판단 근거 |
| 6 | 읽기 전용 `/etc`의 정식 설정 변경 방법 (`B-3`) | **미답 최우선.** 현재 remount 상태다 |
| 7 | 탑뷰 고양이 검출률과 권장 설치 각도 (`B-12`) | **미답.** 계획된 설치가 탑뷰다 |
| 8 | nnstreamer가 두 보드 이미지에 포함되는가 | 권장 경로 후보 중 하나의 전제 |
| 9 | 다른 모델 설정에도 `NPU_QNN_HTP` 기본값이 남아 있는가 (배포 기준) | ①로 Pi5 부적합이 확인됐다. 전수 확인 기준이 필요하다 |
| 10 | 이 회신의 대외 취급 기준 | 저장소 인용 범위를 정하기 위해 |

## 재현 방법

```bash
# 위배 서술 전수 조회 (아카이브 제외)
grep -rn -E 'VisionService|VisionPipeline|libai_vision' --include='*.md' . \
  | grep -v -E '^\./(docs/archive|inbox/archive)/'

grep -rn -E '배포 누락|파일이 없|파일 부재|어디서 받' --include='*.md' . \
  | grep -v -E '^\./(docs/archive|inbox/archive)/'

grep -rn -E 'NPU_QNN|htp_qnn|백엔드가 CPU 하나|backend 문자열|allowlist로 제한' \
  --include='*.md' --include='*.dart' . | grep -v -E '^\./(docs/archive|inbox/archive)/'

# 코드·계약에 비전 경로가 하드코딩되지 않았음을 확인 (README 산문 1줄만 걸린다)
grep -rn -iE 'visionservice|ecoai|qnn|xnnpack|tflite|yolo' \
  packages contracts pi5_gateway pi4_sensor_node flutter_app/lib firmware

# 경합 확인 — 정정 대상 파일을 건드리는 열린 PR
for n in 16 18 19 20 21 22 23 24 25 26 27; do
  git fetch origin "refs/pull/$n/head:pr$n" 2>/dev/null
  echo "--- PR#$n"; git diff --name-only "$(git merge-base pr$n origin/main)" "pr$n"
done
```

## 미확인 사항

| 항목 | 확인 방법 | 담당 |
|---|---|---|
| `cpu` 백엔드가 실제로 XNNPACK delegate로 실행되는가 | Pi4에서 `--backend cpu` 실행 로그의 delegate 표기를 남긴다 | 수현 |
| #24를 재작성할지 닫을지 | 작성자·결정권자 판단. R2의 선행 조건이다 | 현지 |
| 위배 20곳 외에 누락이 있는가 | 위 「재현 방법」의 조회를 R1·R2 병합 직전에 다시 돌려 대조한다 | 재범 |
| 플랫폼 인터페이스 가용성을 전제하지 않는다는 원칙을 `AGENTS.md`에 넣을지 | **(제안)** 이번 사례가 근거다. 팀 합의 필요 | 현지 |

## 근거 목록

- **[실측]** 위 「재현 방법」의 조회를 `main` = `100a6e1`에서 실행
- **[회신]** 2026-09-03 LG 회신 — 출처와 취급은 [LG 회신 검토](2026-09-03-lg-answer-review.md) 「회신 취급」에 있다
- **[문서]** [열린 PR 11건 적대적 검토](2026-09-02-open-pr-adversarial-review.md) — 충돌 매트릭스와 병합 순서
- **[문서]** [`docs/ai-integration-decisions.md`](../../docs/ai-integration-decisions.md), [`docs/decision/P-009-pi4-pi5-role-reallocation.md`](../../docs/decision/P-009-pi4-pi5-role-reallocation.md), [`docs/final-demo-architecture.md`](../../docs/archive/final-demo-architecture.md), [`docs/archive/to-정인.md`](../../docs/archive/to-정인.md), [`docs/2026-08-30-pi4-pi5-role-design-review.md`](../../docs/2026-08-30-pi4-pi5-role-design-review.md)
- **[문서]** [`inbox/archive/category/vision-ai.md`](../archive/category/vision-ai.md), [기기 조사와 비전 실측](../김병직/2026-08-30-device-survey-and-vision-test.md), [확인·결정 목록](../김병직/2026-08-30-open-questions-and-decisions.md), [ATLAS 플랫폼 기준](../김병직/2026-08-29-atlas-platform-baseline.md), [모델 통합](../김병직/2026-08-29-vision-model-consolidation.md)
