# 정정인 Inbox

임베디드, 통신과 fail-safe에 관해 정정인이 정리한 작업 자료를 저장합니다.

새 문서는 상위 [Inbox 작성 규칙](../README.md)을 따릅니다.

## 문서 목록

| 문서 | 내용 |
|---|---|
| [2026-10-24-incense-matrix.md](2026-10-24-incense-matrix.md) | **#65 인센스 원자료(10/24 PM 형식)** — Core 정지·독립 로거로 팬 단수·개수별 SPS30 원값·MLX·팬 ACK, `tsize` 전제 재검토, #110 오프라인 재생, T3 손난로 + 팬 metric, 인센스 1개 거리별(호일로 불씨 가림) |
| [2026-10-22-g3-remeasure-data.md](2026-10-22-g3-remeasure-data.md) | **#65 G3 재실측 원자료** — SafetyCore state 로그·CSV·이벤트 시각표, Q별 요약, fault 해제 tick 발견 |
| [2026-10-22-sps30-saturation.md](2026-10-22-sps30-saturation.md) | **SPS30 고농도 공백** — 2000 초과 포화 처리·재시작 조건·폐기 로그, 10/23 인센스 확인 계획 |
| [2026-10-04-single-display-led-cooktop.md](2026-10-04-single-display-led-cooktop.md) | 듀얼 디스플레이 미지원에 따른 단일 디스플레이 전환, LED 쿡탑, 인덕션 단계 조절(합의 필요) |
| [2026-10-19-led-matrix-power-without-capacitor.md](2026-10-19-led-matrix-power-without-capacitor.md) | 1000 µF 커패시터 없이 WS2812B 매트릭스를 연결하는 임시 절차 (연결 순서·전류 상한) |
| [2026-09-26-sensor-logic-handoff.md](2026-09-26-sensor-logic-handoff.md) | **판단 로직 담당 전달용** 열화상·SPS30 요약 |
| [2026-09-25-wiring-guide.md](2026-09-25-wiring-guide.md) | 전체 부품 배선, 시각 동기화, ESP32, ReSpeaker |
| [2026-09-20-demo-box-threshold-measurement.md](2026-09-20-demo-box-threshold-measurement.md) | 시연 박스 임계값 측정 절차와 결과 |
| [2026-09-20-display-touch-verification.md](2026-09-20-display-touch-verification.md) | 디스플레이 2종 배선·터치 검증 |
| [2026-09-20-pi5-display-troubleshooting.md](2026-09-20-pi5-display-troubleshooting.md) | Pi5 화면 출력 장애 조사 |
| [2026-09-02-pi4-safety-authority-test.md](2026-09-02-pi4-safety-authority-test.md) | P-009 ARC-001 Pi4 Dart 실행 시험 (머지 통합 재작성, 판정 불가) |
| [2026-09-02-hardware-status-and-spec.md](2026-09-02-hardware-status-and-spec.md) | 09-02 하드웨어 스펙 (일부 수치는 09-26 문서로 대체 — 각 문서의 「이전 문서 수정 필요 사항」 참고) |
| [2026-09-02-sensor-metrics-analysis.md](2026-09-02-sensor-metrics-analysis.md) | 09-02 센서 지표 분석 |
| [2026-09-02-progress-report.md](2026-09-02-progress-report.md) | 09-02 진행 현황 |

측정 도구: [mlx_bundle.sh](mlx_bundle.sh) · [sps30_bundle.sh](sps30_bundle.sh) (마스터 PC에서 크로스 컴파일) · [mlx_watch.sh](mlx_watch.sh) · [sps30_watch.sh](sps30_watch.sh) · [ntp_sync.sh](ntp_sync.sh) (Pi `/data`에 설치)
