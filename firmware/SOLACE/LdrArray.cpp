#include "LdrArray.h"

#include "Config.h"

void LdrArray::begin() {
  filterInitialized_ = false;
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
    const float value = r.raw[i] * Config::LDR_GAIN[i];
    const float corrected =
        Config::LDR_BRIGHTER_IS_HIGH ? value : 4095.0f - value;
    if (!filterInitialized_) {
      filtered_[i] = corrected;
    } else {
      filtered_[i] +=
          Config::LDR_FILTER_ALPHA * (corrected - filtered_[i]);
    }
    r.corrected[i] = filtered_[i];
    sum += r.corrected[i];
  }
  filterInitialized_ = true;

  r.top = (r.corrected[Config::LDR_T1] +
           r.corrected[Config::LDR_T2]) * 0.5f;
  r.bottom = (r.corrected[Config::LDR_B1] +
              r.corrected[Config::LDR_B2]) * 0.5f;
  r.left = (r.corrected[Config::LDR_T1] +
            r.corrected[Config::LDR_B1] +
            r.corrected[Config::LDR_L1]) / 3.0f;
  r.right = (r.corrected[Config::LDR_T2] +
             r.corrected[Config::LDR_B2] +
             r.corrected[Config::LDR_R1]) / 3.0f;
  r.average = sum / Config::LDR_COUNT;

  constexpr float epsilon = 1.0f;
  // Positive error increases the corresponding logical servo angle. With the
  // configured shaft convention, that is counter-clockwise:
  //   brighter right side (T2/B2/R1) -> top/azimuth servo CCW
  //   brighter B1 + B2  -> bottom/elevation servo CCW
  r.azimuthError = (r.right - r.left) / (r.left + r.right + epsilon);
  r.elevationError = (r.bottom - r.top) / (r.top + r.bottom + epsilon);
  r.topShadeDifference =
      fabsf(r.corrected[Config::LDR_T1] - r.corrected[Config::LDR_T2]) /
      (r.corrected[Config::LDR_T1] +
       r.corrected[Config::LDR_T2] + epsilon);
  return r;
}
