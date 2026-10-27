"""실제 C++ 송신기를 127.0.0.1 시험 수신기로 검사한다. 장치 제어는 없다."""

import json
from contextlib import contextmanager
import selectors
import socket
import struct
import subprocess
import sys
import threading


# 계약 v1 examples/state.json의 payload. voice-node는 이를 판단에 사용하지 않는다.
STATE = {
    "v": 1, "type": "state", "src": "core", "seq": 40211, "t_ms": 200500,
    "p": {
        "hazard": "warning", "presence": "absent", "lock": "locked_cat",
        "fan": {"level": 3, "min": 3, "requested": 1, "ack": True},
        "induction": {"level": 0, "allowed_on": False, "ack": True},
        "sensors": {
            "thermal": {"valid": True, "max_c": 52.4, "median_c": 28.4, "metric": 24.0},
            "pm": {"valid": True, "pm25": 18.0, "tsize": 0.62, "baseline": 12.0}},
        "vision": {"fresh": True, "person": False, "cat": True},
        "voice": {"last_intent": "IND_ON", "value": None, "accepted": False, "reason": "LOCKED"},
        "reasons": ["TEMP_WARN", "ABSENT", "CAT_NEAR"], "alert": "CAT_LOCK",
        "thresholds": {"t_caution": 16, "t_warn": 22, "t_danger": 30, "pm_warn": 30, "pm_smoke": 50},
        "uptime_s": 812,
    },
}


class Receiver:
    def __init__(self, binary, listen=True, hi_lg=False):
        self.server = socket.socket()
        self.server.bind(("127.0.0.1", 0))
        self.server.settimeout(3)
        if listen:
            self.server.listen()
        self.conf = 0 if hi_lg else .75
        self.process = subprocess.Popen(
            [binary, str(self.server.getsockname()[1])] + (["--hi-lg"] if hi_lg else []), stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8")
        self.peer = None
        self.buffer = b""
        self.messages = []
        self.expected_seq = 0

    def __enter__(self):
        return self

    def __exit__(self, *_):
        if self.peer:
            self.peer.close()
        self.server.close()
        if self.process.poll() is None:
            self.process.stdin.write("quit\n")
            self.process.stdin.flush()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=3)
            raise AssertionError("test client did not stop")
        stderr = self.process.stderr.read()
        assert self.process.returncode == 0, (self.process.returncode, stderr)
        self.process.stdin.close()
        self.process.stdout.close()
        self.process.stderr.close()

    def command(self, line):
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        with selectors.DefaultSelector() as selector:
            selector.register(self.process.stdout, selectors.EVENT_READ)
            assert selector.select(timeout=5), ("client response timeout", line)
        result = self.process.stdout.readline().strip()
        assert result, ("client stopped", line)
        return result

    def wait(self, ms=30):
        result = self.command(f"wait {ms}").split()
        assert result[0] == "status", result
        return bool(int(result[1])), int(result[2])

    def accept(self):
        self.peer, address = self.server.accept()
        assert address[0] == "127.0.0.1"
        self.peer.settimeout(3)
        self.buffer = b""
        self.expected_seq = 0
        hello = self.read()
        assert hello["type"] == "hello" and hello["p"] == {"node": "voice-node", "ver": "0.1.0"}

    def read(self):
        while b"\n" not in self.buffer:
            chunk = self.peer.recv(4096)
            assert chunk, "connection ended before complete message"
            self.buffer += chunk
            assert len(self.buffer) <= 4096, "oversized outgoing frame"
        raw, self.buffer = self.buffer.split(b"\n", 1)
        assert len(raw) + 1 <= 4096
        message = json.loads(raw.decode("utf-8"))
        assert set(message) == {"v", "type", "src", "seq", "t_ms", "p"}
        assert message["v"] == 1 and message["src"] == "voice"
        assert type(message["seq"]) is int and message["seq"] == self.expected_seq
        self.expected_seq += 1
        assert type(message["t_ms"]) is int and message["t_ms"] >= 0
        self.messages.append(message)
        return message

    def no_message(self):
        assert not self.buffer
        with selectors.DefaultSelector() as selector:
            selector.register(self.peer, selectors.EVENT_READ)
            assert not selector.select(timeout=.02), "unexpected message or disconnect"

    def publish(self, id_, text, expected, *, quality=1, budget=1000, conf=None):
        conf = self.conf if conf is None else conf
        hex_text = text.encode("utf-8").hex() or "-"
        result = self.command(f"publish {id_} {quality} {budget} {conf} {hex_text}")
        assert result == expected, (id_, text, expected, result)

    def voice(self, text, intent, value=None, conf=None):
        conf = self.conf if conf is None else conf
        message = self.read()
        assert message["type"] == "voice"
        expected = {"intent": intent, "text": text, "conf": conf}
        if value is not None:
            expected["value"] = value
        assert message["p"] == expected, (message["p"], expected)
        assert len(message["p"]["text"]) <= 200 and 0 <= message["p"]["conf"] <= 1

    def stop_and_expect_eof(self):
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        assert self.process.wait(timeout=3) == 0
        assert self.peer.recv(1) == b"", "client did not close socket on stop"


