# SOLACE

The repository is split into two top-level areas:

- `firmware/` contains the ESP32 Arduino sketch and standalone hardware tests.
- `web/` contains the FastAPI telemetry server and browser UI.

## Run the live demo web app

The route UI and ESP telemetry server now run together through FastAPI:

```sh
cd web
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8080
```

Open `http://localhost:8080`. Set `TELEMETRY_URL` in
`firmware/SOLACE/Config.h` to
`http://<laptop-LAN-IP>:8080/api/telemetry`. The interface uses live ESP data
when available and retains the four simulated temperatures as fallback.

Telemetry endpoints are `POST /api/telemetry`, `GET /api/readings`,
`GET /api/events`, and `GET /api/health`. Route-demo endpoints remain available
at `/streetlamps`, `/paths`, and `/best-route`.

This is a configurable Arduino-ESP32 starting point, not a final wiring plan. It
implements:

- six directly connected calibrated LDR channels using ESP32 ADC1;
- two-axis servo tracking with deadband and movement limits;
- selectable HC-SR04 or A02YYUW ultrasonic occupancy detection;
- a digital heat-threshold input such as a KY-028 `DO` pin;
- occupancy-and-heat-gated fan control through an L293D;
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

`SHADE_AZ_DEG` and `SHADE_EL_DEG` in `Config.h` define the safe fallback shade
pose. The optimizer normally drives the occupied pose using the configured
fixed passenger-zone coordinates. The occupancy sensor only reports that
something entered its beam; it does not report the person's 2D position, so one
sensor cannot dynamically aim a shadow at arbitrary locations.
For that later capability, use two or more ranging zones/sensors or a small
thermal/ToF array and combine passenger position with the measured sun vector.

## Dependencies

Install the Espressif ESP32 Arduino board package. No servo library is needed;
the firmware drives both servos directly with the ESP32 LEDC PWM peripheral.
Open `firmware/SOLACE/SOLACE.ino` in Arduino IDE. The sketch and its supporting
source files remain together in the Arduino-required `SOLACE` folder.

## SG servo PWM calibration

Both outputs use 50 Hz PWM at the ESP32-S3-supported 14-bit resolution. The
initial 1000-2000 microsecond range is deliberately conservative because
Arduino-kit SG90-compatible servos vary.

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

On every reset, the firmware commands both axes to 90 degrees, producing the
initial perpendicular/outward pose, and holds it for four seconds while the
ultrasonic sensor learns the empty waiting zone. If the assembled linkage is
not physically perpendicular at 90 degrees, adjust `EL_RESET_DEG` in `Config.h`
after unloaded servo calibration. `AZ_RESET_DEG` centers the top/azimuth axis.

Physical servo pinout:

| Servo | ESP32-S3 pin | Logical axis |
| --- | --- | --- |
| Top servo | GPIO18 | Azimuth |
| Bottom servo | GPIO12 | Elevation |

## Before connecting hardware

1. Select **ESP32S3 Dev Module** in Arduino IDE and verify every GPIO in
   `Config.h` against the exact board's printed pin labels.
2. Confirm each servo's supply voltage/current and power servos from a separate
   regulated rail with common ground. Never power them from the ESP32 3.3 V pin.
3. Connect the fan motor only through the L293D described below. Never connect
   the motor directly to an ESP32-S3 GPIO.
4. Choose the ultrasonic model with `USE_A02YYUW` in `Config.h`. The default is
   HC-SR04. Its 5 V `ECHO` must pass through a resistor divider or level shifter
   before reaching the 3.3 V ESP32 input. For an A02YYUW, power it at 3.3 V when
   its UART output connects directly to the ESP32 and verify its pin order.
5. Mechanically disconnect the servo horns for the first power-on. Establish
   safe minimum, maximum, and stow angles before allowing loaded movement.
6. Copy `firmware/SOLACE/Secrets.example.h` to
   `firmware/SOLACE/Secrets.h`, then fill in the Wi-Fi credentials locally.
   `Secrets.h` is ignored by Git and must not be published.

