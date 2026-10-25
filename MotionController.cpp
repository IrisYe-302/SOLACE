#include "MotionController.h"

#include <esp_arduino_version.h>

#include "Config.h"

static float limitedStep(float current, float target, float maxStep) {
  return current + constrain(target - current, -maxStep, maxStep);
}

void MotionController::begin() {
  azimuthDeg_ = Config::AZ_RESET_DEG;
  elevationDeg_ = Config::EL_RESET_DEG;
  attachPwm();
  writeAngles();
}

void MotionController::attachPwm() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  const bool topAttached = ledcAttach(Config::PIN_SERVO_TOP,
                                     Config::SERVO_PWM_HZ,
                                     Config::SERVO_PWM_BITS);
  const bool bottomAttached = ledcAttach(Config::PIN_SERVO_BOTTOM,
                                     Config::SERVO_PWM_HZ,
                                     Config::SERVO_PWM_BITS);
  if (!topAttached || !bottomAttached) Serial.println("ERROR: servo LEDC attach failed");
#else
  ledcSetup(Config::SERVO_TOP_CHANNEL, Config::SERVO_PWM_HZ,
            Config::SERVO_PWM_BITS);
  ledcSetup(Config::SERVO_BOTTOM_CHANNEL, Config::SERVO_PWM_HZ,
            Config::SERVO_PWM_BITS);
  ledcAttachPin(Config::PIN_SERVO_TOP, Config::SERVO_TOP_CHANNEL);
  ledcAttachPin(Config::PIN_SERVO_BOTTOM, Config::SERVO_BOTTOM_CHANNEL);
#endif
}

void MotionController::writeServo(uint8_t pin, uint8_t legacyChannel,
                                  float angleDeg, uint16_t minimumUs,
                                  uint16_t maximumUs, bool reversed) {
  float angle = constrain(angleDeg, 0.0f, 180.0f);
  if (reversed) angle = 180.0f - angle;
  const uint32_t pulseUs = minimumUs + lroundf(
      (maximumUs - minimumUs) * angle / 180.0f);
  const uint32_t maximumDuty = (1UL << Config::SERVO_PWM_BITS) - 1UL;
  const uint32_t duty = static_cast<uint32_t>(
      (static_cast<uint64_t>(pulseUs) * maximumDuty * Config::SERVO_PWM_HZ +
       500000ULL) /
      1000000ULL);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  ledcWrite(legacyChannel, duty);
#endif
}

float MotionController::trackingStep(float error, int8_t &lastDirection,
                                     uint8_t &confirmationCount) {
  const float magnitude = fabsf(error);
  if (magnitude <= Config::TRACKING_DEADBAND) {
    lastDirection = 0;
    confirmationCount = 0;
    return 0.0f;
  }

  const int8_t direction = error > 0.0f ? 1 : -1;
  if (direction != lastDirection) {
    lastDirection = direction;
    confirmationCount = 1;
    return Config::TRACKING_CONFIRM_SAMPLES <= 1
               ? direction * Config::MIN_TRACK_STEP_DEG
               : 0.0f;
  }

  if (confirmationCount < Config::TRACKING_CONFIRM_SAMPLES) {
    ++confirmationCount;
  }
  if (confirmationCount < Config::TRACKING_CONFIRM_SAMPLES) return 0.0f;

  // Apply proportional gain only to the portion outside the deadband. Large
  // errors move promptly; small errors approach without five-degree jumps.
  const float step = constrain(
      Config::TRACKING_KP_DEG * (magnitude - Config::TRACKING_DEADBAND),
      Config::MIN_TRACK_STEP_DEG, Config::MAX_TRACK_STEP_DEG);
  return direction * step;
}

void MotionController::track(float azimuthError, float elevationError) {
  const float azStep = trackingStep(
      azimuthError, azimuthDirection_, azimuthConfirmationCount_);
  const float elStep = trackingStep(
      elevationError, elevationDirection_, elevationConfirmationCount_);
  // LdrArray defines positive as the requested counter-clockwise direction.
  // Correct a physically mirrored servo with SERVO_*_REVERSED in Config.h.
  const float nextAzimuth = constrain(
      azimuthDeg_ + azStep, Config::AZ_MIN_DEG, Config::AZ_MAX_DEG);
  const float nextElevation = constrain(
      elevationDeg_ + elStep, Config::EL_MIN_DEG, Config::EL_MAX_DEG);
  if (nextAzimuth != azimuthDeg_ || nextElevation != elevationDeg_) {
    azimuthDeg_ = nextAzimuth;
    elevationDeg_ = nextElevation;
    writeAngles();
  }
}

void MotionController::moveToward(float azimuthTarget, float elevationTarget) {
  hold();
  const float oldAzimuth = azimuthDeg_;
  const float oldElevation = elevationDeg_;
  azimuthDeg_ = limitedStep(azimuthDeg_, azimuthTarget, Config::MAX_TRACK_STEP_DEG);
  elevationDeg_ = limitedStep(elevationDeg_, elevationTarget, Config::MAX_TRACK_STEP_DEG);
  azimuthDeg_ = constrain(azimuthDeg_, Config::AZ_MIN_DEG, Config::AZ_MAX_DEG);
  elevationDeg_ = constrain(elevationDeg_, Config::EL_MIN_DEG, Config::EL_MAX_DEG);
  if (azimuthDeg_ != oldAzimuth || elevationDeg_ != oldElevation) writeAngles();
}

void MotionController::hold() {
  azimuthDirection_ = 0;
  elevationDirection_ = 0;
  azimuthConfirmationCount_ = 0;
  elevationConfirmationCount_ = 0;
}

void MotionController::writeAngles() {
  writeServo(Config::PIN_SERVO_TOP, Config::SERVO_TOP_CHANNEL,
             azimuthDeg_, Config::SERVO_TOP_MIN_US, Config::SERVO_TOP_MAX_US,
             Config::SERVO_TOP_REVERSED);
  writeServo(Config::PIN_SERVO_BOTTOM, Config::SERVO_BOTTOM_CHANNEL,
             elevationDeg_, Config::SERVO_BOTTOM_MIN_US,
             Config::SERVO_BOTTOM_MAX_US, Config::SERVO_BOTTOM_REVERSED);
}
