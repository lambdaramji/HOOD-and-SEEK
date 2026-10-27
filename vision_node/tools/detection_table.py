#!/usr/bin/env python3
"""#49 고양이 인형·비스듬 person 검출률 표, #65 Q8 수치 근거 (PC 전용).

입력: measure.sh가 만든 <dir>/manifest.csv와 <dir>/<id>.jsonl, ROI 파일(roi.json), 현재 판정값(profiles/default.json).
출력: 이슈 댓글에 그대로 붙일 Markdown 표.
임계값 '제안'과 규칙 3 경계는 근거일 뿐이다. P_cat_*·P_person_score 결정은 SafetyCore 담당(#57·#58, #65 STEP 8)이 한다.

  python3 vision_node/tools/detection_table.py --dir ~/hoodseek-measure/q8-2026-10-23 --roi vision_node/config/roi.json
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics

import cv2
import numpy as np

GRID = [0.25, 0.30, 0.35, 0.40, 0.45, 0.50, 0.60]
# Which COCO labels vision-node may report in the contract's `cat` slot (#52 comment: class mapping by #49 evidence).
SLOTS = {"cat": ["cat"], "cat+dog": ["cat", "dog"], "cat+teddy": ["cat", "teddy bear"], "cat+dog+teddy": ["cat", "dog", "teddy bear"]}


def validate_roi(roi):
    w, h, pts = roi.get("frame_w"), roi.get("frame_h"), roi.get("points")
    if roi.get("rotate") not in (0, 180):
        raise ValueError("rotate must be 0 or 180")
    if not (isinstance(w, int) and isinstance(h, int) and w > 0 and h > 0):
        raise ValueError("frame_w/frame_h must be positive integers")
    if not (isinstance(pts, list) and 3 <= len(pts) <= 32):
        raise ValueError("points must hold 3..32 [x, y] pairs")
    for p in pts:
        if not (isinstance(p, list) and len(p) == 2 and all(isinstance(v, int) for v in p) and 0 <= p[0] <= w and 0 <= p[1] <= h):
            raise ValueError(f"point must be integer pixels inside the frame: {p}")
    contour = np.array(pts, dtype=np.float32)
    if abs(cv2.contourArea(contour)) <= 0:
        raise ValueError("ROI has no area")
    # Every vertex on the convex hull, in boundary order (rejects bow-ties and star orderings).
    hull = cv2.convexHull(contour, returnPoints=False).flatten().tolist()
    n = len(pts)
    order = hull[hull.index(0):] + hull[:hull.index(0)] if 0 in hull else []
    if order not in (list(range(n)), [0] + list(range(n - 1, 0, -1))):
        raise ValueError("ROI must be a convex polygon in boundary order")
    return roi


def roi_overlap(bbox, points):
    """bbox∩ROI / bbox, same definition as the C++ node (design §5.2, P-003)."""
    x1, y1, x2, y2 = bbox
    area = (x2 - x1) * (y2 - y1)
    if area <= 0:
        return 0.0
    box = np.array([[x1, y1], [x2, y1], [x2, y2], [x1, y2]], dtype=np.float32)
    inter, _ = cv2.intersectConvexConvex(box, np.array(points, dtype=np.float32))
    return float(min(max(inter / area, 0.0), 1.0))


def slot_best(best, labels):
    found = [best[l] for l in labels if best.get(l)]
    if not found:
        return None
    top = max(found, key=lambda c: c["score"])
    return top["score"], top["bbox"]


def rate(scores, threshold):
    return sum(s is not None and s >= threshold for s in scores), len(scores)


def suggest_threshold(negative_scores):
    seen = [s for s in negative_scores if s is not None]
    above = [t for t in GRID if not seen or t > max(seen)]
    return above[0] if above else None


def pct(hit_total):
    hit, total = hit_total
    return f"{hit}/{total} ({100 * hit / total:.0f}%)" if total else "—"


def pet(x, points):
    """(score, roi_overlap, label) of the frame's contract `cat` slot candidate (cat+dog since #89), or None."""
    found = [(x["best"][l]["score"], l, x["best"][l]["bbox"]) for l in SLOTS["cat+dog"] if x["best"].get(l)]
    if not found:
        return None
    score, label, bbox = max(found, key=lambda t: t[0])   # ties go to the earlier label, as in slot_best
    return score, roi_overlap(bbox, points), label


def longest_run(flags):
    best = run = 0
    for f in flags:
        run = run + 1 if f else 0
        best = max(best, run)
    return best


