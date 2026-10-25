# SOLACE ESP32 firmware

This is a configurable Arduino-ESP32 starting point, not a final wiring plan. It
implements:

- six directly connected calibrated LDR channels using ESP32 ADC1;
- two-axis servo tracking with deadband and movement limits;
- selectable HC-SR04 or A02YYUW ultrasonic occupancy detection;
- a digital heat-threshold input such as a KY-028 `DO` pin;
- occupancy-and-heat-gated fan/cooling output;
- automatic objective switching: solar-power tracking while unoccupied and a
  calibrated shade pose while occupied;
- Wi-Fi status, remote stow commands, incoming weather data, and optional JSON
  telemetry suitable for a heat-map backend;
- separate wind, rain, fault, low-light, shade, tracking, and aligned behavior.

## Power mode versus shade mode

The operating priority is:

```text
FAULT / WIND / RAIN
        ↓
LOW LIGHT
        ↓
PERSON PRESENT → SHADE pose
        ↓
NO PERSON → LDR solar-power tracking
```

`SHADE_AZ_DEG` and `SHADE_EL_DEG` in `Config.h` define the initial shade pose.
Tune them against the fixed miniature passenger area. The occupancy sensor only
reports that something entered its beam; it does not report the person's 2D
position, so one sensor cannot dynamically aim a shadow at arbitrary locations.
For that later capability, use two or more ranging zones/sensors or a small
thermal/ToF array and combine passenger position with the measured sun vector.

## Dependencies

Install the Espressif ESP32 Arduino board package. No servo library is needed;
the firmware drives both servos directly with the ESP32 LEDC PWM peripheral.
Open `SOLACE.ino` in Arduino IDE and keep all files in this folder.

## SG servo PWM calibration

Both outputs use 50 Hz PWM. The initial 1000-2000 microsecond range is
deliberately conservative because Arduino-kit SG90-compatible servos vary.

1. Power the servo from a separate regulated 5 V supply and connect that
   supply's ground to ESP32 ground. Do not power both servos from the ESP32.
2. Disconnect the horn or linkage before the first powered test.
3. Leave the pulse limits at 1000-2000 us and boot at the configured 90-degree
   start position.
4. Fit the horn at the mechanism's physical midpoint.
5. Move toward each end gradually by changing the start angle. Stop before the
   servo buzzes, stalls, or pushes the linkage into a hard stop.
6. Set `AZ_MIN_DEG`, `AZ_MAX_DEG`, `EL_MIN_DEG`, and `EL_MAX_DEG` to the safe
   mechanical angles with margin. Only widen the microsecond pulse range in
   small steps if the mechanism genuinely needs more travel.
7. If an axis moves backward, change its `SERVO_*_REVERSED` flag instead of
   rewiring power.

Pulse endpoints and mechanical angle limits solve different problems: pulse
endpoints characterize the servo; angle limits protect the assembled linkage.

Physical servo pinout:

| Servo | ESP32 pin | Logical axis |
| --- | --- | --- |
| Top servo | GPIO18 | Azimuth |
| Bottom servo | GPIO19 | Elevation |

## Before connecting hardware

1. Select an ESP32-WROOM development-board target in Arduino IDE and verify every
   GPIO in `Config.h` against that board's printed pin labels.
2. Confirm each servo's supply voltage/current and power servos from a separate
   regulated rail with common ground. Never power them from the ESP32 3.3 V pin.
3. Connect the cooling fan only through an appropriate MOSFET/driver with flyback
   protection if required.
4. Choose the ultrasonic model with `USE_A02YYUW` in `Config.h`. The default is
   HC-SR04. Its 5 V `ECHO` must pass through a resistor divider or level shifter
   before reaching the 3.3 V ESP32 input. For an A02YYUW, power it at 3.3 V when
   its UART output connects directly to the ESP32 and verify its pin order.
5. Mechanically disconnect the servo horns for the first power-on. Establish
   safe minimum, maximum, and stow angles before allowing loaded movement.
6. Fill in Wi-Fi credentials locally. Avoid publishing them in source control.

## Six direct LDRs

No analog multiplexer is required. Because these LDR networks use a 5 V supply,
each one needs both a 10 kOhm upper safety resistor and the existing 10 kOhm
resistor to ground:

```text
5 V → 10 kOhm → LDR → ADC node → 10 kOhm → GND
                         │
                         └──────── ESP32 ADC pin
```

The upper 10 kOhm resistor limits the theoretical maximum ADC voltage to about
2.50 V even if the illuminated LDR approaches zero resistance. A direct
`5 V → LDR → ADC → 10 kOhm → GND` connection is unsafe because the ADC node can
approach 5 V. Firmware attenuation does not make an over-voltage safe.

Connect the six protected ADC nodes as follows:

| Sensor | ESP32 pin |
| --- | --- |
| T1 | GPIO32 |
| T2 | GPIO33 |
| B1 | GPIO34 |
| B2 | GPIO35 |
| L1 | GPIO36 / VP |
| R1 | GPIO39 / VN |

`B1` and `B2` are the hinge-side sensors. `T1` and `T2` are mounted on the
opposite free edge of the panel.

The firmware averages T1/T2 and B1/B2 for elevation tracking. L1 and R1 provide
the azimuth error. GPIO34, GPIO35, GPIO36, and GPIO39 are input-only, which is
appropriate for these analog divider outputs. The HC-SR04 moves to GPIO16/17,
and the temperature threshold input moves to GPIO13.

