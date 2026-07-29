#!/usr/bin/env python3
"""Hardware test for the secure ESP-NOW transport, using a single lamp.

Traffic is crafted on the host with test/hil/glow_frames.py and pushed into the
lamp through the INJECT console command, while the lamp echoes its own frames
via TRACE. That makes a full challenge/proof handshake possible without a
second radio, so replay and tampering can be verified on real hardware.

Requires firmware from the esp32c3-integration profile and the group key that
lamp was flashed with:

    python3 test/hil/secure_transport.py --key <64 hex chars> [--port /dev/ttyACM0]
"""

import argparse
import json
import os
import re
import secrets
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import glow_frames as glow
from two_lamp_sync import Lamp, connected_ports, wait_until

PROJECT_ROOT = Path(__file__).resolve().parents[2]
ENVIRONMENT = "esp32c3-integration"
IDENTITY_PATTERN = re.compile(r"\[TEST\] IDENTITY\|([0-9a-f]{12})\|([0-9a-f]{32})\|(\d+)")
NODES_PATTERN = re.compile(r"\[TEST\] NODES\|(.*)")
TX_PATTERN = re.compile(r"\[TEST\] TX\|([0-9a-f]+)")

PEER_MAC = bytes.fromhex("aabbccddee01")
PEER_BOOT_A = bytes.fromhex("0f0e0d0c0b0a09080706050403020100")
PEER_BOOT_B = bytes.fromhex("112233445566778899aabbccddeeff00")

# Counters start high enough to leave room for replays below the proof floor.
START_COUNTER = 500


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)