## ESP32-S3 pinout

This profile targets an ESP32-S3 DevKitC-1-style board. It reserves GPIO19/20
for native USB, GPIO43/44 for UART0, the boot-strapping pins, and GPIO35-37,
which some S3 modules use internally for flash/PSRAM.

| SOLACE connection | ESP32-S3 pin | Power / notes |
| --- | --- | --- |
| LDR T1 ADC node | GPIO4 / ADC1_CH3 | 3.3 V divider below |
| LDR T2 ADC node | GPIO5 / ADC1_CH4 | 3.3 V divider below |
| LDR B1 ADC node | GPIO6 / ADC1_CH5 | Hinge-side sensor |
| LDR B2 ADC node | GPIO7 / ADC1_CH6 | Hinge-side sensor |
| LDR L1 ADC node | GPIO8 / ADC1_CH7 | Left sensor |
| LDR R1 ADC node | GPIO9 / ADC1_CH8 | Right sensor |
| I2C SDA | GPIO21 | Future SHT/INA sensor data |
| I2C SCL | GPIO10 | Future SHT/INA sensor clock |
| Top servo signal | GPIO18 | Azimuth; servo power is external 5 V |
| Bottom servo signal | GPIO12 | Elevation; servo power is external 5 V |
| Temperature module DO | GPIO11 | Left-header pin; power module from 3.3 V |
| L293D EN1 | GPIO14 | Automatic motor enable; add a 10 kOhm pulldown |
| HC-SR04 TRIG | GPIO16 | 3.3 V output is normally accepted |
| HC-SR04 ECHO | GPIO17 | Through the 10 kOhm / 15 kOhm divider below |
| A02YYUW TX, alternative | GPIO16 | Used as UART RX when selected in firmware |

Top-view LDR placement and commanded tracking direction:

```text
             free edge
          T1           T2
       L1                 R1
          B1           B2
               hinge
```

More light on `B1/B2` commands the bottom servo counter-clockwise. More light
on `R1` commands the top servo counter-clockwise. Counter-clockwise means viewed
toward the servo shaft; if an installed servo moves the other way, change only
its `SERVO_TOP_REVERSED` or `SERVO_BOTTOM_REVERSED` flag in `Config.h`.

### HC-SR04 wiring and ECHO divider

| HC-SR04 pin | Connection |
| --- | --- |
| VCC | External regulated 5 V |
| GND | Common GND |
| TRIG | ESP32-S3 GPIO16 directly |
| ECHO | 10 kOhm resistor to the GPIO17 divider node |

Wire the 5 V ECHO output through this divider; do not connect ECHO directly to
the S3:

```text
HC-SR04 ECHO ── 10 kOhm ──┬── GPIO17
                           │
                         15 kOhm
                           │
                          GND
```

This reduces a 5.0 V ECHO pulse to approximately 3.0 V:
`5.0 V * 15 kOhm / (10 kOhm + 15 kOhm) = 3.0 V`. The divider ground must be
the same ground used by the HC-SR04 and ESP32-S3.

For the current hand-under-sensor demo, `OCC_USE_FIXED_DEMO_THRESHOLD` is
enabled: one reading at or below 60 mm marks the zone occupied. Occupancy
clears about one second after the hand is removed. Set that option to `false`
to restore learned-background detection.

### L293D and 130-size motor

During development, power the S3 through USB. Use a separate regulated 5 V,
3 A or larger supply for both servos, the HC-SR04, L293D, and motor. Join the
external supply ground to an S3 GND pin, but do not join its 5 V output to the
board's USB 5 V rail.

Orient the DIP chip with its notch upward. Pin 1 is the upper-left pin and the
numbers run counter-clockwise:

