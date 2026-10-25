#include <Arduino.h>
#include <Wire.h>

#include "Config.h"
#include "LdrArray.h"
#include "MotionController.h"
#include "NetworkManager.h"
#include "OccupancySensor.h"
#include "SystemTypes.h"
#include "TemperatureMonitor.h"
#include "ShadeOptimizer.h"

LdrArray ldrArray;
MotionController motion;
OccupancySensor occupancy;
TemperatureMonitor temperature;
NetworkManager network;
NodeStatus node;
ShadeOptimizer shadeOptimizer;

uint32_t lastTrackMs = 0;
uint32_t lastSerialMs = 0;

static void setCooling(bool enabled) {
  node.coolingOn = enabled;
  const bool level = Config::COOLING_ACTIVE_HIGH ? enabled : !enabled;
  digitalWrite(Config::PIN_COOLING, level ? HIGH : LOW);
}


static SystemState chooseState(RemoteMode remoteMode) {
  if (remoteMode == RemoteMode::FAULT) return SystemState::FAULT;
  if (remoteMode == RemoteMode::WIND) return SystemState::STOW_WIND;
  if (remoteMode == RemoteMode::RAIN) return SystemState::STOW_RAIN;

  // Occupancy takes priority over low-light handling.
  if (node.occupied) return SystemState::SHADE;

  if (node.light.average < Config::LDR_LOW_LIGHT_THRESHOLD) {
    return SystemState::LOW_LIGHT;
  }

  if (fabsf(node.light.azimuthError) <= Config::TRACKING_DEADBAND &&
      fabsf(node.light.elevationError) <= Config::TRACKING_DEADBAND) {
    return SystemState::ALIGNED;
  }

  return SystemState::TRACKING;
}


static void printTelemetry() {
  Serial.printf("state=%s occ=%d dist=%umm hot=%d cool=%d az=%.1f el=%.1f "
                "light=%.0f errAz=%.3f errEl=%.3f shadeTop=%.3f ldr=",
                stateName(node.state), node.occupied, node.distanceMm,
                node.heatWarning, node.coolingOn, node.azimuthDeg,
                node.elevationDeg, node.light.average,
                node.light.azimuthError, node.light.elevationError,
                node.light.topShadeDifference);
  for (uint8_t i = 0; i < Config::LDR_COUNT; ++i) {
    Serial.printf("%s%u", i ? "," : "", node.light.raw[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(Config::PIN_I2C_SDA, Config::PIN_I2C_SCL);
  pinMode(Config::PIN_COOLING, OUTPUT);
  setCooling(false);

  ldrArray.begin();
  motion.begin();
  occupancy.begin();
  temperature.begin();
  network.begin(&node);
  Serial.println("SOLACE booted. Keep the ultrasonic waiting zone empty for 4 seconds.");
}

void loop() {
  const uint32_t now = millis();
  occupancy.update(now);
  temperature.update();
  network.update(now);

  node.occupied = occupancy.occupied();
  node.distanceMm = occupancy.distanceMm();
  node.heatWarning = temperature.heatWarning();
  node.temperatureValid = temperature.validCelsius();
  node.temperatureC = temperature.temperatureC();

  // Heat warning is an independent flag rather than an exclusive state.
  // Safety stow/fault inhibits cooling; AUTO tracking does not.
  const RemoteMode remote = network.remoteMode();
  const bool safeForCooling = remote == RemoteMode::AUTO;
  setCooling(node.occupied && node.heatWarning && safeForCooling);

  if (now - lastTrackMs >= Config::TRACK_PERIOD_MS) {
    lastTrackMs = now;
    node.light = ldrArray.read();
    node.state = chooseState(remote);

    switch (node.state) {
      case SystemState::TRACKING:
        motion.track(node.light.azimuthError, node.light.elevationError);
        break;
      case SystemState::STOW_WIND:
        motion.moveToward(Config::WIND_STOW_AZ_DEG, Config::WIND_STOW_EL_DEG);
        break;
      case SystemState::STOW_RAIN:
        motion.moveToward(Config::RAIN_STOW_AZ_DEG, Config::RAIN_STOW_EL_DEG);
        break;

    case SystemState::SHADE: {
    const float testSunAzimuthDeg = 35.0f;
    const float testSunElevationDeg = 35.0f;
    const float pivotHeightCm = 15.0f;

    ShadeResult shade = shadeOptimizer.optimize(
        testSunAzimuthDeg,
        testSunElevationDeg,
        pivotHeightCm,
        0.0f,  // offsetUcm
        0.0f,  // offsetVcm
        0.0f   // offsetNcm
    );

    Serial.printf(
        "Shade valid=%d, az=%.1f, el=%.1f, coverage=%.2f, target=%d, power=%.3f\n",
        shade.valid,
        shade.azimuthDeg,
        shade.elevationDeg,
        shade.coverage,
        shade.coverageTargetMet,
        shade.powerProxy
    );

    // Only command the servos when the optimizer returns a valid result.
    if (shade.valid) {
        motion.moveToward(
            shade.azimuthDeg,
            shade.elevationDeg
        );
    }

    break;
}

      
      case SystemState::LOW_LIGHT:
      case SystemState::ALIGNED:
      case SystemState::FAULT:
        break; // Hold position and issue no further motion command.
    }
    node.azimuthDeg = motion.azimuth();
    node.elevationDeg = motion.elevation();
  }

  if (now - lastSerialMs >= 1000) {
    lastSerialMs = now;
    printTelemetry();
  }
}
