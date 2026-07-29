# GlowRegistry

`GlowRegistry` stores typed runtime state for one mode. It does not persist data;
NVS persistence belongs to the later configuration phase.

## Types

- `INT`: unsigned integer with inclusive minimum and maximum
- `STRING`: Arduino string
- `BOOL`: boolean
- `COLOR`: six hexadecimal RGB characters such as `FF8014`, without `#`

Settings are declared during mode setup:

```cpp
registry.init("speed", RegistryType::INT, 4, 1, 20);
registry.init("enabled", RegistryType::BOOL, true);
registry.init("color", RegistryType::COLOR, CRGB(255, 128, 20));
registry.setWritable("enabled", false);
```

## Capabilities

`describe()` returns metadata used by the capability document:

```json
{
  "speed": {
    "type": "integer",
    "minimum": 1,
    "maximum": 20,
    "default": 4,
    "writable": true
  }
}
```

`setValue()` is the generic write path. It rejects unknown keys, read-only
settings, wrong JSON types, out-of-range integers and malformed colors.

## State Transfer

`serialize()` returns the current registry values plus display metadata.
`AbstractMode` adds the stable mode ID and state schema version.

`deserialize()` validates the complete incoming registry before applying any
value. Missing, additional or invalid fields reject the whole update, so a mode
cannot be left in a partially updated state. Stable mode ID and schema validation
are performed by `AbstractMode`; title and implementation version may change
without changing the state protocol.
