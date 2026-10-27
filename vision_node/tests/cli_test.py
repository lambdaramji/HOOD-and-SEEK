"""Process-boundary checks: raw observation records, invalid input and stalled source."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile

binary = sys.argv[1]
base = dict(width=100, height=100, model_width=100, model_height=100,
            proposals=[dict(class_id=0, score=.9, bbox=[0, 0, 90, 90]),
                       dict(class_id=15, score=.12, bbox=[10, 10, 80, 80])])
data = "\n".join(json.dumps(dict(base, sequence=i, timestamp_ms=t)) for i, t in [(1, 0), (2, 300)])
run = subprocess.run([binary, "--replay", "-"], input=data, text=True, capture_output=True, timeout=10)
assert run.returncode == 0, run.stderr
events = [json.loads(line) for line in run.stdout.splitlines()]
assert events[0]["event"] == "start" and events[-1] == dict(event="stop", reason="end_of_input")
records = events[1:-1]
assert [r["seq"] for r in records] == [1, 2] and [r["t_ms"] for r in records] == [0, 300]
assert records[0]["frame_w"] == 100 and records[0]["best"]["person"]["bbox"] == [0, 0, 90, 90]
# A low score is reported as-is; judging it is SafetyCore's job.
assert abs(records[0]["best"]["cat"]["score"] - .12) < 1e-6 and records[0]["best"]["dog"] is None
# Invariant: the observation node never emits accumulated or judged state.
for word in ("presence", "present", "consecutive", "debounce", "hits"):
    assert word not in run.stdout, f"judgment field leaked: {word}"

# Measurement runs stop after exactly N inferences (#49: 30 frames per condition).
run = subprocess.run([binary, "--replay", "-", "--max-frames", "1"], input=data, text=True, capture_output=True, timeout=10)
assert run.returncode == 0, run.stderr
events = [json.loads(line) for line in run.stdout.splitlines()]
assert [e.get("seq") for e in events[1:-1]] == [1] and events[-1] == dict(event="stop", reason="max_frames")
# The limit counts processed lines, not the input's sequence numbers (a recording may start at 101).
late = "\n".join(json.dumps(dict(base, sequence=i, timestamp_ms=i)) for i in (101, 102, 103))
run = subprocess.run([binary, "--replay", "-", "--max-frames", "2"], input=late, text=True, capture_output=True, timeout=10)
events = [json.loads(line) for line in run.stdout.splitlines()]
assert [e.get("seq") for e in events[1:-1]] == [101, 102] and events[-1] == dict(event="stop", reason="max_frames"), events

for invalid in ["{broken", json.dumps(dict(base, sequence=1.5, timestamp_ms=0)), json.dumps(dict(base, sequence=1, timestamp_ms=0, width=-1))]:
    run = subprocess.run([binary, "--replay", "-"], input=invalid, text=True, capture_output=True, timeout=10)
    assert run.returncode == 2
    assert json.loads(run.stdout.splitlines()[-1])["event"] == "fault"

with tempfile.TemporaryDirectory() as folder:
    p = Path(folder) / "input.jsonl"
    p.write_text(data)
    run = subprocess.run([binary, "--replay", str(p), "--output", str(p)], capture_output=True, timeout=10)
    assert run.returncode == 2 and p.read_text() == data

# The camera-card video serves frames the camera loop read; replay has none.
run = subprocess.run([binary, "--replay", "-", "--preview-port", "7431"], input="", text=True, capture_output=True, timeout=10)
assert run.returncode == 2 and "--preview-port applies to --camera only" in run.stderr, run.stderr

# --- TCP line-JSON link to SafetyCore (design §2.1, contracts/README.md v1) ---
contracts = Path(os.environ.get("HOODSEEK_CONTRACTS", Path(__file__).resolve().parents[2] / "contracts"))
schemas = {}
if (contracts / "vision.schema.json").exists():   # absent until contract v1 (#52) is merged into this branch
    from jsonschema import Draft202012Validator
    schemas = {n: Draft202012Validator(json.loads((contracts / f"{n}.schema.json").read_text())) for n in ("hello", "vision")}
print(f"contract schema check: {'on (' + str(contracts) + ')' if schemas else 'skipped (no contracts/vision.schema.json)'}")

score_of = lambda i: round(.5 + i / 1000, 4)   # the person score tags the frame, so a replayed backlog shows

def frame_line(i):
    proposals = [dict(base["proposals"][0], score=score_of(i)), base["proposals"][1]]
    return json.dumps(dict(base, proposals=proposals, sequence=i, timestamp_ms=i * 300)) + "\n"

def read_message(stream):
    line = stream.readline()
    assert line.endswith("\n") and len(line.encode()) <= 4096, line
    message = json.loads(line)
    if schemas:
        errors = list(schemas[message["type"]].iter_errors(message))
        assert not errors, errors[0].message
    return message

with tempfile.TemporaryDirectory() as folder:
    roi = Path(folder) / "roi.json"
    roi.write_text(json.dumps(dict(frame_w=100, frame_h=100, rotate=0, points=[[0, 0], [100, 0], [100, 100], [0, 100]])))
    run = subprocess.run([binary, "--replay", "-", "--core", "127.0.0.1:9"], input="", text=True, capture_output=True, timeout=10)
    assert run.returncode == 2 and "--roi" in run.stderr, run.stderr
    run = subprocess.run([binary, "--replay", "-", "--core", "localhost", "--roi", str(roi)], input="", text=True, capture_output=True, timeout=10)
    assert run.returncode == 2, "core address must be IPv4:port"

    server = socket.socket(); server.bind(("127.0.0.1", 0)); server.listen(1); server.settimeout(5)
    port = server.getsockname()[1]
    node = subprocess.Popen([binary, "--replay", "-", "--core", f"127.0.0.1:{port}", "--roi", str(roi)],
                            stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    try:
        node.stdin.write(frame_line(1)); node.stdin.flush()
        conn, _ = server.accept(); stream = conn.makefile("r")
        hello = read_message(stream)
        assert hello["type"] == "hello" and hello["seq"] == 0 and hello["p"] == dict(node="vision-node", ver="0.1.0")
        first = read_message(stream)
        assert first["type"] == "vision" and first["seq"] == 1
        assert first["p"]["person"]["seen"] and first["p"]["person"]["bbox"] == [0, 0, 90, 90]
        assert first["p"]["cat"] == dict(seen=True, score=0.12, bbox=[10, 10, 80, 80], roi_overlap=1.0), first["p"]["cat"]
        node.stdin.write(frame_line(2)); node.stdin.flush()
        assert read_message(stream)["seq"] == 2
        # Server drops the link: the node reconnects after the 1 s backoff with a fresh hello and seq restarting
        # (seq is per connection), and frames inferred while disconnected are dropped, not replayed.
        stream.close(); conn.close()
        server.settimeout(0.2)
        conn = None
        for i in range(3, 40):
            node.stdin.write(frame_line(i)); node.stdin.flush()
            try:
                conn, _ = server.accept(); break
            except socket.timeout:
                pass
        assert conn, "node did not reconnect"
        stream = conn.makefile("r")
        assert read_message(stream)["type"] == "hello"
        # Frames 3..i-1 arrived while disconnected and must be gone; frame i opened the new connection.
        resumed = read_message(stream)
        assert resumed["seq"] == 1 and resumed["p"]["person"]["score"] == score_of(i), ("backlog replayed or seq not reset", resumed)
        node.stdin.write(frame_line(100)); node.stdin.flush()
        after = read_message(stream)
        assert after["seq"] == 2 and after["p"]["person"]["score"] == score_of(100), after
        node.stdin.close()
        assert node.wait(timeout=5) == 0, node.stderr.read()
        conn.close()
    finally:
        if node.poll() is None:
            node.kill()
        server.close()

# contracts/examples (contract v1 freeze, 10/19): the node reproduces the documented hello/vision examples
# from equivalent observations, and keeps publishing whatever state lines Core sends (valid or not).
examples = contracts / "examples"
if (examples / "vision.cat-in-roi.json").exists():
    example = lambda name: json.loads((examples / name).read_text())
    frame640 = lambda i, proposals: json.dumps(dict(width=640, height=480, model_width=640, model_height=480,
                                                    sequence=i, timestamp_ms=i, proposals=proposals)) + "\n"
    cases = [("vision.json", [dict(class_id=0, score=.81, bbox=[120, 40, 300, 480])]),
             ("vision.cat-in-roi.json", [dict(class_id=15, score=.67, bbox=[350, 260, 470, 400])]),
             ("vision.empty.json", [])]
    with tempfile.TemporaryDirectory() as folder:
        roi = Path(folder) / "roi.json"   # 96×126 of the 120×140 cat box → roi_overlap 0.72
        roi.write_text(json.dumps(dict(frame_w=640, frame_h=480, rotate=0, points=[[350, 260], [446, 260], [446, 386], [350, 386]])))
        server = socket.socket(); server.bind(("127.0.0.1", 0)); server.listen(1); server.settimeout(5)
        node = subprocess.Popen([binary, "--replay", "-", "--core", f"127.0.0.1:{server.getsockname()[1]}", "--roi", str(roi)],
                                stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        try:
            node.stdin.write(frame640(1, cases[0][1])); node.stdin.flush()
            conn, _ = server.accept(); stream = conn.makefile("r")
            hello, want = read_message(stream), example("hello.json")
            assert {k: hello[k] for k in ("v", "type", "src", "seq", "p")} == {k: want[k] for k in ("v", "type", "src", "seq", "p")}, hello
            states = [json.dumps(json.loads(p.read_text())) + "\n" for p in sorted(examples.glob("state*.json")) + sorted((examples / "invalid").glob("state*.json"))]
            for seq, (name, proposals) in enumerate(cases, start=1):
                if seq > 1:
                    node.stdin.write(frame640(seq, proposals)); node.stdin.flush()
                got, want = read_message(stream), example(name)
                assert got["seq"] == seq, "link restarted: a state line broke the connection"
                for msg in (got, want):
                    msg["p"].pop("infer_ms")
                assert got["p"] == want["p"], (name, got["p"])
                conn.sendall("".join(states).encode())   # Core state lines are read and discarded
            node.stdin.close()
            assert node.wait(timeout=5) == 0, node.stderr.read()
        finally:
            if node.poll() is None:
                node.kill()
            server.close()
    print(f"contracts/examples check: on ({len(cases)} vision + hello, {len(states)} state lines)")

process = subprocess.Popen([binary, "--replay", "-", "--stall-ms", "200"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
try:
    # Keep stdin open and silent: the watchdog must end a stalled source itself.
    process.wait(timeout=5)
    assert process.returncode == 3
    events = [json.loads(line) for line in process.stdout.read().splitlines()]
    assert events[-1] == dict(event="fault", reason="source_stalled")
finally:
    if process.poll() is None:
        process.kill()
    process.communicate()

# The watchdog must still exit when the main thread is stuck writing to an output nobody reads.
with tempfile.TemporaryDirectory() as folder:
    p = Path(folder) / "many.jsonl"
    p.write_text("\n".join(json.dumps(dict(base, sequence=i, timestamp_ms=i)) for i in range(1, 5001)))
    process = subprocess.Popen([binary, "--replay", str(p), "--stall-ms", "200"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        assert process.wait(timeout=5) == 3, "watchdog blocked behind a stuck output"
        assert b"source_stalled" in process.stderr.read()
    finally:
        if process.poll() is None:
            process.kill()
        process.communicate()

# The exit must not depend on any write: stdout and stderr share one pipe that is already full
# (a stalled ssh channel), so even the start event and the stderr note block.
read_end, write_end = os.pipe()
os.set_blocking(write_end, False)
for chunk in (b"x" * 4096, b"x"):
    try:
        while True:
            os.write(write_end, chunk)
    except BlockingIOError:
        pass
os.set_blocking(write_end, True)
with tempfile.TemporaryDirectory() as folder:
    p = Path(folder) / "one.jsonl"
    p.write_text(frame_line(1))
    process = subprocess.Popen([binary, "--replay", str(p), "--stall-ms", "200"], stdout=write_end, stderr=write_end)
    os.close(write_end)
    try:
        assert process.wait(timeout=5) == 3, "exit waited on a blocked write"
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        os.close(read_end)
print("vision CLI checks passed")
