#include <Arduino.h>

// SOLACE ESP32-S3 raw-pulse servo test.
// This intentionally does not use LEDC, a servo library, Wi-Fi, or any other
// project code. It generates standard servo pulses directly with digitalWrite.

constexpr uint8_t PIN_SERVO_TOP = 18;
constexpr uint8_t PIN_SERVO_BOTTOM = 12;

constexpr uint16_t SERVO_MIN_US = 1000;
constexpr uint16_t SERVO_MAX_US = 2000;
constexpr uint32_t SERVO_FRAME_US = 20000; // 50 Hz
constexpr int TEST_STEP_DEG = 2;
constexpr uint8_t FRAMES_PER_STEP = 2;

uint16_t angleToPulseUs(int angleDeg) {
  const int angle = constrain(angleDeg, 0, 180);
  return SERVO_MIN_US +
      static_cast<uint32_t>(SERVO_MAX_US - SERVO_MIN_US) * angle / 180;
}

void sendFrame(uint8_t pin, int angleDeg) {
  const uint16_t pulseUs = angleToPulseUs(angleDeg);
  digitalWrite(pin, HIGH);
  delayMicroseconds(pulseUs);
  digitalWrite(pin, LOW);
  delayMicroseconds(SERVO_FRAME_US - pulseUs);
}

void holdAngle(uint8_t pin, int angleDeg, uint32_t durationMs) {
  const uint32_t frames = durationMs / 20;
  for (uint32_t frame = 0; frame < frames; ++frame) {
    sendFrame(pin, angleDeg);
  }
}

void sweepServo(const char *name, uint8_t pin) {
  Serial.printf("%s: hold 0 degrees\n", name);
  holdAngle(pin, 0, 1000);

  Serial.printf("%s: sweep 0 -> 180 degrees\n", name);
  for (int angle = 0; angle <= 180; angle += TEST_STEP_DEG) {
    for (uint8_t frame = 0; frame < FRAMES_PER_STEP; ++frame) {
      sendFrame(pin, angle);
    }
  }
  holdAngle(pin, 180, 1000);

  Serial.printf("%s: sweep 180 -> 0 degrees\n", name);
  for (int angle = 180; angle >= 0; angle -= TEST_STEP_DEG) {
    for (uint8_t frame = 0; frame < FRAMES_PER_STEP; ++frame) {
      sendFrame(pin, angle);
    }
  }

  Serial.printf("%s: return to center\n", name);
  holdAngle(pin, 90, 1000);
  digitalWrite(pin, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SERVO_TOP, OUTPUT);
  pinMode(PIN_SERVO_BOTTOM, OUTPUT);
  digitalWrite(PIN_SERVO_TOP, LOW);
  digitalWrite(PIN_SERVO_BOTTOM, LOW);

  Serial.println("Raw servo pulse test starts in 2 seconds.");
  delay(2000);
}

void loop() {
  sweepServo("TOP / GPIO18", PIN_SERVO_TOP);
  delay(1000);
  sweepServo("BOTTOM / GPIO12", PIN_SERVO_BOTTOM);
  Serial.println("Both raw-pulse tests complete. Repeating in 3 seconds.");
  delay(3000);
}
