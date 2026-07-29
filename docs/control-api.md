# GlowLight Control API

The controller exposes a transport-neutral JSON API. The integration console is
the first adapter; a later HTTP or captive-portal service should pass requests to
the same `Controller::executeControl()` method instead of implementing its own
mode logic.

## Discovery

`Controller::capabilities()` returns the modes included in the current firmware
build. Mode IDs are stable protocol identifiers; display names and implementation
versions may change without changing the ID.

```json
{
  "schema": "glow.capabilities",
  "schemaVersion": 1,
  "controlApi": "glow.control/1",
  "features": {
    "infrastructureWifi": true,
    "groupControl": true,
    "syncPolicy": {
      "supported": true,
      "controls": ["follow", "publish"],
      "runtimeMutable": true,
      "defaults": {"follow": true, "publish": true}
    },
    "captivePortal": false,
    "ota": false
  },
  "modes": [
    {
      "id": "rainbow",
      "name": "Rainbow",
      "implementationVersion": "1.0.0",
      "stateSchemaVersion": 1,
      "settings": {
        "speed": {
          "type": "integer",
          "minimum": 1,
          "maximum": 20,
          "default": 4,
          "writable": true
        }
      },
      "options": [],
      "commands": {}
    }
  ]
}
```

Runtime state is separate and available through `Controller::state()`:

```json
{
  "schema": "glow.state",
  "schemaVersion": 1,
  "sync": {
    "follow": true,
    "publish": true,
    "status": "synchronized",
    "localDirty": false,
    "transportAvailable": true,
    "revision": 12,
    "origin": 305419896
  },
  "mode": {
    "id": "rainbow",
    "title": "Rainbow",
    "version": "1.0.0",
    "schemaVersion": 1,
    "registry": {}
  }
}
```

Capability documents are local management data and are never sent over the
512-byte ESP-NOW application payload.

## Requests

Every request uses `api: "glow.control/1"`. `requestId` is optional and copied to
the response. Mutating operations accept `scope: "local"` or `scope: "group"`;
the default is local.

Supported operations:

- `capabilities.get`
- `state.get`
- `sync.configure`
- `mode.select`
- `mode.option.set`
- `mode.setting.set`
- `mode.command`

### Sync policy

Synchronization has two independent local controls:

- `follow` applies incoming group state, commands, wipe gestures and live level
  updates. Discovery, authentication, heartbeats and sync requests remain active.
- `publish` allows local state, commands, wipe gestures and live level updates to
  be sent to the group. State snapshots requested by a joining peer remain
  available so that disabling application publishing does not break convergence.

Change both controls locally:

```json
{
  "api": "glow.control/1",
  "operation": "sync.configure",
  "scope": "local",
  "sync": {"follow": false, "publish": false}
}
```

The runtime status is one of `unavailable`, `detached`, `joining`, or
`synchronized`. Enabling `follow` starts a rejoin and temporarily blocks group
publishing until a valid state snapshot has been accepted. Local changes made
while detached are marked with `localDirty`; a valid group snapshot wins when
the lamp rejoins. Group mutations fail with `SYNC_PUBLISH_DISABLED` while
`publish` is disabled or the lamp is joining.

A group command is rejected with `LOCAL_STATE_DIVERGED` while `localDirty` is
set. Publish the complete local state first, for example with a group-scoped
`mode.select` for the active mode, then retry the command. This prevents a
command from hiding an older local divergence behind a newer command version.

Group state uses a Lamport-style `(revision, origin)` version. Higher revisions
win; equal revisions are resolved by the numerically higher origin node ID. The
pair is diagnostic runtime data and clients must not modify it.

Select a mode on the complete lamp group:

```json
{
  "api": "glow.control/1",
  "requestId": "42",
  "operation": "mode.select",
  "scope": "group",
  "target": {"mode": "sunset"}
}
```

Set a validated registry setting:

```json
{
  "api": "glow.control/1",
  "operation": "mode.setting.set",
  "scope": "local",
  "target": {"mode": "rainbow"},
  "setting": "speed",
  "value": 8
}
```

Invoke a declared mode command:

```json
{
  "api": "glow.control/1",
  "operation": "mode.command",
  "scope": "group",
  "target": {"mode": "sunset"},
  "command": "start",
  "arguments": {"duration": 1}
}
```

Successful mutations return `ok`. Errors return a stable code and explanatory
message without changing state.

```json
{
  "api": "glow.control/1",
  "requestId": "42",
  "ok": false,
  "error": {
    "code": "INVALID_SETTING",
    "message": "Setting is unknown, read-only, or invalid"
  }
}
```

## Integration Console

The `esp32c3-integration` profile exposes:

```text
CAPABILITIES
STATE
CONTROL {"api":"glow.control/1",...}
```

Responses use `[TEST] CAPABILITIES|`, `[TEST] STATE|`, or `[TEST] CONTROL|`
followed by one JSON document.

## Security Boundary

The API performs schema and value validation but does not authenticate its
caller. ESP-NOW requests inherit the encrypted group trust model. A future HTTP
adapter must authenticate clients before calling `executeControl()`; WLAN access
alone is not authorization.
