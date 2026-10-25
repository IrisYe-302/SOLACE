#include "LdrArray.h"

#include "Config.h"

void LdrArray::begin() {
  for (uint8_t i = 0; i < Config::LDR_COUNT; ++i) {
    pinMode(Config::PIN_LDR_ADC[i], INPUT);
    analogSetPinAttenuation(Config::PIN_LDR_ADC[i], ADC_11db);
  }
  analogReadResolution(12);
}

uint16_t LdrArray::readPin(uint8_t pin) {
  analogRead(pin); // Discard the first conversion after changing ADC channels.
  uint32_t total = 0;
  for (uint8_t i = 0; i < Config::LDR_SAMPLES; ++i) {
    total += analogRead(pin);
  }
  return total / Config::LDR_SAMPLES;
}

LdrReadings LdrArray::read() {
  LdrReadings r;
  float sum = 0;
  for (uint8_t i = 0; i < Config::LDR_COUNT; ++i) {
    r.raw[i] = readPin(Config::PIN_LDR_ADC[i]);
    float value = r.raw[i] * Config::LDR_GAIN[i];
    r.corrected[i] = Config::LDR_BRIGHTER_IS_HIGH ? value : 4095.0f - value;
    sum += r.corrected[i];
  }

  r.top = (r.corrected[0] + r.corrected[1]) * 0.5f;
  r.bottom = (r.corrected[2] + r.corrected[3]) * 0.5f;
  r.left = r.corrected[4];
  r.right = r.corrected[5];
  r.average = sum / Config::LDR_COUNT;

  constexpr float epsilon = 1.0f;
  r.azimuthError = (r.left - r.right) / (r.left + r.right + epsilon);
  r.elevationError = (r.top - r.bottom) / (r.top + r.bottom + epsilon);
  r.topShadeDifference = fabsf(r.corrected[0] - r.corrected[1]) /
                         (r.corrected[0] + r.corrected[1] + epsilon);
  return r;
}
