#!/usr/bin/env python3
"""Send an ESP32 application firmware.bin directly over MQTT/TLS (no HTTP)."""
import argparse
import getpass
import hashlib
import json
import os
import queue
import re
import struct
import sys
import threading
import time
import uuid
from pathlib import Path

CHUNK_SIZE = 1024


def frame(session, offset, data):
    if not re.fullmatch(r"[0-9a-f]{32}", session):
        raise ValueError("Invalid session")
    if not 0 < len(data) <= CHUNK_SIZE:
        raise ValueError("Invalid chunk length")
    return session.encode("ascii") + struct.pack(">I", offset) + data


class Sender:
    def __init__(self, client, prefix, session, timeout=3, retries=5):
        self.client = client
        self.prefix = prefix
        self.session = session
        self.timeout = timeout
        self.retries = retries
        self.replies = queue.Queue(maxsize=32)
        self.ready = threading.Event()
        self.disconnected = threading.Event()
        self.error = None

    def on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            self.error = f"Broker rejected connection: {reason_code}"
            self.ready.set()
            return
        result, _ = client.subscribe(self.prefix + "/ota/ack", qos=1)
        if result != 0:
            self.error = "Failed to subscribe to OTA ACK topic"
            self.ready.set()

    def on_subscribe(self, client, userdata, mid, reason_codes, properties):
        if any(code.is_failure for code in reason_codes):
            self.error = "Broker denied OTA ACK subscription; check ACL"
        self.ready.set()

    def on_disconnect(self, client, userdata, flags, reason_code, properties):
        self.disconnected.set()
        self.ready.set()

    def on_message(self, client, userdata, message):
        if message.retain or len(message.payload) > 512:
            return
        try:
            body = json.loads(message.payload)
            if isinstance(body, dict) and body.get("session") == self.session:
                self.replies.put_nowait(body)
        except (ValueError, UnicodeError, queue.Full):
            pass

    def exchange(self, topic, payload, result, next_offset):
        for _ in range(self.retries):
            if self.disconnected.is_set():
                raise RuntimeError("MQTT disconnected; rerun to start a new transfer")
            info = self.client.publish(self.prefix + topic, payload, qos=1, retain=False)
            if info.rc != 0:
                raise RuntimeError(f"MQTT publish failed: {info.rc}")
            deadline = time.monotonic() + self.timeout
            while time.monotonic() < deadline:
                if self.disconnected.is_set():
                    raise RuntimeError("MQTT disconnected; rerun to start a new transfer")
                try:
                    ack = self.replies.get(timeout=min(0.2, max(0.001, deadline - time.monotonic())))
                except queue.Empty:
                    continue
                status = ack.get("result")
                if status == result and ack.get("next") == next_offset:
                    return
                # Delayed ACKs from an earlier chunk/start are harmless.
                if status in ("ready", "chunk"):
                    continue
                raise RuntimeError(f"ESP32 rejected transfer: {ack}")
        raise RuntimeError(f"No ESP32 {result} ACK at offset {next_offset}; check device/ACL. "
                           "If waiting for complete, firmware may already have been activated.")

    def control(self, action, **fields):
        return json.dumps(dict(session=self.session, action=action, **fields), separators=(",", ":"))

    def upload(self, firmware):
        self.exchange("/ota/control", self.control(
            "start", size=len(firmware), sha256=hashlib.sha256(firmware).hexdigest(),
            expires_at=int(time.time()) + 120), "ready", 0)
        last_percent = -1
        for offset in range(0, len(firmware), CHUNK_SIZE):
            data = firmware[offset:offset + CHUNK_SIZE]
            self.exchange("/ota/data", frame(self.session, offset, data), "chunk", offset + len(data))
            percent = (offset + len(data)) * 100 // len(firmware)
            if percent != last_percent:
                print(f"\rTransfer: {percent:3d}% ({offset + len(data)}/{len(firmware)} bytes)", end="", flush=True)
                last_percent = percent
        print()
        self.exchange("/ota/control", self.control("finish"), "complete", len(firmware))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path, help="Application firmware.bin, not merged/bootloader binary")
    parser.add_argument("--host", required=True, help="MQTT TCP/TLS hostname")
    parser.add_argument("--port", type=int, default=8883)
    parser.add_argument("--device", default="namika-01")
    parser.add_argument("--username", required=True, help="Uploader account with OTA topic permissions")
    parser.add_argument("--ca", help="Broker CA PEM; defaults to system trusted CAs")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,48}", args.device):
        parser.error("Invalid device ID")
    try:
        firmware = args.firmware.read_bytes()
        if len(firmware) < 32 or firmware[0] != 0xE9:
            raise ValueError("File is not an ESP application image")
        if len(firmware) > 0x140000:
            raise ValueError("Firmware exceeds this project's default OTA slot (1,310,720 bytes)")
        import paho.mqtt.client as mqtt
        password = os.environ.get("NAMIKA_MQTT_PASSWORD") or getpass.getpass("MQTT password: ")
        session = uuid.uuid4().hex
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                             client_id="namika-ota-" + session[:16],
                             protocol=mqtt.MQTTv311, clean_session=True,
                             reconnect_on_failure=False)
        client.max_inflight_messages_set(1)
        client.max_queued_messages_set(8)
        sender = Sender(client, "namika/" + args.device, session)
        client.on_connect = sender.on_connect
        client.on_subscribe = sender.on_subscribe
        client.on_disconnect = sender.on_disconnect
        client.on_message = sender.on_message
        client.username_pw_set(args.username, password)
        client.tls_set(ca_certs=args.ca)
        client.connect(args.host, args.port, keepalive=30)
        client.loop_start()
        completed = False
        try:
            if not sender.ready.wait(15):
                raise RuntimeError("Timed out connecting/subscribing to broker")
            if sender.error:
                raise RuntimeError(sender.error)
            if sender.disconnected.is_set():
                raise RuntimeError("Disconnected before OTA started")
            print(f"Uploading to {args.device}; session {session}")
            sender.upload(firmware)
            completed = True
            print("Firmware verified and selected for boot. ESP32 will restart in about 5 seconds. "
                  "Check Serial/monitoring after reconnect to confirm the new firmware runs.")
        finally:
            if not completed and client.is_connected():
                info = client.publish(sender.prefix + "/ota/control", sender.control("abort"), qos=1, retain=False)
                try:
                    info.wait_for_publish(timeout=2)
                except (ValueError, RuntimeError):
                    pass
            client.disconnect()
            client.loop_stop()
    except (OSError, ValueError, RuntimeError, ImportError, KeyboardInterrupt) as exc:
        print(f"OTA failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
