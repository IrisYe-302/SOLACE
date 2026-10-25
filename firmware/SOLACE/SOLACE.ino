#include <Arduino.h>
#include <Wire.h>

#include "Config.h"
#include "LdrArray.h"
#include "MotionController.h"
#include "SolaceNetworkManager.h"
#include "OccupancySensor.h"
#include "SystemTypes.h"
#include "TemperatureMonitor.h"
#include "ShadeOptimizer.h"

LdrArray ldrArray;
MotionController motion;
OccupancySensor occupancy;
TemperatureMonitor temperature;
SolaceNetworkManager network;
NodeStatus node;
ShadeOptimizer shadeOptimizer;
ShadeResult shadePlan;

uint32_t lastTrackMs = 0;
uint32_t lastSerialMs = 0;
SystemState previousState = SystemState::LOW_LIGHT;
float solarAzimuthEstimateDeg = Config::AZ_RESET_DEG;
float solarElevationEstimateDeg = Config::EL_RESET_DEG;

static void setCooling(bool enabled) {
  node.coolingOn = enabled;
  const bool level = Config::COOLING_ACTIVE_HIGH ? enabled : !enabled;
  digitalWrite(Config::PIN_COOLING, level ? HIGH : LOW);
}

static void computeShadePlan() {
  // Immediately before occupancy, the LDR controller has aimed the panel normal
  // toward the brightest direction. Use that pose as the sun estimate; do not
  // recompute it after the panel has moved away to make shade.
  const ShadeRequest request{
      solarAzimuthEstimateDeg,
      constrain(solarElevationEstimateDeg, 1.0f, 90.0f),
      Config::SHADE_PIVOT_HEIGHT_CM,
      Config::SHADE_PANEL_OFFSET_U_CM,
      Config::SHADE_PANEL_OFFSET_V_CM,
      Config::SHADE_PANEL_OFFSET_N_CM,
      Config::SHADE_TARGET_CENTER_X_CM,
      Config::SHADE_TARGET_CENTER_Y_CM,
      Config::SHADE_SEARCH_AZ_MIN_DEG,
      Config::SHADE_SEARCH_AZ_MAX_DEG,
      Config::SHADE_SEARCH_EL_MIN_DEG,
      Config::SHADE_SEARCH_EL_MAX_DEG,
      Config::SHADE_SEARCH_STEP_DEG};

  shadePlan = shadeOptimizer.optimize(request);
  node.shadePlanValid = shadePlan.valid;
  node.shadeCoverageTargetMet = shadePlan.coverageTargetMet;
  node.shadeTargetAzimuthDeg =
      shadePlan.valid ? shadePlan.azimuthDeg : Config::SHADE_AZ_DEG;
  node.shadeTargetElevationDeg =
      shadePlan.valid ? shadePlan.elevationDeg : Config::SHADE_EL_DEG;
  node.shadeCoverage = shadePlan.coverage;
  node.shadePowerProxy = shadePlan.powerProxy;

  Serial.printf(
      "SHADE_PLAN valid=%d sun=%.1f/%.1f target=%.1f/%.1f "
      "coverage=%.1f%% targetMet=%d power=%.3f\n",
      shadePlan.valid, request.sunAzimuthDeg, request.sunElevationDeg,
      node.shadeTargetAzimuthDeg, node.shadeTargetElevationDeg,
      shadePlan.coverage * 100.0f, shadePlan.coverageTargetMet,
      shadePlan.powerProxy);
}


static SystemState chooseState(RemoteMode remoteMode) {
  if (remoteMode == RemoteMode::FAULT) return SystemState::FAULT;
  if (remoteMode == RemoteMode::WIND) return SystemState::STOW_WIND;
  if (remoteMode == RemoteMode::RAIN) return SystemState::STOW_RAIN;

  // Occupancy takes priority over low-light handling.
  if (node.occupied) return SystemState::SHADE;

  if (!Config::DEMO_FORCE_LIGHT_TRACKING &&
      node.light.average < Config::LDR_LOW_LIGHT_THRESHOLD) {
    return SystemState::LOW_LIGHT;
  }

  if (fabsf(node.light.azimuthError) <= Config::TRACKING_DEADBAND &&
      fabsf(node.light.elevationError) <= Config::TRACKING_DEADBAND) {
    return SystemState::ALIGNED;
  }

  return SystemState::TRACKING;
}


static void printTelemetry() {
  static const char *const ldrNames[Config::LDR_COUNT] = {
      "T1", "T2", "B1", "B2", "L1", "R1"};
  Serial.printf("state=%s occ=%d dist=%umm hot=%d cool=%d az=%.1f el=%.1f "
                "light=%.0f errAz=%.3f errEl=%.3f shadeTop=%.3f ldr=",
                stateName(node.state), node.occupied, node.distanceMm,
                node.heatWarning, node.coolingOn, node.azimuthDeg,
                node.elevationDeg, node.light.average,
                node.light.azimuthError, node.light.elevationError,
                node.light.topShadeDifference);
  for (uint8_t i = 0; i < Config::LDR_COUNT; ++i) {
    Serial.printf("%s%s:%u", i ? "," : "", ldrNames[i], node.light.raw[i]);
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
  node.azimuthDeg = motion.azimuth();
  node.elevationDeg = motion.elevation();
  if (Config::OCC_USE_FIXED_DEMO_THRESHOLD) {
    Serial.printf("SOLACE booted. Ultrasonic demo threshold: %u mm.\n",
                  Config::OCC_DEMO_MAX_MM);
  } else {
    Serial.println("SOLACE booted in the perpendicular/outward reset pose. "
                   "Keep the ultrasonic waiting zone empty for 4 seconds.");
  }
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

  // Demo mode lets occupancy directly enable the L293D. Normal operation also
  // requires a heat warning. Safety stow/fault modes always inhibit the motor.
  const RemoteMode remote = network.remoteMode();
  const bool safeForCooling = remote == RemoteMode::AUTO;
  const bool coolingDemand =
      node.occupied &&
      (Config::DEMO_MOTOR_ON_OCCUPANCY || node.heatWarning);
  setCooling(coolingDemand && safeForCooling);

  // Hold the reset pose briefly before normal shade/tracking/stow motion.
  if (now >= Config::RESET_POSE_HOLD_MS &&
      now - lastTrackMs >= Config::TRACK_PERIOD_MS) {
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
        if (previousState != SystemState::SHADE) computeShadePlan();
        motion.moveToward(node.shadeTargetAzimuthDeg,
                          node.shadeTargetElevationDeg);
        break;
      }
      
      case SystemState::LOW_LIGHT:
      case SystemState::ALIGNED:
      case SystemState::FAULT:
        motion.hold();
        break; // Hold position and issue no further motion command.
    }
    node.azimuthDeg = motion.azimuth();
    node.elevationDeg = motion.elevation();
    if (node.state == SystemState::TRACKING ||
        node.state == SystemState::ALIGNED) {
      solarAzimuthEstimateDeg = node.azimuthDeg;
      solarElevationEstimateDeg = node.elevationDeg;
    }
    previousState = node.state;
  }

  if (now - lastSerialMs >= 1000) {
    lastSerialMs = now;
    printTelemetry();
  }
}
