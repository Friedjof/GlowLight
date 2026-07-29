"""Builder for GlowLight secure ESP-NOW frames.

An independent implementation of the wire format used by
lib/CommunicationService. It lets the hardware tests craft replayed, tampered
and foreign-key traffic and feed it into a lamp through the INJECT console
command of the esp32c3-integration profile.

Frame layout (48 byte header, then ciphertext, then a 16 byte GCM tag):

    0   2   magic 'GL'
    2   1   version (1)
    3   1   frame type
    4   1   reserved (0)
    5   1   fragment index
    6   1   fragment count
    7   1   reserved (0)
    8   8   group tag   = HKDF(key, salt, "group-tag")[:8]
    16  6   sender MAC
    22  16  sender boot id
    38  8   frame counter, big endian
    46  2   plaintext length, big endian
"""

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDF

HKDF_SALT = b"GlowLight ESP-NOW v1"
GROUP_TAG_INFO = b"group-tag"
BOOT_KEY_INFO = b"boot-key"
NONCE_DOMAIN = b"GLW\x01"

HEADER_SIZE = 48
GCM_TAG_SIZE = 16
ESP_NOW_MTU = 250
MAX_FRAGMENT_SIZE = ESP_NOW_MTU - HEADER_SIZE - GCM_TAG_SIZE  # 186
MAX_PLAINTEXT_SIZE = 512

OFFSET_VERSION = 2
OFFSET_TYPE = 3
OFFSET_FRAGMENT_INDEX = 5
OFFSET_FRAGMENT_COUNT = 6
OFFSET_GROUP_TAG = 8
OFFSET_MAC = 16
OFFSET_BOOT_ID = 22
OFFSET_COUNTER = 38
OFFSET_LENGTH = 46

HELLO = 1
CHALLENGE = 2
PROOF = 3
HEARTBEAT = 4
DATA = 5


def hkdf(key: bytes, info: bytes, length: int) -> bytes:
    return HKDF(
        algorithm=hashes.SHA256(), length=length, salt=HKDF_SALT, info=info
    ).derive(key)


def group_tag(group_key: bytes) -> bytes:
    return hkdf(group_key, GROUP_TAG_INFO, 8)


def boot_key(group_key: bytes, mac: bytes, boot_id: bytes) -> bytes:
    return hkdf(group_key, BOOT_KEY_INFO + mac + boot_id, 32)


def nonce(counter: int) -> bytes:
    return NONCE_DOMAIN + counter.to_bytes(8, "big")


class Peer:
    """A simulated group member."""

    def __init__(self, mac: bytes, boot_id: bytes, group_key: bytes):
        assert len(mac) == 6 and len(boot_id) == 16 and len(group_key) == 32
        self.mac = mac
        self.boot_id = boot_id
        self.group_key = group_key
        self.counter = 0

    def build(self, frame_type, payload=b"", fragment_index=0, fragment_count=1,
              counter=None):
        if counter is None:
            self.counter += 1
            counter = self.counter

        header = bytearray(HEADER_SIZE)
        header[0:2] = b"GL"
        header[OFFSET_VERSION] = 1
        header[OFFSET_TYPE] = frame_type
        header[OFFSET_FRAGMENT_INDEX] = fragment_index
        header[OFFSET_FRAGMENT_COUNT] = fragment_count
        header[OFFSET_GROUP_TAG:OFFSET_GROUP_TAG + 8] = group_tag(self.group_key)
        header[OFFSET_MAC:OFFSET_MAC + 6] = self.mac
        header[OFFSET_BOOT_ID:OFFSET_BOOT_ID + 16] = self.boot_id
        header[OFFSET_COUNTER:OFFSET_COUNTER + 8] = counter.to_bytes(8, "big")
        header[OFFSET_LENGTH:OFFSET_LENGTH + 2] = len(payload).to_bytes(2, "big")

        key = boot_key(self.group_key, self.mac, self.boot_id)
        sealed = AESGCM(key).encrypt(nonce(counter), payload, bytes(header))
        return bytes(header) + sealed

    def build_data(self, payload: bytes):
        """Splits a payload the way the firmware does."""
        assert len(payload) <= MAX_PLAINTEXT_SIZE
        count = max(1, (len(payload) + MAX_FRAGMENT_SIZE - 1) // MAX_FRAGMENT_SIZE)
        return [
            self.build(
                DATA,
                payload[i * MAX_FRAGMENT_SIZE:(i + 1) * MAX_FRAGMENT_SIZE],
                fragment_index=i,
                fragment_count=count,
            )
            for i in range(count)
        ]

    def open(self, frame: bytes):
        """Decrypts a frame a lamp transmitted. Returns None if it is not ours."""
        if len(frame) < HEADER_SIZE + GCM_TAG_SIZE:
            return None
        length = int.from_bytes(frame[OFFSET_LENGTH:OFFSET_LENGTH + 2], "big")
        if len(frame) != HEADER_SIZE + length + GCM_TAG_SIZE:
            return None

        header = frame[:HEADER_SIZE]
        mac = frame[OFFSET_MAC:OFFSET_MAC + 6]
        remote_boot_id = frame[OFFSET_BOOT_ID:OFFSET_BOOT_ID + 16]
        counter = int.from_bytes(frame[OFFSET_COUNTER:OFFSET_COUNTER + 8], "big")

        key = boot_key(self.group_key, mac, remote_boot_id)
        try:
            payload = AESGCM(key).decrypt(nonce(counter), frame[HEADER_SIZE:], header)
        except Exception:
            return None

        return {
            "type": frame[OFFSET_TYPE],
            "fragment_index": frame[OFFSET_FRAGMENT_INDEX],
            "fragment_count": frame[OFFSET_FRAGMENT_COUNT],
            "mac": mac,
            "boot_id": remote_boot_id,
            "counter": counter,
            "payload": payload,
        }


def challenge_payload(target_mac: bytes, target_boot_id: bytes, challenge: bytes) -> bytes:
    return target_mac + target_boot_id + challenge


def node_id(mac: bytes) -> int:
    """The 32 bit id the firmware folds a MAC into."""
    return (mac[3] << 24) | (mac[4] << 16) | (mac[5] << 8) | (mac[0] ^ mac[1] ^ mac[2])


def flip_bit(frame: bytes, index: int) -> bytes:
    tampered = bytearray(frame)
    tampered[index] ^= 0x01
    return bytes(tampered)
