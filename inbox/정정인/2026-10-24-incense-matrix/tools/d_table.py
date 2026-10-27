#!/usr/bin/env python3
"""D·H 회차(인센스 1개 거리별) 한 줄씩 표로 (#65 10/24).

  python3 d_table.py D28f3_1024 D50f0_1024 D45f0h_1024 ...

- 놓은 구간: 「놓음」 ~ 정확히 「치움」인 이벤트 (「치움 입력만 …」은 건너뜀)
- 포화: PM2.5 원값 > 2000 · Δ = PM2.5 − baseline_pm25(meta.txt, 처음 55 s 평균)
- MLX: max − median (Core T_metric과 같은 식). 배포본 mlx_stream의 delta 열(p95 − median)은 쓰지 않는다
- 복귀: 「치움」 뒤 PM2.5 ≤ 기준+5가 처음 30 s 이어진 시작까지
"""
import csv
import statistics
import sys
from pathlib import Path


def run(d):
    d = Path(d)
    meta = dict(l.split("=", 1) for l in (d / "meta.txt").read_text().split("\n") if "=" in l)
    base = float(meta["baseline_pm25"])
    ev = [(float(r["sec"]), r["event"]) for r in csv.DictReader(open(d / "events.csv"))]
    t_on = next(t for t, e in ev if e == "놓음")
    t_off = next(t for t, e in ev if e == "치움")
    rows = [r for r in csv.DictReader(open(d / "sps30.csv")) if r["status"] in ("ok", "sat")]
    pm = [float(r["pm25"]) for r in rows if t_on <= float(r["sec"]) <= t_off]
    sat = sum(p > 2000 for p in pm)
    over = sum(p - base >= 50 for p in pm)
    mlx = [float(r["max"]) - float(r["median"]) for r in csv.DictReader(open(d / "mlx.csv")) if t_on <= float(r["sec"]) <= t_off]
    back, start = None, None
    for r in rows:
        t, p = float(r["sec"]), float(r["pm25"])
        if t < t_off:
            continue
        if p <= base + 5:
            start = t if start is None else start
            if t - start >= 30:
                back = start - t_off
                break
        else:
            start = None
    print(
        f"| {d.name} | {base:.1f} | {t_off - t_on:.0f} | {100 * sat / len(pm):.0f}% | {statistics.median(pm):.0f} | {max(pm):.0f}"
        f" | {100 * over / len(pm):.0f}% | {min(mlx):.1f}~{max(mlx):.1f} | {sum(m >= 13.4 for m in mlx)}/{len(mlx)}"
        f" | {'%.0f' % back if back is not None else '-'} |"
    )


if __name__ == "__main__":
    print("| 회차 | 기준 PM2.5 | 놓은 s | 포화 | PM2.5 중앙 | 최대 | Δ≥50 | MLX max−median | ≥13.4 | 복귀 s |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for a in sys.argv[1:]:
        run(a)
