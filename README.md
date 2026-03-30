# ⚡ Automated Voltage Regulator System

**Project Overview & Developer Guide**

## ⚙️ System Overview

This system automatically regulates AC voltage output from a **Stavol TDGC2-2KVA Variable Autotransformer (Variac)** using a continuous rotation servo. It is designed for electrical engineering research where a generator feeds the Variac, and the output must be held within a user-defined target range even as the generator's voltage drifts over time.

The firmware follows a **State Machine** architecture with **Vout closed-loop regulation**, using real-time ZMPT101B sensor feedback to drive servo corrections.

## 🔬 The Problem: Vin Drifts, Servo Must Compensate

The Variac is a variable transformer — its knob position outputs a **ratio** of the input voltage, not an absolute value.

**The Formula:**
```
Vout = Vin × (Rotary / 220)
```
Where:
- **Vin** = Generator output (variable)
- **Rotary** = Dial position on the Variac (0–250)
- **220** = Fixed max input rating of the transformer
- **Vout** = Regulated voltage delivered to the load

**Example:** If Vin = 220V and dial is at 25 → `Vout = 220 × (25/220) = 25V`

If Vin drops to 200V, the same dial position gives only 22.7V — the system detects this and **automatically turns the knob** to compensate.

## 🏗️ System Architecture

### Control Loop (Vout Closed-Loop)
```
Every cycle:
  1. Read Vin  (ZMPT101B on A3)
  2. Read Vout (ZMPT101B on A0, 10-sample moving average)
  3. Is |Vout - Target| > Tolerance?
     YES → Wait 3 seconds (confirmation window)
           → Still out? → Pulse servo proportionally
     NO  → Hold position (servo detached)
```

### Boot Sequence
```
Power ON
  ├─ Init LCD, sensors, load EEPROM settings
  ├─ ACS712 auto-zero calibration (no load)
  ├─ Home servo to 0V via POT feedback (A2)
  └─ If saved target exists → auto-start regulation
```

### State Machine
```
UI States:           Regulator States:      Servo States:
├─ HOME              ├─ REG_IDLE            ├─ SERVO_IDLE
├─ CONFIGURE         ├─ REG_ACTIVE          ├─ SERVO_PULSING
├─ INPUT_TARGET      ├─ REG_UNDER_VOLTAGE   └─ SERVO_SETTLING
├─ INPUT_TOLERANCE   └─ REG_OVER_VOLTAGE
├─ VIEW_STATUS
├─ VIEW_INPUT
└─ VIEW_OUTPUT
```

## 🎮 Keypad Legend

### Physical Keypad Layout
```
┌─────┬─────┬─────┬─────┐
│  1  │  2  │  3  │  A  │
├─────┼─────┼─────┼─────┤
│  4  │  5  │  6  │  B  │
├─────┼─────┼─────┼─────┤
│  7  │  8  │  9  │  C  │
├─────┼─────┼─────┼─────┤
│  *  │  0  │  #  │  D  │
└─────┴─────┴─────┴─────┘
```

### Global Keys (Work from ANY screen)
| Key | Function |
|:----|:---------|
| `#` | Toggle LCD backlight ON/OFF |

### Per-Screen Key Functions

#### HOME Screen
```
[A] Configure
[B] View Status
```
| Key | Function |
|:----|:---------|
| `A` | Go to Configure menu |
| `B` | Go to View Status menu |

#### CONFIGURE Screen
```
[A] Set Voltage
[B] Tolerance
```
| Key | Function |
|:----|:---------|
| `A` | Enter target voltage input |
| `B` | Enter tolerance input |
| `D` | Back to HOME |

#### INPUT_TARGET Screen
```
Set target
_                  ← blinking cursor
```
| Key | Function |
|:----|:---------|
| `0-9` | Type digits (max 3, range 0–250) |
| `*` | Backspace (delete last digit) |
| `A` | Confirm target → starts regulation |
| `D` | Cancel → back to CONFIGURE |

#### INPUT_TOLERANCE Screen
```
Set tolerance
_                  ← blinking cursor
```
| Key | Function |
|:----|:---------|
| `0-9` | Type digits (max 2, range 1–99) |
| `*` | Backspace (delete last digit) |
| `A` | Confirm tolerance |
| `D` | Cancel → back to CONFIGURE |

#### VIEW_STATUS Screen
```
[A] Input
[B] Output
```
| Key | Function |
|:----|:---------|
| `A` | View input voltage (Vin) |
| `B` | View output voltage, current, power |
| `D` | Back to HOME |

#### VIEW_INPUT Screen
```
Voltage: <Vin>V
```
| Key | Function |
|:----|:---------|
| `D` | Back to VIEW_STATUS |

#### VIEW_OUTPUT Screen
```
V:<Vout>V I:<A/mA>
P:<W>W    <alarm>
```
| Key | Function |
|:----|:---------|
| `D` | Back to VIEW_STATUS |