@contextmanager
def states_during_inference(receiver, reconnect=False):
    """추론이 진행되는 동안 시험 Core가 0.1초마다 state를 보낸다."""
    stopped = threading.Event()
    errors = []
    payload = json.dumps(STATE).encode() + b"\n"

    def send_states():
        try:
            if reconnect:
                if stopped.wait(.25):
                    return
                receiver.peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
                receiver.peer.close()
                receiver.peer = None
                receiver.accept()
            while not stopped.is_set():
                receiver.peer.sendall(payload)
                stopped.wait(.1)
        except Exception as error:
            errors.append(error)

    sender = threading.Thread(target=send_states)
    sender.start()
    try:
        yield
    finally:
        stopped.set()
        sender.join(timeout=4)
        assert not sender.is_alive(), "test Core sender did not stop"
        assert not errors, errors


def main(binary):
    report = {"scenarios": [], "received_messages": []}
    with Receiver(binary) as receiver:
        assert receiver.wait()[0]
        receiver.accept()
        supported = [
            ("후드 약풍", "FAN_SET", 1), ("후드 중풍", "FAN_SET", 2),
            ("후드 강풍", "FAN_SET", 3), ("후드 팬 꺼", "FAN_OFF", None),
            ("후드 인덕션 꺼", "IND_OFF", None), ("후드 인덕션 켜", "IND_ON", None),
            ("후드 화력 올려", "IND_UP", None), ("후드 화력 내려", "IND_DOWN", None),
        ]
        id_ = 0
        for text, intent, value in supported:
            id_ += 1
            receiver.publish(id_, text, "sent")
            receiver.voice(text, intent, value)
        for text, canonical, intent, value in [
                ("푸드 강풍", "후드 강풍", "FAN_SET", 3),
                ("호대 종풍", "후드 중풍", "FAN_SET", 2),
                ("후드, 화력. 올려.", "후드 화력 올려.", "IND_UP", None)]:
            id_ += 1
            receiver.publish(id_, text, "sent")
            receiver.voice(canonical, intent, value)
        rejected = ["후드 켜지 마", "후드 인덕션 끄지 마", "후드 바람 좀 바꿔줘",
                    "후드 세게 했다가 꺼줘", "후드 한 단계 올려줘", "후드 인덕션 켜도 돼?",
                    '후드 "인덕션" \\ 켜\t\n\r', "후드" + "😀" * 198]
        for text in rejected:
            id_ += 1
            receiver.publish(id_, text, "sent")
            receiver.voice(text, "NONE")
        for text in ["", "오늘 날씨 좋네", "인덕션 켜줘", "푸드 인덕션 켜지 마", "후드" + "가" * 199]:
            id_ += 1
            receiver.publish(id_, text, "invalid")
        id_ += 1
        receiver.publish(id_, "후드 강풍", "quality_rejected", quality=0)
        receiver.publish(id_, "후드 강풍", "duplicate")
        id_ += 1
        receiver.publish(id_, "후드 강풍", "expired", budget=-1)
        receiver.publish(id_, "후드 강풍", "duplicate")
        id_ += 1
        receiver.publish(id_, "후드 강풍", "invalid", conf=1.01)
        id_ += 1
        receiver.publish(id_, "후드 강풍", "invalid", conf=-.01)
        receiver.no_message()
        for conf in [0, 1]:
            id_ += 1
            receiver.publish(id_, "후드 강풍", "sent", conf=conf)
            receiver.voice("후드 강풍", "FAN_SET", 3, conf)
        for conf, rounded in [(.72, .72), (.41, .41), (.123456, .1235), (1e-9, 1e-9)]:
            id_ += 1
            receiver.publish(id_, "후드 강풍", "sent", conf=conf)
            receiver.voice("후드 강풍", "FAN_SET", 3, rounded)
        receiver.publish(id_, "후드 강풍", "duplicate")
        receiver.publish(id_ - 1, "후드 강풍", "duplicate")
        receiver.no_message()
        # state 분할/병합 수신, seq 건너뜀. 본문은 판단에 사용하지 않는다.
        state = json.dumps(STATE, ensure_ascii=False).encode()
        receiver.peer.sendall(state[:13])
        assert receiver.wait() == (True, 0)
        STATE["seq"] += 7
        receiver.peer.sendall(state[13:] + b"\r\n" + json.dumps(STATE).encode() + b"\n")
        assert receiver.wait() == (True, 2)
        # JSON 의미 검사가 아닌 수신 프레이밍 상한만의 경계 검사다.
        receiver.peer.sendall(b" " * 4095 + b"\n")
        assert receiver.wait() == (True, 3)
        receiver.peer.sendall(b"x" * 4097)
        assert receiver.wait() == (False, 3)
        receiver.peer.close()
        receiver.peer = None
        id_ += 1
        receiver.publish(id_, "후드 강풍", "disconnected")
        assert receiver.wait(1200)[0]
        receiver.accept()
        receiver.publish(id_, "후드 강풍", "duplicate")
        receiver.no_message()
        id_ += 1
        receiver.publish(id_, "후드 강풍", "sent")
        receiver.voice("후드 강풍", "FAN_SET", 3)
        # 동일 문장이라도 새 발화라면 보낼 수 있다.
        id_ += 1
        receiver.publish(id_, "후드 강풍", "sent")
        receiver.voice("후드 강풍", "FAN_SET", 3)
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].extend([
            "hello-first", "8-command-outcomes", "NONE-for-unsupported", "omit-invalid-or-no-hood",
            "quality-gate", "expired-result", "one-attempt-per-utterance", "confidence-boundaries",
            "UTF8-and-JSON-escaping", "fragmented-coalesced-state-drain", "4096-byte-receive-limit",
            "reconnect-new-seq", "no-disconnected-command-replay", "new-utterance-same-text",
        ])
    # 연결 거부 후 복구해도 끊긴 동안의 발화를 큐에서 재전송하지 않는다.
    with Receiver(binary, listen=False) as receiver:
        assert not receiver.wait()[0]
        receiver.publish(1, "후드 팬 꺼", "disconnected")
        receiver.server.listen()
        assert receiver.wait(1200)[0]
        receiver.accept()
        receiver.publish(1, "후드 강풍", "duplicate")
        receiver.no_message()
        receiver.publish(2, "후드 팬 꺼", "sent")
        receiver.voice("후드 팬 꺼", "FAN_OFF")
        # peer가 갑자기 reset해도 프로세스가 SIGPIPE로 죽거나 요청을 재전송하지 않는다.
        receiver.peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        receiver.peer.close()
        receiver.peer = None
        receiver.publish(3, "후드 강풍", "io_error")
        receiver.publish(3, "후드 강풍", "duplicate")
        assert receiver.wait(1200)[0]
        receiver.accept()
        receiver.no_message()
        receiver.publish(4, "후드 팬 꺼", "sent")
        receiver.voice("후드 팬 꺼", "FAN_OFF")
        receiver.stop_and_expect_eof()
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].extend(["connection-refused-recovery", "peer-reset-no-SIGPIPE-or-replay",
                                    "socket-closed-on-stop"])
    # 완료 확인이 1초보다 늦어져도 이미 완료된 connect를 버리지 않는다.
    with Receiver(binary) as receiver:
        assert receiver.command("start 1200") == "status 0 0"
        assert receiver.wait()[0]
        receiver.accept()
        receiver.publish(1, "후드 강풍", "sent")
        receiver.voice("후드 강풍", "FAN_SET", 3)
        # 추론 때문에 service 호출이 멈춰도 publish 자체가 만료 링크를 차단한다.
        receiver.command("idle 3100")
        receiver.publish(2, "후드 인덕션 켜", "disconnected")
        receiver.peer.close()
        receiver.peer = None
        assert receiver.wait(1200)[0]
        receiver.accept()
        receiver.publish(2, "후드 인덕션 켜", "duplicate")
        receiver.no_message()
        receiver.publish(3, "후드 팬 꺼", "sent")
        receiver.voice("후드 팬 꺼", "FAN_OFF")
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].extend(["connect-completion-after-delayed-service", "stale-publish-without-service"])
    # 서버가 hello만 받은 뒤 FIN 없이 침묵하면 3초 뒤 닫고 hello부터 다시 연결한다.
    with Receiver(binary) as receiver:
        assert receiver.wait()[0]
        receiver.accept()
        assert not receiver.wait(3100)[0]
        receiver.publish(1, "후드 인덕션 켜", "disconnected")
        receiver.peer.close()
        receiver.peer = None
        assert receiver.wait(1200)[0]
        receiver.accept()
        receiver.publish(1, "후드 인덕션 켜", "duplicate")
        receiver.no_message()
        receiver.publish(2, "후드 팬 꺼", "sent")
        receiver.voice("후드 팬 꺼", "FAN_OFF")
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].append("silent-peer-watchdog-reconnect")
    with Receiver(binary) as receiver:
        assert receiver.wait()[0]
        receiver.accept()
        assert receiver.wait(2100)[0]
        receiver.peer.sendall(json.dumps(STATE).encode() + b"\n")
        assert receiver.wait() == (True, 1)
        assert receiver.wait(1200)[0]  # hello로부터 3초가 지나도 새 완성 줄이 수명을 갱신한다.
        receiver.peer.sendall(b"{")
        assert not receiver.wait(1900)[0]  # 미완성 줄은 3초 감시를 연장하지 않는다.
        receiver.publish(1, "후드 인덕션 켜", "disconnected")
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].extend(["newline-refreshes-watchdog", "partial-line-does-not-refresh-watchdog"])
    # 실제 추론과 같은 대기 경로에서 3초를 넘는 작업 중에도 TCP를 계속 처리한다.
    with Receiver(binary) as receiver:
        assert receiver.wait()[0]
        receiver.accept()
        with states_during_inference(receiver):
            status = receiver.command("infer 4200").split()
            assert status[:2] == ["status", "1"] and int(status[2]) >= 10, status
            error_status = receiver.command("infer-fail 100").split()
            assert error_status[:3] == ["task_error", "status", "1"], error_status
        receiver.no_message()  # service만으로 voice를 발행하지 않는다.
        receiver.publish(1, "후드 팬 꺼", "sent")
        receiver.voice("후드 팬 꺼", "FAN_OFF")
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].extend(["link-live-during-4.2s-inference", "inference-error-propagates",
                                    "inference-wait-does-not-publish"])
    with Receiver(binary) as receiver:
        assert receiver.wait()[0]
        receiver.accept()
        assert receiver.command("infer 3200").split()[:2] == ["status", "0"]
        receiver.publish(1, "후드 인덕션 켜", "disconnected")
        receiver.peer.close()
        receiver.peer = None
        assert receiver.wait(1200)[0]
        receiver.accept()
        receiver.publish(1, "후드 인덕션 켜", "duplicate")
        receiver.no_message()
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].append("silent-peer-expires-during-inference-no-replay")
    with Receiver(binary) as receiver:
        assert receiver.wait()[0]
        receiver.accept()
        with states_during_inference(receiver, reconnect=True):
            status = receiver.command("infer 4200").split()
            assert status[:2] == ["status", "1"] and int(status[2]) >= 10, status
        receiver.no_message()
        receiver.publish(1, "후드 강풍", "sent")
        receiver.voice("후드 강풍", "FAN_SET", 3)
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].append("reconnect-during-inference-hello-first")
    with Receiver(binary, hi_lg=True) as receiver:
        assert receiver.wait()[0]
        receiver.accept()
        cases = [
            ("하이 엘쥐 후드 약풍", "하이엘지, 후드 약풍", "FAN_SET", 1),
            ("하이엘지 팬 중풍", "하이엘지, 팬 중풍", "FAN_SET", 2),
            ("하이엘지 후드 강풍", "하이엘지, 후드 강풍", "FAN_SET", 3),
            ("하이엘지 후드 팬 꺼줘", "하이엘지, 후드 팬 꺼줘", "FAN_OFF", None),
            ("하이엘지 인덕션 꺼줘", "하이엘지, 인덕션 꺼줘", "IND_OFF", None),
            ("하이엘지 인덕션 켜줘", "하이엘지, 인덕션 켜줘", "IND_ON", None),
            ("하이엘지 불 세기 올려줘", "하이엘지, 불 세기 올려줘", "IND_UP", None),
            ("하이엘지 불 색이 내려줘", "하이엘지, 불 색이 내려줘", "IND_DOWN", None),
            ("하이엘지 인덕션 켜지 마", "하이엘지, 인덕션 켜지 마", "NONE", None),
            ("하이엘지 후드 켜줘", "하이엘지, 후드 켜줘", "FAN_SET", 1),
            ("하이엘지 인덕션 꺼?", "하이엘지, 인덕션 꺼", "IND_OFF", None),
            ("하이엘지 인덕션 켜？", "하이엘지, 인덕션 켜", "IND_ON", None),
            ("하이엘지 후드 약풍?", "하이엘지, 후드 약풍", "FAN_SET", 1),
            ("하이엘지 후드 켜?", "하이엘지, 후드 켜", "FAN_SET", 1),
            ("하이엘지 꺼", "하이엘지, 꺼", "NONE", None),
            ("하이엘지 인덕션 켜도 돼?", "하이엘지, 인덕션 켜도 돼?", "NONE", None),
            ("하이엘지 인덕션 끄지 마?", "하이엘지, 인덕션 끄지 마?", "NONE", None),
        ]
        for id_, (raw, final, intent, value) in enumerate(cases, 1):
            receiver.publish(id_, raw, "sent")
            receiver.voice(final, intent, value)
        next_id = len(cases) + 1
        receiver.publish(next_id, "후드 강풍", "invalid")
        receiver.publish(next_id+1, "하이엘지 후드 강풍", "invalid", conf=.7)
        receiver.publish(next_id+2, "하이엘지 인덕션 켜줘", "expired", budget=-1)
        receiver.publish(next_id+2, "하이엘지 인덕션 켜줘", "duplicate")
        receiver.no_message()
        report["received_messages"].extend(receiver.messages)
        report["scenarios"].extend(["hi-lg-v1-all-intents-conf-unavailable",
                                    "hi-lg-terminal-question-mark-is-command-punctuation",
                                    "hi-lg-permission-and-negation-still-none",
                                    "hi-lg-default-fan-on-level-one",
                                    "hi-lg-bare-action-no-target-none",
                                    "hi-lg-drop-score-expiry-no-replay"])
    report["result"] = "PASS"
    report["voice_messages"] = sum(m["type"] == "voice" for m in report["received_messages"])
    report["hello_messages"] = sum(m["type"] == "hello" for m in report["received_messages"])
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main(sys.argv[1])
