"""#49 analysis checks: ROI overlap parity with the C++ node, ROI validation, slot scores and rates; #65 Q8 tables."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from detection_table import roi_overlap, validate_roi, slot_best, rate, suggest_threshold, pet

square = [[50, 0], [150, 0], [150, 100], [50, 100]]
assert abs(roi_overlap([0, 0, 100, 100], square) - .5) < 1e-6, "overlap is bbox∩ROI / bbox"
assert roi_overlap([200, 200, 210, 210], square) == 0

roi = dict(frame_w=640, frame_h=480, rotate=0, points=square)
validate_roi(roi)
for bad in (dict(roi, points=[[0, 0], [100, 100], [0, 100], [100, 0]]),   # self-intersecting
            dict(roi, points=[[0, 0], [700, 0], [700, 100]]),             # outside the frame
            dict(roi, points=[[0, 0], [10, 0]]),                          # too few points
            dict(roi, rotate=90)):
    try:
        validate_roi(bad)
    except ValueError:
        continue
    raise AssertionError(f"invalid ROI accepted: {bad}")

best = {"person": None, "cat": {"score": .3, "bbox": [0, 0, 10, 10]}, "dog": None, "teddy bear": {"score": .6, "bbox": [5, 5, 20, 20]}}
assert slot_best(best, ["cat"]) == (.3, [0, 0, 10, 10])
assert slot_best(best, ["cat", "teddy bear"]) == (.6, [5, 5, 20, 20]), "slot takes the best of its classes"
assert slot_best(best, ["dog"]) is None
tie = {"cat": {"score": .5, "bbox": [0, 0, 10, 10]}, "dog": {"score": .5, "bbox": [0, 0, 10, 10]}}
assert pet({"best": tie}, square)[2] == "cat" and slot_best(tie, ["cat", "dog"]) == (.5, [0, 0, 10, 10]), "tie: same winner as slot_best"

assert rate([.6, .2, None], .35) == (1, 3)
assert rate([], .35) == (0, 0)
# Lowest grid threshold strictly above every hard-negative score.
assert suggest_threshold([.33, .12, None]) == .35
assert suggest_threshold([None]) == .25, "no hard negative seen: grid floor"

# #65 Q8 tables, end to end through the CLI. The fixture brings its own ROI and profile so a recalibration (#98)
# or the STEP 8 profile values cannot change what this checks.
import hashlib, json, statistics, subprocess, tempfile
tool = Path(__file__).resolve().parents[1] / "tools" / "detection_table.py"
work = Path(tempfile.mkdtemp())
roi_file, profile_file = work / "roi.json", work / "profile.json"
roi_file.write_text(json.dumps(dict(roi, points=[[200, 200], [500, 200], [500, 460], [200, 460]])))
profile_file.write_text(json.dumps({"P_cat_score": .35, "P_cat_overlap": .25, "P_cat_hits": 5, "P_person_score": .35}))
ov = lambda b: roi_overlap(b, json.loads(roi_file.read_text())["points"])


def frame_best(**found):
    best = dict.fromkeys(("person", "cat", "dog", "teddy bear"))
    best.update({k: {"score": s, "bbox": b} for k, (s, b) in found.items()})
    return best


IN, BOTTOM, OUT0, OUT1 = [300, 300, 400, 380], [300, 420, 400, 480], [20, 300, 120, 380], [150, 300, 250, 380]
conds = {
    ("n01", "negative", "empty", "none"): [frame_best(), frame_best(dog=(.20, [10, 10, 40, 40])), frame_best(), frame_best()],
    ("r01", "negative", "reflection_side", "none"): [frame_best(person=(.40, [250, 300, 350, 450])), frame_best()],
    ("p01", "person", "center", "none"): [frame_best(person=(.60, [180, 0, 470, 110])),
                                          frame_best(person=(.55, [190, 2, 460, 115]), dog=(.31, [200, 0, 260, 40])), frame_best()],
    ("d01", "doll", "in_center", "true"): [frame_best(dog=(.80, IN)), frame_best(dog=(.75, IN), cat=(.40, IN)), frame_best()]
                                          + [frame_best(dog=(.78, IN))] * 5,
    ("d02", "doll", "edge_bottom", "true"): [frame_best(dog=(.60, BOTTOM))] * 2,
    ("d03", "doll", "out_left", "false"): [frame_best(dog=(.70, OUT0), person=(.30, OUT0)), frame_best(dog=(.70, OUT1))],
}


def table(conditions, *extra, cwd=None):
    d = Path(tempfile.mkdtemp())
    manifest = ["id,kind,position,angle,in_roi,note"]
    for (cid, kind, pos, in_roi), fs in conditions.items():
        manifest.append(f"{cid},{kind},{pos},-,{in_roi},")
        recs = [{"event": "start"}] + [{"seq": i + 1, "t_ms": 1000 + 300 * i, "frame_w": 640, "frame_h": 480, "best": b}
                                       for i, b in enumerate(fs)] + [{"event": "stop"}]
        (d / f"{cid}.jsonl").write_text("".join(json.dumps(r) + "\n" for r in recs))
    (d / "manifest.csv").write_text("\n".join(manifest) + "\n")
    run = subprocess.run([sys.executable, "-I", str(tool), "--dir", str(d), "--roi", str(roi_file), *extra],
                         capture_output=True, text=True, cwd=cwd)
    assert run.returncode == 0, run.stdout + run.stderr
    return run.stdout


text = table(conds, "--profile", str(profile_file))
row = lambda start, out=text: ([l for l in out.splitlines() if l.startswith(start)] or [""])[-1]   # last table that has it
assert hashlib.sha256(roi_file.read_bytes()).hexdigest() in text, "header names the ROI by hash"

# #65 rule 3 (09-20 C-2): boundary = normal max + (event min - normal max) * 0.3, separation = gap / pstdev(normal)
for key, normal, event in (("P_cat_score", [0, .20, 0, 0] + [0, 0] + [0, .31, 0], [.80, .75] + [.78] * 5 + [.60] * 2),
                           ("P_cat_overlap", [ov(OUT0), ov(OUT1)], [ov(IN)] * 7 + [ov(BOTTOM)] * 2),
                           ("P_person_score", [0] * 4 + [.40, 0] + [0] * 10 + [.30, 0], [.60, .55])):
    line, gap = row(f"| {key} "), min(event) - max(normal)
    for v in (max(normal), min(event), gap, max(normal) + gap * .3):
        assert f"{v:.3f}" in line, (key, v, line)
    assert f"{gap / statistics.pstdev(normal):.1f}" in line, (key, line)
assert "미검출 1/10" in row("| P_cat_score ") and "미검출 1/3" in row("| P_person_score "), "misses counted"
# d01 hits at 0.35 / 0.25: [1, 1, 0, 1, 1, 1, 1, 1] -> longest run 5; dog wins the slot over cat 0.40
assert row("| d01 ").endswith("| 5 |") and "cat 0 / dog 7" in row("| d01 "), row("| d01 ")
assert "300 ms" in text and "1.5 s" in text, "P_cat_hits 5 at the fixture's 300 ms interval"
# Band: at P_person_score the doll's 0.30 person box drops out; every recorded box still shows in the all-box rows.
# Both directions: real above (fake min - real max) and real below (real min - fake max).
assert row("| y1 | ≥ P_person_score") == "| y1 | ≥ P_person_score | 0 ~ 2 | 300 ~ 300 | 298 | -300 |"
assert row("| y2 | ≥ P_person_score") == "| y2 | ≥ P_person_score | 110 ~ 115 | 450 ~ 450 | 335 | -340 |"
assert row("| y2 | 전체") == "| y2 | 전체 (≥ 0.10) | 110 ~ 115 | 380 ~ 450 | 265 | -340 |"

# An --overlap candidate is labelled as one; the rule 3 "current" column keeps the profile value.
text = table(conds, "--profile", str(profile_file), "--overlap", "0.4")
assert "겹침 후보 0.4 (--overlap, 프로필 P_cat_overlap 0.25)" in text and row("| P_cat_overlap ", text).startswith("| P_cat_overlap | 0.25 |")
# Only one side measured so far: the normal side still shows its max and σ.
text = table({k: v for k, v in conds.items() if k[0] == "n01"}, "--profile", str(profile_file))
assert f"| 4 | 0.200 | {statistics.pstdev([0, .2, 0, 0]):.3f} | 0 (미검출 0/0) | — |" in row("| P_cat_score ", text)
# The default profile is the repo's, wherever the tool is started from.
assert "profiles/default.json" in table(conds, cwd=tempfile.gettempdir()).splitlines()[0]
print("detection table checks passed")
