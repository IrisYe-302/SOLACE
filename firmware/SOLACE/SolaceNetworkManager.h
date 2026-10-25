#pragma once

#include <WebServer.h>

#include "SystemTypes.h"

class SolaceNetworkManager {
 public:
  void begin(NodeStatus *status);
  void update(uint32_t nowMs);
  RemoteMode remoteMode() const { return remoteMode_; }

 private:
  String statusJson() const;
  void handleStatus();
  void handleCommand();
  void handleWeather();
  void postTelemetry();
  void startWifi();

  WebServer server_{80};
  NodeStatus *status_ = nullptr;
  RemoteMode remoteMode_ = RemoteMode::AUTO;
  uint32_t lastWifiAttemptMs_ = 0;
  uint32_t lastTelemetryMs_ = 0;
  bool wifiWasConnected_ = false;
  int lastTelemetryHttpCode_ = 0;
};
