#include "SolaceNetworkManager.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "Config.h"

void SolaceNetworkManager::begin(NodeStatus *status) {
  status_ = status;
  WiFi.mode(WIFI_STA);
  WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

  server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/command", HTTP_GET, [this]() { handleCommand(); });
  server_.on("/weather", HTTP_GET, [this]() { handleWeather(); });
  server_.onNotFound([this]() {
    server_.send(404, "application/json",
                 "{\"error\":\"Use /status, /command?mode=auto|wind|rain|fault, "
                 "or /weather?wind=12.0&rain=1\"}");
  });
  server_.begin();
}

void SolaceNetworkManager::update(uint32_t nowMs) {
  server_.handleClient();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wifiWasConnected_) {
    Serial.print("Wi-Fi connected; SOLACE IP: ");
    Serial.println(WiFi.localIP());
  }
  wifiWasConnected_ = connected;

  if (!connected && nowMs - lastWifiAttemptMs_ >= 15000) {
    lastWifiAttemptMs_ = nowMs;
    WiFi.disconnect();
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
  }
  if (nowMs - lastTelemetryMs_ >= Config::TELEMETRY_PERIOD_MS) {
    lastTelemetryMs_ = nowMs;
    postTelemetry();
  }
}

String SolaceNetworkManager::statusJson() const {
  String j;
  j.reserve(700);
  j += "{\"nodeId\":\"" + String(Config::NODE_ID) + "\"";
  j += ",\"uptimeMs\":" + String(millis());
  j += ",\"state\":\"" + String(stateName(status_->state)) + "\"";
  j += ",\"x\":" + String(Config::NODE_MAP_X, 4);
  j += ",\"y\":" + String(Config::NODE_MAP_Y, 4);
  j += ",\"latitude\":" + String(Config::NODE_LATITUDE, 6);
  j += ",\"longitude\":" + String(Config::NODE_LONGITUDE, 6);
  j += ",\"temperatureC\":";
  j += status_->temperatureValid ? String(status_->temperatureC, 2) : "null";
  j += ",\"heatWarning\":" + String(status_->heatWarning ? "true" : "false");
  j += ",\"occupied\":" + String(status_->occupied ? "true" : "false");
  j += ",\"coolingOn\":" + String(status_->coolingOn ? "true" : "false");
  j += ",\"distanceMm\":" + String(status_->distanceMm);
  j += ",\"windMps\":" + String(status_->windMps, 2);
  j += ",\"raining\":" + String(status_->raining ? "true" : "false");
  j += ",\"azimuthDeg\":" + String(status_->azimuthDeg, 1);
  j += ",\"elevationDeg\":" + String(status_->elevationDeg, 1);
  j += ",\"lightAverage\":" + String(status_->light.average, 1);
  j += ",\"azimuthError\":" + String(status_->light.azimuthError, 4);
  j += ",\"elevationError\":" + String(status_->light.elevationError, 4);
  j += ",\"ldr\":[";
  for (uint8_t i = 0; i < Config::LDR_COUNT; ++i) {
    if (i) j += ',';
    j += String(status_->light.corrected[i], 1);
  }
  j += "]}";
  return j;
}

void SolaceNetworkManager::handleStatus() {
  server_.send(200, "application/json", statusJson());
}

void SolaceNetworkManager::handleCommand() {
  const String mode = server_.arg("mode");
  if (mode == "auto") remoteMode_ = RemoteMode::AUTO;
  else if (mode == "wind") remoteMode_ = RemoteMode::WIND;
  else if (mode == "rain") remoteMode_ = RemoteMode::RAIN;
  else if (mode == "fault") remoteMode_ = RemoteMode::FAULT;
  else {
    server_.send(400, "application/json", "{\"error\":\"invalid mode\"}");
    return;
  }
  server_.send(200, "application/json", statusJson());
}

void SolaceNetworkManager::handleWeather() {
  if (server_.hasArg("wind")) status_->windMps = server_.arg("wind").toFloat();
  if (server_.hasArg("rain")) status_->raining = server_.arg("rain").toInt() != 0;

  if (status_->raining) remoteMode_ = RemoteMode::RAIN;
  else if (status_->windMps >= Config::WIND_STOW_THRESHOLD_MPS) remoteMode_ = RemoteMode::WIND;
  else remoteMode_ = RemoteMode::AUTO;
  server_.send(200, "application/json", statusJson());
}

void SolaceNetworkManager::postTelemetry() {
  if (WiFi.status() != WL_CONNECTED || strlen(Config::TELEMETRY_URL) == 0) return;
  HTTPClient http;
  http.setTimeout(1500);
  if (!http.begin(Config::TELEMETRY_URL)) return;
  http.addHeader("Content-Type", "application/json");
  http.POST(statusJson());
  http.end();
}
