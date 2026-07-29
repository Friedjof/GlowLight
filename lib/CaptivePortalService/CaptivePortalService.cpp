#include "CaptivePortalService.h"

#include <WiFi.h>
#include <esp_system.h>

namespace {
const char PORTAL_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GlowLight setup</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#101014;color:#f6f3ea}body{margin:0;padding:2rem 1rem}
main{max-width:38rem;margin:auto}h1{font-size:2rem;margin:.25rem 0 1.5rem;color:#ffd27a}fieldset{border:1px solid #383842;border-radius:.7rem;margin:1rem 0;padding:1rem}label{display:block;margin:.75rem 0}input{box-sizing:border-box;width:100%;margin-top:.3rem;padding:.7rem;border:1px solid #555562;border-radius:.4rem;background:#1c1c23;color:inherit}input[type=checkbox]{width:auto;margin-right:.5rem}button{padding:.8rem 1rem;border:0;border-radius:.4rem;background:#ffd27a;color:#18130a;font-weight:700}#status{min-height:1.5rem;margin-top:1rem}.hint{color:#aaa7b0;font-size:.9rem}
</style></head><body><main><h1>GlowLight setup</h1><p>Configure this lamp. Existing passwords and group keys stay unchanged when their fields are empty.</p>
<form id="config"><fieldset><legend>WiFi</legend><label><input id="we" type="checkbox">Enable infrastructure WiFi</label><label>SSID<input id="ssid" maxlength="32"></label><label>Password<input id="wp" type="password" maxlength="64"></label><label>Hostname<input id="host" maxlength="63"></label></fieldset>
<fieldset><legend>Group</legend><label><input id="ge" type="checkbox">Enable secure group communication</label><label>256-bit group key<input id="key" type="password" maxlength="64" placeholder="64 hexadecimal characters"></label><label><input id="follow" type="checkbox">Follow group changes</label><label><input id="publish" type="checkbox">Publish local changes</label><label>Fallback channel<input id="channel" type="number" min="1" max="13"></label></fieldset>
<fieldset><legend>Firmware updates</legend><label><input id="oe" type="checkbox">Enable OTA over infrastructure WiFi</label><label>OTA password<input id="op" type="password" minlength="12" maxlength="63" placeholder="Leave empty to keep the current password"></label></fieldset>
<fieldset><legend>Home Assistant</legend><label><input id="me" type="checkbox">Publish to an MQTT broker</label><label>Broker host<input id="mh" maxlength="253" placeholder="mqtt.local"></label><label>Broker port<input id="mp" type="number" min="1" max="65535"></label><label>Username<input id="mu" maxlength="64"></label><label>Password<input id="mw" type="password" maxlength="64" placeholder="Leave empty to keep the current password"></label><label>Discovery prefix<input id="md" maxlength="48"></label></fieldset>
<button type="submit">Save and restart</button><p id="status"></p></form><p class="hint">The setup access point closes after restart.</p></main><script>
let token='';const s=document.querySelector('#status');fetch('/api/config').then(r=>r.json()).then(c=>{token=c.sessionToken;we.checked=c.wifi.enabled;ssid.value=c.wifi.ssid;host.value=c.wifi.hostname;ge.checked=c.group.enabled;follow.checked=c.group.follow;publish.checked=c.group.publish;channel.value=c.radio.fallbackChannel;oe.checked=c.ota.enabled;me.checked=c.mqtt.enabled;mh.value=c.mqtt.host;mp.value=c.mqtt.port;mu.value=c.mqtt.user;md.value=c.mqtt.discoveryPrefix});
config.onsubmit=async e=>{e.preventDefault();s.textContent='Saving...';const body={api:'glow.config/1',wifi:{enabled:we.checked,ssid:ssid.value,password:wp.value,hostname:host.value},radio:{fallbackChannel:Number(channel.value)},group:{enabled:ge.checked,key:key.value,follow:follow.checked,publish:publish.checked},ota:{enabled:oe.checked,password:op.value},mqtt:{enabled:me.checked,host:mh.value,port:Number(mp.value),user:mu.value,password:mw.value,discoveryPrefix:md.value}};const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json','X-Glow-Token':token},body:JSON.stringify(body)});const j=await r.json();s.textContent=j.ok?'Saved. Restarting...':j.error.message};
</script></body></html>)HTML";

String makeToken() {
  char token[33];
  for (uint8_t i = 0; i < 4; ++i) {
    snprintf(token + i * 8, sizeof(token) - i * 8, "%08lx",
             static_cast<unsigned long>(esp_random()));
  }
  token[32] = '\0';
  return String(token);
}
}  // namespace

bool CaptivePortalService::setup(ConfigStore& configStore,
                                 NetworkService& networkService,
                                 const DeviceConfig& configuration) {
  if (!networkService.isProvisioning()) return false;
  this->store = &configStore;
  this->network = &networkService;
  this->current = configuration;
  this->sessionToken = makeToken();

  const char* requestedHeaders[] = {"X-Glow-Token"};
  this->server.collectHeaders(requestedHeaders, 1);
  this->server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
  this->server.on("/api/config", HTTP_GET,
                  [this]() { this->handleGetConfig(); });
  this->server.on("/api/config", HTTP_POST,
                  [this]() { this->handleSaveConfig(); });
  this->server.on("/api/factory-reset", HTTP_POST,
                  [this]() { this->handleFactoryReset(); });
  this->server.on("/generate_204", HTTP_ANY,
                  [this]() { this->server.sendHeader("Location", "/", true); this->server.send(302); });
  this->server.on("/hotspot-detect.html", HTTP_ANY,
                  [this]() { this->server.sendHeader("Location", "/", true); this->server.send(302); });
  this->server.onNotFound([this]() {
    this->server.sendHeader("Location", "/", true);
    this->server.send(302);
  });

  this->dns.start(53, "*", WiFi.softAPIP());
  this->server.begin();
  this->started = true;
  this->startedAt = millis();
  Serial.println("[INFO] Captive portal started");
  return true;
}

void CaptivePortalService::loop() {
  if (!this->started) return;
  this->dns.processNextRequest();
  this->server.handleClient();
  if (!this->restartPending && millis() - this->startedAt >= PORTAL_TIMEOUT_MS) {
    Serial.println("[INFO] Captive portal timed out, restarting");
    this->scheduleRestart();
  }
  if (this->restartPending && millis() - this->restartAt >= RESTART_DELAY_MS) {
    ESP.restart();
  }
}

bool CaptivePortalService::authorized() {
  return this->server.hasHeader("X-Glow-Token") &&
         this->server.header("X-Glow-Token") == this->sessionToken;
}

void CaptivePortalService::addNoStoreHeaders() {
  this->server.sendHeader("Cache-Control", "no-store");
  this->server.sendHeader("X-Content-Type-Options", "nosniff");
}

void CaptivePortalService::sendJson(int status, const JsonDocument& document) {
  String response;
  serializeJson(document, response);
  this->addNoStoreHeaders();
  this->server.send(status, "application/json", response);
}

void CaptivePortalService::sendError(int status, const char* code,
                                     const String& message) {
  JsonDocument response;
  response["api"] = "glow.config/1";
  response["ok"] = false;
  response["error"]["code"] = code;
  response["error"]["message"] = message;
  this->sendJson(status, response);
}

void CaptivePortalService::handleRoot() {
  this->addNoStoreHeaders();
  this->server.send_P(200, "text/html", PORTAL_HTML);
}

void CaptivePortalService::handleGetConfig() {
  JsonDocument response = this->current.redacted();
  response["ok"] = true;
  response["sessionToken"] = this->sessionToken;
  this->sendJson(200, response);
}

void CaptivePortalService::handleSaveConfig() {
  if (!this->authorized()) {
    this->sendError(403, "INVALID_SESSION", "A valid portal session token is required");
    return;
  }
  if (!this->server.hasArg("plain") ||
      this->server.arg("plain").length() > MAX_REQUEST_SIZE) {
    this->sendError(413, "INVALID_SIZE", "Configuration request is missing or too large");
    return;
  }

  JsonDocument request;
  if (deserializeJson(request, this->server.arg("plain")) ||
      request["api"].as<String>() != "glow.config/1") {
    this->sendError(400, "INVALID_REQUEST", "Expected a glow.config/1 JSON request");
    return;
  }
  DeviceConfig candidate;
  String error;
  if (!DeviceConfig::fromJson(request.as<JsonObjectConst>(), this->current,
                              &candidate, &error)) {
    this->sendError(422, "INVALID_CONFIG", error);
    return;
  }
  if (this->store == nullptr || !this->store->save(candidate)) {
    this->sendError(500, "STORE_FAILED", "Configuration could not be persisted");
    return;
  }

  this->current = candidate;
  JsonDocument response;
  response["api"] = "glow.config/1";
  response["ok"] = true;
  response["restart"] = true;
  this->sendJson(200, response);
  this->scheduleRestart();
}

void CaptivePortalService::handleFactoryReset() {
  if (!this->authorized()) {
    this->sendError(403, "INVALID_SESSION", "A valid portal session token is required");
    return;
  }
  if (this->store == nullptr || !this->store->clear()) {
    this->sendError(500, "STORE_FAILED", "Stored configuration could not be cleared");
    return;
  }
  JsonDocument response;
  response["api"] = "glow.config/1";
  response["ok"] = true;
  response["restart"] = true;
  this->sendJson(200, response);
  this->scheduleRestart();
}

void CaptivePortalService::scheduleRestart() {
  this->restartPending = true;
  this->restartAt = millis();
}
