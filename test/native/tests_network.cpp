// WiFi state machine and the channel it leaves the radio on. ESP-NOW rides on
// that channel, so getting it wrong splits a lamp group apart.

#include "NetworkService.h"
#include "ESPmDNS.h"
#include "WiFi.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "support.h"

using namespace glowtest;

namespace {

NetworkConfig enabledConfig() {
  NetworkConfig config;
  config.enabled = true;
  config.ssid = "testnet";
  config.password = "secret123";
  config.hostname = "glow-test";
  config.fallbackChannel = 6;
  return config;
}

NetworkConfig disabledConfig() {
  NetworkConfig config = enabledConfig();
  config.enabled = false;
  return config;
}

// Runs the service for a while without letting the connection succeed.
void advance(NetworkService& service, uint32_t milliseconds, uint32_t step = 250) {
  for (uint32_t elapsed = 0; elapsed < milliseconds; elapsed += step) {
    glow_shim::clockMillis += step;
    service.loop();
  }
}

bool advanceUntilNextAttempt(NetworkService& service, uint32_t timeoutMs) {
  int callsBefore = glow_shim::wifiBeginCalls;
  for (uint32_t elapsed = 0; elapsed <= timeoutMs; elapsed += 250) {
    glow_shim::clockMillis += 250;
    service.loop();
    if (glow_shim::wifiBeginCalls > callsBefore) return true;
  }
  return false;
}

void resetAll() {
  glow_shim::clockMillis = 0;
  glow_shim::serialLog.clear();
  glow_shim::resetNetwork();
  glow_shim::resetEspNow();
  glow_shim::seedRandom(12345);
}

}  // namespace

// Without credentials the radio must still be usable for ESP-NOW, parked on the
// configured fallback channel.
GLOW_TEST(disabled_wifi_parks_on_the_fallback_channel) {
  resetAll();
  NetworkService service;
  service.setup(disabledConfig());

  CHECK(service.state() == NetworkService::State::Inactive);
  CHECK(!service.isConnected());
  CHECK_EQ(service.channel(), static_cast<uint8_t>(6));
  CHECK_EQ(glow_shim::radioChannel, static_cast<uint8_t>(6));
  CHECK_EQ(glow_shim::wifiMode, WIFI_STA);
  CHECK_EQ(glow_shim::wifiBeginCalls, 0);
}

GLOW_TEST(empty_ssid_counts_as_disabled) {
  resetAll();
  NetworkConfig config = enabledConfig();
  config.ssid = "";

  NetworkService service;
  service.setup(config);
  CHECK(service.state() == NetworkService::State::Inactive);
  CHECK_EQ(glow_shim::wifiBeginCalls, 0);
}

// Modem sleep makes a station miss ESP-NOW frames between beacons.
GLOW_TEST(power_save_is_always_switched_off) {
  resetAll();
  NetworkService service;
  service.setup(disabledConfig());
  CHECK(!glow_shim::wifiSleep);

  resetAll();
  NetworkService connecting;
  connecting.setup(enabledConfig());
  CHECK(!glow_shim::wifiSleep);
}

GLOW_TEST(provisioning_uses_ap_sta_on_the_esp_now_fallback_channel) {
  resetAll();
  NetworkService service;
  CHECK(service.setupProvisioning(disabledConfig(), "glow-setup", "unique-pass"));

  CHECK(service.isProvisioning());
  CHECK(service.state() == NetworkService::State::Provisioning);
  CHECK_EQ(glow_shim::wifiMode, WIFI_AP_STA);
  CHECK(!glow_shim::wifiSleep);
  CHECK_EQ(glow_shim::wifiSoftApCalls, 1);
  CHECK_EQ(glow_shim::wifiSoftApChannel, 6);
  CHECK_EQ(glow_shim::radioChannel, static_cast<uint8_t>(6));
  CHECK_EQ(glow_shim::wifiBeginCalls, 0);

  uint32_t before = glow_shim::clockMillis;
  service.loop();
  CHECK_EQ(glow_shim::clockMillis, before);
}

