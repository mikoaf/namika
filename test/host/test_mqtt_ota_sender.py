import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("mqtt_ota", Path(__file__).resolve().parents[2] / "tools/mqtt_ota.py")
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


class FakeBroker:
    """Device emulator: discard first chunk ACK to force retransmission."""
    def __init__(self):
        self.received = bytearray()
        self.calls = []
        self.dropped = False
        self.bad_hash = False
        self.expected_hash = None

    def publish(self, topic, payload, qos, retain):
        assert qos == 1 and not retain
        self.calls.append((topic, payload))
        if topic.endswith("control"):
            command = json.loads(payload)
            self.session = command["session"]
            action = command["action"]
            if action == "start":
                self.expected_hash = command["sha256"]
                status = "ready"
            else:
                assert self.expected_hash == hashlib.sha256(self.received).hexdigest()
                status = "hash_mismatch" if self.bad_hash else "complete"
        else:
            assert payload[:32].decode() == self.session
            at = struct.unpack(">I", payload[32:36])[0]
            if at == len(self.received):
                self.received.extend(payload[36:])
            else:
                assert bytes(self.received[at:at + len(payload[36:])]) == payload[36:]
            if not self.dropped:
                self.dropped = True
                return type("Result", (), {"rc": 0})()
            # A delayed older ACK must not advance the next transfer.
            self.sender.replies.put(dict(session=self.session, result="chunk", next=0))
            status = "chunk"
        self.sender.replies.put(dict(session=self.session, result=status, next=len(self.received)))
        return type("Result", (), {"rc": 0})()


class SenderTests(unittest.TestCase):
    def sender(self):
        broker = FakeBroker()
        sender = ota.Sender(broker, "namika/test", "a" * 32, timeout=0.01, retries=2)
        broker.sender = sender
        return sender, broker

    def test_binary_transfer_retries_without_duplicating_firmware(self):
        sender, broker = self.sender()
        firmware = bytes(range(256)) * 9 + b"\x00\xff"
        sender.upload(firmware)
        self.assertEqual(bytes(broker.received), firmware)
        chunks = [payload for topic, payload in broker.calls if topic.endswith("data")]
        self.assertEqual(chunks[0], chunks[1])
        self.assertEqual(len(chunks), 4)

    def test_hash_rejection_propagates(self):
        sender, broker = self.sender()
        broker.bad_hash = True
        with self.assertRaisesRegex(RuntimeError, "hash_mismatch"):
            sender.upload(b"\xe9" * 64)

    def test_retained_malformed_and_other_session_ack_ignored(self):
        sender, _ = self.sender()
        for payload, retain in [(b"[]", False), (b"invalid", False), (b"{}", False),
                                (b'{"session":"' + b"a" * 32 + b'"}', True)]:
            sender.on_message(None, None, type("Message", (), {"payload": payload, "retain": retain})())
        self.assertTrue(sender.replies.empty())

    def test_disconnect_stops_upload(self):
        sender, _ = self.sender()
        sender.disconnected.set()
        with self.assertRaisesRegex(RuntimeError, "disconnected"):
            sender.upload(b"\xe9" * 64)

    def test_timeout_is_bounded(self):
        sender, broker = self.sender()
        broker.publish = lambda *a, **kw: type("Result", (), {"rc": 0})()
        with self.assertRaisesRegex(RuntimeError, "No ESP32 ready ACK"):
            sender.upload(b"\xe9" * 64)

    def test_frame_validation(self):
        with self.assertRaises(ValueError):
            ota.frame("a" * 32, 0, b"x" * 1025)
        with self.assertRaises(ValueError):
            ota.frame("bad", 0, b"x")
        self.assertEqual(ota.frame("a" * 32, 0x12345678, b"\x00\xff")[32:], b"\x12\x34\x56\x78\x00\xff")


if __name__ == "__main__":
    unittest.main()
