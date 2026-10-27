"""모델·마이크 없이 발행 옵션의 필수 수신기와 ASR별 정책을 검사한다."""
import subprocess
import sys

binary = sys.argv[1]
base = [binary, "--sensevoice-dir", "unused", "--vad-model", "unused", "--wav", "unused"]
cases = [
    (["--publish"], "publication requires receiver"),
    (["--min-score", "0.7", "--min-score-raise", "0.7"], "SenseVoice does not provide Whisper quality scores"),
    (["--fallback-model", "unused"], "SenseVoice does not provide Whisper quality scores"),
    (["--model", "unused"], "exactly one ASR model"),
    (["--pulse-source", "unused"], "exactly one input mode"),
    (["--capture-raw", "unused.raw"], "capture-raw requires live PulseAudio input"),
]
for extra, expected in cases:
    result = subprocess.run(base + extra, capture_output=True, text=True, timeout=5)
    assert result.returncode == 1, (extra, result)
    assert expected in result.stderr, (extra, result.stderr)
    assert "model_loaded" not in result.stdout and "capture_ready" not in result.stdout
print(f"asr_option_checks={len(cases)} failures=0")
