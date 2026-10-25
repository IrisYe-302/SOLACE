#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// ESP32-WROOM development-board hardware profile
// -----------------------------------------------------------------------------
// This avoids common boot-strapping pins and keeps the LDR input on ADC1 because
// ESP32 ADC2 is unavailable while Wi-Fi is active. Verify the labels against the
// exact development board in hand before connecting hardware.
namespace Config {

constexpr uint8_t LDR_COUNT = 6;
// Direct ADC1 inputs: T1, T2, B1, B2, L1, R1. B1/B2 are nearest the hinge;
// T1/T2 are on the free edge. ADC1 remains usable with Wi-Fi.
constexpr uint8_t PIN_LDR_ADC[LDR_COUNT] = {32, 33, 34, 35, 36, 39};
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t PIN_SERVO_TOP = 18;    // Physical top servo; drives azimuth.
constexpr uint8_t PIN_SERVO_BOTTOM = 19; // Physical bottom servo; drives elevation.
constexpr uint8_t PIN_TEMP_DIGITAL = 13;  // e.g. KY-028 DO
constexpr uint8_t PIN_COOLING = 23;       // MOSFET/driver input

// Direct LEDC PWM for SG-style positional micro servos. Start conservatively at
// 1000-2000 us and widen only after testing each unloaded servo. Clone limits vary.
constexpr uint32_t SERVO_PWM_HZ = 50;
constexpr uint8_t SERVO_PWM_BITS = 16;
constexpr uint8_t SERVO_TOP_CHANNEL = 0; // Used by Arduino-ESP32 core 2.x.
constexpr uint8_t SERVO_BOTTOM_CHANNEL = 1;
constexpr uint16_t SERVO_TOP_MIN_US = 1000;
constexpr uint16_t SERVO_TOP_MAX_US = 2000;
constexpr uint16_t SERVO_BOTTOM_MIN_US = 1000;
constexpr uint16_t SERVO_BOTTOM_MAX_US = 2000;
constexpr bool SERVO_TOP_REVERSED = false;
constexpr bool SERVO_BOTTOM_REVERSED = false;

// Select the sensor actually installed. HC-SR04 is the default bench-demo
// option. Its 5 V ECHO signal requires a divider/level shifter before GPIO17.
constexpr bool USE_A02YYUW = false;
constexpr uint8_t PIN_A02YYUW_RX = 16;    // A02YYUW TX -> ESP32 UART RX
constexpr uint8_t PIN_HCSR04_TRIGGER = 16;
constexpr uint8_t PIN_HCSR04_ECHO = 17;

// Set to 1 after adding an Adafruit SHT31/SHT40-style I2C temperature sensor
// and adapting TemperatureMonitor.cpp. The default supports a threshold module.
constexpr bool USE_DIGITAL_TEMPERATURE_THRESHOLD = true;
constexpr bool TEMP_THRESHOLD_ACTIVE_LOW = true;

// Direct sensor order: T1, T2, B1, B2, L1, R1.
constexpr float LDR_GAIN[LDR_COUNT] = {1, 1, 1, 1, 1, 1};
constexpr bool LDR_BRIGHTER_IS_HIGH = true;
// Safe 5 V network for every input:
// 5 V -> 10 kOhm -> LDR -> ADC node -> 10 kOhm -> GND.
// Even with an ideal 0-Ohm LDR, the ADC node is limited to about 2.50 V.
constexpr uint32_t LDR_SUPPLY_MV = 5000;
constexpr uint32_t LDR_SERIES_OHMS = 10000;
constexpr uint32_t LDR_PULLDOWN_OHMS = 10000;
constexpr uint32_t LDR_MAX_ADC_MV =
    LDR_SUPPLY_MV * LDR_PULLDOWN_OHMS /
    (LDR_SERIES_OHMS + LDR_PULLDOWN_OHMS);
static_assert(LDR_MAX_ADC_MV <= 3100,
              "LDR divider can exceed the ESP32 ADC measurement range");
constexpr uint16_t LDR_LOW_LIGHT_THRESHOLD = 500; // Raw ADC starting point; calibrate.
constexpr uint8_t LDR_SAMPLES = 8;

// Motion limits. These must match the printed mechanism before first movement.
constexpr float AZ_MIN_DEG = 15;
constexpr float AZ_MAX_DEG = 165;
constexpr float EL_MIN_DEG = 15;
constexpr float EL_MAX_DEG = 165;
constexpr float AZ_START_DEG = 90;
constexpr float EL_START_DEG = 90;
constexpr float TRACKING_DEADBAND = 0.05f;
constexpr float TRACKING_KP_DEG = 12.0f;
constexpr float MAX_TRACK_STEP_DEG = 2.0f;
constexpr uint32_t TRACK_PERIOD_MS = 350;

// Stow positions are deliberately separate because the mechanically safest
// direction depends on the enclosure and linkage. Adjust after bench testing.
constexpr float WIND_STOW_AZ_DEG = 90;
constexpr float WIND_STOW_EL_DEG = 15;
constexpr float RAIN_STOW_AZ_DEG = 90;
constexpr float RAIN_STOW_EL_DEG = 165;

// Occupied-mode shade pose for a fixed waiting zone. Tune these with the lamp
// at several positions and mark the pose that covers the passenger target. A
// single distance sensor tells us that somebody is present, but not where they
// are standing, so it cannot calculate this pose on its own.
constexpr float SHADE_AZ_DEG = 90;
constexpr float SHADE_EL_DEG = 90;

// Ultrasonic occupancy detection. Start with the waiting zone empty so the unit
// can learn the background surface. A person is a closer-than-background change.
constexpr uint32_t ULTRASONIC_BAUD = 9600;
constexpr uint32_t ULTRASONIC_PING_PERIOD_MS = 100;
constexpr uint16_t OCC_VALID_MIN_MM = 30;
constexpr uint16_t OCC_VALID_MAX_MM = 4500;
constexpr uint16_t OCC_CHANGE_MM = 250;
constexpr uint16_t OCC_FALLBACK_MAX_MM = 1600;
constexpr uint8_t OCC_ON_COUNT = 3;
constexpr uint8_t OCC_OFF_COUNT = 8;
constexpr uint32_t OCC_HOLD_MS = 30000;
constexpr uint32_t OCC_BASELINE_LEARN_MS = 4000;

// A MOSFET or motor-driver input, never the fan/motor itself, connects here.
constexpr bool COOLING_ACTIVE_HIGH = true;

// Remote weather values sent to /weather can trigger these safety states.
constexpr float WIND_STOW_THRESHOLD_MPS = 10.0f;
constexpr uint32_t TELEMETRY_PERIOD_MS = 10000;

// -----------------------------------------------------------------------------
// Wi-Fi and heat-map endpoint
// -----------------------------------------------------------------------------
// Fill these in locally. Do not commit real credentials to a public repository.
constexpr char WIFI_SSID[] = "YOUR_WIFI_NAME";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// Leave empty to disable outbound HTTP telemetry. The local /status endpoint
// still works. Replace this example IP with the laptop's LAN IP.
// Example: "http://192.168.1.50:8080/api/telemetry"
constexpr char TELEMETRY_URL[] = "";
constexpr char NODE_ID[] = "solace-demo-01";
// Fixed location on the dashboard, normalized from 0.0 (left/top) to 1.0
// (right/bottom). Give every deployed node its own position.
constexpr float NODE_MAP_X = 0.5f;
constexpr float NODE_MAP_Y = 0.5f;
constexpr double NODE_LATITUDE = 0.0;
constexpr double NODE_LONGITUDE = 0.0;

} // namespace Config
