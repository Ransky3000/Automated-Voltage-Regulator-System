# ⚡ Automated Voltage Regulator System

**Project Overview & Developer Guide**

## ⚙️ System Overview

This system automatically regulates AC voltage output from a **Stavol TDGC2-2KVA Variable Autotransformer (Variac)** using a continuous rotation servo. It is designed for electrical engineering research where a generator (1V–48V) feeds the Variac, and the output must be held within a user-defined target range (e.g. 20V–24V) even as the generator's voltage drifts over time.

The firmware follows a **State Machine** architecture, consistent with our previous project ([Automated Cacao Processor](Projects_we_made/Automated-Cacao-Processor/)).

## 🔬 The Problem: Vin Drifts, Rotary Must Compensate

The Variac is a variable transformer — its knob position outputs a **ratio** of the input voltage, not an absolute value.

**The Formula:**
```
Vout = Vin × (Rotary / 220)
```
Where:
- **Vin** = Generator output (variable, 1V–48V)
- **Rotary** = Dial position on the Variac (0–250)
- **220** = Fixed max input rating of the transformer
- **Vout** = Regulated voltage delivered to the load

**Example:** If Vin = 30V and the target Vout is 20V–24V:
```
Rotary_min = 20 × 220 / 30 = 146.7
Rotary_max = 24 × 220 / 30 = 176.0
```
But if Vin drifts down to 23V (over time), the same target requires:
```
Rotary_min = 20 × 220 / 23 = 191.3
Rotary_max = 24 × 220 / 23 = 229.6
```
The rotary must **move higher to compensate**. Currently, a person must watch the output and manually turn the knob. This project automates that.

### The Math Reality

| Vin | Desired Vout | Rotary Needed | Achievable? |
|:----|:-------------|:--------------|:------------|
| 30V | 20–24V | 146–176 | ✅ Both achievable |
| 20V | 20–24V | 220–264 | ⚠️ Can reach 20V (220), but NOT 24V (264) |
| 15V | 20–24V | 293–352 | ❌ Can't reach either end |

When Vin is too low, the transformer **physically cannot boost enough**. The system detects this and triggers an `UNDER_VOLTAGE_ALARM`. Similarly, if Vin is too high, it triggers an `OVER_VOLTAGE_ALARM`.

## 🏗️ System Architecture

### Control Loop (Vout Closed-Loop + Physical Position Feedback)
```
Every cycle:
  1. Read Vin  (from ZMPT101B #1 on A3)
  2. Read Vout (from ZMPT101B #2 on A0)
  3. If Vout is outside target range:
       → Calculate new Rotary = Vout_target × 220 / Vin
       → Servo adjusts to that position using pulse-and-wait
  4. If Vout is within range:
       → Do nothing, hold position (servo detached)
```

### State Machine
```cpp
enum SystemState {
    MENU,                // User configures target voltage via Keypad
    REGULATING,          // Normal operation: monitoring Vin/Vout, servo adjusting
    UNDER_VOLTAGE_ALARM, // Vin too low, Variac maxed out. Buzzer + LCD warning.
    OVER_VOLTAGE_ALARM   // Vin too high, Variac bottomed out. Buzzer + LCD warning.
};
```

### Alarm Behavior
| State | Servo Action | Why |
|:------|:-------------|:----|
| `UNDER_VOLTAGE_ALARM` | Hold at MAX (250) | Squeeze out every possible volt |
| `OVER_VOLTAGE_ALARM` | Hold at MIN (0) | Reduce output as much as possible |
| `REGULATING` | Actively adjusting | Hunting for the target Vout |

Alarms are **self-recovering**: the system continuously monitors Vin even while in alarm, and automatically returns to `REGULATING` the moment conditions improve.

## 🎛️ Servo Mechanics (MG996R Continuous Rotation)

The servo is a **modified MG996R** for continuous 360° rotation. Unlike a standard servo where you set an angle, this one controls **speed and direction**:
- `1500µs` (90°) = **STOP**
- `> 1500µs` = **Clockwise** (increase Variac voltage)
- `< 1500µs` = **Counter-clockwise** (decrease Variac voltage)

### Position Feedback (Internal Potentiometer on A2)
Since a continuous rotation servo has no built-in position awareness, the MG996R's internal potentiometer wiper has been **externalized** and wired to Arduino pin **A2**. This provides real-time physical position feedback (POT range 10–670 maps to Variac dial 0–250V).

### Pulse-and-Wait Control
To prevent overshoot and oscillation, the servo uses a **pulse-and-wait** strategy:
1. **PULSE** — Spin for 80ms in the required direction
2. **SETTLE** — Stop and wait 150ms for the mechanics to settle
3. **READ** — Check the pot value to see where we ended up
4. **DECIDE** — If still not at target, pulse again; if at target, detach and go idle

### Proportional Speed
The servo speed scales based on proximity to the target:
| Error | Speed | Behavior |
|:------|:------|:---------|
| > 30V | Fast (±200µs from center) | Full speed travel |
| 10–30V | Medium (±100µs from center) | Moderate approach |
| < 10V | Slow (±50µs from center) | Fine creep positioning |

### Direction-Aware Safety Limits
Emergency stops only trigger when heading **toward** a physical limit, not away from it:
- At POT_MIN → Only blocks Reverse (going further into the stop)
- At POT_MAX → Only blocks Forward (going further into the stop)

