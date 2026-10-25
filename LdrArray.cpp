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

  r.top = (r.corrected[Config::LDR_T1] +
           r.corrected[Config::LDR_T2]) * 0.5f;
  r.bottom = (r.corrected[Config::LDR_B1] +
              r.corrected[Config::LDR_B2]) * 0.5f;
  r.left = r.corrected[Config::LDR_L1];
  r.right = r.corrected[Config::LDR_R1];
  r.average = sum / Config::LDR_COUNT;

  constexpr float epsilon = 1.0f;
  // Positive error increases the corresponding logical servo angle. With the
  // configured shaft convention, that is counter-clockwise:
  //   brighter R1       -> top/azimuth servo CCW
  //   brighter B1 + B2  -> bottom/elevation servo CCW
  r.azimuthError = (r.right - r.left) / (r.left + r.right + epsilon);
  r.elevationError = (r.bottom - r.top) / (r.top + r.bottom + epsilon);
  r.topShadeDifference =
      fabsf(r.corrected[Config::LDR_T1] - r.corrected[Config::LDR_T2]) /
      (r.corrected[Config::LDR_T1] +
       r.corrected[Config::LDR_T2] + epsilon);
  return r;
}