GLOW_TEST(provisioning_rejects_weak_credentials_and_ap_failures) {
  resetAll();
  NetworkService weak;
  CHECK(!weak.setupProvisioning(disabledConfig(), "glow-setup", "short"));
  CHECK_EQ(glow_shim::wifiSoftApCalls, 0);

  resetAll();
  glow_shim::wifiSoftApShouldFail = true;
  NetworkService failed;
  CHECK(!failed.setupProvisioning(disabledConfig(), "glow-setup", "unique-pass"));
  CHECK(!failed.isProvisioning());
}

GLOW_TEST(an_invalid_fallback_channel_falls_back_to_one) {
  resetAll();
  NetworkConfig config = disabledConfig();
  config.fallbackChannel = 44;

  NetworkService service;
  service.setup(config);
  CHECK_EQ(service.channel(), static_cast<uint8_t>(1));
}

GLOW_TEST(successful_connection_adopts_the_access_point_channel) {
  resetAll();
  NetworkService service;
  service.setup(enabledConfig());
  CHECK(service.state() == NetworkService::State::Connecting);
  CHECK_EQ(glow_shim::wifiSsid, std::string("testnet"));
  CHECK_EQ(glow_shim::wifiHostname, std::string("glow-test"));

  // The access point runs on channel 11.
  glow_shim::radioChannel = 11;
  glow_shim::wifiStatus = WL_CONNECTED;
  service.loop();

  CHECK(service.isConnected());
  CHECK_EQ(service.channel(), static_cast<uint8_t>(11));
  CHECK_EQ(glow_shim::mdnsHostname, std::string("glow-test"));
}

// Every lamp that loses the network must end up on the same channel again,
// otherwise the group scatters across channels.
GLOW_TEST(losing_the_connection_parks_on_the_fallback_channel) {
  resetAll();
  NetworkService service;
  service.setup(enabledConfig());
  glow_shim::radioChannel = 11;
  glow_shim::wifiStatus = WL_CONNECTED;
  service.loop();
  CHECK(service.isConnected());

  glow_shim::wifiStatus = WL_DISCONNECTED;
  service.loop();

  CHECK(!service.isConnected());
  CHECK(service.state() == NetworkService::State::Retrying);
  CHECK_EQ(service.channel(), static_cast<uint8_t>(6));
  CHECK_EQ(glow_shim::radioChannel, static_cast<uint8_t>(6));
}

GLOW_TEST(a_failed_attempt_parks_and_retries) {
  resetAll();
  NetworkService service;
  service.setup(enabledConfig());
  CHECK_EQ(glow_shim::wifiBeginCalls, 1);

  // Nothing happens before the connect timeout expires.
  advance(service, NetworkService::CONNECT_TIMEOUT_MS - 500);
  CHECK(service.state() == NetworkService::State::Connecting);

  advance(service, 1000);
  CHECK(service.state() == NetworkService::State::Retrying);
  CHECK_EQ(service.channel(), static_cast<uint8_t>(6));

  // ...and after the backoff a new attempt starts.
  CHECK(advanceUntilNextAttempt(service, NetworkService::backoffFor(1) + 250));
  CHECK_EQ(glow_shim::wifiBeginCalls, 2);
}

GLOW_TEST(a_terminal_connection_error_parks_without_waiting_for_timeout) {
  resetAll();
  NetworkService service;
  service.setup(enabledConfig());

  glow_shim::wifiStatus = WL_CONNECT_FAILED;
  service.loop();

  CHECK(service.state() == NetworkService::State::Retrying);
  CHECK_EQ(glow_shim::radioChannel, static_cast<uint8_t>(6));
}

GLOW_TEST(backoff_grows_and_is_capped) {
  CHECK_EQ(NetworkService::backoffFor(0), NetworkService::BACKOFF_MIN_MS);
  CHECK_EQ(NetworkService::backoffFor(1), NetworkService::BACKOFF_MIN_MS);
  CHECK_EQ(NetworkService::backoffFor(2), NetworkService::BACKOFF_MIN_MS * 2);
  CHECK_EQ(NetworkService::backoffFor(3), NetworkService::BACKOFF_MIN_MS * 4);
  CHECK_EQ(NetworkService::backoffFor(200), NetworkService::BACKOFF_MAX_MS);
}

