# Pi4·Pi5 역할 설계 검토

- 작성자: 임재범
- 작성일: 2026-08-30
- 상태: verified
- 카테고리: project-management
- 관련 이슈/PR: `P-009`
- 본문: [Pi4 · Pi5 역할, 설계가 맞는가](../../docs/2026-08-30-pi4-pi5-role-design-review.md)

## 요약

논리 경계(관측 / 판단 / 집행 / 표시)는 맞다. 현행 보드 매핑은 ATLAS 프로파일에 맞춘 타협이고, P-009는 센서 옆에서 판단하는 대안이다. 이 메모는 확정이 아니다. 드라이버를 심기 전에 A/B를 고른다.

## 확인된 사실

- 정책은 `hood_seek_protocol` 한곳, Flutter는 판단하지 않는다.
- 센서는 Pi4 I2C 예정, 팬 USB는 Pi5. 랜선이 안전 경로 안에 있다.
- `pi4_sensor_node`는 데모 4샘플 순환. `cat-detection` 발행자가 없다.

## 결정 사항

- 이 검토로 P-009를 닫지 않는다. 결정권자는 현지·정인.
- 바꿔서는 안 되는 네 계층은 본문에 적었다.

## 미확인 사항

- ATLAS Headless Dart AOT, ESP32를 Pi4로, 카메라를 Pi5로 — P-009 선행 확인.

## 근거

- [P-009 본문](../../docs/decision/P-009-pi4-pi5-role-reallocation.md)
- [ATLAS 플랫폼 기준](../김병직/2026-08-29-atlas-platform-baseline.md)
