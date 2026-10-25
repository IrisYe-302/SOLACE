#pragma once

#include <Arduino.h>

class TemperatureMonitor {
 public:
  void begin();
  void update();
  bool heatWarning() const { return heatWarning_; }
  bool validCelsius() const { return false; }
  float temperatureC() const { return NAN; }

 private:
  bool heatWarning_ = false;
};
