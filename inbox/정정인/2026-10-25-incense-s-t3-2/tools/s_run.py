#!/usr/bin/env python3
"""S 회차(인센스 개수 × 팬) 한 줄씩 — 포화 비율·최장 연속 포화·15연속 처음 시점 (#65 10/25).

  python3 s_run.py S37n1f3_1025 S37n1f2_1025:96:293.9 ...

- 구간: 기본은 「놓음」 ~ 정확히 「치움」. `회차:시작:끝`(초)으로 바꿀 수 있다
  (팬이 놓은 중간에 바뀐 회차, 재점화·문 닫힘처럼 앞부분을 버리는 회차)
- 포화: PM2.5 원값 > 2000, status ok·sat 줄만
- 15연속: D-054 heavy 조건 `P_heavy_smoke_samples`(15)와 같은 길이. SPS30 샘플은 1 s
- 「15연속 처음」은 구간 시작부터 잰 초
"""
import csv
import statistics
import sys
from collections import Counter
from pathlib import Path

HEAVY = 15


def run(arg):
    name, *cut = arg.split(":")
    d = Path(name)
    ev = [(float(r["sec"]), r["event"]) for r in csv.DictReader(open(d / "events.csv"))]
    t0 = float(cut[0]) if cut else next(t for t, e in ev if e == "놓음")
    t1 = float(cut[1]) if cut else next(t for t, e in ev if e == "치움")
    seg = [r for r in csv.DictReader(open(d / "sps30.csv"))
           if t0 <= float(r["sec"]) <= t1 and r["status"] in ("ok", "sat")]
    pm = [float(r["pm25"]) for r in seg]
    best = cur = 0
    firsts = []
    for r, p in zip(seg, pm):
        cur = cur + 1 if p > 2000 else 0
        best = max(best, cur)
        if cur == HEAVY:
            firsts.append(round(float(r["sec"]) - t0))
    fan = dict(Counter(r["fan_cmd"] for r in seg))
    print(f"| {d.name} | {t0:.1f}~{t1:.1f} | {len(pm)} | {100 * sum(p > 2000 for p in pm) / len(pm):.0f}%"
          f" | {statistics.median(pm):.0f} | {best} | {', '.join(map(str, firsts)) or '-'} | {fan} |")


if __name__ == "__main__":
    print(f"| 회차 | 구간 s | 샘플 | 포화 | PM2.5 중앙 | 최장 연속 | {HEAVY}연속 처음 (구간+s) | fan_cmd |")
    print("|---|---|---|---|---|---|---|---|")
    for a in sys.argv[1:]:
        run(a)