The firmware configures each LDR channel for 11 dB ADC attenuation, whose
documented ESP32 measurement range extends to approximately 3.1 V. The raw
low-light threshold in `Config.h` is only a starting point and must be calibrated
with the completed sensor mounts and actual lighting.

## Local Wi-Fi interface

After reading the ESP32 IP address from your router, browser, or serial debug:

```text
GET http://DEVICE_IP/status
GET http://DEVICE_IP/command?mode=wind
GET http://DEVICE_IP/command?mode=rain
GET http://DEVICE_IP/command?mode=auto
GET http://DEVICE_IP/weather?wind=12.4&rain=0
```

`/weather` is intentionally provider-neutral. A laptop, phone, dashboard, or
later cloud function can fetch a weather service and forward only wind/rain
values to the device. Local tracking and cooling continue without Wi-Fi.

## Heat-map telemetry

Start the dashboard with `cd webapp && python3 server.py`, then set
`TELEMETRY_URL` to `http://<laptop-LAN-IP>:8080/api/telemetry`. Set `NODE_ID`,
`NODE_MAP_X`, and `NODE_MAP_Y` in `Config.h` for each ESP. Every ten
seconds the sketch can POST a JSON record containing location, temperature
status, occupancy, light readings, tracking errors, and system state.

The current 37-in-1 digital threshold module cannot supply a numerical
temperature, so `temperatureC` is sent as `null`. For an actual heat map, add a
calibrated, shaded, ventilated I2C temperature/humidity sensor such as an SHT31
or SHT40. I2C devices can share the existing SDA/SCL pair with a future INA219.

## Multimeter-only solar demo

A multimeter is enough for snapshots:

- Measure panel voltage **across** a fixed load.
- Measure current by moving the meter lead to its current jack and inserting the
  meter **in series** with that load.
- Compute `power_W = voltage_V * current_A`.
- Record one sample every fixed interval and estimate energy with
  `energy_Wh += power_W * interval_seconds / 3600`.

An easier and safer one-meter method is to use a known resistor as the load and
measure only its voltage. Then calculate `current_A = voltage_V / resistance_ohm`
and `power_W = voltage_V * voltage_V / resistance_ohm`. Choose a resistor with a
power rating comfortably above the calculated power. If the panel label gives
`Vmp` and `Imp`, a useful first load is approximately `R = Vmp / Imp`.

Do not place a meter in current mode directly across the solar panel; that is a
short circuit. Do not use open-circuit voltage alone as an energy measurement.
For a defensible comparison, use the same load, lamp geometry, sampling interval,
and trial duration for fixed and tracking runs.

# SOLACE — Solar-Powered Adaptive Shading System

SOLACE is a prototype solar-powered adaptive shading system developed for HackIoT. It combines embedded control, light sensing, occupancy detection, and geometric optimization to balance shading performance and solar energy collection.

## Project Goals

- **Occupancy-aware shading:** Prioritize shading a designated ground area when a person is detected.
- **Solar tracking:** Prioritize solar alignment when the target area does not need to be shaded.
- **Safety-aware control:** Support fault handling and wind/rain stow states.
- **Environmental monitoring:** Monitor ambient light, occupancy, and temperature.

## System Overview

**Microcontroller:** ESP32-WROOM

**Key components:**
- Six LDR sensors for relative light measurements
- Occupancy detection using an ultrasonic sensor
- Temperature monitoring
- Dual-axis servo control
- Network communication and remote operating modes
- Geometric shading optimization

## Shading Optimization

The `ShadeOptimizer` module searches candidate panel orientations to maximize coverage of a fixed rectangular ground target.

The current optimization strategy is:

1. Search candidate panel azimuth and elevation angles.
2. Project the panel onto the ground using the estimated sun direction.
3. Calculate the overlap between the projected panel and the target region.
4. If a candidate achieves at least 70% target coverage, select the candidate with the highest direct-sun alignment proxy.
5. If no candidate reaches 70% coverage, prioritize maximum target coverage.

The current geometric model uses a panel size of 11.5 × 6.5 cm and a target region of 11.5 × 6.5 cm. The rotation pivot height is currently set to 15 cm.

## Control Logic

The system prioritizes operating states in the following order:

1. Fault, wind protection, and rain protection
2. Occupancy-aware shading
3. Low-light handling when the target area is unoccupied
4. Solar tracking or aligned-position holding

## Current Development Status

- [x] Implement the `ShadeOptimizer` interface and geometric search algorithm.
- [x] Integrate occupancy-priority logic into state selection.
- [ ] Validate geometric projection and coverage calculations.
- [ ] Calibrate the rotation-pivot offset and servo angle conventions.
- [ ] Connect a validated sun-direction estimate to the optimizer.
- [ ] Verify servo motion and measured ground coverage.
- [ ] Test the complete occupancy-aware control workflow.

**Current stage:** Prototype implementation and validation.

The optimizer currently uses test sun-direction inputs during initial integration. The geometric model, mechanical offsets, and servo-angle mapping still require validation before the optimization results can safely control the physical mechanism.

## Repository Structure

```text
SOLACE/
├── SOLACE.ino
├── ShadeOptimizer.h
├── ShadeOptimizer.cpp
├── LdrArray.h
├── LdrArray.cpp
└── ...
```

The tree above highlights the main files; additional modules handle motion control, occupancy sensing, temperature monitoring, networking, configuration, and system state definitions.

## Technologies

- C++
- Arduino framework
- ESP32
- Embedded sensing and control
- Computational geometry
- Optimization

## Project Context

Developed for HackIoT, SOLACE explores how embedded systems and geometric optimization can support adaptive shading while balancing human comfort and solar energy collection.