### Servo Detach (Drift Prevention)
When idle, the servo PWM signal is **detached** (`myServo.detach()`) to prevent the MG996R from drifting due to unbalanced internal resistors. It re-attaches only when a correction is needed.

### EEPROM 51-Point Calibration
Because gear coupling between the servo shaft and the Variac knob introduces mechanical error, a **51-point physical calibration** (every 5V from 0–250V) is stored in EEPROM. The auto-positioning code uses **interpolation** between these calibrated points instead of a simple linear `map()`.

## 📌 Pin Map

| Pin | Assignment |
|:----|:-----------|
| **A0** | ZMPT101B #2 — Output Voltage (Vout) |
| **A1** | CSM2 — Output Current |
| **A2** | Servo POT — Position Feedback |
| **A3** | ZMPT101B #1 — Input Voltage (Vin) |
| **A4** | SDA (LCD I2C via PCF8574) |
| **A5** | SCL (LCD I2C via PCF8574) |
| **D3** | Servo PWM (tap selector) |
| **D5** | Buzzer |
| **D6–D9** | Keypad Rows |
| **D10–D13** | Keypad Cols |

> **Note:** CSM1 (Input Current on A2) was removed from the original schematic to free up A2 for the Servo Potentiometer feedback. Input current monitoring is not critical for the regulation loop.

## 📦 Dependencies

| Library | Purpose |
|:--------|:--------|
| **ZMPT101B_DRIVER** | Custom AC Voltage sensing (Blocking & Non-Blocking modes) |
| **Servo.h** | Continuous rotation servo control |
| **EEPROM.h** | Persistent calibration storage |
| **LiquidCrystal_I2C** | 16×2 LCD via I2C (PCF8574) |
| **Keypad** | 4×4 matrix keypad input |

## 📁 Directory Structure

```
Automated-Voltage-Regulator-System/
├── __main__/                       ← 🎯 Main application sketch
│   ├── __main__.ino
│   ├── Machine.h                   ← State Machine class definition
│   └── Machine.cpp                 ← State Machine implementation
├── tests/                          ← 🧪 Test & calibration sketches
│   ├── ZMPT101B_test/              ← Blocking voltage sensor test
│   ├── Non-blocking_test/          ← Non-blocking voltage sensor test
│   ├── Servo360_test/              ← Basic servo direction/speed tester
│   ├── ServoPotFeedback_test/      ← Reads internal pot while spinning
│   ├── ServoMap_test/              ← Maps POT range to Variac voltage
│   ├── ServoCalibration_test/      ← Guided 11-point calibration wizard
│   └── ServoAutoPosition_test/     ← Full auto-positioning with EEPROM calibration
├── docs/                           ← 📄 Documentation & simulations
│   ├── Datasheet/                  ← Component datasheets
│   ├── ZMPT101B_sensor_Simulation/ ← ZMPT101B Proteus simulation
│   └── Automated-Voltage-Regulator-System_Simulation/
│       ├── *.PDF                   ← Circuit schematics (exported)
│       ├── Screenshots/            ← PCB layer screenshots
│       └── Voltage Regulator Actual picture/
├── A recap/                        ← 📸 Screenshots of early design discussions
├── README.md
├── library.properties
└── .gitignore
```

## 🚀 Getting Started

1. Install the **ZMPT101B_DRIVER** library in your Arduino `libraries/` folder.
2. Open `__main__/__main__.ino` in Arduino IDE.
3. Select your board (Arduino Uno) and port.
4. Upload to the device.

## 🧪 Testing

| Sketch | Purpose |
|:-------|:--------|
| `tests/ZMPT101B_test/` | Verify voltage readings using **blocking** mode |
| `tests/Non-blocking_test/` | Verify voltage readings using **non-blocking** mode (4kHz sampling) |
| `tests/Servo360_test/` | Test servo direction, speed, and stop via Serial commands |
| `tests/ServoPotFeedback_test/` | Read the MG996R internal pot (A2) while spinning |
| `tests/ServoMap_test/` | Map POT range (0–670) to Variac voltage (0–250V) with safety limits |
| `tests/ServoCalibration_test/` | Guided calibration wizard for POT-to-Voltage mapping |
| `tests/ServoAutoPosition_test/` | Full auto-positioning: pulse-and-wait, EEPROM calibration, proportional speed, safety limits |

## 🛠️ Development

- Implement core regulation logic in `Machine.cpp`.
- Define system states in the `enum SystemState` inside `Machine.h`.
- Keep `__main__.ino` minimal — hardware wiring + `machine.runState()` calls only.
- Create new test sketches in `tests/` to verify subsystems.

## 💡 Technical Specifications

| Spec | Value |
|:-----|:------|
| **Processor** | Arduino Uno / ATmega328P |
| **Voltage Sensors** | 2× ZMPT101B (Vin on A3, Vout on A0) |
| **Current Sensor** | ACS712 20A (Output Current on A1) |
| **Servo** | MG996R (modified for continuous rotation) |
| **Variac** | Stavol TDGC2-2KVA (0–250V, Single Phase, 220V/50-60Hz) |
| **Display** | 16×2 LCD via I2C (PCF8574, address 0x27) |
| **Input** | 4×4 Matrix Keypad |
| **Architecture** | State Machine (`Machine` class) |
