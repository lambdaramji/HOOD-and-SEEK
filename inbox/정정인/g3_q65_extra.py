# Q6 회차별 프레임 수, Q3 회차별 PM 공백·fault→normal, Q2 단계 전환 시각·상승 속도 (g3_1021_core_state.csv, sec 기준)
import csv
r = list(csv.DictReader(open("g3_1021_core_state.csv")))
print("Q6 회차: 최대 metric, metric≥16 줄 수, metric≥16 서로 다른 값(≈프레임) 수, 처음~끝 sec")
for n, (a, b) in enumerate([(5080, 5095), (5110, 5135), (5150, 5172)], 1):
    w = [x for x in r if a <= float(x["sec"]) < b and x["metric"]]
    hi = [x for x in w if float(x["metric"]) >= 16]
    print(f"  {n}: max {max(float(x['metric']) for x in w):.2f}, ≥16 {len(hi)}줄, 값 {len({x['metric'] for x in hi})}개, "
          f"{hi[0]['sec']}~{hi[-1]['sec']} s")
print("Q3 회차: PM 공백(pm.valid=false 연속, 시작 sec·길이 s), fault→normal 줄")
for n, (a, b) in enumerate([(5366, 5960), (5961, 6760), (6761, 7535)], 1):
    w = [x for x in r if a <= float(x["sec"]) < b]
    gaps, s, prev = [], None, "True"
    for x in w:
        if x["pm_valid"] == "False" and prev == "True": s = float(x["sec"])
        if x["pm_valid"] == "True" and prev == "False": gaps.append((s, float(x["sec"]) - s))
        prev = x["pm_valid"]
    fn = [x for i, x in enumerate(w[1:], 1) if x["hazard"] == "normal" and w[i - 1]["hazard"] == "fault"]
    last_bad = max((float(x["sec"]) for x in w if x["hazard"] in ("fault", "danger")), default=None)
    print(f"  {n}: 공백 {len(gaps)}회 {[(round(g[0], 1), round(g[1], 1)) for g in gaps]}, 최대 {max(g[1] for g in gaps):.1f} s")
    for x in fn:
        print(f"     fault→normal {x['sec']} s hazard={x['hazard']} fan={x['fan_level']}/{x['fan_min']} pm25={x['pm25']} tsize={x['tsize']} reasons={x['reasons']}")
    print(f"     마지막 fault/danger {last_bad} s")


def at(s):
    return min((x for x in r if x["metric"]), key=lambda x: abs(float(x["sec"]) - s))


print("Q2 단계 전환: 꺼냄(직전 단계 끝 뒤 metric 1 이상 하락 첫 줄)·넣음(caution 재진입)·안정 시작, 넣음→안정 평균 상승 속도")
for name, end, ins, stab in [("45→50", 2579, 2715.0, 2772), ("50→55", 2863, 2986.1, 3055),
                             ("55→60", 3146, 3263.1, 3313), ("60→65", 3495, 3549.7, 3697)]:
    base = float(at(end)["metric"])
    out = next(x for x in r if end <= float(x["sec"]) < ins and x["metric"] and float(x["metric"]) < base - 1.0)
    x, y = at(ins), at(stab)
    rate = (float(y["metric"]) - float(x["metric"])) / (float(y["sec"]) - float(x["sec"]))
    print(f"  {name}: 꺼냄 {out['sec']} s ({out['metric']}) → 넣음 {x['sec']} s ({x['metric']}) → 안정 {y['sec']} s ({y['metric']}), "
          f"{rate:.3f}/s")
