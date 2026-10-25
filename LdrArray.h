#pragma once

#include "Config.h"
#include "SystemTypes.h"

class LdrArray {
 public:
  void begin();
  LdrReadings read();

 private:
  uint16_t readPin(uint8_t pin);
  float filtered_[Config::LDR_COUNT]{};
  bool filterInitialized_ = false;
};
