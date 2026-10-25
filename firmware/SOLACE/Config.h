#pragma once

#include <Arduino.h>

// Keep deployment credentials in an untracked Secrets.h beside this file.
// Copy Secrets.example.h to Secrets.h and fill in the local values.
#if __has_include("Secrets.h")
#include "Secrets.h"
#endif

#ifndef SOLACE_WIFI_SSID
#define SOLACE_WIFI_SSID ""
#endif

#ifndef SOLACE_WIFI_PASSWORD
#define SOLACE_WIFI_PASSWORD ""
#endif

// -----------------------------------------------------------------------------
// ESP32-S3 DevKitC-1-style development-board hardware profile
// -----------------------------------------------------------------------------
// This avoids GPIO0/3/45/46 (boot straps), GPIO19/20 (native USB), GPIO43/44
// (UART0), and GPIO35/36/37 (may be internal flash/PSRAM on some S3 modules).
// Verify the labels against the exact development board before wiring it.
namespace Config {

constexpr uint8_t LDR_COUNT = 6;
// Top-view sensor layout. B1/B2 are nearest the hinge and T1/T2 are on the
// free edge:
//
//             free edge
//          T1           T2
//       L1                 R1
//          B1           B2
//               hinge
//
// Keep these named indices synchronized with PIN_LDR_ADC and LDR_GAIN.
enum LdrIndex : uint8_t { LDR_T1, LDR_T2, LDR_B1, LDR_B2, LDR_L1, LDR_R1 };
// Direct ADC1 inputs in the named order above. ADC1 remains usable with Wi-Fi.
constexpr uint8_t PIN_LDR_ADC[LDR_COUNT] = {4, 5, 6, 7, 8, 9};
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 10;
constexpr uint8_t PIN_SERVO_TOP = 18;    // Physical top servo; drives azimuth.
constexpr uint8_t PIN_SERVO_BOTTOM = 12; // Physical bottom servo; drives elevation.
constexpr uint8_t PIN_TEMP_DIGITAL = 11;  // Left-header pin; e.g. KY-028 DO.
constexpr uint8_t PIN_COOLING = 14;       // L293D pin 1 (EN1), with 10k pulldown.

// Direct LEDC PWM for SG-style positional micro servos. Start conservatively at
// 1000-2000 us and widen only after testing each unloaded servo. Clone limits vary.
constexpr uint32_t SERVO_PWM_HZ = 50;
// ESP32-S3 LEDC supports at most 14-bit duty resolution. Requesting 16 bits
// makes ledcAttach() fail, leaving the servo signal pins inactive.
constexpr uint8_t SERVO_PWM_BITS = 14;
constexpr uint8_t SERVO_TOP_CHANNEL = 0; // Used by Arduino-ESP32 core 2.x.
constexpr uint8_t SERVO_BOTTOM_CHANNEL = 1;
constexpr uint16_t SERVO_TOP_MIN_US = 1000;
constexpr uint16_t SERVO_TOP_MAX_US = 2000;
constexpr uint16_t SERVO_BOTTOM_MIN_US = 1000;
constexpr uint16_t SERVO_BOTTOM_MAX_US = 2000;
// With both values false, increasing the logical angle is expected to be
// counter-clockwise when looking at each servo shaft. Flip only the affected
// value if the physical mounting makes that servo turn clockwise instead.
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
// Safe 3.3 V network for every input:
// 3V3 -> 1 kOhm -> LDR -> ADC node -> 10 kOhm -> GND.
// Even with an ideal 0-Ohm LDR, the ADC node is limited to about 3.00 V.
constexpr uint32_t LDR_SUPPLY_MV = 3300;
constexpr uint32_t LDR_SERIES_OHMS = 1000;
constexpr uint32_t LDR_PULLDOWN_OHMS = 10000;
constexpr uint32_t LDR_MAX_ADC_MV =
    LDR_SUPPLY_MV * LDR_PULLDOWN_OHMS /
    (LDR_SERIES_OHMS + LDR_PULLDOWN_OHMS);
static_assert(LDR_MAX_ADC_MV <= 3100,
              "LDR divider can exceed the ESP32 ADC measurement range");
constexpr uint16_t LDR_LOW_LIGHT_THRESHOLD = 500; // Raw ADC starting point; calibrate.
constexpr uint8_t LDR_SAMPLES = 8;
// Low-pass the six channels before calculating direction. This keeps ADC and
// lamp-flicker noise from looking like a changing sun direction.
constexpr float LDR_FILTER_ALPHA = 0.45f;

// Motion limits. These must match the printed mechanism before first movement.
constexpr float AZ_MIN_DEG = 15;
constexpr float AZ_MAX_DEG = 165;
constexpr float EL_MIN_DEG = 15;
constexpr float EL_MAX_DEG = 165;
// On every reset, center the top axis and command the bottom axis to the
// mechanism's perpendicular/outward pose. Adjust EL_RESET_DEG after calibration
// if the linkage's physical 90-degree position is not actually perpendicular.
constexpr float AZ_RESET_DEG = 90;
constexpr float EL_RESET_DEG = 90;
constexpr uint32_t RESET_POSE_HOLD_MS = 4000;
// Force mode is useful indoors before the low-light threshold is calibrated.
// The filtered controller below still prevents small fluctuations from moving
// the mechanism continuously.
constexpr bool DEMO_FORCE_LIGHT_TRACKING = true;
constexpr float TRACKING_DEADBAND = 0.02f;
constexpr float TRACKING_KP_DEG = 24.0f;
constexpr float MIN_TRACK_STEP_DEG = 0.75f;
constexpr float MAX_TRACK_STEP_DEG = 4.0f;
constexpr uint8_t TRACKING_CONFIRM_SAMPLES = 2;
constexpr uint32_t TRACK_PERIOD_MS = 150;

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
// Shade optimizer geometry. X/Y locate the center of the fixed passenger zone
// relative to the rotation pivot on the ground plane. These demo values match a
// 35-degree azimuth/elevation lamp with a 15 cm pivot and must be measured on
// the final model. Optimizer search is restricted to reachable servo commands.
constexpr float SHADE_PIVOT_HEIGHT_CM = 15.0f;
constexpr float SHADE_PANEL_OFFSET_U_CM = 0.0f;
constexpr float SHADE_PANEL_OFFSET_V_CM = 0.0f;
constexpr float SHADE_PANEL_OFFSET_N_CM = 0.0f;
constexpr float SHADE_TARGET_CENTER_X_CM = -17.6f;
constexpr float SHADE_TARGET_CENTER_Y_CM = -12.3f;
constexpr float SHADE_SEARCH_AZ_MIN_DEG = AZ_MIN_DEG;
constexpr float SHADE_SEARCH_AZ_MAX_DEG = AZ_MAX_DEG;
constexpr float SHADE_SEARCH_EL_MIN_DEG = EL_MIN_DEG;
constexpr float SHADE_SEARCH_EL_MAX_DEG = 85.0f;
constexpr float SHADE_SEARCH_STEP_DEG = 5.0f;

// Ultrasonic occupancy detection. Demo mode uses a fixed close-range threshold;
// normal mode learns the empty background and detects a closer object.
constexpr uint32_t ULTRASONIC_BAUD = 9600;
constexpr uint32_t ULTRASONIC_PING_PERIOD_MS = 100;
// Demo mode: require a hand within 8 cm. A rolling median and consecutive-hit
// requirement reject isolated short echoes without making detection slow.
constexpr bool OCC_USE_FIXED_DEMO_THRESHOLD = true;
constexpr uint16_t OCC_DEMO_MAX_MM = 80;
constexpr uint16_t OCC_VALID_MIN_MM = 20;
constexpr uint16_t OCC_VALID_MAX_MM = 4500;
constexpr uint16_t OCC_CHANGE_MM = 250;
constexpr uint16_t OCC_FALLBACK_MAX_MM = 1600;
constexpr uint8_t OCC_MEDIAN_SAMPLES = 5;
constexpr uint8_t OCC_ON_COUNT = 2;
constexpr uint8_t OCC_OFF_COUNT = 3;
constexpr uint32_t OCC_HOLD_MS = 750;
constexpr uint32_t OCC_STALE_MS = 600;
constexpr uint32_t OCC_BASELINE_LEARN_MS = 4000;

// GPIO14 drives only the L293D enable input, never the motor itself.
constexpr bool COOLING_ACTIVE_HIGH = true;
// During the demo, occupancy alone enables the motor. Set this false to require
// both occupancy and the digital heat-warning input in normal operation.
constexpr bool DEMO_MOTOR_ON_OCCUPANCY = true;

// Remote weather values sent to /weather can trigger these safety states.
constexpr float WIND_STOW_THRESHOLD_MPS = 10.0f;
constexpr uint32_t TELEMETRY_PERIOD_MS = 10000;

// -----------------------------------------------------------------------------
// Wi-Fi and heat-map endpoint
// -----------------------------------------------------------------------------
// Fill these in locally. Do not commit real credentials to a public repository.
constexpr char WIFI_SSID[] = SOLACE_WIFI_SSID;
constexpr char WIFI_PASSWORD[] = SOLACE_WIFI_PASSWORD;

// Leave empty to disable outbound HTTP telemetry. The local /status endpoint
// still works. Replace this example IP with the laptop's LAN IP.
// Example: "http://192.168.1.50:8080/api/telemetry"
constexpr char TELEMETRY_URL[] = "http://10.23.172.224:8080/api/telemetry";
constexpr char NODE_ID[] = "solace-demo-01";
// Fixed location on the dashboard, normalized from 0.0 (left/top) to 1.0
// (right/bottom). Give every deployed node its own position.
constexpr float NODE_MAP_X = 0.5f;
constexpr float NODE_MAP_Y = 0.5f;
constexpr double NODE_LATITUDE = 0.0;
constexpr double NODE_LONGITUDE = 0.0;

} // namespace Config
