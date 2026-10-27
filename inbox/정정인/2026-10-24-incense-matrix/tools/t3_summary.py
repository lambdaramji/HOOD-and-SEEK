#!/usr/bin/env python3
"""T3(손난로 + 팬) 단계 요약: events.csv의 「안정 X」~「끝 X」 구간마다 Core metric 통계 (#65 10/24).

  python3 t3_summary.py runs/T3_1024 [--trim 55=10]
--trim TAG=S: 「끝 TAG」 S초 전까지만 센다 (T3_1024는 55에서 손난로를 「끝 55」보다 약 10 s 먼저 꺼냄)
「안정 X 전환용」은 「전환 …」까지, 「안정 X 전환후」는 「끝 전환」까지 센다. 「전환 …」 뒤 첫 danger까지 걸린 시간도 보여 준다.
Core state 로그(core.log, journalctl -o short-unix)의 thermal.metric = max − median.
"""
import json
import re
import statistics as st
import sys
from pathlib import Path

args = sys.argv[1:]
trim = {}
if "--trim" in args:
    i = args.index("--trim")
    k, v = args[i + 1].split("=")
    trim[k] = float(v)
    del args[i:i + 2]
d = Path(args[0])
ev = [(float(a), b) for a, b in (l.strip().split(",", 1) for l in open(d / "events.csv") if l[:1].isdigit())]
rows = []
for line in open(d / "core.log", encoding="utf-8", errors="replace"):
    m = re.search(r"\[state\] (\{.*\})", line)
    if m:
        p = json.loads(m.group(1))["p"]
        th = p["sensors"]["thermal"]
        rows.append((float(line.split()[0]), th["metric"], th["max_c"], th["median_c"], p["hazard"], p["fan"]["level"], p["lock"], tuple(p["reasons"])))
for i, (t, e) in enumerate(ev):
    if not e.startswith("안정 "):
        continue
    tag = e[3:]
    if tag.endswith("전환용"):
        end = next((a for a, b in ev[i + 1:] if b.startswith("전환 ")), None)
    elif tag.endswith("전환후"):
        end = next((a for a, b in ev[i + 1:] if b == "끝 전환"), None)
    else:
        end = next((a for a, b in ev[i + 1:] if b == "끝 " + tag), None)
    if end is not None:
        end -= trim.get(tag, 0)
    put = next((a for a, b in reversed(ev[:i]) if b.startswith("넣음")), None)
    if end is None:
        print(f"■ {tag}: 「끝 {tag}」 없음")
        continue
    seg = [r for r in rows if t <= r[0] <= end]
    m = [r[1] for r in seg]
    print(f"■ {tag}: 측정 {end - t:.0f} s, {len(seg)}줄  metric 평균 {st.mean(m):.3f} ±{st.pstdev(m):.3f}  범위 {min(m):.2f}~{max(m):.2f}"
          f"  max {st.mean(r[2] for r in seg):.2f}  median {st.mean(r[3] for r in seg):.2f}")
    print(f"   판정 {sorted(set((r[4], 'fan', r[5], r[6]) for r in seg))}  reasons {sorted(set(r[7] for r in seg))}")
    if put:
        print(f"   넣음 → 안정 {t - put:.0f} s")
for t, e in ev:
    if e.startswith("전환 "):
        hit = next((r[0] for r in rows if r[0] >= t and r[4] == "danger"), None)
        print(f"■ {e}: 첫 danger까지 {hit - t:.1f} s" if hit else f"■ {e}: danger 없음")
