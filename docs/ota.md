# Password-Protected OTA

GlowLight can install PlatformIO firmware images over infrastructure WiFi. OTA is
disabled by default and uses a credential that is separate from the captive
portal password.

## Enabling OTA

The installer can generate a unique OTA password, or OTA can be enabled later
through the physically activated captive portal. The fixed username is
`glowlight`; the password must contain 12-63 characters.

```cpp
#define GLOW_OTA_ENABLED true
#define GLOW_OTA_PASSWORD "a-unique-password"
```

Validated NVS values override these compile-time defaults. Existing version-1
NVS configuration is migrated to schema version 2 with OTA disabled.

OTA starts only after `NetworkService` reaches the `Online` state. It is never
bound in captive-portal mode and stops immediately when the infrastructure WiFi
connection is lost. After reconnect it starts again automatically.

## Installing Firmware

1. Build the profile intended for the lamp, for example `make build`.
2. Open `http://<hostname>.local/update` on the same trusted network.
3. Authenticate as `glowlight` with the generated OTA password.
4. Upload `.pio/build/esp32c3/firmware.bin`.
5. Wait for the success response and automatic restart.

The server uses HTTP Digest authentication so the password is not sent directly
as HTTP Basic credentials. Each authenticated update page also creates a one-time
upload token, and the HTTP server is recreated after every reconnect so old
Digest nonces expire. Authentication and the token are checked before
`Update.begin()` and authentication is checked again before accepting the final
request.

After writing, GlowLight runs the ESP-IDF image verifier over the complete target
partition. Truncated, interrupted, rejected and invalid uploads restore the
running partition as the boot target and do not trigger a restart.

`platformio.ini` explicitly selects the repository's `partitions.csv` layout
with two 1,280-KiB application slots. `make build-profiles` validates the OTA
metadata and rejects any firmware image larger than the smaller slot.

## Security Boundary

Digest authentication and the one-time token protect access and request replay,
but plain HTTP
does not encrypt the firmware body and does not provide trusted image signing.
Use OTA only on a trusted local network. An active network attacker could alter
traffic, and a syntactically valid malicious image would pass the ESP32 image
format checks.

Production deployments needing protection from an active LAN attacker require
signed images and ESP32 Secure Boot, ideally combined with flash encryption and
TLS. Those platform provisioning steps are intentionally outside this software
phase.

OTA processing uses Arduino's synchronous `WebServer`; animations, disconnect
detection and ESP-NOW application processing can pause while a request is being
parsed. The
running firmware remains in the other app slot until the full new image passes
`Update.end(true)`.
