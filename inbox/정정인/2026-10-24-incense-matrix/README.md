# 2026-10-24 인센스 원자료 (#65)

설명·조건·요약·재현 명령은 [../2026-10-24-incense-matrix.md](../2026-10-24-incense-matrix.md)에 있습니다.

- 회차 폴더: `A1_1024` … `E1_1024` (무효: `B1_1024` 스틱 고정 안 됨, `C1_1024` 인센스 꺼짐) — 각각 `sps30.csv`·`fan.csv`·`mlx.csv`·`events.csv`·`meta.txt`
- 저녁 회차:
  - `P1old_f0_1024`·`P2old_f2_1024`: 이전 제품 1개(참고용, P2는 팬 전원 빠짐·기준 오염으로 부분 무효)
  - `T3_1024`: 손난로 45/50/55/65 °C + 팬, Core 켬 — `core.log`(Core state, IP는 `<Mac>`·`<Pi5>`로 가림)·`events.csv`(Pi 시각)
  - `D28f3_1024`·`D50f0_1024`: 1개 · 맨 불씨 · 28 cm 팬 3 / 50 cm 팬 0
  - `D45f0h_1024`·`D45f3h_1024`·`D28f0h_1024`·`D28f3h_1024`·`D37f0h_1024`·`D37f3h_1024`: 1개 · 호일 터널(`h`) · 거리 cm · 팬 단수
  - `D28f3_1024_invalid_snuffed`: 무효(놓기 전에 꺼짐)
  - `foiltest_1024/mlx_raw.csv`: 호일 시험, mlx_stream 열만
  - 이벤트를 고친 회차는 원본이 `events.raw.csv`
- `prelim/`: 오전 예비 3회 (이전 제품, 비교 금지)
- `tools/`: `smoke_log.c`·`ev`(기록), `summarize.py`(회차 요약), `replay110.py`(#110 판정 오프라인 재생, `--no-tsize` 변형), `sps30_raw.c`(prelim 형식), `t3_summary.py`·`corewatch_t3.sh`(T3), `d_table.py`(D·H 표)
- 제품 HEM 골드 에디션 스틱 인센스 100p(P1·P2 제외) · 불씨 끝 → SPS30 약 28 cm(D·H는 회차 이름의 거리) · 같은 받침 · 「점화」는 참고용, 분석은 「놓음」 기준
