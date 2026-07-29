#!/usr/bin/env python3
"""Hardware test: two lamps with different group keys must ignore each other.

Flashes each of the two connected lamps with a different group key and verifies
that neither authenticates the other, neither lists the other as a node, and
that a mode change on one lamp does not propagate to the other. Afterwards both
lamps are flashed back with the key that was in include/GlowConfig.h, and the
file is restored.

    python3 test/hil/two_lamp_group_isolation.py [--ports /dev/ttyACM0 /dev/ttyACM1]
"""

import argparse
import os
import re
import secrets
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "scripts" / "setup"))

from two_lamp_sync import Lamp, connected_ports, wait_until

CONFIG_PATH = PROJECT_ROOT / "include" / "GlowConfig.h"
ENVIRONMENT = "esp32c3-integration"
KEY_PATTERN = re.compile(r'^#define\s+GLOW_GROUP_KEY_HEX\s+"([0-9a-fA-F]{64})"',
                         re.MULTILINE)
IDENTITY_PATTERN = re.compile(r"\[TEST\] IDENTITY\|[0-9a-f]{12}\|[0-9a-f]{32}\|(\d+)")
NODES_PATTERN = re.compile(r"\[TEST\] NODES\|(.*)")

# Long enough for several heartbeat rounds of the integration profile.
OBSERVATION_SECONDS = 12


def read_key():
    match = KEY_PATTERN.search(CONFIG_PATH.read_text())
    if match is None:
        raise SystemExit(
            "include/GlowConfig.h has no group key. Provision one with the setup first."
        )
    return match.group(1).lower()


def write_key(key):
    content = CONFIG_PATH.read_text()
    content = KEY_PATTERN.sub(f'#define GLOW_GROUP_KEY_HEX "{key}"', content)
    CONFIG_PATH.write_text(content)
    os.chmod(CONFIG_PATH, 0o600)


def flash(port):
    subprocess.run(
        ["pio", "run", "--environment", ENVIRONMENT, "--target", "upload",
         "--upload-port", port],
        cwd=PROJECT_ROOT,
        check=True,
        stdout=subprocess.DEVNULL,
    )


def flash_with_key(port, key, label):
    print(f"  flashing {port} with the {label} key")
    write_key(key)
    flash(port)


def ask(lamp, command, pattern, description, timeout=5):
    before = len(lamp.snapshot())
    lamp.send(command)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for line in lamp.snapshot()[before:]:
            match = pattern.search(line)
            if match:
                return match
        time.sleep(0.1)
    raise AssertionError(f"{lamp.port} did not report {description}")


def node_id(lamp):
    return int(ask(lamp, "IDENTITY", IDENTITY_PATTERN, "its identity").group(1))


def nodes(lamp):
    raw = ask(lamp, "NODES", NODES_PATTERN, "its node list").group(1).strip()
    return {int(value) for value in raw.split(",") if value}


def status(lamp, timeout=5):
    from two_lamp_sync import STATUS_PATTERN
    match = ask(lamp, "STATUS", STATUS_PATTERN, "its status", timeout)
    return match.group(1), int(match.group(2))


def run_checks(ports):
    lamps = [Lamp(port) for port in ports]
    try:
        for lamp in lamps:
            lamp.reset()
        wait_until(
            "both firmware instances to start",
            lambda: all(
                lamp.count_since_boot("GlowLight started") >= 1 for lamp in lamps
            ),
            timeout=25,
        )
        for lamp in lamps:
            assert lamp.count_since_boot("Communication disabled") == 0, (
                f"{lamp.port} has no usable group key"
            )
            assert lamp.count_since_boot("Secure communication initialized") >= 1, (
                f"{lamp.port} did not bring up the encrypted transport"
            )

        ids = [node_id(lamp) for lamp in lamps]
        assert ids[0] != ids[1], f"both lamps report the same node id: {ids}"
        print(f"  node ids: {ids[0]} and {ids[1]}")

        # Both lamps are broadcasting the whole time; give them ample chance.
        print(f"  listening for {OBSERVATION_SECONDS}s ...")
        time.sleep(OBSERVATION_SECONDS)

        for index, lamp in enumerate(lamps):
            peer = ids[1 - index]
            assert lamp.count_since_boot("Authenticated node") == 0, (
                f"{lamp.port} authenticated a lamp from a different group"
            )
            visible = nodes(lamp)
            assert peer not in visible, (
                f"{lamp.port} discovered the foreign lamp {peer}: {visible}"
            )
            assert not visible, f"{lamp.port} discovered unexpected nodes: {visible}"
        print("  neither lamp discovered the other")

        # A mode change must not cross the group boundary.
        before = status(lamps[1])
        lamps[0].send("NEXT_MODE")
        time.sleep(3)
        after = status(lamps[1])
        assert after == before, (
            f"a mode change crossed group boundaries: {before} -> {after}"
        )
        assert status(lamps[0])[0] != before[0], (
            "the sending lamp did not change its own mode, the check proves nothing"
        )
        print("  a mode change on one lamp did not reach the other")

        for lamp in lamps:
            lines = lamp.lines_since_boot()
            assert not any("Guru Meditation" in line for line in lines), (
                f"{lamp.port} crashed"
            )
            assert not any("Rebooting" in line for line in lines), (
                f"{lamp.port} rebooted"
            )
    finally:
        for lamp in lamps:
            lamp.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ports", nargs=2, metavar=("LAMP_1", "LAMP_2"),
                        help="exactly two lamps; the isolation check pairs them")
    parser.add_argument(
        "--keep-keys",
        action="store_true",
        help="leave the lamps on the two different keys instead of restoring",
    )
    args = parser.parse_args()

    ports = args.ports or connected_ports()
    if len(ports) != 2:
        raise SystemExit(
            f"The isolation check needs exactly two lamps, found: {ports}.\n"
            "Pass --ports to pick two of them."
        )

    original_key = read_key()
    backup = CONFIG_PATH.with_suffix(".h.isolation-backup")
    shutil.copy2(CONFIG_PATH, backup)

    try:
        foreign_key = secrets.token_hex(32)
        flash_with_key(ports[0], original_key, "original")
        flash_with_key(ports[1], foreign_key, "foreign")
        run_checks(ports)
        print("\nPASS: lamps with different group keys stay isolated")
        result = 0
    except AssertionError as error:
        print(f"\nFAIL: {error}")
        result = 1
    finally:
        shutil.copy2(backup, CONFIG_PATH)
        os.chmod(CONFIG_PATH, 0o600)
        backup.unlink()
        if not args.keep_keys:
            print("  restoring the original key on both lamps")
            for port in ports:
                flash(port)

    return result


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
