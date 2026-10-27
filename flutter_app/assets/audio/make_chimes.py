"""HOOD&SEEK 차임 4개 (CAUTION·WARNING·APP_START·TOUCH_FEEDBACK) 생성 — Python 표준 라이브러리만.

기존 PM 차임을 보존·재사용한다. D-059부터 정상 중불·강불은 무음이며 CAUTION은 계약 ID·파일만 남긴다.
수현 님 음성 클립과 같은 형식: PCM 16-bit, 모노, 48 kHz.
차임은 앞 120 ms·뒤 200 ms 무음, 조작음은 앞 5 ms·뒤 15 ms 무음.
사인파 + 약한 배음, 음마다 짧은 어택과 지수 감쇠. AI 음성·외부 음원 없음.

    python3 make_chimes.py   # → 이 폴더의 CAUTION·WARNING·APP_START·TOUCH_FEEDBACK.wav (같은 입력이면 같은 바이트)
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 48000
LEAD_S, TRAIL_S = 0.120, 0.200
TARGET_RMS_DB = -20.0  # 차임 본체 RMS. 수현 님 음성 클립의 말하는 구간 RMS는 약 −16.7 dBFS — 순음은 같은 RMS라도
                       # 더 크게 들리고 정상 조리 중에 울리므로 조금 작게 둔다
PEAK_MAX = 0.70        # 약 −3.1 dBFS를 넘지 않게


def note(freq, dur, harmonics=((1, 1.0), (2, 0.18), (3, 0.06)), attack=0.006, decay=4.5):
    n = int(RATE * dur)
    out = []
    for i in range(n):
        t = i / RATE
        env = min(1.0, t / attack) * math.exp(-decay * t)
        tail = min(1.0, (n - i) / (RATE * 0.010))  # 끝 10 ms 페이드 — 클릭 방지
        s = sum(a * math.sin(2 * math.pi * freq * k * t) for k, a in harmonics)
        out.append(env * tail * s)
    return out


def chime(notes, gap, **note_options):
    body = []
    for freq, dur in notes:
        body += note(freq, dur, **note_options) + [0.0] * int(RATE * gap)
    body = body[: len(body) - int(RATE * gap)]
    rms = math.sqrt(sum(x * x for x in body) / len(body))
    gain = 10 ** (TARGET_RMS_DB / 20) / rms
    gain = min(gain, PEAK_MAX / max(abs(x) for x in body))
    body = [x * gain for x in body]
    return [0.0] * int(RATE * LEAD_S) + body + [0.0] * int(RATE * TRAIL_S)


def write(path, samples):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(round(x * 32767))) for x in samples))


if __name__ == "__main__":
    out = Path(__file__).resolve().parent
    out.mkdir(exist_ok=True)
    # 기존 CAUTION 보존: 낮은 두 음 상행 E5 → A5, 부드럽게
    write(out / "CAUTION.wav", chime([(659.25, 0.26), (880.00, 0.34)], gap=0.04))
    # 기존 WARNING 상행 차임은 앱 기동음으로 보존한다.
    write(out / "APP_START.wav", chime([(783.99, 0.18), (1046.50, 0.18), (1318.51, 0.30)], gap=0.03))
    # 새 WARNING: 같은 높이의 전자음을 네 번 반복한다.
    write(out / "WARNING.wav", chime([(720.0, 0.18)] * 4, gap=0.09,
        harmonics=((1, 1.0), (3, 0.22), (5, 0.06)), attack=0.008, decay=1.6))

    # TOUCH_FEEDBACK V5: 키보드를 가볍게 누르는 인상을 목표로 짧고 작은 소프트 탭을 합성한다.
    rng = random.Random(20261026)
    body = []
    noise = 0.0
    alpha = 1.0 - math.exp(-2.0 * math.pi * 650.0 / RATE)
    count = int(RATE * 0.040)
    for i in range(count):
        t = i / RATE
        noise += alpha * (rng.uniform(-1.0, 1.0) - noise)
        attack = 0.5 - 0.5 * math.cos(math.pi * min(1.0, t / 0.004))
        tail = min(1.0, (count - i) / (RATE * 0.009))
        resonance = (math.sin(2*math.pi*280.0*t)*math.exp(-100.0*t)
                     + 0.12*math.sin(2*math.pi*520.0*t)*math.exp(-170.0*t)
                     + 0.035*math.sin(2*math.pi*870.0*t)*math.exp(-240.0*t))
        body.append(attack * tail * (resonance + 0.06*noise*math.exp(-220.0*t)))
    rms = math.sqrt(sum(x*x for x in body) / len(body))
    gain = min(10**(-33.0/20) / rms, 0.06 / max(abs(x) for x in body))
    write(out / "TOUCH_FEEDBACK.wav", [0.0]*int(RATE*0.005) + [x*gain for x in body] + [0.0]*int(RATE*0.015))