```text
                 notch
             +---U---+
 GPIO14 EN1  1|       |16  VSS logic 5 V
 5 V     1A  2|       |15  4A → GND (unused)
 motor A 1Y  3| L293D |14  4Y → no connection
 GND         4|       |13  GND
 GND         5|       |12  GND
 motor B 2Y  6|       |11  3Y → no connection
 GND     2A  7|       |10  3A → GND (unused)
 motor 5 V   8|       |9   EN2 → GND (unused)
             +-------+
```

Add a 10 kOhm resistor from pin 1/GPIO14 to GND so the motor stays off while
the S3 resets. Add a 0.1 uF ceramic capacitor across pins 16 and GND, another
0.1 uF across pins 8 and GND, 470-1000 uF across the external 5 V rail, and a
0.1 uF ceramic capacitor directly across the motor terminals. The L293D already
contains motor clamp diodes. If the motor rotates backward, swap the wires on
pins 3 and 6.

For an isolated motor check, flash
`firmware/tests/motor_test/motor_test.ino`. It drives only
GPIO14, alternating two seconds on and three seconds off. If that sketch works,
the hardware path is good. The current demo configuration lets `occ=1` enable
the motor without requiring `hot=1`; normal operation can be restored by setting
`DEMO_MOTOR_ON_OCCUPANCY` to `false`. If Serial reports `cool=1` but the motor
does not turn, measure GPIO14 and L293D pin 1 before changing application logic.

## Six direct LDRs

No analog multiplexer is required. Power the LDR networks from 3.3 V so they do
not depend on a 5 V output from the S3 board. Each channel uses this circuit:

```text
3V3 → 1 kOhm → LDR → ADC node → 10 kOhm → GND
                       │
                       └──────── ESP32-S3 ADC pin
```

The upper 1 kOhm resistor limits the theoretical maximum ADC voltage to about
3.00 V even if the illuminated LDR approaches zero resistance.

Connect the six protected ADC nodes as follows:

| Sensor | ESP32-S3 pin |
| --- | --- |
| T1 | GPIO4 |
| T2 | GPIO5 |
| B1 | GPIO6 |
| B2 | GPIO7 |
| L1 | GPIO8 |
| R1 | GPIO9 |

`B1` and `B2` are the hinge-side sensors. `T1` and `T2` are mounted on the
opposite free edge of the panel.

The firmware compares the T1/T2 pair against the B1/B2 pair for elevation. For
azimuth it compares the average of T1/B1/L1 against T2/B2/R1, so all six sensors
participate in directional tracking. All inputs are on ESP32-S3 ADC1, available
while Wi-Fi is active. The HC-SR04 uses GPIO16/17, and the temperature threshold
input uses GPIO11 on the left header.

The tracking input is averaged and low-pass filtered. An axis must remain
outside the deadband in the same direction for two consecutive control samples
before it moves. Large errors use faster steps while small errors use slower
steps, so a passing shadow or one noisy ADC reading should not reverse a servo.
The current demo profile checks every 150 ms and moves by 0.75-4 degrees, so a
steady directional light should begin producing motion within about 300 ms.
Tune `LDR_FILTER_ALPHA`, `TRACKING_DEADBAND`, and the tracking step limits in
`Config.h` if the final sensor shroud has substantially different behavior.

To distinguish feedback noise from a power problem, watch `az=` and `el=` in
Serial. If those commanded angles change, inspect the six `ldr=` values and
sensor placement. If the angles stay fixed but a servo twitches, the cause is
electrical or mechanical: use a separate regulated 5 V supply, common ground,
short signal/ground wiring, and a 470-1000 uF capacitor near the servo rail.

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

## Digital temperature threshold and heat-map telemetry

For a KY-028-style module, connect `+` to ESP32-S3 `3V3`, `G`/`-` to common
ground, and `DO` to GPIO11. The potentiometer sets the digital hot/cool trip
point. The firmware sends this as `heatWarning`, and the web UI displays the
live node as `HOT` or `clear`; it is a threshold state, not a temperature in °C.

