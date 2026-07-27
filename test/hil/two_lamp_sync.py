#!/usr/bin/env python3
"""Hardware integration test for two connected GlowLight lamps."""

import argparse
from collections import Counter
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
NODE_PATTERN = re.compile(r"NodeID: (\d+)")
DISCOVERY_PATTERN = re.compile(r"Discovered node (\d+)")


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

    def latest_status(self):
        for line in reversed(self.snapshot()):
            match = STATUS_PATTERN.search(line)
            if match:
                return match.group(1), int(match.group(2))
        return None

    def node_id(self):
        for line in reversed(self.snapshot()):
            match = NODE_PATTERN.search(line)
            if match:
                return match.group(1)
        return None

    def discoveries(self):
        return Counter(
            match.group(1)
            for line in self.snapshot()
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
        lines = lamp.snapshot()
        assert not any("Guru Meditation" in line for line in lines), f"{lamp.port} crashed"
        assert not any("Rebooting" in line for line in lines), f"{lamp.port} rebooted"
        assert not any("Message too large" in line for line in lines), (
            f"{lamp.port} exceeded the ESP-NOW payload limit"
        )


def request_statuses(lamps):
    for lamp in lamps:
        lamp.send("STATUS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ports", nargs=2, metavar=("LAMP_1", "LAMP_2"))
    parser.add_argument("--skip-flash", action="store_true")
    args = parser.parse_args()

    ports = args.ports or connected_ports()
    if len(ports) != 2:
        raise SystemExit(f"Expected exactly two ESP32-C3 lamps, found: {ports}")

    if not args.skip_flash:
        for port in ports:
            flash(port)

    lamps = [Lamp(port) for port in ports]
    try:
        for lamp in lamps:
            lamp.reset()

        wait_until(
            "both firmware instances to start",
            lambda: all(lamp.count("GlowLight started") >= 1 for lamp in lamps),
            timeout=20,
        )
        node_ids = [lamp.node_id() for lamp in lamps]
        assert None not in node_ids and node_ids[0] != node_ids[1], (
            f"Invalid local node IDs: {node_ids}"
        )
        wait_until(
            "mutual ESP-NOW discovery",
            lambda: all(
                lamps[index].discoveries()[node_ids[1 - index]] == 1
                for index in range(2)
            ),
            timeout=10,
        )

        request_statuses(lamps)
        wait_until(
            "both lamps to settle in Static Light",
            lambda: all(lamp.latest_status() == ("Static Light", 0) for lamp in lamps),
            timeout=10,
        )

        lamps[0].send("NEXT_MODE")
        wait_until(
            "mode synchronization",
            lambda: (
                request_statuses(lamps) is None
                and all(lamp.latest_status() == ("Color Picker", 0) for lamp in lamps)
            ),
            timeout=8,
        )

        lamps[0].send("NEXT_OPTION")
        wait_until(
            "option synchronization",
            lambda: (
                request_statuses(lamps) is None
                and all(lamp.latest_status() == ("Color Picker", 1) for lamp in lamps)
            ),
            timeout=8,
        )

        time.sleep(4)
        for index, lamp in enumerate(lamps):
            discoveries = lamp.discoveries()
            assert discoveries[node_ids[1 - index]] == 1
            assert all(count == 1 for count in discoveries.values()), (
                f"Repeated discovery on {lamp.port}: {discoveries}"
            )
        assert_healthy(lamps)

        print("PASS: discovery, mode sync, option sync, and heartbeat stability")
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
