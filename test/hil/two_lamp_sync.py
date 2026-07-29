#!/usr/bin/env python3
"""Hardware integration test for a group of connected GlowLight lamps.

Works with two or more lamps; lamps[0] is the lamp under test and the rest
stand in for the group.
"""

import argparse
from collections import Counter
import json
import re
import subprocess
import sys
import threading
import time
from pathlib import Path

import serial
from serial.tools import list_ports


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ENVIRONMENT = "esp32c3-integration"
STATUS_PATTERN = re.compile(r"\[TEST\] STATUS\|([^|]*)\|(\d+)")
IDENTITY_PATTERN = re.compile(r"\[TEST\] IDENTITY\|[0-9a-f]{12}\|[0-9a-f]{32}\|(\d+)")
# A peer only counts as discovered once it has answered a challenge with a proof.
DISCOVERY_PATTERN = re.compile(r"\[INFO\] Authenticated node (\d+)")
# First line the firmware prints, before any service comes up.
BOOT_MARKER = "Starting Glow"


class Lamp:
    def __init__(self, port):
        self.port = port
        self.serial = serial.Serial(port, 115200, timeout=0.2, write_timeout=1)
        self.lines = []
        self.lock = threading.Lock()
        self.running = True
        self.thread = threading.Thread(target=self._read, daemon=True)
        self.thread.start()

    def _read(self):
        while self.running:
            line = self.serial.readline().decode("utf-8", errors="replace").strip()
            if line:
                with self.lock:
                    self.lines.append(line)
                print(f"{self.port}: {line}")

    def reset(self):
        with self.lock:
            self.lines.clear()
        self.serial.dtr = False
        self.serial.rts = True
        time.sleep(0.1)
        self.serial.rts = False

    def send(self, command):
        self.serial.write((command + "\n").encode("ascii"))
        self.serial.flush()

    def snapshot(self):
        with self.lock:
            return list(self.lines)

    def count(self, text):
        return sum(text in line for line in self.snapshot())

    def lines_since_boot(self):
        """Output of the current firmware run only.

        Clearing the buffer in reset() is not enough: the lamp keeps printing
        for a moment after the reset pulse, and output already sitting in the
        USB buffer arrives afterwards. Anything before the last boot banner
        belongs to the previous session. Returns nothing while no banner has
        been seen, so a stale line can never be mistaken for a fresh one.
        """
        lines = self.snapshot()
        for index in range(len(lines) - 1, -1, -1):
            if BOOT_MARKER in lines[index]:
                return lines[index:]
        return []

    def count_since_boot(self, text):
        return sum(text in line for line in self.lines_since_boot())

    def latest_status(self):
        for line in reversed(self.snapshot()):
            match = STATUS_PATTERN.search(line)
            if match:
                return match.group(1), int(match.group(2))
        return None

    def node_id(self, timeout=5):
        """Asks the lamp for its own node id over the integration console."""
        before = len(self.snapshot())
        self.send("IDENTITY")
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for line in self.snapshot()[before:]:
                match = IDENTITY_PATTERN.search(line)
                if match:
                    return match.group(1)
            time.sleep(0.1)
        return None

    def request_json(self, command, label, timeout=5):
        before = len(self.snapshot())
        self.send(command)
        prefix = f"[TEST] {label}|"
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for line in self.snapshot()[before:]:
                if line.startswith(prefix):
                    return json.loads(line[len(prefix):])
            time.sleep(0.1)
        return None

    def control(self, request, timeout=5):
        encoded = json.dumps(request, separators=(",", ":"))
        return self.request_json(f"CONTROL {encoded}", "CONTROL", timeout)

    def sync_status(self, timeout=5):
        state = self.request_json("STATE", "STATE", timeout)
        if state is None:
            return None
        return state.get("sync", {}).get("status")

    def discoveries(self):
        return Counter(
            match.group(1)
            for line in self.lines_since_boot()
            if (match := DISCOVERY_PATTERN.search(line))
        )

    def close(self):
        self.running = False
        self.thread.join(timeout=1)
        self.serial.close()


