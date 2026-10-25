#include <Arduino.h>

// Standalone SOLACE L293D test for the ESP32-S3 wiring in Config.h.
// Flash this sketch only for a supervised bench test with the mechanism clear.
constexpr uint8_t PIN_MOTOR_ENABLE = 14;  // L293D pin 1 (EN1)
constexpr uint32_t OFF_TIME_MS = 3000;
constexpr uint32_t ON_TIME_MS = 2000;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_MOTOR_ENABLE, OUTPUT);
  digitalWrite(PIN_MOTOR_ENABLE, LOW);
  Serial.println("L293D motor test starts in 3 seconds. Keep clear of the motor.");
  delay(OFF_TIME_MS);
}

void loop() {
  Serial.println("GPIO14 HIGH: motor should run for 2 seconds");
  digitalWrite(PIN_MOTOR_ENABLE, HIGH);
  delay(ON_TIME_MS);

  Serial.println("GPIO14 LOW: motor should stop for 3 seconds");
  digitalWrite(PIN_MOTOR_ENABLE, LOW);
  delay(OFF_TIME_MS);
}
