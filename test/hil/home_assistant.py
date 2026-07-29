#!/usr/bin/env python3
"""Hardware test for the Home Assistant MQTT adapter.

Connects to the same broker the lamp uses, then checks that the lamp announces
itself, reports its state, reacts to commands and disappears cleanly.

    pip install paho-mqtt
    python3 test/hil/home_assistant.py --broker mqtt.local --device glow-1384610827

With two lamps in one group, --peer-port additionally verifies that a command
sent to one lamp reaches the other over ESP-NOW.
"""

import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

try:
    import paho.mqtt.client as mqtt
except ImportError:  # pragma: no cover - depends on the operator's machine
    raise SystemExit("paho-mqtt is required: pip install paho-mqtt")

from two_lamp_sync import Lamp, wait_until


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)


class Broker:
    """Collects retained and live messages for one lamp."""

    def __init__(self, host, port, username, password, base_topic, discovery_prefix):
        self.messages = {}
        self.base_topic = base_topic
        self.discovery_prefix = discovery_prefix
        self.client = mqtt.Client()
        if username:
            self.client.username_pw_set(username, password)
        self.client.on_message = self._on_message
        self.client.connect(host, port, keepalive=30)
        self.client.subscribe(f"{base_topic}/#")
        self.client.subscribe(f"{discovery_prefix}/+/+/+/config")
        self.client.loop_start()

    def _on_message(self, client, userdata, message):
        self.messages[message.topic] = message.payload.decode("utf-8", "replace")

    def get(self, topic, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if topic in self.messages:
                return self.messages[topic]
            time.sleep(0.2)
        return None

    def wait_for(self, topic, expected, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.messages.get(topic) == expected:
                return True
            time.sleep(0.2)
        return False

    def publish(self, topic, payload):
        self.client.publish(topic, payload)

    def discovery_topics(self):
        return [t for t in self.messages if t.startswith(self.discovery_prefix + "/")]

    def close(self):
        self.client.loop_stop()
        self.client.disconnect()


def run_checks(broker, lamp, peer):
    # 1. The device announces itself and is marked available.
    check(broker.get(f"{broker.base_topic}/status") == "online",
          "the lamp did not report itself as online")

    discovery = broker.discovery_topics()
    check(discovery, "no discovery messages were retained")
    light_topic = next((t for t in discovery if t.endswith("/light/config")), None)
    check(light_topic is not None, "no light entity was announced")

    light = json.loads(broker.messages[light_topic])
    check(light.get("schema") == "json", "the light entity is not a JSON schema light")
    check(light["device"]["identifiers"], "the light entity carries no device block")
    print(f"  {len(discovery)} entities announced")

    # 2. Nothing published may contain a secret.
    for topic, payload in broker.messages.items():
        lowered = payload.lower()
        for secret in ("groupkey", "password", "otapassword"):
            check(secret not in lowered, f"{topic} looks like it leaks a secret")
    print("  no published payload contains a secret")

    # 3. State is reported.
    state = broker.get(f"{broker.base_topic}/state")
    check(state is not None, "the lamp published no state document")
    parsed = json.loads(state)
    check(parsed.get("schema") == "glow.state", "the state document has a wrong schema")
    mode_name = broker.get(f"{broker.base_topic}/mode/state")
    check(mode_name, "the lamp published no active mode")
    print(f"  state published, active mode {mode_name!r}")

    # 4. A command changes the lamp.
    broker.publish(f"{broker.base_topic}/light/set", json.dumps({"state": "OFF"}))
    check(broker.wait_for(f"{broker.base_topic}/light/state",
                          json.dumps({"state": "OFF", "brightness": 0},
                                     separators=(",", ":")), timeout=10) or
          json.loads(broker.get(f"{broker.base_topic}/light/state"))["state"] == "OFF",
          "the lamp did not switch off on command")

    broker.publish(f"{broker.base_topic}/light/set",
                   json.dumps({"state": "ON", "brightness": 200}))
    deadline = time.monotonic() + 10
    brightness = None
    while time.monotonic() < deadline:
        payload = broker.get(f"{broker.base_topic}/light/state", timeout=1)
        if payload:
            brightness = json.loads(payload).get("brightness")
            if brightness == 200:
                break
    check(brightness == 200, f"brightness did not follow the command (got {brightness})")
    print("  light command applied and reported back")

    # 5. A mode change through Home Assistant reaches the group.
    if peer is not None:
        before = peer.latest_status()
        modes = json.loads(broker.messages[
            next(t for t in discovery if t.endswith("/mode/config"))])["options"]
        target = next((m for m in modes if m != before[0]), None)
        check(target is not None, "no second mode available to switch to")

        broker.publish(f"{broker.base_topic}/mode/set", target)
        wait_until(
            "the peer lamp to follow the mode change",
            lambda: (peer.send("STATUS") is None
                     and peer.latest_status() is not None
                     and peer.latest_status()[0] == target),
            timeout=15,
        )
        print(f"  mode change reached the second lamp: {target!r}")

    # 6. The lamp stays healthy.
    lines = lamp.lines_since_boot()
    check(not any("Guru Meditation" in line for line in lines), "the lamp crashed")
    check(not any("Rebooting" in line for line in lines), "the lamp rebooted")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--broker", required=True)
    parser.add_argument("--broker-port", type=int, default=1883)
    parser.add_argument("--username")
    parser.add_argument("--password")
    parser.add_argument("--device", required=True,
                        help="device id the lamp uses, for example glow-1384610827")
    parser.add_argument("--base-prefix", default="glowlight")
    parser.add_argument("--discovery-prefix", default="homeassistant")
    parser.add_argument("--port", required=True, help="serial port of the lamp")
    parser.add_argument("--peer-port", help="serial port of a second lamp in the group")
    args = parser.parse_args()

    lamp = Lamp(args.port)
    peer = Lamp(args.peer_port) if args.peer_port else None
    broker = None
    try:
        wait_until(
            "the lamp to be running",
            lambda: lamp.count_since_boot("GlowLight started") >= 1,
            timeout=25,
        )
        broker = Broker(args.broker, args.broker_port, args.username, args.password,
                        f"{args.base_prefix}/{args.device}", args.discovery_prefix)
        run_checks(broker, lamp, peer)
    except Failure as failure:
        print(f"\nFAILED: {failure}")
        return 1
    finally:
        if broker is not None:
            broker.close()
        lamp.close()
        if peer is not None:
            peer.close()

    print("\nHome Assistant integration checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
