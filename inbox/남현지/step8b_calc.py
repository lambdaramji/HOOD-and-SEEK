#!/usr/bin/env python3
"""STEP 8 (나) PM 근거 표 — #65 Q4(가습기)·인센스 뒤 P_PM_warn·P_PM_smoke 판단용 (저장소 밖 작업 도구).

입력
  --core CSV:이름:종류:시작-끝   g3_state.py csv 형식(sec,pm_valid,pm25,tsize,baseline,hazard,reasons…). sec 구간.
  --raw  CSV:이름:종류           09-26 형식(sec,pm25,tsize, '#' 주석 줄). baseline = pm25 하위 10 %.
  종류: normal(정상) | mist(가습기·증기, 규칙 3의 「정상」에 넣음) | smoke(인센스, 「사건」)
  --profile profiles/default.json  (P_tsize·P_PM_warn·P_PM_smoke·P_smoke_samples)

지표 (Policy와 같게: 새 샘플 단위, 작은 입자 = tsize < P_tsize, Δ = pm25 − baseline)
  k연속 최소 = 작은 입자 샘플 k(=P_smoke_samples)개 연속 구간의 Δ 최솟값. 경고·연기는 이 값이 임계 이상일 때 난다.
  정상 쪽 값 = normal·mist 구간의 「k연속 최소」 최댓값 (이 값보다 임계가 커야 오탐 0)
  사건 쪽 값 = smoke 구간마다, 정상 최대 Δ를 처음 넘은 뒤 작은 입자 샘플 k개의 Δ 최솟값 → 회차 중 최솟값
  규칙 3 경계 = 정상 + (사건 − 정상) × 0.3, 분리도 = (사건 − 정상) / 정상 σ(normal·mist 작은 입자 Δ의 모표준편차)
"""
import argparse, csv, json, statistics as st
from pathlib import Path

SAT = 2000.0  # safety_core/src/sensors.h kSpsPm25Max


def dedup(samples):
    out, prev = [], None
    for s in samples:
        key = (s["pm25"], s["tsize"])
        if key != prev:
            out.append(s)
        prev = key
    return out


def load_core(spec):
    path, name, kind, rng = spec.split(":")
    lo, hi = map(float, rng.split("-"))
    rows = [r for r in csv.DictReader(open(path, encoding="utf-8")) if lo <= float(r["sec"]) < hi]
    valid = [dict(sec=float(r["sec"]), pm25=float(r["pm25"]), tsize=float(r["tsize"]), base=float(r["baseline"]))
             for r in rows if r["pm_valid"] == "True" and r["pm25"] and r["tsize"] and r["baseline"]]
    # 공백 = pm_valid가 false인 연속 구간 길이(state 시각 기준)
    gap, start = 0.0, None
    for r in rows:
        s = float(r["sec"])
        if r["pm_valid"] != "True":
            start = s if start is None else start
            gap = max(gap, s - start)
        else:
            start = None
    order = ["normal", "caution", "warning", "danger", "fault"]
    hz = max((r["hazard"] for r in rows), key=order.index, default="-")
    return dict(name=name, kind=kind, samples=dedup(valid), gap=gap, hazard=hz,
                fault=any(r["hazard"] == "fault" for r in rows), src=f"{Path(path).name} {lo:g}~{hi:g} s")


