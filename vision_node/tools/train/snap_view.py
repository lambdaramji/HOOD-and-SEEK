#!/usr/bin/env python3
"""학습 사진 수집 (PC). Pi 5 카메라 화면을 보면서 s를 누르면 그 장면을 PNG로 저장한다.

  python3 ~/hoodseek-train/snap_view.py --pi5 root@<Pi5 IP> --tag led1
  조작: s = 저장, q = 종료. 저장 순간 화면에 손·팔·사람이 없어야 한다.

vision_node/tools/live_view.py --pi5와 같은 방식(Pi 5 GStreamer JPEG)이다. 학습용이라 JPEG 품질을 95로 올렸고,
ROI 선과 글자는 보여 주기만 하고 저장하는 사진에는 넣지 않는다. vision-node가 카메라를 쓰고 있으면 먼저 멈춘다.
"""
import argparse
import json
import os
import pathlib
import subprocess
import time

import cv2
import numpy as np

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--pi5", required=True, help="Pi 5 ssh 대상. 예: root@<Pi5 IP>")
parser.add_argument("--tag", required=True, help="파일 이름 앞부분. 예: led1, led0")
parser.add_argument("--roi", default=str(pathlib.Path(os.environ.get("HOODSEEK_REPO", pathlib.Path.home() / "HOOD-SEEK")) / "vision_node/config/roi.json"))
parser.add_argument("--out", default=str(pathlib.Path.home() / "hoodseek-train/raw-1023/snap"))
parser.add_argument("--auto", type=float, default=0.7, help="r로 켠 동안 이 간격(초)마다 자동 저장")
parser.add_argument("--delay", type=float, default=0.0, help="r을 누른 뒤 이만큼(초) 기다렸다 자동 저장 시작 (화면 밖으로 나갈 시간)")
parser.add_argument("--duration", type=float, default=0.0, help="0보다 크면 자동 저장을 이만큼(초) 하고 저절로 멈춤")
args = parser.parse_args()
roi = json.load(open(args.roi))
out = pathlib.Path(args.out)
out.mkdir(parents=True, exist_ok=True)
count = len(list(out.glob(f"{args.tag}_*.png")))

# Same pipeline and orphan-gst guard as live_view.py --pi5, JPEG quality 95.
pipeline = ("LLVM_PROFILE_FILE=/dev/null gst-launch-1.0 -q v4l2src device=/dev/video0 ! "
            "video/x-raw,format=YUY2,width=640,height=480 ! "
            "videorate ! video/x-raw,framerate=10/1 ! jpegenc quality=95 ! fdsink fd=1 & "
            "gst=$!; exec 3<&0; (cat <&3; kill $gst) >/dev/null 2>&1 & "
            "trap 'kill $gst' HUP INT TERM; wait $gst")
ssh = subprocess.Popen(["ssh", "-o", "BatchMode=yes", args.pi5, pipeline], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
buf = b""
recording, last_save, rec_start, rec_end = False, 0.0, 0.0, 0.0
print(f"s = 저장, r = 자동 저장 켜기/끄기({args.auto} s마다), q = 종료. 저장 위치 {out} (지금 {args.tag} {count}장)")
try:
    for chunk in iter(lambda: ssh.stdout.read1(65536), b""):
        buf += chunk
        frame = None
        while (start := buf.find(b"\xff\xd8")) >= 0 and (end := buf.find(b"\xff\xd9", start + 2)) >= 0:
            frame = cv2.imdecode(np.frombuffer(buf[start:end + 2], np.uint8), cv2.IMREAD_COLOR)
            buf = buf[end + 2:]
        if frame is None:
            continue
        shown = frame.copy()
        cv2.polylines(shown, [np.array(roi["points"], np.int32)], True, (0, 0, 255), 2)
        cv2.putText(shown, f"{args.tag} {count}" + (" REC" if recording else ""), (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.9,
                    (0, 0, 255) if recording else (0, 255, 255), 2)
        cv2.imshow("snap (s = save, r = auto, q = quit)", shown)
        key = cv2.waitKey(1) & 0xFF
        now = time.monotonic()
        if key == ord("r"):
            recording = not recording
            rec_start = now + args.delay
            rec_end = rec_start + args.duration if args.duration > 0 else float("inf")
            print("자동 저장 " + (f"켜짐 ({args.delay:.0f} s 뒤 시작)" if recording else "꺼짐"))
        if recording and now >= rec_end:
            recording = False
            print("\a자동 저장 끝 (시간 다 됨)")
        if key == ord("s") or (recording and rec_start <= now and now - last_save >= args.auto):
            last_save = time.monotonic()
            path = out / f"{args.tag}_{count:04d}.png"
            cv2.imwrite(str(path), frame)
            count += 1
            print(f"저장 {path.name}")
        elif key == ord("q"):
            break
    else:
        print("Pi 5 영상이 끊겼습니다. vision-node가 카메라를 쓰고 있으면 먼저 멈추세요")
finally:
    ssh.stdin.close()
    ssh.terminate()
    cv2.destroyAllWindows()
    print(f"끝. {args.tag} {count}장")