Start the dashboard with `cd web && uvicorn main:app --host 0.0.0.0 --port 8080`,
then set
`TELEMETRY_URL` to `http://<laptop-LAN-IP>:8080/api/telemetry`. Set `NODE_ID`,
`NODE_MAP_X`, and `NODE_MAP_Y` in `Config.h` for each ESP. Every ten
seconds the sketch can POST a JSON record containing location, temperature
status, occupancy, light readings, tracking errors, and system state.

On Serial, `Wi-Fi connected; SOLACE IP: ...` confirms association. A successful
dashboard upload prints `Telemetry POST: HTTP 201`; the web header then changes
to `ESP CONNECTED · LIVE DATA`. If no fresh packet arrives for 35 seconds, the
dashboard marks the ESP offline instead of treating an old reading as live.
`TELEMETRY_URL` must point to the laptop's LAN IP, not `localhost`. Open guest
networks with captive portals may associate but still block the ESP's HTTP
traffic; a phone hotspot or ordinary 2.4 GHz LAN is usually easier for a demo.

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

## Project overview

SOLACE is a prototype solar-powered adaptive shading system developed for HackIoT. It combines embedded control, light sensing, occupancy detection, and geometric optimization to balance shading performance and solar energy collection.

## Project Goals

- **Occupancy-aware shading:** Prioritize shading a designated ground area when a person is detected.
- **Solar tracking:** Prioritize solar alignment when the target area does not need to be shaded.
- **Safety-aware control:** Support fault handling and wind/rain stow states.
- **Environmental monitoring:** Monitor ambient light, occupancy, and temperature.

## System Overview

**Microcontroller:** ESP32-S3

**Key components:**
- Six LDR sensors for relative light measurements
- Occupancy detection using an ultrasonic sensor
- Temperature monitoring
- Dual-axis servo control
- Network communication and remote operating modes
- Geometric shading optimization

## Shading Optimization

The `ShadeOptimizer` module searches candidate panel orientations to maximize coverage of a fixed rectangular ground target.

The consolidated firmware captures the last LDR-tracked panel pose when
occupancy begins and uses it as the sun-direction estimate. It computes one
shade plan on entry to `SHADE`, restricts the search to reachable servo angles,
and then moves toward that plan. If the request is invalid, it falls back to
`SHADE_AZ_DEG` / `SHADE_EL_DEG`. The resulting target angles, coverage, and
power proxy are included in `/status` and outbound telemetry.

`SHADE_TARGET_CENTER_X_CM` and `SHADE_TARGET_CENTER_Y_CM` locate the passenger
zone relative to the rotation pivot. The included values match the 35°/35° demo
lamp geometry; measure and replace them for the physical model. Run the native
optimizer regression test with:

```sh
c++ -std=c++17 -Ifirmware/SOLACE \
  firmware/tests/shade_optimizer_test.cpp \
  firmware/SOLACE/ShadeOptimizer.cpp -o /tmp/solace-shade-test-bin
/tmp/solace-shade-test-bin
```

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

The optimizer now controls the occupied target pose within configured servo
limits. The geometric target coordinates, mechanical offsets, and servo-angle
conventions still require physical calibration before the reported coverage can
be treated as a real-world measurement.

## Repository Structure

```text
SOLACE/
├── firmware/
│   ├── SOLACE/
│   │   ├── SOLACE.ino
│   │   ├── Config.h
│   │   ├── LdrArray.cpp
│   │   ├── MotionController.cpp
│   │   └── ...
│   └── tests/
│       ├── motor_test/
│       └── servo_test/
├── web/
│   ├── main.py
│   ├── index.html
│   ├── script.js
│   ├── style.css
│   └── requirements.txt
├── README.md
└── .gitignore
```

The firmware source remains in one Arduino sketch folder, while the web server
keeps its backend, static assets, and Python dependencies together.

## Technologies

- C++
- Arduino framework
- ESP32
- Embedded sensing and control
- Computational geometry
- Optimization

## Project Context

Developed for HackIoT, SOLACE explores how embedded systems and geometric optimization can support adaptive shading while balancing human comfort and solar energy collection.