GLOW_TEST(a_successful_connection_resets_the_backoff) {
  resetAll();
  NetworkService service;
  service.setup(enabledConfig());

  // Two failed attempts. A lamp that has never connected needs a full scan
  // timeout on every attempt because it has no known AP channel yet.
  advance(service, NetworkService::CONNECT_TIMEOUT_MS + 250);
  CHECK(service.state() == NetworkService::State::Retrying);
  CHECK(advanceUntilNextAttempt(service, NetworkService::backoffFor(1) + 250));
  CHECK(service.state() == NetworkService::State::Connecting);
  advance(service, NetworkService::CONNECT_TIMEOUT_MS + 250);
  CHECK(service.state() == NetworkService::State::Retrying);
  CHECK(advanceUntilNextAttempt(service, NetworkService::backoffFor(2) + 250));
  CHECK(service.state() == NetworkService::State::Connecting);

  glow_shim::wifiStatus = WL_CONNECTED;
  glow_shim::radioChannel = 3;
  service.loop();
  CHECK(service.isConnected());

  // After a drop the next retry uses the shortest delay again.
  int beginsBefore = glow_shim::wifiBeginCalls;
  glow_shim::wifiStatus = WL_DISCONNECTED;
  service.loop();
  CHECK(advanceUntilNextAttempt(service, NetworkService::BACKOFF_MIN_MS + 250));
  CHECK_EQ(glow_shim::wifiBeginCalls, beginsBefore + 1);
}

GLOW_TEST(reconnect_uses_the_last_access_point_channel_and_a_short_timeout) {
  resetAll();
  NetworkService service;
  service.setup(enabledConfig());
  glow_shim::radioChannel = 11;
  glow_shim::wifiStatus = WL_CONNECTED;
  service.loop();

  glow_shim::wifiStatus = WL_DISCONNECTED;
  service.loop();
  CHECK(advanceUntilNextAttempt(service, NetworkService::BACKOFF_MIN_MS + 250));

  CHECK(service.state() == NetworkService::State::Connecting);
  CHECK_EQ(glow_shim::wifiBeginChannel, 11);

  advance(service, NetworkService::RECONNECT_TIMEOUT_MS + 250);
  CHECK(service.state() == NetworkService::State::Retrying);
  CHECK_EQ(glow_shim::radioChannel, static_cast<uint8_t>(6));
}

// ESP-NOW has to be told when the channel moves under it.
GLOW_TEST(channel_changes_are_reported_once_each) {
  resetAll();
  std::vector<uint8_t> reported;

  NetworkService service;
  service.onChannelChanged([&reported](uint8_t channel) { reported.push_back(channel); });
  service.setup(enabledConfig());

  glow_shim::radioChannel = 11;
  glow_shim::wifiStatus = WL_CONNECTED;
  service.loop();
  service.loop();  // no further report while nothing changes
  service.loop();

  glow_shim::wifiStatus = WL_DISCONNECTED;
  service.loop();

  // No report while the first association is still in flight: the radio hops
  // channels during association anyway, so there is nothing stable to announce.
  CHECK_EQ(reported.size(), static_cast<size_t>(2));
  CHECK_EQ(reported[0], static_cast<uint8_t>(11));  // adopted from the AP
  CHECK_EQ(reported[1], static_cast<uint8_t>(6));   // parked again after the loss
}

// An access point that switches channel must drag ESP-NOW along.
GLOW_TEST(a_roaming_access_point_moves_the_reported_channel) {
  resetAll();
  std::vector<uint8_t> reported;

  NetworkService service;
  service.onChannelChanged([&reported](uint8_t channel) { reported.push_back(channel); });
  service.setup(enabledConfig());
  glow_shim::radioChannel = 1;
  glow_shim::wifiStatus = WL_CONNECTED;
  service.loop();

  glow_shim::radioChannel = 13;
  service.loop();

  CHECK_EQ(service.channel(), static_cast<uint8_t>(13));
  CHECK_EQ(reported.back(), static_cast<uint8_t>(13));
}

