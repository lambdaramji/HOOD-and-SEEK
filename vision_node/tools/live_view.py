#!/usr/bin/env python3
"""카메라 화면 보기 (PC). 카메라 위치를 맞추거나, 사람이 화면 밖에 있는지 확인할 때 쓴다.

  python3 vision_node/tools/live_view.py --pi5 root@<Pi5 IP> --roi vision_node/config/roi.json
  python3 vision_node/tools/live_view.py --device 0      # PC에 꽂은 카메라
  조작: 창에서 q = 종료

--pi5는 Pi 5의 GStreamer로 JPEG를 받아 보므로, vision-node가 카메라를 쓰고 있으면 먼저 멈춘다.
기본값(640×480, rotate 0)은 vision-node와 config/roi.json의 값이다.
"""
import argparse
import json
import subprocess

import cv2
import numpy as np

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--device", type=int, default=0, help="카메라 인덱스 (보통 0, 안 나오면 1도 시도)")
parser.add_argument("--pi5", help="Pi 5에 꽂힌 카메라를 ssh로 받아 봄. 예: root@<Pi5 IP> (vision-node가 꺼져 있어야 함)")
parser.add_argument("--rotate", type=int, default=0, choices=[0, 90, 180, 270])
parser.add_argument("--width", type=int, default=640)
parser.add_argument("--height", type=int, default=480)
parser.add_argument("--roi", help="vision_node/config/roi.json을 화면에 빨간 선으로 겹침. 카메라를 캘리브레이션 때 자리로 되돌릴 때 씀")
args = parser.parse_args()
roi = json.load(open(args.roi)) if args.roi else None

rotate_map = {
    90: cv2.ROTATE_90_CLOCKWISE,
    180: cv2.ROTATE_180,
    270: cv2.ROTATE_90_COUNTERCLOCKWISE,
}


def local_frames():
    cap = cv2.VideoCapture(args.device)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
    if not cap.isOpened():
        raise SystemExit(f"카메라 열기 실패: index={args.device}")
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                print("프레임 읽기 실패")
                return
            yield frame
    finally:
        cap.release()


def pi5_frames():
    # Pi 5 (ATLAS) has GStreamer but no OpenCV/ffmpeg: encode JPEG there, split on SOI/EOI markers here.
    # A background watcher kills gst when our stdin hits EOF: a logout once left gst orphaned on Pi 5
    # holding /dev/video0 [실측 2026-10-21]. Our stdin pipe closes however this process ends. The shell
    # waits on gst, so a gst failure (camera busy) still ends the session. fd 3: a background job's
    # stdin is /dev/null in a non-interactive shell.
    pipeline = (f"LLVM_PROFILE_FILE=/dev/null gst-launch-1.0 -q v4l2src device=/dev/video0 ! "
                f"video/x-raw,format=YUY2,width={args.width},height={args.height} ! "
                "videorate ! video/x-raw,framerate=10/1 ! jpegenc quality=80 ! fdsink fd=1 & "
                "gst=$!; exec 3<&0; (cat <&3; kill $gst) >/dev/null 2>&1 & "
                "trap 'kill $gst' HUP INT TERM; wait $gst")
    ssh = subprocess.Popen(["ssh", "-o", "BatchMode=yes", args.pi5, pipeline], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    buf = b""
    try:
        for chunk in iter(lambda: ssh.stdout.read1(65536), b""):
            buf += chunk
            while (start := buf.find(b"\xff\xd8")) >= 0 and (end := buf.find(b"\xff\xd9", start + 2)) >= 0:
                frame = cv2.imdecode(np.frombuffer(buf[start:end + 2], np.uint8), cv2.IMREAD_COLOR)
                buf = buf[end + 2:]
                if frame is not None:
                    yield frame
        print("Pi 5 영상이 끊겼습니다. vision-node가 카메라를 쓰고 있으면 먼저 멈추세요")
    finally:
        ssh.stdin.close()
        ssh.terminate()


frames = pi5_frames() if args.pi5 else local_frames()
print("종료하려면 창에서 q 누르세요")
for frame in frames:
    if args.rotate in rotate_map:
        frame = cv2.rotate(frame, rotate_map[args.rotate])
    if roi:
        # ROI points are pixels of the rotated frame it was clicked on; another size or rotation would misplace the line.
        if (frame.shape[1], frame.shape[0], args.rotate) != (roi["frame_w"], roi["frame_h"], roi["rotate"]):
            frames.close()
            raise SystemExit(f"ROI는 {roi['frame_w']}x{roi['frame_h']} rotate {roi['rotate']} 기준입니다. "
                             f"--width {roi['frame_w']} --height {roi['frame_h']} --rotate {roi['rotate']}로 다시 실행하세요")
        cv2.polylines(frame, [np.array(roi["points"], np.int32)], True, (0, 0, 255), 2)

    cv2.imshow("camera positioning", frame)
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

frames.close()
cv2.destroyAllWindows()
