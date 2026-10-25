#pragma once

#include <Arduino.h>

#include "Config.h"

enum class SystemState : uint8_t {
  LOW_LIGHT,
  TRACKING,
  ALIGNED,
  SHADE,
  STOW_WIND,
  STOW_RAIN,
  FAULT
};

enum class RemoteMode : uint8_t { AUTO, WIND, RAIN, FAULT };

struct LdrReadings {
  uint16_t raw[Config::LDR_COUNT]{};
  float corrected[Config::LDR_COUNT]{};
  float top = 0;
  float bottom = 0;
  float left = 0;
  float right = 0;
  float average = 0;
  float azimuthError = 0;
  float elevationError = 0;
  float topShadeDifference = 0;
};

struct NodeStatus {
  SystemState state = SystemState::LOW_LIGHT;
  LdrReadings light;
  float azimuthDeg = 90;
  float elevationDeg = 90;
  bool shadePlanValid = false;
  bool shadeCoverageTargetMet = false;
  float shadeTargetAzimuthDeg = 90;
  float shadeTargetElevationDeg = 90;
  float shadeCoverage = 0;
  float shadePowerProxy = 0;
  bool occupied = false;
  uint16_t distanceMm = 0;
  bool heatWarning = false;
  bool coolingOn = false;
  bool temperatureValid = false;
  float temperatureC = NAN;
  float windMps = 0;
  bool raining = false;
};

const char *stateName(SystemState state);