GLOW_TEST(a_failed_channel_change_reports_the_actual_radio_channel) {
  resetAll();
  glow_shim::radioChannel = 11;
  glow_shim::channelSetResult = ESP_FAIL;
  std::vector<uint8_t> reported;

  NetworkService service;
  service.onChannelChanged([&reported](uint8_t channel) { reported.push_back(channel); });
  service.setup(disabledConfig());

  CHECK_EQ(service.channel(), static_cast<uint8_t>(11));
  CHECK_EQ(reported.size(), static_cast<size_t>(1));
  CHECK_EQ(reported[0], static_cast<uint8_t>(11));

  glow_shim::channelSetResult = ESP_OK;
  advance(service, NetworkService::FALLBACK_RETRY_MS);
  CHECK_EQ(service.channel(), static_cast<uint8_t>(6));
  CHECK_EQ(reported.size(), static_cast<size_t>(2));
  CHECK_EQ(reported[1], static_cast<uint8_t>(6));
}

GLOW_TEST(a_channel_read_error_is_not_published_as_a_change) {
  resetAll();
  glow_shim::channelGetResult = ESP_FAIL;
  std::vector<uint8_t> reported;

  NetworkService service;
  service.onChannelChanged([&reported](uint8_t channel) { reported.push_back(channel); });
  service.setup(disabledConfig());

  CHECK(reported.empty());
}

// The lamp animates LEDs in the same loop, so nothing here may block.
GLOW_TEST(the_loop_never_advances_the_clock) {
  resetAll();
  NetworkService service;
  service.setup(enabledConfig());

  uint32_t before = glow_shim::clockMillis;
  for (int i = 0; i < 50; ++i) service.loop();
  CHECK_EQ(glow_shim::clockMillis, before);
}

GLOW_TEST(mdns_failure_does_not_break_the_connection) {
  resetAll();
  glow_shim::mdnsShouldFail = true;

  NetworkService service;
  service.setup(enabledConfig());
  glow_shim::radioChannel = 4;
  glow_shim::wifiStatus = WL_CONNECTED;
  service.loop();

  CHECK(service.isConnected());
  CHECK_EQ(service.channel(), static_cast<uint8_t>(4));
}

GLOW_TEST(mdns_is_restarted_after_a_wifi_reconnect) {
  resetAll();
  NetworkService service;
  service.setup(enabledConfig());
  glow_shim::radioChannel = 4;
  glow_shim::wifiStatus = WL_CONNECTED;
  service.loop();
  CHECK_EQ(glow_shim::mdnsBeginCalls, 1);

  glow_shim::wifiStatus = WL_DISCONNECTED;
  service.loop();
  CHECK_EQ(glow_shim::mdnsEndCalls, 1);

  CHECK(advanceUntilNextAttempt(service, NetworkService::BACKOFF_MIN_MS + 250));
  glow_shim::radioChannel = 4;
  glow_shim::wifiStatus = WL_CONNECTED;
  service.loop();
  CHECK_EQ(glow_shim::mdnsBeginCalls, 2);
  CHECK_EQ(glow_shim::mdnsHostname, std::string("glow-test"));
}

GLOW_TEST(channel_changes_reannounce_the_esp_now_node) {
  resetAll();
  NetworkService network;
  network.setup(enabledConfig());

  CommunicationService communication;
  communication.setup();
  network.onChannelChanged([&communication](uint8_t channel) {
    communication.radioChannelChanged(channel);
  });

  CHECK_EQ(glow_shim::addPeerCalls, 1);
  CHECK_EQ(glow_shim::addedPeerChannel, static_cast<uint8_t>(0));
  communication.loop();
  glow_shim::sentFrames.clear();

  glow_shim::radioChannel = 11;
  glow_shim::wifiStatus = WL_CONNECTED;
  network.loop();
  communication.loop();

  CHECK_EQ(glow_shim::sentFrames.size(), static_cast<size_t>(1));
  CHECK_EQ(glow_shim::sentFrames[0].data[kOffsetType],
           static_cast<uint8_t>(Frame::HELLO));
  CHECK_EQ(glow_shim::sentFrames[0].channel, static_cast<uint8_t>(11));

  glow_shim::sentFrames.clear();
  network.loop();
  communication.loop();
  CHECK(glow_shim::sentFrames.empty());

  glow_shim::wifiStatus = WL_DISCONNECTED;
  network.loop();
  communication.loop();
  CHECK_EQ(glow_shim::sentFrames.size(), static_cast<size_t>(1));
  CHECK_EQ(glow_shim::sentFrames[0].data[kOffsetType],
           static_cast<uint8_t>(Frame::HELLO));
  CHECK_EQ(glow_shim::sentFrames[0].channel, static_cast<uint8_t>(6));
}
