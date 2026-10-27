#!/usr/bin/env python3
"""아두이노 온습도 줄을 Mac 시각과 함께 CSV로 남긴다 (#65 10/24).

  ~/.platformio/penv/bin/python rh_log.py rh_mist2_1024.csv          # Ctrl-C로 끝
  ~/.platformio/penv/bin/python rh_log.py rh_mist2_1024.csv 240      # 240 s 뒤 저절로 끝
  ~/.platformio/penv/bin/python rh_log.py rh_mist2_1024.csv 240 /dev/cu.usbmodem1101

Pi 4 시각과 맞추려고 시작할 때 Mac 시각을 찍는다. 나중에 ssh root@$PI4 date와 비교해 차이를 뺀다.
"""
import glob
import sys
import time

import serial  # PlatformIO penv에 들어 있음

if len(sys.argv) < 2:
    sys.exit(__doc__)
out_path = sys.argv[1]
dur_s = float(sys.argv[2]) if len(sys.argv) > 2 else float("inf")
port = sys.argv[3] if len(sys.argv) > 3 else (sorted(glob.glob("/dev/cu.usbmodem*")) or [None])[0]
if not port:
    sys.exit("아두이노 포트를 못 찾음 — USB 연결 확인")
ser = serial.Serial(port, 115200, timeout=2)
with open(out_path, "x") as f:  # 기존 파일을 덮어쓰지 않는다
    f.write("mac_clock,mac_epoch,uno_ms,type,temp_c,rh\n")
    print(f"기록 시작 {time.strftime('%H:%M:%S')} ({port} → {out_path})")
    t0 = time.time()
    try:
        while time.time() - t0 < dur_s:
            line = ser.readline().decode(errors="replace").strip()
            if not line or line.startswith("#"):
                continue
            now = time.time()
            clock = time.strftime("%H:%M:%S", time.localtime(now))
            f.write(f"{clock},{now:.2f},{line}\n")
            f.flush()
            print(clock, line)
    except KeyboardInterrupt:
        pass
    print("끝", time.strftime("%H:%M:%S"))