def spread(v):
    return f"{min(v):.3f} · {statistics.median(v):.3f} · {max(v):.3f}" if v else "—"


def rule3(key, current, normal, event, total):
    """#65 규칙 3 (09-20 C-2): 경계 = 정상 최대 + (사건 최소 − 정상 최대) × 0.3, 분리도 = 간격 / 정상 σ(모표준편차)."""
    head = f"| {key} | {current} | {len(normal)} | "
    head += f"{max(normal):.3f} | {statistics.pstdev(normal):.3f} | " if normal else "— | — | "
    head += f"{len(event)} (미검출 {total - len(event)}/{total}) | " + (f"{min(event):.3f} | " if event else "— | ")
    if not normal or not event:
        return head + "— | — | — |"
    top, sigma = max(normal), statistics.pstdev(normal)
    gap = min(event) - top
    sep = f"{gap / sigma:.1f}" if sigma else "∞ (정상 σ 0)"
    return head + f"{gap:.3f} | {sep} | {top + gap * 0.3:.3f} |"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", required=True, type=Path)
    ap.add_argument("--roi", required=True, type=Path)
    ap.add_argument("--profile", type=Path, default=Path(__file__).resolve().parents[2] / "profiles" / "default.json",
                    help="현재 판정값 (기본: 저장소의 profiles/default.json)")
    ap.add_argument("--overlap", type=float, help="P_cat_overlap 후보 (기본: --profile의 값)")
    a = ap.parse_args()
    roi = validate_roi(json.loads(a.roi.read_text()))
    profile = json.loads(a.profile.read_text())
    overlap_label = f"P_cat_overlap {profile['P_cat_overlap']}"
    if a.overlap is None:
        a.overlap = profile["P_cat_overlap"]
    else:
        overlap_label = f"겹침 후보 {a.overlap} (--overlap, 프로필 {overlap_label})"
    rows = list(csv.DictReader((a.dir / "manifest.csv").open(encoding="utf-8")))
    frames = {}
    for r in rows:
        recs = [json.loads(l) for l in (a.dir / f"{r['id']}.jsonl").open() if l.strip()]
        frames[r["id"]] = [x for x in recs if "seq" in x]
        for x in frames[r["id"]]:
            if (x["frame_w"], x["frame_h"]) != (roi["frame_w"], roi["frame_h"]):
                raise SystemExit(f"{r['id']}: frame {x['frame_w']}x{x['frame_h']} does not match ROI — recalibrate")

    def slot_scores(cid, labels, need_overlap):
        out = []
        for x in frames[cid]:
            b = slot_best(x["best"], labels)
            ok = b and (not need_overlap or roi_overlap(b[1], roi["points"]) >= a.overlap)
            out.append(b[0] if ok else None)
        return out

    # #65 Q8: the numbers only hold for this ROI file, so the header names it by hash.
    print(f"ROI `{a.roi}` sha256 `{hashlib.sha256(a.roi.read_bytes()).hexdigest()}` ({roi['frame_w']}×{roi['frame_h']}, rotate {roi['rotate']}), "
          f"겹침 기준 {a.overlap}, 판정값 `{a.profile}`\n")
    print("**조건별 클래스 검출 (검출 하한 이상 프레임 수 / 최고 score)**\n")
    print("| id | 종류 | 위치 | 각도 | ROI | 프레임 | cat | dog | teddy bear | person |\n|---|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        cells = []
        for label in ("cat", "dog", "teddy bear", "person"):
            s = [x["best"][label]["score"] for x in frames[r["id"]] if x["best"].get(label)]
            cells.append(f"{len(s)} / {max(s):.3f}" if s else "0")
        print(f"| {r['id']} | {r['kind']} | {r['position']} | {r['angle']} | {r['in_roi']} | {len(frames[r['id']])} | " + " | ".join(cells) + " |")

    dolls_in = [r["id"] for r in rows if r["kind"] == "doll" and r["in_roi"] == "true"]
    dolls_out = [r["id"] for r in rows if r["kind"] == "doll" and r["in_roi"] == "false"]
    negatives = [r["id"] for r in rows if r["kind"] == "negative"]
    print("\n**cat 슬롯 후보별 근거** — 임계 제안 = hard negative(겹침 기준 통과분) 최고 score보다 높은 첫 격자값\n")
    print("| 슬롯 | hard neg 최고 (전체 / 겹침 통과) | 임계 제안 | ROI 안 인형 검출률 (전체) | 조건별 최저 | ROI 밖 인형 오검출률 |\n|---|---|---|---|---|---|")
    for name, labels in SLOTS.items():
        neg_all = [s for c in negatives for s in slot_scores(c, labels, False)]
        neg_roi = [s for c in negatives for s in slot_scores(c, labels, True)]
        t = suggest_threshold(neg_roi)
        top = lambda xs: f"{max(s for s in xs if s is not None):.3f}" if any(s is not None for s in xs) else "없음"
        if t is None:
            print(f"| {name} | {top(neg_all)} / {top(neg_roi)} | 격자 밖 | — | — | — |")
            continue
        pooled = [s for c in dolls_in for s in slot_scores(c, labels, True)]
        worst = min((rate(slot_scores(c, labels, True), t) for c in dolls_in), key=lambda h: h[0] / h[1] if h[1] else 1, default=(0, 0))
        outside = [s for c in dolls_out for s in slot_scores(c, labels, True)]
        print(f"| {name} | {top(neg_all)} / {top(neg_roi)} | {t:.2f} | {pct(rate(pooled, t))} | {pct(worst)} | {pct(rate(outside, t))} |")

    overlaps = [roi_overlap(b[1], roi["points"]) for c in dolls_in for x in frames[c] if (b := slot_best(x["best"], SLOTS["cat+dog+teddy"]))]
    if overlaps:
        q = statistics.quantiles(overlaps, n=10) if len(overlaps) >= 2 else [overlaps[0]] * 9
        print(f"\nROI 안 인형의 겹침률(검출 프레임 {len(overlaps)}개): 최저 {min(overlaps):.2f}, 하위 10% {q[0]:.2f}, 중앙 {statistics.median(overlaps):.2f}")

    people = [r for r in rows if r["kind"] == "person"]
    if people:
        print("\n**비스듬 각도 person 검출률**\n\n| id | 자세(위치) | 프레임 | ≥0.10 | ≥0.25 | ≥0.35 | ≥0.50 |\n|---|---|---|---|---|---|---|")
        for r in people:
            s = [x["best"]["person"]["score"] if x["best"].get("person") else None for x in frames[r["id"]]]
            print(f"| {r['id']} | {r['position']} {r['note']} | {len(s)} | " + " | ".join(pct(rate(s, t)) for t in (0.10, 0.25, 0.35, 0.50)) + " |")

    # #65 Q8: per-position pet-slot distribution and whether today's profile would reach a lock candidate.
    cs, co, hits, ps = profile["P_cat_score"], a.overlap, profile["P_cat_hits"], profile["P_person_score"]
    pets = {r["id"]: [pet(x, roi["points"]) for x in frames[r["id"]]] for r in rows}
    print(f"\n**Q8 위치별 반려동물 칸(cat+dog)** — hit = score ≥ P_cat_score {cs} ∧ 겹침 ≥ {overlap_label}, 잠금 후보는 연속 P_cat_hits {hits}회\n")
    print("| id | 종류 | 위치 | ROI | 프레임 | 검출 (칸을 차지한 클래스) | score 최소 · 중앙 · 최대 | 겹침 최소 · 중앙 · 최대 | hit | 최장 연속 hit |\n|---|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        seen = [p for p in pets[r["id"]] if p]
        flags = [bool(p) and p[0] >= cs and p[1] >= co for p in pets[r["id"]]]
        won = " / ".join(f"{l} {sum(p[2] == l for p in seen)}" for l in SLOTS["cat+dog"])
        print(f"| {r['id']} | {r['kind']} | {r['position']} | {r['in_roi']} | {len(flags)} | {won} | {spread([p[0] for p in seen])} | "
              f"{spread([p[1] for p in seen])} | {sum(flags)} | {longest_run(flags)} |")
    # P-004: how long P_cat_hits consecutive frames take at the real vision rate (#77: about 3 Hz).
    gaps = [q["t_ms"] - p["t_ms"] for r in rows for p, q in zip(frames[r["id"]], frames[r["id"]][1:]) if "t_ms" in p and "t_ms" in q]
    if gaps:
        med = statistics.median(gaps)
        print(f"\n프레임 간격 중앙값 {med:.0f} ms (≈ {1000 / med:.1f} Hz, 간격 {len(gaps)}개) → 연속 P_cat_hits {hits}회 확인 ≈ {hits * med / 1000:.1f} s")

    # Normal = what must not trigger, event = what must. A frame with nothing seen is score 0 (contract `seen` false).
    no_cat = [r["id"] for r in rows if r["kind"] in ("negative", "person")]
    doll_in, doll_out = [p for c in dolls_in for p in pets[c]], [p for c in dolls_out for p in pets[c] if p]
    person_of = lambda x: x["best"]["person"]["score"] if x["best"].get("person") else 0.0
    no_person = [x for r in rows if r["kind"] in ("negative", "doll") for x in frames[r["id"]]]
    person_in = [person_of(x) for r in people for x in frames[r["id"]]]
    print("\n**Q8 규칙 3 근거** (#65 규칙 3: 경계 = 정상 최대 + (사건 최소 − 정상 최대) × 0.3, 분리도 = 간격 / 정상 σ)\n")
    print("| 키 | 현재 값 | 정상 n | 정상 최대 | 정상 σ | 사건 n | 사건 최소 | 간격 | 분리도 | 규칙 3 경계 |\n|---|---|---|---|---|---|---|---|---|---|")
    print(rule3("P_cat_score", cs, [p[0] if p else 0.0 for c in no_cat for p in pets[c]], [p[0] for p in doll_in if p], len(doll_in)))
    print(rule3("P_cat_overlap", profile["P_cat_overlap"], [p[1] for p in doll_out], [p[1] for p in doll_in if p], len(doll_in)))
    print(rule3("P_person_score", ps, [person_of(x) for x in no_person], [s for s in person_in if s > 0], len(person_in)))
    print("\n- `P_cat_score` 정상 = 빈 장면·반사(negative)와 사람(person) 조건의 모든 프레임, 사건 = ROI 안 인형의 검출 프레임")
    print("- `P_cat_overlap` 정상 = ROI 밖 인형의 검출 프레임, 사건 = ROI 안 인형의 검출 프레임")
    print("- `P_person_score` 정상 = 사람이 화면에 없는 조건(negative·doll)의 모든 프레임, 사건 = 사람 조건의 검출 프레임")

    # #49 PM decision: user-zone band boundary. Only the top person box per frame is recorded, so a reflection
    # outscored by the real person in the same frame is not here; measure reflections with the person out of view.
    boxes = {r["id"]: [x["best"]["person"] for x in frames[r["id"]] if x["best"].get("person")] for r in rows}
    print("\n**Q8 사람 상자 위치 (사람 띠, #49)** — 프레임마다 최고 후보 1개만 기록됨. 반사는 사람이 화면 밖에 있는 조건에서 잰다\n")
    print("| id | 종류 | 위치 | 상자 (≥ P_person_score) | x1 ~ x2 | y1 범위 | y2 범위 |\n|---|---|---|---|---|---|---|")
    for r in rows:
        if b := [p["bbox"] for p in boxes[r["id"]]]:
            print(f"| {r['id']} | {r['kind']} | {r['position']} | {len(b)} ({sum(p['score'] >= ps for p in boxes[r['id']])}) | "
                  f"{min(v[0] for v in b)} ~ {max(v[2] for v in b)} | {min(v[1] for v in b)} ~ {max(v[1] for v in b)} | {min(v[3] for v in b)} ~ {max(v[3] for v in b)} |")
    # SafetyCore only counts a person at P_person_score, so the band is read from those boxes; the all-box rows
    # show what a lower threshold would bring in.
    real = [p for r in people for p in boxes[r["id"]]]
    fake = [p for r in rows if r["kind"] != "person" for p in boxes[r["id"]]]
    print("\n둘 중 한 간격이라도 양수면 가로선 하나로 나뉜다.\n\n"
          "| 축 | 상자 | 진짜 사람 (person 조건) | 가짜 (negative·doll 조건) | 진짜가 위 (가짜 최소 − 진짜 최대) | 진짜가 아래 (진짜 최소 − 가짜 최대) |\n"
          "|---|---|---|---|---|---|")
    for which, floor in (("≥ P_person_score", ps), ("전체 (≥ 0.10)", 0.0)):
        rb, fb = [p["bbox"] for p in real if p["score"] >= floor], [p["bbox"] for p in fake if p["score"] >= floor]
        for name, i in (("y1", 1), ("y2", 3)):
            rng = lambda b: f"{min(v[i] for v in b)} ~ {max(v[i] for v in b)}" if b else "—"
            up = min(v[i] for v in fb) - max(v[i] for v in rb) if rb and fb else "—"
            down = min(v[i] for v in rb) - max(v[i] for v in fb) if rb and fb else "—"
            print(f"| {name} | {which} | {rng(rb)} | {rng(fb)} | {up} | {down} |")


if __name__ == "__main__":
    main()
