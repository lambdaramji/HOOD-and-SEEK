#!/usr/bin/env python3
"""smoke_log sps30.csv를 현재 main(#110) SafetyCore 연기·PM 판정과 히스테리시스로 다시 돌린다 (오프라인 추정).

  python3 replay110.py runs/D1_1024 [...]
  python3 replay110.py --no-tsize runs/D1_1024 [...]   # 변형: small 조건(포화 ∨ tsize < 0.49)을 빼고 Δ만 본다

옮긴 것 (safety_core/src/policy.cpp, profiles/default.json, 10/24 main 91c3efd 기준):
- PM2.5는 2000에서 자르고 2000 초과면 pm_saturated (sensors.cpp kSpsPm25Max)
- small = pm_saturated or tsize < P_tsize(0.49)
- baseline: 처음 P_baseline_init_s(60) 동안 중앙값, 그 뒤 Δ<P_PM_warn ∧ hazard≤caution일 때만 τ=300 s로 따라감
- smoke = small ∧ Δ≥P_PM_smoke(50) 새 샘플 3개 연속 → danger, pm_warn = small ∧ Δ≥30 3개 연속 → warning
- 내려갈 때: danger는 10 s, 그 밖은 5 s 동안 계속 낮아야 그동안 본 가장 높은 아래 단계로
옮기지 않은 것: 열화상, fault, vision, lock. → 이 재생은 「연기 경로만」의 hazard다.
  실제 Core는 불씨가 MLX 시야에 있으면 T_metric(max − median)으로 따로 danger가 걸린다(3개 이상 회차 대부분 ≥ 30.4 °C,
  D1core 실측). 연기 경로 해제가 hazard 해제로 이어지는 것은 열원이 시야 밖이거나 식었을 때다.
"""
import csv
import statistics
import sys
from pathlib import Path

P = dict(tsize=0.49, smoke=50, warn=30, n=3, init=60, tau=300, rel=5, rel_danger=10)
NAMES = {0: "normal", 2: "warning", 3: "danger"}


NO_TSIZE = False


def replay(d):
    d = Path(d)
    ev = [(float(r["sec"]), r["event"]) for r in csv.DictReader(open(d / "events.csv"))]
    rows = [r for r in csv.DictReader(open(d / "sps30.csv")) if r["status"] in ("ok", "sat")]
    t_on = next((t for t, e in ev if e == "놓음"), None)
    t_off = next((t for t, e in ev if e == "치움"), None)
    base, win, last = None, [], None
    smoke_run = warn_run = 0
    hazard, rel_since, rel_target = 0, None, 0
    changes, prev = [], 0
    for r in rows:
        t, raw_pm, ts = float(r["sec"]), float(r["pm25"]), float(r["tsize"])
        sat = raw_pm > 2000
        pm = min(raw_pm, 2000.0)
        small = True if NO_TSIZE else (sat or ts < P["tsize"])
        if base is None:
            win.append(pm)
            if t >= P["init"]:
                base = statistics.median(win)
        else:
            dlt = pm - base
            smoke_run = smoke_run + 1 if small and dlt >= P["smoke"] else 0
            warn_run = warn_run + 1 if small and dlt >= P["warn"] else 0
            if dlt < P["warn"] and hazard <= 1 and last is not None:
                base += min(1.0, (t - last) / P["tau"]) * (pm - base)
        last = t
        raw = 3 if base is not None and smoke_run >= P["n"] else 2 if base is not None and warn_run >= P["n"] else 0
        if raw >= hazard:
            hazard, rel_since = raw, None
        else:
            if rel_since is None:
                rel_since, rel_target = t, raw
            else:
                rel_target = max(rel_target, raw)
            if t - rel_since >= (P["rel_danger"] if hazard == 3 else P["rel"]):
                hazard, rel_since = rel_target, None
        if hazard != prev:
            changes.append((t, prev, hazard, raw_pm, ts))
            prev = hazard
    print(f"■ {d.name}  baseline {base:.1f}  이벤트 " + ", ".join(f"{e}@{t:.0f}" for t, e in ev))
    for t, a, b, pm, ts in changes:
        where = ""
        if t_on is not None:
            where = f"놓고 {t - t_on:+.0f}s" if t_off is None or t <= t_off else f"치우고 {t - t_off:+.0f}s"
        print(f"  {t:6.1f}s ({where:>10})  {NAMES[a]:>7} → {NAMES[b]:<7}  PM2.5 원값 {pm:8.1f}  tsize {ts:.3f}")
    if t_on is not None and t_off is not None:
        drops = [c for c in changes if t_on < c[0] <= t_off and c[2] < c[1]]
        print(f"  ▶ 놓은 동안 위험이 풀린 횟수: {len(drops)}")


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--no-tsize":
        NO_TSIZE, args = True, args[1:]
        print("[변형] small = True (tsize·포화 조건 없음)")
    for a in args:
        replay(a)
