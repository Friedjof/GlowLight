# Home Assistant

A lamp on the infrastructure WiFi can publish itself to Home Assistant over MQTT.
It announces its own entities, so nothing has to be configured in Home Assistant
by hand, and no YAML is involved.

## What you need

- The lamp joined to your WiFi (see [configuration.md](configuration.md))
- An MQTT broker Home Assistant already talks to, usually the Mosquitto add-on
- The MQTT integration enabled in Home Assistant

## Enabling it

Either in the setup before flashing:

```bash
./install.sh     # step "Home Assistant"
```

or later through the captive portal, which stores the settings in NVS and needs
no reflash. Both ask for broker host, port, optional credentials and the
discovery prefix (`homeassistant` unless you changed it in Home Assistant).

Home Assistant requires infrastructure WiFi. Enabling MQTT without WiFi is
rejected by the configuration validator rather than silently ignored.

## What appears

Every lamp becomes one device, identified by `glow-<node id>`, which is derived
from its MAC. Device id, base topic, MQTT client id and every entity id are
therefore unique even though all lamps are flashed from the same configuration.

The device **name** follows the hostname, which for the same reason gets the last
three MAC bytes appended unless you named the lamp yourself in the portal. So the
lamps appear as `glowlight-52c82c`, `glowlight-9d6fc0` and so on. Rename them in
Home Assistant to whatever suits the room; that only changes the display name.

The entities are generated from what the modes declare, so a firmware with more
modes simply offers more entities:

| Entity | Comes from |
|---|---|
| Light (on/off, brightness) | the `brightness` value every mode inherits |
| Mode (select) | the list of modes in the firmware |
| *Mode* option (select) | the options a mode registered, one entity per mode |
| One control per setting | every writable registry key of a mode |
| Follow group, Publish to group (switches) | the two synchronization controls |
| Sync status (sensor) | the runtime synchronization state |

An integer setting becomes a number with the declared minimum and maximum, a
boolean becomes a switch, a string becomes a text field. Entities that belong to
a mode are marked unavailable while another mode is active, so the dashboard only
shows what is actually adjustable right now.

The full state document is published to `glowlight/<device>/state` and attached
to the sync status sensor as attributes, which is the place to look when you want
to template on something that has no entity of its own.

### Colour

The light entity carries brightness only. Colour differs too much between modes —
one mode has an RGB value, another has hue and saturation — so it is exposed as
the individual settings of the active mode rather than guessed into one entity.
A template light in Home Assistant can combine them if you want a single control.

## Groups

By default a command from Home Assistant applies to the whole lamp group: change
the mode on one lamp and the others follow over ESP-NOW. That only happens while
the lamp is actually synchronized and allowed to publish. If it is detached, or
still joining, the command applies to that lamp alone instead of failing. The
`Follow group` and `Publish to group` switches decide which of the two it is.

This means one lamp is enough to control a whole group from Home Assistant. If
you want each lamp separately, switch `Publish to group` off on the lamps that
should keep to themselves.

## Topics

```
glowlight/<device>/status                     online | offline (last will)
glowlight/<device>/state                      full state document
glowlight/<device>/light/state | /set
glowlight/<device>/mode/state  | /set
glowlight/<device>/mode/<mode>/available
glowlight/<device>/option/<mode>/state | /set
glowlight/<device>/setting/<mode>/<key>/state | /set
glowlight/<device>/sync/follow/state  | /set
glowlight/<device>/sync/publish/state | /set
glowlight/<device>/sync/status
```

`<device>` is `glow-<node id>`. Discovery documents are retained under
`<discovery prefix>/<component>/<device>/<object>/config`.

## Security

- Broker credentials live in NVS once set through the portal, not in the image.
- The group key, the OTA password and the broker password are never published;
  a test asserts that no generated payload contains them.
- MQTT itself is unauthenticated towards the lamp: anyone who can publish to the
  broker can control the lamp. Restrict broker access accordingly — the lamp
  trusts the broker the same way it trusts the local network for OTA.
- The connection is plain MQTT. Use it on a network you control.

## Limitations

- Reconnecting to an unreachable broker briefly blocks the main loop, because the
  MQTT client connects synchronously. The retry interval backs off to a minute,
  so a broker that is down causes a short hitch at most once per minute.
- Discovery is published once per connection. If you delete entities in Home
  Assistant, restart the lamp to have them announced again.

## Testing

```bash
pip install paho-mqtt
make test-homeassistant BROKER=mqtt.local DEVICE=glow-1384610827 PORT=/dev/ttyACM0
```

Add `PEER_PORT=/dev/ttyACM1` to also verify that a command sent to one lamp
reaches the second one over ESP-NOW.

The mapping itself is covered by host tests in `test/native/tests_homeassistant.cpp`,
including a mode the MQTT layer has never heard of, which has to appear in
discovery with its options and settings anyway.