def connected_ports():
    ports = [
        port.device
        for port in list_ports.comports()
        if port.device.startswith("/dev/ttyACM")
        and (port.vid == 0x303A or "Espressif" in (port.description or ""))
    ]
    return sorted(ports)


def flash(port):
    subprocess.run(
        [
            "pio",
            "run",
            "--environment",
            ENVIRONMENT,
            "--target",
            "upload",
            "--upload-port",
            port,
        ],
        cwd=PROJECT_ROOT,
        check=True,
    )


def wait_until(description, predicate, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.25)
    raise AssertionError(f"Timed out waiting for {description}")


def assert_healthy(lamps):
    for lamp in lamps:
        lines = lamp.lines_since_boot()
        assert not any("Guru Meditation" in line for line in lines), f"{lamp.port} crashed"
        assert not any("Rebooting" in line for line in lines), f"{lamp.port} rebooted"
        assert not any("Message too large" in line for line in lines), (
            f"{lamp.port} exceeded the ESP-NOW payload limit"
        )
        assert not any("Communication disabled" in line for line in lines), (
            f"{lamp.port} has no usable group key; provision one in include/GlowConfig.h"
        )
        assert not any("Secure peer capacity" in line for line in lines), (
            f"{lamp.port} ran out of session slots"
        )
        # The alert is a local overlay and must never be broadcast as a mode.
        assert not any("unknown mode" in line for line in lines), (
            f"{lamp.port} received a state event for a mode it does not have"
        )
        assert not any("schema version mismatch" in line for line in lines), (
            f"{lamp.port} received a state event for a different mode schema"
        )
        assert not any("Min is greater than max" in line for line in lines), (
            f"{lamp.port} initialises a registry range with swapped bounds"
        )


def assert_home_assistant_is_alive(lamps, timeout=25):
    """A lamp that announces Home Assistant has to actually talk to the broker.

    Success or failure does not matter here; silence does. The service was once
    constructed and configured but never added to loop(), which produced exactly
    this: an 'enabled' line and nothing afterwards.
    """
    # Only a lamp that actually reached the network can talk to a broker, so a
    # lamp that never joined WiFi says nothing about this bug either way.
    announcing = [
        lamp for lamp in lamps
        if lamp.count_since_boot("Home Assistant enabled") >= 1
        and lamp.count_since_boot("WiFi connected") >= 1
    ]
    if not announcing:
        return

    def spoke(lamp):
        return (lamp.count_since_boot("MQTT connected") >= 1
                or lamp.count_since_boot("MQTT connect failed") >= 1)

    wait_until(
        "every lamp with Home Assistant enabled to reach the broker or report why not",
        lambda: all(spoke(lamp) for lamp in announcing),
        timeout=timeout,
    )


def assert_distinct_hostnames(lamps):
    """Lamps share one compiled configuration but must not share a name."""
    names = []
    for lamp in lamps:
        for line in lamp.lines_since_boot():
            match = re.search(r"Reachable as (\S+)\.local", line)
            if match:
                names.append(match.group(1))
                break
    if len(names) < 2:
        return
    assert len(set(names)) == len(names), f"Lamps announce the same mDNS name: {names}"


def assert_secure_transport(lamps):
    for lamp in lamps:
        assert lamp.count_since_boot("Secure communication initialized") >= 1, (
            f"{lamp.port} did not bring up the encrypted transport"
        )


