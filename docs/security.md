# Secure lamp-to-lamp communication

GlowLight lamps synchronise over ESP-NOW broadcasts. ESP-NOW broadcast traffic is
not encrypted by the radio, so GlowLight secures it in the application layer: all
lamps of a group share one 256-bit key, and every frame is encrypted and
authenticated with AES-256-GCM.

This document describes what that protects against, what it does not, and how to
provision and rotate a group key.

## Threat model

**Protected against**

- **Eavesdropping.** Frame payloads are encrypted. An observer learns frame sizes
  and timing, not content.
- **Forgery and tampering.** Every frame carries a GCM tag over the full 48-byte
  header and the payload. Changing a single bit in the type, counter, sender MAC,
  boot id, fragment fields or ciphertext makes the frame fail authentication.
- **Replay.** A peer is only trusted after it answers a fresh 128-bit challenge.
  The counter of that proof frame becomes the replay floor: nothing at or below it
  is ever accepted again. Above the floor a 64-frame sliding window rejects
  duplicates while tolerating loss and reordering.
- **Outsiders.** A lamp without the group key cannot join, be discovered, or
  influence any lamp in the group. Old plaintext firmware is ignored entirely.

**Not protected against**

- **Group members.** Everyone holding the group key can read every message and
  impersonate any other member. The key is a group secret, not a per-lamp
  identity. Only share it with lamps you control.
- **Jamming.** Anyone can drown out the channel. There is no availability
  guarantee on an open radio band.
- **Live relaying.** An attacker who forwards genuine frames in real time between
  two locations extends the group's range. Frames stay valid because they are
  genuine.
- **Physical access.** The group key sits in flash as a compile-time constant.
  Whoever can read the flash can read the key and thus join the group.
- **Traffic analysis.** Each frame carries an 8-byte group tag derived from the
  group key. It is a plaintext, stable identifier that lets an observer recognise
  frames of the same group across time and locations, and count the lamps.
  It exists so foreign traffic can be discarded before any crypto work.
  Rotating the group key is what changes it.

## How a session is set up

1. Every lamp draws a random 128-bit **boot id** at start-up and keeps a 64-bit
   frame counter that only ever increases. The counter never wraps: a lamp stops
   sending rather than reuse one.
2. The key for a boot session is
   `HKDF-SHA256(group key, salt = "GlowLight ESP-NOW v1", info = "boot-key" ‖ MAC ‖ boot id)`.
   Because the boot id is fresh on every start, the nonce
   (`"GLW\x01" ‖ counter`) is never reused with the same key.
3. A new peer is answered with a **CHALLENGE** naming the receiver's own MAC and
   boot id plus 128 fresh random bits. Only a peer that is really there and holds
   the group key can return the matching **PROOF**.
4. The proof promotes the peer: its boot session becomes authenticated, the proof
   counter becomes the replay floor, and the peer appears in discovery.

Until a peer completes this exchange its data frames are discarded, it does not
appear in `getNodes()`, and it cannot refresh anyone's last-seen time.

Key properties the implementation maintains:

- **Nothing changes state before the GCM tag verifies.** Session slots, replay
  windows and discovery are only touched by authenticated frames.
- **An authenticated peer is only ever replaced by a fresh proof.** A recording
  from an earlier boot session of the same lamp triggers a rate-limited challenge
  the attacker cannot answer; the live session is untouched.
- **Fail closed.** With a missing, malformed or all-zero group key the whole
  communication service stays disabled instead of falling back to plaintext.

## Limits

| Property | Value |
|---|---|
| Lamps per group | 8 |
| Group key | 256 bit, 64 hex characters |
| Largest message | 512 bytes, sent as up to 3 fragments |
| Frame overhead | 48 byte header + 16 byte GCM tag |
| Replay window | 64 frames above the floor |
| Challenge lifetime | 5 s, up to 4 retries |
| Reassembly timeout | 2 s |
| Session slot reuse | after `GLOW_NODE_TIMEOUT` of silence |

