#pragma once

#include <Arduino.h>

#include "Config.h"

class OccupancySensor {
 public:
  void begin();
  void update(uint32_t nowMs);
  bool occupied() const { return occupied_; }
  uint16_t distanceMm() const { return distanceMm_; }
  bool baselineReady() const { return baselineReady_; }

 private:
  void acceptDistance(uint16_t mm, uint32_t nowMs);
  void updateA02(uint32_t nowMs);
  void updateHcsr04(uint32_t nowMs);
  uint8_t frame_[4]{};
  uint8_t frameIndex_ = 0;
  uint16_t distanceMm_ = 0;
  uint32_t baselineTotal_ = 0;
  uint16_t baselineSamples_ = 0;
  uint16_t baselineMm_ = 0;
  bool baselineReady_ = false;
  bool occupied_ = false;
  uint8_t positiveCount_ = 0;
  uint8_t negativeCount_ = 0;
  uint32_t lastOccupiedMs_ = 0;
  uint32_t lastPingMs_ = 0;
  uint32_t lastValidDistanceMs_ = 0;
  uint16_t recentDistances_[Config::OCC_MEDIAN_SAMPLES]{};
  uint8_t recentDistanceCount_ = 0;
  uint8_t recentDistanceIndex_ = 0;
};