def request_statuses(lamps):
    for lamp in lamps:
        lamp.send("STATUS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ports", nargs="+", metavar="PORT")
    parser.add_argument("--skip-flash", action="store_true")
    args = parser.parse_args()

    ports = args.ports or connected_ports()
    if len(ports) < 2:
        raise SystemExit(f"Need at least two ESP32-C3 lamps, found: {ports}")

    if not args.skip_flash:
        for port in ports:
            flash(port)

    lamps = [Lamp(port) for port in ports]
    try:
        for lamp in lamps:
            lamp.reset()

        wait_until(
            "every firmware instance to start",
            lambda: all(
                lamp.count_since_boot("GlowLight started") >= 1 for lamp in lamps
            ),
            timeout=20,
        )
        assert_secure_transport(lamps)
        capabilities = lamps[0].request_json("CAPABILITIES", "CAPABILITIES")
        assert capabilities is not None, "Capability document was not returned"
        assert capabilities["schema"] == "glow.capabilities"
        mode_ids = [mode["id"] for mode in capabilities["modes"]]
        assert mode_ids == ["static", "color-picker", "rainbow", "random-glow"], (
            f"Unexpected standard-profile capabilities: {mode_ids}"
        )
        assert "alert" not in mode_ids
        assert capabilities["modes"][0]["settings"]["color"]["format"] == "rgb-hex"

        node_ids = [lamp.node_id() for lamp in lamps]
        assert None not in node_ids, f"Invalid local node IDs: {node_ids}"
        assert len(set(node_ids)) == len(node_ids), (
            f"Lamps report duplicate node IDs: {node_ids}"
        )
        # Every lamp has to find every other lamp, exactly once each.
        wait_until(
            "mutual ESP-NOW discovery",
            lambda: all(
                lamps[index].discoveries()[node_ids[other]] == 1
                for index in range(len(lamps))
                for other in range(len(lamps))
                if other != index
            ),
            timeout=10 + 5 * len(lamps),
        )

        # Lamps of the same group that are not part of this run still publish
        # their state, which silently overrides whatever the test sets up.
        expected = set(node_ids)
        for lamp in lamps:
            seen = set(lamp.discoveries())
            strangers = seen - expected
            assert not strangers, (
                f"{lamp.port} sees lamps that are not part of this test: "
                f"{sorted(strangers)}. Power them off or give them a different "
                "group key, otherwise their state wins over the test's."
            )

        # The boot alert has to finish first, so keep asking rather than
        # polling a status that was requested once and never refreshed.
        wait_until(
            "every lamp to settle in Static Light",
            lambda: (
                request_statuses(lamps) is None
                and all(lamp.latest_status() == ("Static Light", 0) for lamp in lamps)
            ),
            timeout=20,
        )

        local_response = lamps[0].control({
            "api": "glow.control/1",
            "requestId": "local-only",
            "operation": "mode.select",
            "scope": "local",
            "target": {"mode": "color-picker"},
        })
        assert local_response and local_response["ok"], (
            f"Local mode control failed: {local_response}"
        )
        wait_until(
            "local scope to affect one lamp only",
            lambda: (
                request_statuses(lamps) is None
                and lamps[0].latest_status() == ("Color Picker", 0)
                and all(l.latest_status() == ("Static Light", 0) for l in lamps[1:])
            ),
            timeout=5,
        )

        # Settling in a mode does not imply the rejoin finished. A group command
        # issued while a lamp is still joining is rejected with
        # SYNC_PUBLISH_DISABLED, which made this step fail intermittently.
        wait_until(
            "every lamp to finish joining the group",
            lambda: all(lamp.sync_status() == "synchronized" for lamp in lamps),
            timeout=15,
        )

        response = lamps[0].control({
            "api": "glow.control/1",
            "requestId": "select-color-picker",
            "operation": "mode.select",
            "scope": "group",
            "target": {"mode": "color-picker"},
        })
        assert response and response["ok"], f"Mode control failed: {response}"
        wait_until(
            "mode synchronization",
            lambda: (
                request_statuses(lamps) is None
                and all(lamp.latest_status() == ("Color Picker", 0) for lamp in lamps)
            ),
            timeout=8,
        )

        response = lamps[0].control({
            "api": "glow.control/1",
            "requestId": "select-saturation",
            "operation": "mode.option.set",
            "scope": "group",
            "target": {"mode": "color-picker"},
            "option": 1,
        })
        assert response and response["ok"], f"Option control failed: {response}"
        wait_until(
            "option synchronization",
            lambda: (
                request_statuses(lamps) is None
                and all(lamp.latest_status() == ("Color Picker", 1) for lamp in lamps)
            ),
            timeout=8,
        )

        detached = lamps[0].control({
            "api": "glow.control/1",
            "requestId": "detach",
            "operation": "sync.configure",
            "scope": "local",
            "sync": {"follow": False, "publish": False},
        })
        assert detached and detached["ok"], f"Could not detach lamp: {detached}"

        local_rainbow = lamps[0].control({
            "api": "glow.control/1",
            "operation": "mode.select",
            "scope": "local",
            "target": {"mode": "rainbow"},
        })
        assert local_rainbow and local_rainbow["ok"]
        wait_until(
            "detached lamp to diverge locally",
            lambda: (
                request_statuses(lamps) is None
                and lamps[0].latest_status() == ("Rainbow", 0)
                and all(l.latest_status() == ("Color Picker", 1) for l in lamps[1:])
            ),
            timeout=5,
        )

        group_static = lamps[1].control({
            "api": "glow.control/1",
            "operation": "mode.select",
            "scope": "group",
            "target": {"mode": "static"},
        })
        assert group_static and group_static["ok"]
        wait_until(
            "detached lamp to ignore group state",
            lambda: (
                request_statuses(lamps) is None
                and lamps[0].latest_status() == ("Rainbow", 0)
                and all(l.latest_status() == ("Static Light", 0) for l in lamps[1:])
            ),
            timeout=5,
        )

        rejoin = lamps[0].control({
            "api": "glow.control/1",
            "requestId": "rejoin",
            "operation": "sync.configure",
            "scope": "local",
            "sync": {"follow": True, "publish": False},
        })
        assert rejoin and rejoin["ok"], f"Could not rejoin lamp: {rejoin}"
        wait_until(
            "rejoined follower to adopt the group state",
            lambda: (
                request_statuses(lamps) is None
                and all(lamp.latest_status() == ("Static Light", 0) for lamp in lamps)
            ),
            timeout=8,
        )
        rejoined_state = lamps[0].request_json("STATE", "STATE")
        assert rejoined_state["sync"]["status"] == "synchronized"
        assert rejoined_state["sync"]["origin"] == int(node_ids[1])
        assert rejoined_state["mode"]["id"] == "static"
        # A real sensor update may legitimately make the follower dirty again
        # immediately after it accepted the snapshot while publishing is off.

        blocked = lamps[0].control({
            "api": "glow.control/1",
            "operation": "mode.select",
            "scope": "group",
            "target": {"mode": "rainbow"},
        })
        assert blocked and not blocked["ok"]
        assert blocked["error"]["code"] == "SYNC_PUBLISH_DISABLED"

        enabled = lamps[0].control({
            "api": "glow.control/1",
            "operation": "sync.configure",
            "scope": "local",
            "sync": {"follow": True, "publish": True},
        })
        assert enabled and enabled["ok"]

        time.sleep(4)
        for index, lamp in enumerate(lamps):
            discoveries = lamp.discoveries()
            for other in range(len(lamps)):
                if other == index:
                    continue
                assert discoveries[node_ids[other]] == 1, (
                    f"{lamp.port} discovered {node_ids[other]} "
                    f"{discoveries[node_ids[other]]} times"
                )
            assert all(count == 1 for count in discoveries.values()), (
                f"Repeated discovery on {lamp.port}: {discoveries}"
            )
        assert_distinct_hostnames(lamps)
        assert_home_assistant_is_alive(lamps)
        assert_healthy(lamps)

        print(f"PASS ({len(lamps)} lamps): capabilities, sync policy, rejoin, "
              "group control, and heartbeat stability")
        return 0
    finally:
        for lamp in lamps:
            lamp.close()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, serial.SerialException, subprocess.CalledProcessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
