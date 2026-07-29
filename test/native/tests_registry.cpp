#include "GlowRegistry.h"
#include "support.h"

using namespace glowtest;

namespace {

GlowRegistry configuredRegistry() {
  GlowRegistry registry;
  registry.setTitle("Test Mode");
  registry.setVersion("1.0.0");
  registry.init("speed", RegistryType::INT, 2, 1, 4);
  registry.init("enabled", RegistryType::BOOL, true);
  registry.init("color", RegistryType::COLOR, CRGB(255, 128, 20));
  return registry;
}

}  // namespace

GLOW_TEST(registry_describes_types_ranges_defaults_and_access) {
  GlowRegistry registry = configuredRegistry();
  registry.setWritable("enabled", false);
  JsonDocument description = registry.describe();

  CHECK(description["speed"]["type"].as<String>() == "integer");
  CHECK_EQ(description["speed"]["minimum"].as<int>(), 1);
  CHECK_EQ(description["speed"]["maximum"].as<int>(), 4);
  CHECK_EQ(description["speed"]["default"].as<int>(), 2);
  CHECK(description["speed"]["writable"].as<bool>());
  CHECK(!description["enabled"]["writable"].as<bool>());
  CHECK(description["color"]["format"].as<String>() == "rgb-hex");
}

GLOW_TEST(registry_generic_writes_are_strict) {
  GlowRegistry registry = configuredRegistry();
  JsonDocument values;
  values["valid"] = 4;
  values["tooHigh"] = 5;
  values["wrongType"] = "4";
  values["color"] = "00ff7F";
  values["badColor"] = "not-a-color";

  CHECK(registry.setValue("speed", values["valid"]));
  CHECK_EQ(registry.getInt("speed"), static_cast<uint16_t>(4));
  CHECK(!registry.setValue("speed", values["tooHigh"]));
  CHECK(!registry.setValue("speed", values["wrongType"]));
  CHECK(registry.setValue("color", values["color"]));
  CHECK(registry.getColor("color") == CRGB(0, 255, 127));
  CHECK(!registry.setValue("color", values["badColor"]));
}

GLOW_TEST(registry_rejects_writes_to_read_only_settings) {
  GlowRegistry registry = configuredRegistry();
  registry.setWritable("enabled", false);
  JsonDocument value;
  value.set(false);

  CHECK(!registry.setValue("enabled", value.as<JsonVariantConst>()));
  CHECK(registry.getBool("enabled"));
}

GLOW_TEST(registry_deserialization_is_atomic) {
  GlowRegistry registry = configuredRegistry();
  JsonDocument invalid;
  invalid["title"] = "Test Mode";
  invalid["version"] = "1.0.0";
  invalid["registry"]["speed"] = 4;
  invalid["registry"]["enabled"] = "false";
  invalid["registry"]["color"] = "FF8014";

  CHECK(!registry.deserialize(invalid));
  CHECK_EQ(registry.getInt("speed"), static_cast<uint16_t>(2));
  CHECK(registry.getBool("enabled"));
}

GLOW_TEST(registry_requires_an_exact_state_shape) {
  GlowRegistry registry = configuredRegistry();
  JsonDocument missing;
  missing["title"] = "Test Mode";
  missing["version"] = "1.0.0";
  missing["registry"]["speed"] = 3;
  missing["registry"]["enabled"] = false;
  CHECK(!registry.deserialize(missing));

  JsonDocument complete;
  complete["title"] = "Test Mode";
  complete["version"] = "1.0.0";
  complete["registry"]["speed"] = 3;
  complete["registry"]["enabled"] = false;
  complete["registry"]["color"] = "010203";
  CHECK(registry.deserialize(complete));
  CHECK_EQ(registry.getInt("speed"), static_cast<uint16_t>(3));
  CHECK(!registry.getBool("enabled"));
  CHECK(registry.getColor("color") == CRGB(1, 2, 3));
}
