#!/usr/bin/env python3
"""조리면 ROI 캘리브레이션 (PC). raw 프레임 위에서 LED 판 꼭짓점을 차례로 클릭 → roi.json.

  python3 vision_node/tools/roi_click.py --frame <dump>/frame_000030.ppm --rotate 0 --out vision_node/config/roi.json
  조작: 왼쪽 클릭 = 점 추가, u = 마지막 점 취소, s = 저장, q = 저장 없이 종료
  창 없이: --points "120,300 520,300 600,470 40,470"

--rotate는 그 프레임을 찍을 때 vision-node에 준 값이다(덤프는 회전 후 프레임).
카메라 위치·해상도·회전이 바뀌면 다시 찍는다. 불일치하면 분석 도구와 vision-node가 거부한다.
"""
import argparse
import json
from pathlib import Path
import sys

import cv2
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from detection_table import validate_roi  # noqa: E402


def save(out, frame, rotate, points, note):
    roi = validate_roi(dict(frame_w=frame.shape[1], frame_h=frame.shape[0], rotate=rotate,
                            points=[[int(x), int(y)] for x, y in points], note=note))
    out.write_text(json.dumps(roi, ensure_ascii=False, indent=2) + "\n")
    print(f"saved {out}: {roi['points']}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frame", required=True, type=Path)
    ap.add_argument("--rotate", required=True, type=int, choices=[0, 180])
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--note", default="")
    ap.add_argument("--points", help='"x,y x,y ..." 를 주면 창 없이 저장')
    a = ap.parse_args()
    frame = cv2.imread(str(a.frame))
    if frame is None:
        raise SystemExit(f"cannot read {a.frame}")
    if a.points:
        save(a.out, frame, a.rotate, [tuple(map(int, p.split(","))) for p in a.points.split()], a.note)
        return
    points = []
    cv2.namedWindow("roi")
    cv2.setMouseCallback("roi", lambda ev, x, y, *_: points.append((x, y)) if ev == cv2.EVENT_LBUTTONDOWN else None)
    while True:
        view = frame.copy()
        if len(points) >= 3:
            overlay = view.copy()
            cv2.fillPoly(overlay, [np.array(points, dtype=np.int32)], (0, 200, 255))
            view = cv2.addWeighted(overlay, 0.35, view, 0.65, 0)
        for i, p in enumerate(points):
            cv2.circle(view, p, 4, (0, 0, 255), -1)
            cv2.putText(view, str(i + 1), (p[0] + 6, p[1] - 6), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
        cv2.imshow("roi", view)
        key = cv2.waitKey(30) & 0xFF
        if key == ord("u") and points:
            points.pop()
        elif key == ord("s"):
            try:
                save(a.out, frame, a.rotate, points, a.note)
                break
            except ValueError as e:
                print(f"저장 안 함: {e} — u로 고치세요")
        elif key == ord("q"):
            break
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
