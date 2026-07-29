# Runtime Configuration and Captive Portal

GlowLight loads network, radio, group and synchronization defaults from NVS at
boot. If NVS has no valid record, the values in `include/GlowConfig.h` are used.
If neither source validates, the firmware fails closed with infrastructure WiFi
and group communication disabled on fallback channel 1.

Hardware pins, compiled lighting modes, queue sizes and maximum group size remain
build-time configuration.

## Stored Configuration

The `glow.config` schema contains:

- infrastructure WiFi enabled state, SSID, password and hostname
- ESP-NOW fallback channel
- secure group communication enabled state and 256-bit group key
- default `follow` and `publish` synchronization policy
- OTA enabled state and write-only OTA password

The `PreferencesConfigStore` keeps two CRC-protected slots in the `glowcfg` NVS
namespace. A save writes the older slot with a higher generation and verifies it
before reporting success. At boot the highest valid generation wins; a corrupt or
partially written slot is ignored as a whole.

NVS persistence does not imply flash encryption. A person with physical flash
access can recover stored WiFi, group and OTA credentials unless ESP32 flash/NVS
encryption is enabled for the product.

## Opening the Portal

The installer can enable the portal and generates an individual WPA2 password.
With the feature enabled:

1. Disconnect power from the lamp.
2. Hold the lamp button.
3. Apply power and keep the button held during boot.
4. Join `<hostname>-setup` using the generated portal password.
5. Open `http://192.168.4.1/` if the operating system does not open the captive
   page automatically. The actual address is printed on the serial console.

The portal never opens merely because an infrastructure access point is down and
automatically restarts the lamp after ten minutes.
It runs in `WIFI_AP_STA` mode on the configured fallback channel, leaves modem
sleep disabled and does not deinitialize ESP-NOW. Existing secure group
communication therefore remains available to lamps on the same channel.

An ESP32-C3 has one radio. A peer connected to an infrastructure access point on
a different channel cannot communicate with the provisioning lamp until both are
again on the same channel.

## Configuration API

The portal exposes `glow.config/1` only while the physically activated setup AP
is running:

```text
GET  /api/config
POST /api/config
POST /api/factory-reset
```

`GET /api/config` returns redacted state and a per-boot CSRF `sessionToken`. WiFi
passwords and group keys are never returned. Mutating requests require that token
in the `X-Glow-Token` header. Access authentication is provided by the individual
WPA2 portal password; the token prevents cross-origin form submission. An empty
password or key field preserves the stored secret.

```json
{
  "api": "glow.config/1",
  "wifi": {
    "enabled": true,
    "ssid": "home",
    "password": "write-only",
    "hostname": "glow-bedroom"
  },
  "radio": {"fallbackChannel": 6},
  "group": {
    "enabled": true,
    "key": "64 hexadecimal characters, write-only",
    "follow": true,
    "publish": true
  },
  "ota": {"enabled": true, "password": "write-only"}
}
```

Updates are validated and stored atomically, then the lamp restarts. Active
WiFi credentials and group keys are never replaced live. Factory reset clears
NVS overrides and restarts into the compile-time configuration.

The portal deliberately does not expose `Controller::executeControl()`. The
configuration AP and session token protect provisioning, but are not a general
authorization mechanism for lamp control.

OTA credentials are independent from the portal credentials. An empty OTA
password preserves the existing value. See [ota.md](ota.md) for update behavior
and its trusted-network security boundary.
