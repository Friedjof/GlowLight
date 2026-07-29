"""Pins the host-side frame builder against the firmware's wire format.

Runs without hardware. The golden frame is the first HELLO the native test
suite observes for a known key, MAC, boot id and counter
(test/native/tests_crypto.cpp, golden_hello_frame_is_byte_stable). If the two
implementations ever drift apart, this test says so.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import glow_frames as glow


GROUP_KEY = bytes.fromhex(
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
)
LOCAL_MAC = bytes.fromhex("983dae52877c")
LOCAL_BOOT_ID = bytes.fromhex("c94a8ae0594a3032356f4e53cae2ee45")
GOLDEN_HELLO = bytes.fromhex(
    "474c010100000100b0784cfb44b2e8d5983dae52877cc94a8ae0594a3032356f4e"
    "53cae2ee450000000000000001000080fd97ba798c87acd65a2848e995c1f1"
)


class FrameFormatTests(unittest.TestCase):
    def setUp(self):
        self.peer = glow.Peer(LOCAL_MAC, LOCAL_BOOT_ID, GROUP_KEY)

    def test_hello_matches_the_firmware_byte_for_byte(self):
        built = self.peer.build(glow.HELLO, b"", counter=1)
        self.assertEqual(built.hex(), GOLDEN_HELLO.hex())

    def test_group_tag_is_the_first_eight_hkdf_bytes(self):
        tag = glow.group_tag(GROUP_KEY)
        self.assertEqual(
            tag, GOLDEN_HELLO[glow.OFFSET_GROUP_TAG:glow.OFFSET_GROUP_TAG + 8]
        )

    def test_open_round_trips_a_firmware_frame(self):
        opened = self.peer.open(GOLDEN_HELLO)
        self.assertIsNotNone(opened)
        self.assertEqual(opened["type"], glow.HELLO)
        self.assertEqual(opened["counter"], 1)
        self.assertEqual(opened["mac"], LOCAL_MAC)
        self.assertEqual(opened["boot_id"], LOCAL_BOOT_ID)
        self.assertEqual(opened["payload"], b"")

    def test_tampering_is_detected(self):
        for index in (glow.OFFSET_VERSION, glow.OFFSET_COUNTER + 7,
                      len(GOLDEN_HELLO) - 1):
            with self.subTest(index=index):
                self.assertIsNone(self.peer.open(glow.flip_bit(GOLDEN_HELLO, index)))

    def test_a_foreign_key_cannot_open_the_frame(self):
        stranger = glow.Peer(LOCAL_MAC, LOCAL_BOOT_ID, bytes(32))
        self.assertIsNone(stranger.open(GOLDEN_HELLO))

    def test_fragmentation_matches_the_firmware_split(self):
        payload = bytes(glow.MAX_PLAINTEXT_SIZE)
        fragments = self.peer.build_data(payload)
        self.assertEqual(len(fragments), 3)

        lengths = [
            int.from_bytes(frame[glow.OFFSET_LENGTH:glow.OFFSET_LENGTH + 2], "big")
            for frame in fragments
        ]
        self.assertEqual(lengths, [186, 186, 140])
        self.assertEqual(sum(lengths), glow.MAX_PLAINTEXT_SIZE)

        for index, frame in enumerate(fragments):
            self.assertEqual(frame[glow.OFFSET_FRAGMENT_INDEX], index)
            self.assertEqual(frame[glow.OFFSET_FRAGMENT_COUNT], 3)
            self.assertLessEqual(len(frame), glow.ESP_NOW_MTU)

    def test_counters_increase_and_never_repeat(self):
        counters = [
            int.from_bytes(
                self.peer.build(glow.HEARTBEAT)[
                    glow.OFFSET_COUNTER:glow.OFFSET_COUNTER + 8
                ],
                "big",
            )
            for _ in range(5)
        ]
        self.assertEqual(counters, sorted(counters))
        self.assertEqual(len(set(counters)), len(counters))

    def test_node_id_folding_matches_the_firmware(self):
        # id = mac[3..5] in the top bytes, mac[0]^mac[1]^mac[2] in the low byte.
        mac = bytes.fromhex("aabbccddee01")
        expected = (0xDD << 24) | (0xEE << 16) | (0x01 << 8) | (0xAA ^ 0xBB ^ 0xCC)
        self.assertEqual(glow.node_id(mac), expected)


if __name__ == "__main__":
    unittest.main(verbosity=2)