class Console:
    """Serial console around a lamp running the integration profile."""

    def __init__(self, lamp: Lamp):
        self.lamp = lamp

    def mark(self):
        return len(self.lamp.snapshot())

    def send(self, command, settle=0.15):
        # Long INJECT lines are written in chunks so the firmware, which only
        # drains the console once per loop, can keep up.
        payload = (command + "\n").encode("ascii")
        for offset in range(0, len(payload), 96):
            self.lamp.serial.write(payload[offset:offset + 96])
            self.lamp.serial.flush()
            time.sleep(0.01)
        time.sleep(settle)

    def inject(self, frame: bytes, settle=0.2):
        self.send("INJECT " + frame.hex(), settle)

    def _await(self, pattern, since, description, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for line in self.lamp.snapshot()[since:]:
                match = pattern.search(line)
                if match:
                    return match
            time.sleep(0.1)
        raise Failure(f"lamp did not report {description}")

    def identity(self):
        since = self.mark()
        self.lamp.send("IDENTITY")
        match = self._await(
            IDENTITY_PATTERN, since, "its identity (integration profile flashed?)"
        )
        return (
            bytes.fromhex(match.group(1)),
            bytes.fromhex(match.group(2)),
            int(match.group(3)),
        )

    def nodes(self):
        since = self.mark()
        self.lamp.send("NODES")
        match = self._await(NODES_PATTERN, since, "its node list")
        return {int(value) for value in match.group(1).strip().split(",") if value}

    def transmitted(self, peer: glow.Peer, since: int):
        """Frames the lamp broadcast since the mark, decrypted with the group key."""
        frames = []
        for line in self.lamp.snapshot()[since:]:
            match = TX_PATTERN.search(line)
            if match is None:
                continue
            opened = peer.open(bytes.fromhex(match.group(1)))
            if opened is not None:
                frames.append(opened)
        return frames

    def challenge_for(self, peer: glow.Peer, since: int, timeout=6):
        """The challenge the lamp issued for this peer's current boot session."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for frame in reversed(self.transmitted(peer, since)):
                payload = frame["payload"]
                if frame["type"] != glow.CHALLENGE or len(payload) != 38:
                    continue
                if payload[:6] == peer.mac and payload[6:22] == peer.boot_id:
                    return payload[22:38]
            time.sleep(0.2)
        raise Failure("the lamp did not challenge the peer")


def handshake(console: Console, peer: glow.Peer):
    """HELLO -> CHALLENGE -> PROOF. Returns the counter of the proof frame."""
    since = console.mark()
    console.inject(peer.build(glow.HELLO))
    challenge = console.challenge_for(peer, since)
    console.inject(peer.build(glow.PROOF, challenge))
    time.sleep(0.4)
    check(
        glow.node_id(peer.mac) in console.nodes(),
        "the lamp did not authenticate the peer after a valid proof",
    )
    return peer.counter


def wipe_message(count: int) -> bytes:
    return json.dumps({"type": 3, "message": {"numberOfWipes": count}}).encode()


def run_checks(console: Console, group_key: bytes):
    console.send("TRACE ON")
    local_mac, local_boot_id, local_node = console.identity()
    print(f"  lamp mac={local_mac.hex()} boot={local_boot_id.hex()} node={local_node}")

    peer = glow.Peer(PEER_MAC, PEER_BOOT_A, group_key)
    peer.counter = START_COUNTER
    peer_node = glow.node_id(PEER_MAC)

    # --- traffic that must never be accepted --------------------------------

    stranger = glow.Peer(PEER_MAC, PEER_BOOT_B, secrets.token_bytes(32))
    for _ in range(5):
        console.inject(stranger.build(glow.HELLO))
    check(peer_node not in console.nodes(), "a lamp from a different group was accepted")

    console.inject(peer.build_data(wipe_message(9))[0])
    check(peer_node not in console.nodes(), "unauthenticated data created a node")

    valid = peer.build(glow.HELLO)
    for index, label in (
        (len(valid) - 1, "gcm tag"),
        (glow.OFFSET_COUNTER + 7, "counter"),
        (glow.OFFSET_GROUP_TAG, "group tag"),
        (glow.OFFSET_VERSION, "version"),
    ):
        console.inject(glow.flip_bit(valid, index))
        check(
            peer_node not in console.nodes(),
            f"a frame with a tampered {label} was accepted",
        )
    print("  unauthenticated, foreign-key and tampered traffic rejected")

    # --- a recording taken before the handshake -----------------------------

    recorded_before = peer.build_data(wipe_message(7))[0]
    recorded_counter = peer.counter

    proof_counter = handshake(console, peer)
    check(
        proof_counter > recorded_counter,
        "test setup: the recording must predate the handshake",
    )
    print(f"  peer authenticated, replay floor at counter {proof_counter}")

    # Fresh data must arrive; the recording must not.
    since = console.mark()
    for fragment in peer.build_data(wipe_message(2)):
        console.inject(fragment)
    time.sleep(0.5)

    console.inject(recorded_before)
    console.inject(recorded_before)
    time.sleep(0.3)
    check(
        peer_node in console.nodes(),
        "the peer was dropped while replaying an old recording",
    )
    print("  recording from before the handshake rejected")

    # --- a replayed frame from an earlier boot session ----------------------

    previous_boot = glow.Peer(PEER_MAC, PEER_BOOT_B, group_key)
    previous_boot.counter = 10
    for _ in range(5):
        console.inject(previous_boot.build(glow.HELLO))
    check(
        peer_node in console.nodes(),
        "a replayed frame from an old boot session unseated the live peer",
    )

    # The live peer must still be able to talk after the replay burst.
    since = console.mark()
    for fragment in peer.build_data(wipe_message(3)):
        console.inject(fragment)
    time.sleep(0.4)
    check(peer_node in console.nodes(), "the peer went missing after a replay burst")
    print("  replayed old-boot frames did not unseat the authenticated peer")

    # --- fragmentation ------------------------------------------------------

    padding = "p" * (glow.MAX_PLAINTEXT_SIZE - 44)
    largest = json.dumps({"type": 3, "message": {"numberOfWipes": 1, "pad": padding}})
    largest = largest.encode()[: glow.MAX_PLAINTEXT_SIZE]
    fragments = peer.build_data(largest)
    check(len(fragments) == 3, f"expected three fragments, built {len(fragments)}")
    for fragment in fragments:
        console.inject(fragment)
    time.sleep(0.5)
    check(peer_node in console.nodes(), "the lamp dropped the peer on a large message")
    print("  three-fragment message accepted")

    # A message with a fragment missing must not disturb the session.
    incomplete = peer.build_data(largest)
    console.inject(incomplete[0])
    console.inject(incomplete[2])
    time.sleep(0.5)
    check(peer_node in console.nodes(), "an incomplete message disturbed the session")
    print("  incomplete message handled without side effects")

    # --- the lamp must have survived all of it ------------------------------

    lines = console.lamp.snapshot()
    check(
        not any("Guru Meditation" in line for line in lines),
        "the lamp crashed while processing injected traffic",
    )
    check(
        not any("Rebooting" in line for line in lines),
        "the lamp rebooted while processing injected traffic",
    )
    console.send("TRACE OFF")


def flash(port):
    subprocess.run(
        ["pio", "run", "--environment", ENVIRONMENT, "--target", "upload",
         "--upload-port", port],
        cwd=PROJECT_ROOT,
        check=True,
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port")
    parser.add_argument(
        "--key",
        help="group key of the flashed lamp, 64 hex characters "
             "(or set GLOW_GROUP_KEY_HEX)",
    )
    parser.add_argument("--skip-flash", action="store_true")
    args = parser.parse_args()

    key_hex = args.key or os.environ.get("GLOW_GROUP_KEY_HEX")
    if not key_hex or len(key_hex) != 64:
        raise SystemExit(
            "A 64 character group key is required: pass --key or set "
            "GLOW_GROUP_KEY_HEX. It must match include/GlowConfig.h of the "
            "flashed lamp."
        )
    group_key = bytes.fromhex(key_hex)

    port = args.port
    if port is None:
        ports = connected_ports()
        if len(ports) != 1:
            raise SystemExit(
                f"Expected exactly one lamp, found {ports}. Pass --port explicitly."
            )
        port = ports[0]

    if not args.skip_flash:
        flash(port)

    lamp = Lamp(port)
    try:
        lamp.reset()
        wait_until(
            "the firmware to start",
            lambda: lamp.count("GlowLight started") >= 1,
            timeout=20,
        )
        run_checks(Console(lamp), group_key)
    except Failure as failure:
        print(f"\nFAILED: {failure}")
        return 1
    finally:
        lamp.close()

    print("\nSecure transport hardware checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