Replay state lives in RAM only; no counters are written to flash. After a reboot a
lamp trusts nobody until each peer has proved itself again.

## Provisioning a group key

Run the setup and follow the ESP-NOW section:

```bash
./install.sh          # or: python3 scripts/setup/main.py
```

Choose **create** on the first lamp — it generates a key with `secrets.token_hex`
and shows only the last 8 characters plus a SHA-256 fingerprint. Read the full key
out of your local `include/GlowConfig.h`, then choose **join** on every other lamp
and paste it; the input is not echoed.

The setup keeps `include/GlowConfig.h` and all backups at mode `0600` and never
prints a group key in full. `include/GlowConfig.h` is gitignored and must stay
that way.

Two lamps are in the same group exactly when their `GLOW_GROUP_KEY_HEX` values
match. Lamps with different keys neither discover nor influence each other.

## Rotating a key

There is no over-the-air group key rotation protocol. A key is changed by
reflashing or independently through each lamp's physically activated captive
portal. Portal updates are persisted to NVS and become active only after reboot.

1. Generate a new key (setup, **create**, on one lamp).
2. Flash **all** lamps of the group with it.

While the rollout is in progress, lamps on the old key and lamps on the new key
form two separate groups and ignore each other. Rotate when you suspect a key was
disclosed, when a lamp leaves your control, or when you want to break the
linkability of the public group tag.

The portal uses an individual WPA2 access-point password and a per-boot request
token. WiFi passwords and group keys are write-only and never returned by its
API. NVS itself is not encrypted by this application; physical flash extraction
remains outside the software threat boundary unless platform flash encryption is
enabled.

OTA uses a separate password and HTTP Digest authentication. This prevents an
unauthenticated client from starting a flash write, but it does not encrypt or
sign the firmware body. OTA is therefore restricted to trusted infrastructure
networks; signed-image enforcement and ESP32 Secure Boot remain required against
an active network attacker. See [ota.md](ota.md).

The optional Home Assistant connection widens the boundary once more. The lamp
accepts commands from an MQTT broker without authenticating the sender: anyone
who can publish to the configured topics can control the lamp, exactly as anyone
who can reach the OTA endpoint with the password can reflash it. Broker
credentials protect the connection to the broker, not the commands themselves.
The connection is plain MQTT, so it belongs on a network you control. What the
lamp publishes never contains the group key, the OTA password or the broker
password; a host test asserts this over every generated payload. See
[home-assistant.md](home-assistant.md).

## Migration from older firmware

Firmware from before the secure transport sent plaintext JSON over ESP-NOW. That
format is gone, and there is no compatibility mode:

- Old lamps cannot see or control lamps running this firmware, and vice versa.
- Their plaintext frames are dropped at the first header check.
- Update every lamp of a group and provision the same key on each. A lamp that is
  not updated simply stops participating.

`MESH_ON` now defaults to `false` in the template, so a fresh checkout builds
without networking until a key has been provisioned.

## Verifying

```bash
make test              # host tests: crypto vectors, tampering, replay, fragmentation
make build-profiles    # all three firmware profiles
```

The host suite in `test/native/` compiles the real `CommunicationService` against
thin Arduino/ESP-NOW/FreeRTOS shims and drives it with crafted frames built by an
independent implementation (`test/native/peer.cpp`). `test/hil/glow_frames.py`
mirrors the same wire format for hardware tests and is pinned against the firmware
by a golden frame.

On hardware:

```bash
make test-integration              # two lamps: discovery, mode and option sync
make test-security KEY=<64 hex>    # one lamp: replay, tampering, foreign keys
```

`make test-security` uses console commands (`INJECT`, `TRACE`, `IDENTITY`,
`NODES`) that exist **only** in the `esp32c3-integration` profile. They are
compiled out of `esp32c3` and `esp32c3-all-modes`; never ship integration firmware
to a real lamp.