def load_raw(spec):
    path, name, kind = spec.split(":")
    lines = [l for l in open(path, encoding="utf-8") if not l.startswith("#")]
    v = [dict(sec=float(r["sec"]), pm25=float(r["pm25"]), tsize=float(r["tsize"])) for r in csv.DictReader(lines)]
    base = sorted(s["pm25"] for s in v)[len(v) // 10]
    for s in v:
        s["base"] = base
    gaps = [b["sec"] - a["sec"] for a, b in zip(v, v[1:])]
    return dict(name=name, kind=kind, samples=v, gap=max(gaps, default=0.0), hazard="-", fault=None,
                src=f"{Path(path).name} (baseline p10 {base:.1f})")


def kmin_runs(samples, ptsize, k):
    """작은 입자 샘플이 k개 연속인 구간마다 Δ 최솟값."""
    out, run = [], []
    for s in samples:
        if s["tsize"] < ptsize:
            run.append(s["pm25"] - s["base"])
            if len(run) >= k:
                out.append(min(run[-k:]))
        else:
            run = []
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--core", action="append", default=[])
    ap.add_argument("--raw", action="append", default=[])
    ap.add_argument("--profile", type=Path, required=True)
    a = ap.parse_args()
    p = json.loads(a.profile.read_text())
    pt, k = p["P_tsize"], p["P_smoke_samples"]
    wins = [load_core(s) for s in a.core] + [load_raw(s) for s in a.raw]

    print(f"판정값 `{a.profile}`: P_tsize {pt}, P_PM_warn {p['P_PM_warn']}, P_PM_smoke {p['P_PM_smoke']}, 연속 {k}\n")
    print("| 구간 | 종류 | 출처 | 샘플 | Δ 최대 | 작은 입자 Δ 최대 | 작은 입자 k연속 최소의 최대 | tsize 범위 | 포화(≥2000) / 그중 큰 입자 | 최장 공백 s | 최고 hazard |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for w in wins:
        s = w["samples"]
        d = [x["pm25"] - x["base"] for x in s]
        ds = [x["pm25"] - x["base"] for x in s if x["tsize"] < pt]
        km = kmin_runs(s, pt, k)
        sat = [x for x in s if x["pm25"] >= SAT]
        print(f"| {w['name']} | {w['kind']} | {w['src']} | {len(s)} | {max(d):.1f} | {max(ds):.1f} | "
              f"{max(km):.1f} | {min(x['tsize'] for x in s):.3f}~{max(x['tsize'] for x in s):.3f} | "
              f"{len(sat)} / {sum(x['tsize'] >= pt for x in sat)} | {w['gap']:.1f} | {w['hazard']} |"
              if s and ds and km else f"| {w['name']} | {w['kind']} | {w['src']} | {len(s)} | — | — | — | — | — | {w['gap']:.1f} | {w['hazard']} |")

    normal = [w for w in wins if w["kind"] in ("normal", "mist")]
    smoke = [w for w in wins if w["kind"] == "smoke"]
    small_n = [x["pm25"] - x["base"] for w in normal for x in w["samples"] if x["tsize"] < pt]
    top = max((m for w in normal for m in kmin_runs(w["samples"], pt, k)), default=None)
    dmax = max(small_n, default=0.0)
    ev = []
    for w in smoke:
        seq = w["samples"]
        i = next((j for j, x in enumerate(seq) if x["pm25"] - x["base"] > dmax), None)
        firsts = [x["pm25"] - x["base"] for x in seq[i:] if x["tsize"] < pt][:k] if i is not None else []
        if len(firsts) == k:
            ev.append((w["name"], min(firsts)))
    print(f"\n**규칙 3 (k연속 지표, k = {k})**\n")
    if top is None or not ev:
        print("정상 또는 사건 자료가 부족합니다.")
        return
    evmin = min(v for _, v in ev)
    sigma = st.pstdev(small_n)
    gap = evmin - top
    print(f"- 정상(normal·mist) k연속 최소의 최대 = **{top:.1f}**, 작은 입자 Δ σ = {sigma:.2f} (n {len(small_n)})")
    print("- 사건(smoke) 회차별 첫 작은 입자 k개 최소 = " + ", ".join(f"{n} {v:.1f}" for n, v in ev) + f" → 최소 **{evmin:.1f}**")
    print(f"- 간격 {gap:.1f}, 분리도 {gap / sigma:.1f}" if sigma else f"- 간격 {gap:.1f}, 분리도 ∞")
    print(f"- 규칙 3 경계 = {top:.1f} + {gap:.1f} × 0.3 = **{top + gap * 0.3:.1f}**")
    for key in ("P_PM_warn", "P_PM_smoke"):
        v = p[key]
        print(f"- 현재 {key} {v}: 정상 쪽 오탐 {'없음' if v > top else '**있음**'} (k연속 최소 최대 {top:.1f}), "
              f"사건 회차 중 이 값 미만 {sum(x < v for _, x in ev)}개")


if __name__ == "__main__":
    main()
