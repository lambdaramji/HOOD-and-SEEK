#!/usr/bin/env python3
"""#65 A 방식: SafetyCore [state] 로그 → CSV · 구간 통계 · hazard 전이.

입력은 journalctl -o short-unix 줄이든 safetycore 직접 실행 stdout이든 "[state] {json}"만 찾아 읽는다.

  python3 g3_state.py csv   g3_core_state.log > g3_core_state.csv
  python3 g3_state.py stats g3_core_state.log metric 120 210      # sec 120~210 구간
  python3 g3_state.py split g3_core_state.log metric 0 60 300 390  # 정상 구간 vs 사건 구간 (09-20 C-3 식)
  python3 g3_state.py trans g3_core_state.log                      # hazard·lock·fan 단계가 바뀐 시각

sec = 첫 줄 t_ms 기준 경과 초. wall = journal 시각(unix, Pi 4 시계 — NTP 없음).
같은 센서 샘플이 여러 state에 반복될 수 있어(state 1 Hz, MLX 프레임 약 1 Hz) 통계는 `--dedup`이면 값이 바뀐 줄만 쓴다.
"""
import json
import statistics as st
import sys

COLS = ["sec", "wall", "t_ms", "hazard", "lock", "presence", "fan_level", "fan_min", "fan_ack",
        "ind_level", "allowed_on", "ind_ack", "th_valid", "max_c", "median_c", "metric",
        "pm_valid", "pm25", "tsize", "baseline", "alert", "reasons"]


def rows(path):
    t0 = None
    for line in open(path, encoding="utf-8", errors="replace"):
        i = line.find("[state] ")
        if i < 0:
            continue
        try:
            m = json.loads(line[i + 8:])
        except json.JSONDecodeError:
            continue  # 잘린 줄 (Ctrl-C 직전 등)
        p = m["p"]
        th, pm = p["sensors"]["thermal"], p["sensors"]["pm"]
        t0 = m["t_ms"] if t0 is None else t0
        head = line[:i].split()
        yield {
            "sec": round((m["t_ms"] - t0) / 1000, 1),
            "wall": head[0] if head and head[0].replace(".", "").isdigit() else "",
            "t_ms": m["t_ms"], "hazard": p["hazard"], "lock": p["lock"], "presence": p["presence"],
            "fan_level": p["fan"]["level"], "fan_min": p["fan"]["min"], "fan_ack": p["fan"]["ack"],
            "ind_level": p["induction"]["level"], "allowed_on": p["induction"]["allowed_on"],
            "ind_ack": p["induction"]["ack"], "th_valid": th["valid"], "max_c": th["max_c"],
            "median_c": th["median_c"], "metric": th["metric"], "pm_valid": pm["valid"],
            "pm25": pm["pm25"], "tsize": pm["tsize"], "baseline": pm["baseline"],
            "alert": p.get("alert"), "reasons": "|".join(p.get("reasons", [])),
        }


def pick(path, col, a, b, dedup):
    out, last = [], None
    for r in rows(path):
        v = r[col]
        if v is None or not (a <= r["sec"] < b):
            continue
        if dedup and v == last:
            continue
        out.append(float(v))
        last = v
    return out


def desc(name, v):
    if not v:
        return f"{name}: n=0 (값 없음 — 구간·valid 확인)"
    return (f"{name}: n={len(v)} 평균={st.mean(v):.3f} σ={st.pstdev(v):.3f} "
            f"최소={min(v):.3f} 최대={max(v):.3f}")


def main(argv):
    dedup = "--dedup" in argv
    argv = [a for a in argv if a != "--dedup"]
    if len(argv) < 2:
        sys.exit(__doc__)
    cmd, path = argv[0], argv[1]
    if cmd == "csv":
        print(",".join(COLS))
        for r in rows(path):
            print(",".join("" if r[c] is None else str(r[c]) for c in COLS))
    elif cmd == "stats":
        col, a, b = argv[2], float(argv[3]), float(argv[4])
        print(desc(f"{col} [{a:g},{b:g})", pick(path, col, a, b, dedup)))
    elif cmd == "split":
        col = argv[2]
        n = pick(path, col, float(argv[3]), float(argv[4]), dedup)
        e = pick(path, col, float(argv[5]), float(argv[6]), dedup)
        print(desc("정상", n))
        print(desc("사건", e))
        if n and e:
            gap = min(e) - max(n)
            print(f"간격(사건 최소 − 정상 최대) = {gap:.3f}")
            if gap <= 0:
                print("⚠️ 간격 ≤ 0: 두 분포가 겹침 → 경계로 쓰지 않고 같은 단계로 묶음 (#65 절차서 3절 규칙 1)")
            print(f"분리도 = {gap / (st.pstdev(n) or 1e-9):.2f}")
            print(f"제안 경계(오탐 기준, ×0.3) = {max(n) + gap * 0.3:.3f}")
            print(f"제안 경계(온도 단계, 중간값) = {(max(n) + min(e)) / 2:.3f}")
            print("※ tsize처럼 작아야 사건인 지표는 정상·사건 최소/최대를 뒤집어 해석")
    elif cmd == "trans":
        prev = None
        for r in rows(path):
            key = (r["hazard"], r["lock"], r["fan_level"], r["ind_level"])
            if key != prev:
                print(f'{r["sec"]:>8}s  hazard={r["hazard"]:<8} lock={r["lock"]:<13} '
                      f'fan={r["fan_level"]} ind={r["ind_level"]} metric={r["metric"]} '
                      f'pm25={r["pm25"]} reasons={r["reasons"]}')
                prev = key
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
