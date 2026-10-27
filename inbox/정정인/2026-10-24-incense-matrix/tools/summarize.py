#!/usr/bin/env python3
"""smoke_log 회차 폴더를 PM 요청 #65 한 줄 요약 형식으로 정리한다.

  python3 summarize.py runs/B1b_1024 [runs/B2_1024 ...]

- 기준값: meta.txt baseline_pm25 (처음 55 s 평균)
- 놓은 구간: events '놓음' ~ '치움' (없으면 끝까지)
- 포화: PM2.5 원값 > 2000 (Core kSpsPm25Max와 같은 선)
- #110 재현: Δ(=PM2.5−기준) ≥ 50 이고 (포화 또는 tsize < 0.49)가 새 샘플 3개 연속 → SMOKE
- 기준+5 복귀: '치움' 뒤 PM2.5 ≤ 기준+5 가 처음 30 s 연속된 시작 시각
"""
import csv
import statistics
import sys
from pathlib import Path


def load(p):
    with open(p, newline="") as f:
        return list(csv.DictReader(f))


def pct(v, q):
    s = sorted(v)
    return s[min(len(s) - 1, int(len(s) * q))] if s else float("nan")


def summarize(d):
    d = Path(d)
    meta = dict(l.split("=", 1) for l in (d / "meta.txt").read_text().split("\n") if "=" in l)
    base = float(meta["baseline_pm25"])
    ev = {r["event"]: float(r["sec"]) for r in load(d / "events.csv")}
    rows = [r for r in load(d / "sps30.csv") if r["status"] in ("ok", "sat")]
    for r in rows:
        for k in ("sec", "pm1", "pm25", "pm10", "nc05", "nc1", "nc25", "nc10", "tsize"):
            r[k] = float(r[k])
    t_on = ev.get("놓음")
    t_off = ev.get("치움", rows[-1]["sec"])
    win = [r for r in rows if t_on is not None and t_on <= r["sec"] <= t_off]
    pm = [r["pm25"] for r in win]
    sat = [r for r in win if r["pm25"] > 2000]
    ts = [r["tsize"] for r in win]

    smoke, run = None, 0
    for r in rows:
        if t_on is None or r["sec"] < t_on:
            continue
        small = r["pm25"] > 2000 or r["tsize"] < 0.49
        run = run + 1 if (r["pm25"] - base >= 50 and small) else 0
        if run == 3:
            smoke = r["sec"] - t_on
            break

    back = None
    start = None
    for r in rows:
        if r["sec"] < t_off:
            continue
        if r["pm25"] <= base + 5:
            start = r["sec"] if start is None else start
            if r["sec"] - start >= 30:
                back = start - t_off
                break
        else:
            start = None

    mlx = load(d / "mlx.csv")
    # Core T_metric = max − median. 배포본 mlx_stream의 delta 열은 p95 − median이라 쓰지 않는다 (10/24 정정)
    mlx_win = [float(r["max"]) - float(r["median"]) for r in mlx if t_on is not None and t_on <= float(r["sec"]) <= t_off]

    def rng(v, f="{:.3f}"):
        return f"{f.format(min(v))}~{f.format(max(v))}" if v else "-"

    fan = sorted({r["fan_cmd"] for r in win}) or sorted({r["fan_cmd"] for r in rows})
    print(f"■ {d.name}  팬 {','.join(fan)}  이벤트 {', '.join(f'{k}@{v:.0f}s' for k, v in ev.items())}")
    if not win:
        print("  놓은 구간 없음")
        return
    print(
        f"  기준 {base:.1f} | 놓은 {t_off - t_on:.0f}s 동안 PM2.5 중앙 {statistics.median(pm):.0f} · 상위10% {pct(pm, 0.9):.0f}"
        f" · 최대 {max(pm):.1f} | 포화 {len(sat)}줄 ({100 * len(sat) / len(win):.0f}%)"
    )
    print(
        f"  tsize {rng(ts)} (비포화 {rng([r['tsize'] for r in win if r['pm25'] <= 2000])})"
        f" | PM1/2.5 {rng([r['pm1'] / r['pm25'] for r in win])} | PM10/2.5 {rng([r['pm10'] / r['pm25'] for r in win])}"
        f" | nc0.5/nc10 {rng([r['nc05'] / r['nc10'] for r in win if r['nc10']])}"
    )
    print(
        f"  #110 SMOKE: {'놓고 %.0f s' % smoke if smoke is not None else '없음'}"
        f" | 치운 뒤 기준+5 복귀: {'%.0f s' % back if back is not None else '못 돌아옴(기록 끝까지)'}"
        f" | MLX max−median(놓은 동안) {rng(mlx_win, '{:.1f}')} °C"
    )


if __name__ == "__main__":
    for a in sys.argv[1:]:
        summarize(a)
