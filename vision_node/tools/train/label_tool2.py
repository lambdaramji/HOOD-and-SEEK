#!/usr/bin/env python3
"""사람·인형 라벨 (PC 전용, 사람 사진은 외부로 내보내지 않음 #72). 결과는 <폴더>/labels.json.

  python3 ~/hoodseek-train/label_tool2.py ~/hoodseek-train/raw-1024-person [배율=2]   # 창 확대, 상자는 640x480 그대로 저장
  p = 사람 상자 모드(빨강, 기본), c = 인형(cat) 상자 모드(초록)
  마우스로 끌면 상자 하나 추가(여러 개 가능), u = 마지막 상자 지우기
  Enter/Space = 저장하고 다음(상자 0개면 빈 장면), d = 버림(흐림·판단 불가), b = 이전, q = 종료(이어서 가능)

사람 상자: 화면에 보이는 그 사람의 모든 부분(다리·몸통·팔·손)을 감싸는 가장 작은 상자, 한 사람에 상자 하나.
화면 끝에서 잘렸으면 화면 끝까지. 유리에 비친 모습은 상자를 그리지 않는다.
"""
import json
import pathlib
import sys

import cv2

root = pathlib.Path(sys.argv[1]).expanduser()
SCALE = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0  # window zoom; boxes are saved in original 640x480 pixels
out = root / "labels.json"
labels = json.loads(out.read_text()) if out.exists() else {}
pre_path = root / "prefill.json"  # optional suggested boxes {name: [{cls, box}]} (bgsub_prefill.py, teacher_prefill.py): shown first, fix or accept with Enter
pre = json.loads(pre_path.read_text()) if pre_path.exists() else {}
items = sorted(root.glob("*.png"))
COLOR = {"person": (0, 0, 255), "cat": (0, 255, 0)}
boxes, drag, cur, mode = [], None, None, "person"


def on_mouse(event, x, y, flags, _):
    global drag, cur
    x, y = max(0, min(639, int(x / SCALE))), max(0, min(479, int(y / SCALE)))
    if event == cv2.EVENT_LBUTTONDOWN:
        drag, cur = (x, y), None
    elif event == cv2.EVENT_MOUSEMOVE and drag:
        cur = (min(drag[0], x), min(drag[1], y), max(drag[0], x), max(drag[1], y))
    elif event == cv2.EVENT_LBUTTONUP and drag:
        b = (min(drag[0], x), min(drag[1], y), max(drag[0], x), max(drag[1], y))
        if b[2] - b[0] > 3 and b[3] - b[1] > 3:
            boxes.append({"cls": mode, "box": list(b)})
        drag, cur = None, None


win = "label2 (p=person c=cat drag=add u=undo Enter=save d=drop b=back q=quit)"
cv2.namedWindow(win)
cv2.setMouseCallback(win, on_mouse)
i = next((k for k, p in enumerate(items) if p.name not in labels), len(items))
print(f"전체 {len(items)}장, 남은 것 {len(items) - len(labels)}장")
while i < len(items):
    path = items[i]
    image = cv2.imread(str(path))
    old = labels.get(path.name)
    boxes = [dict(b) for b in (old if isinstance(old, list) else pre.get(path.name, []))]
    while True:
        shown = image.copy()
        for b in boxes:
            cv2.rectangle(shown, tuple(b["box"][:2]), tuple(b["box"][2:]), COLOR[b["cls"]], 2)
        if cur:
            cv2.rectangle(shown, cur[:2], cur[2:], COLOR[mode], 1)
        cv2.putText(shown, f"{i + 1}/{len(items)} {path.name} mode={mode} boxes={len(boxes)}", (10, 25),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)
        cv2.imshow(win, cv2.resize(shown, None, fx=SCALE, fy=SCALE, interpolation=cv2.INTER_LINEAR))
        k = cv2.waitKey(20) & 0xFF
        if k == ord("p"):
            mode = "person"
        elif k == ord("c"):
            mode = "cat"
        elif k == ord("u") and boxes:
            boxes.pop()
        elif k in (13, 32):
            labels[path.name] = boxes
            break
        elif k == ord("d"):
            labels[path.name] = "drop"
            break
        elif k == ord("b") and i > 0:
            i -= 2
            break
        elif k == ord("q"):
            out.write_text(json.dumps(labels, indent=1))
            raise SystemExit(f"중단. {len(labels)}장 저장 → {out}")
    out.write_text(json.dumps(labels, indent=1))
    i += 1
cv2.destroyAllWindows()
vals = [v for v in labels.values() if isinstance(v, list)]
print(f"끝. 사람 상자 {sum(b['cls'] == 'person' for v in vals for b in v)} / 인형 상자 {sum(b['cls'] == 'cat' for v in vals for b in v)}"
      f" / 빈 장면 {sum(not v for v in vals)} / 버림 {list(labels.values()).count('drop')} → {out}")
