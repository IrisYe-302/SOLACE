#include "OccupancySensor.h"

#include "Config.h"

HardwareSerial occupancySerial(1);

namespace {
volatile uint32_t hcsrEchoRiseUs = 0;
volatile uint32_t hcsrEchoDurationUs = 0;
volatile bool hcsrEchoReady = false;

void IRAM_ATTR onHcsrEchoChange() {
  const uint32_t nowUs = micros();
  if (digitalRead(Config::PIN_HCSR04_ECHO)) {
    hcsrEchoRiseUs = nowUs;
  } else {
    hcsrEchoDurationUs = nowUs - hcsrEchoRiseUs;
    hcsrEchoReady = true;
  }
}
} // namespace

void OccupancySensor::begin() {
  if (Config::USE_A02YYUW) {
    // A02YYUW TX connects to RX; its RX wire is not needed for continuous output.
    occupancySerial.begin(Config::ULTRASONIC_BAUD, SERIAL_8N1,
                          Config::PIN_A02YYUW_RX, -1);
  } else {
    pinMode(Config::PIN_HCSR04_TRIGGER, OUTPUT);
    pinMode(Config::PIN_HCSR04_ECHO, INPUT);
    digitalWrite(Config::PIN_HCSR04_TRIGGER, LOW);
    attachInterrupt(digitalPinToInterrupt(Config::PIN_HCSR04_ECHO),
                    onHcsrEchoChange, CHANGE);
  }
}

void OccupancySensor::update(uint32_t nowMs) {
  if (Config::USE_A02YYUW) updateA02(nowMs);
  else updateHcsr04(nowMs);
}

void OccupancySensor::updateA02(uint32_t nowMs) {
  while (occupancySerial.available()) {
    const uint8_t b = occupancySerial.read();
    if (frameIndex_ == 0 && b != 0xFF) continue;
    frame_[frameIndex_++] = b;
    if (frameIndex_ < 4) continue;

    frameIndex_ = 0;
    const uint8_t checksum = static_cast<uint8_t>(frame_[0] + frame_[1] + frame_[2]);
    if (checksum != frame_[3]) continue;
    acceptDistance((static_cast<uint16_t>(frame_[1]) << 8) | frame_[2], nowMs);
  }
}

void OccupancySensor::updateHcsr04(uint32_t nowMs) {
  if (hcsrEchoReady) {
    noInterrupts();
    const uint32_t echoUs = hcsrEchoDurationUs;
    hcsrEchoReady = false;
    interrupts();
    // Round-trip time at approximately 0.343 mm/us, divided by two.
    acceptDistance(static_cast<uint16_t>(echoUs * 0.1715f), nowMs);
  }

  if (nowMs - lastPingMs_ >= Config::ULTRASONIC_PING_PERIOD_MS) {
    lastPingMs_ = nowMs;
    digitalWrite(Config::PIN_HCSR04_TRIGGER, LOW);
    delayMicroseconds(2);
    digitalWrite(Config::PIN_HCSR04_TRIGGER, HIGH);
    delayMicroseconds(10);
    digitalWrite(Config::PIN_HCSR04_TRIGGER, LOW);
  }
}

void OccupancySensor::acceptDistance(uint16_t mm, uint32_t nowMs) {
  if (mm < Config::OCC_VALID_MIN_MM || mm > Config::OCC_VALID_MAX_MM) return;
  distanceMm_ = mm;

  if (!Config::OCC_USE_FIXED_DEMO_THRESHOLD && !baselineReady_) {
    baselineTotal_ += mm;
    ++baselineSamples_;
    if (nowMs >= Config::OCC_BASELINE_LEARN_MS && baselineSamples_ > 5) {
      baselineMm_ = baselineTotal_ / baselineSamples_;
      baselineReady_ = true;
    }
    return;
  }

  const bool detected = Config::OCC_USE_FIXED_DEMO_THRESHOLD
                            ? mm <= Config::OCC_DEMO_MAX_MM
                            : (baselineMm_ > Config::OCC_CHANGE_MM
                                   ? mm < baselineMm_ - Config::OCC_CHANGE_MM
                                   : mm < Config::OCC_FALLBACK_MAX_MM);
  if (detected) {
    if (positiveCount_ < Config::OCC_ON_COUNT) ++positiveCount_;
    negativeCount_ = 0;
    lastOccupiedMs_ = nowMs;
    if (positiveCount_ >= Config::OCC_ON_COUNT) occupied_ = true;
  } else {
    if (negativeCount_ < Config::OCC_OFF_COUNT) ++negativeCount_;
    positiveCount_ = 0;
    if (negativeCount_ >= Config::OCC_OFF_COUNT &&
        nowMs - lastOccupiedMs_ >= Config::OCC_HOLD_MS) {
      occupied_ = false;
    }
  }
}