### Quick Reference Card
```
┌──────────────────────────────────────────────┐
│  A = Select / Confirm    B = Select Option   │
│  D = Back / Cancel       * = Backspace       │
│  # = Toggle Backlight    0-9 = Type Digits   │
│  C = (reserved)                              │
└──────────────────────────────────────────────┘
```

## 🎛️ Servo Mechanics (MG996R Continuous Rotation)

The servo is a **modified MG996R** for continuous 360° rotation:
- `1500µs` = **STOP**
- `> 1500µs` = **Clockwise** (increase Variac voltage)
- `< 1500µs` = **Counter-clockwise** (decrease Variac voltage)

### Pulse-and-Wait Control
1. **PULSE** — Spin for 80ms in the required direction
2. **SETTLE** — Stop and wait 150ms for the mechanics to settle
3. **READ** — Check Vout to see if we're within tolerance
4. **DECIDE** — If still off, pulse again; if within tolerance, detach and idle

### Proportional Speed
| Error | Speed | Behavior |
|:------|:------|:---------|
| > 30V | Fast (±200µs from center) | Full speed travel |
| 10–30V | Medium (±100µs from center) | Moderate approach |
| < 10V | Slow (±50µs from center) | Fine positioning |

### 3-Second Confirmation Window
Before the servo acts, Vout must stay **continuously outside tolerance for 3 seconds**. This prevents hunting caused by momentary noise or fluctuations.

### Homing on Boot
On startup, the servo spins CCW using POT feedback (A2) until it reaches the physical 0V stop (`POT_HOME_VALUE`). This ensures a known starting position before regulation begins.

## 📌 Pin Map

| Pin | Assignment |
|:----|:-----------|
| **A0** | ZMPT101B #2 — Output Voltage (Vout) |
| **A1** | ACS712 20A — Output Current |
| **A2** | Servo POT — Position Feedback (homing only) |
| **A3** | ZMPT101B #1 — Input Voltage (Vin) |
| **A4** | SDA (LCD I2C via PCF8574) |
| **A5** | SCL (LCD I2C via PCF8574) |
| **D3** | Servo PWM (tap selector) |
| **D5** | Buzzer |
| **D6–D9** | Keypad Rows |
| **D10–D13** | Keypad Cols |

## 💾 EEPROM Map

| Address | Data | Range |
|:--------|:-----|:------|
| Byte 0 | Tolerance (±V) | 1–99 (default 2) |
| Byte 1 | Target voltage | 0–250 (0xFF = none) |

## 📦 Dependencies

| Library | Purpose |
|:--------|:--------|
| **ZMPT101B_DRIVER** | Custom AC Voltage sensing (Non-Blocking, 4kHz sampling) |
| **ACS712-driver** | Custom AC Current sensing (Blocking RMS + Non-Blocking modes) |
| **Servo.h** | Continuous rotation servo control |
| **EEPROM.h** | Persistent target & tolerance storage |
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
│   ├── ServoCalibration_test/      ← Guided calibration wizard
│   └── ServoAutoPosition_test/     ← Full auto-positioning with EEPROM calibration
├── For refences/                   ← 📚 Reference sketches
│   └── For current sensor/         ← ACS712 examples (NonBlocking, Calibration)
├── docs/                           ← 📄 Documentation & simulations
├── README.md
├── library.properties
└── .gitignore
```

## 🚀 Getting Started

1. Install **ZMPT101B_DRIVER** and **ACS712-driver** libraries in your Arduino `libraries/` folder.
2. Open `__main__/__main__.ino` in Arduino IDE.
3. Select board (Arduino Uno) and port.
4. Set Serial Monitor to **115200 baud**.
5. Upload and monitor the serial debug output.

## 🖥️ Serial Debug Output

The system prints a fixed-width debug line every 500ms at 115200 baud:
```
Vin: 220V | Vout:  24V | V_ave:  25V | I: 300mA | POT: 342 | Tgt:  25V | Tol:+- 1V | Reg:ACTIVE  | Srv:IDLE   | Cfm:---
```

| Column | Description |
|:-------|:------------|
| `Vin` | Input voltage from generator |
| `Vout` | Raw output voltage (before averaging) |
| `V_ave` | Smoothed output voltage (10-sample moving average) |
| `I` | Output current (auto-switches A/mA below 1A) |
| `POT` | Servo potentiometer raw ADC value |
| `Tgt` | User-set target voltage |
| `Tol` | Tolerance setting |
| `Reg` | Regulator state (IDLE/ACTIVE/UNDER_V/OVER_V) |
| `Srv` | Servo state (IDLE/PULSE/SETTLE) |
| `Cfm` | Confirmation timer progress |

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
| **Serial** | 115200 baud (fixed-width debug output) |
| **Architecture** | State Machine (`Machine` class) |
