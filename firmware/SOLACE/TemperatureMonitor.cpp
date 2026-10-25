#include "TemperatureMonitor.h"

#include "Config.h"

void TemperatureMonitor::begin() {
  pinMode(Config::PIN_TEMP_DIGITAL, INPUT);
}

void TemperatureMonitor::update() {
  const bool level = digitalRead(Config::PIN_TEMP_DIGITAL);
  heatWarning_ = Config::TEMP_THRESHOLD_ACTIVE_LOW ? !level : level;
}
