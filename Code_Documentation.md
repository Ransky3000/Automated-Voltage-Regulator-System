# 📖 Code Documentation — Automated Voltage Regulator

This document describes every file, constant, variable, enum, and method in the `__main__/` program.

---

## 📂 File Overview

| File | Purpose | Lines |
|:-----|:--------|:------|
| `__main__.ino` | Entry point — hardware wiring, `setup()`, `loop()` | ~41 |
| `Machine.h` | Class definition — constants, enums, member declarations | ~161 |
| `Machine.cpp` | Class implementation — all logic and behavior | ~739 |

---

## 📄 `__main__.ino` — Entry Point

This file is intentionally minimal. It handles only hardware instantiation and the Arduino `setup()`/`loop()` lifecycle.

### What It Does
1. **Includes** — Pulls in `Machine.h` and all required libraries.
2. **Keypad Setup** — Defines the 4×4 key matrix and pin mappings (rows D6–D9, cols D10–D13).
3. **LCD Setup** — Creates a `LiquidCrystal_I2C` object at I2C address `0x27`, 16 columns × 2 rows.
4. **Machine Object** — Instantiates the `Machine` class, passing the keypad and LCD by reference.
5. **`setup()`** — Starts Serial at 115200 baud and calls `machine.Initialize()`.
6. **`loop()`** — Calls `machine.keypadPress()` then `machine.runState()` every iteration.

### Why It's Minimal
All logic lives inside the `Machine` class. This keeps `__main__.ino` as a thin wiring layer — no business logic, no state management. If the hardware changes (different pins, different LCD), only this file and the `#define` constants in `Machine.h` need updating.

---

## 📄 `Machine.h` — Class Definition

### Libraries Used

| Library | Include | Purpose |
|:--------|:--------|:--------|
| Arduino Core | `<Arduino.h>` | Core Arduino functions (`millis()`, `analogRead()`, etc.) |
| Keypad | `<Keypad.h>` | 4×4 matrix keypad scanning |
| Wire | `<Wire.h>` | I2C communication bus |
| LCD | `<LiquidCrystal_I2C.h>` | 16×2 LCD display via PCF8574 I2C expander |
| Servo | `<Servo.h>` | PWM servo control (continuous rotation) |
| EEPROM | `<EEPROM.h>` | Persistent storage for target and tolerance |
| Voltage Sensor | `<ZMPT101B_DRIVER.h>` | AC voltage sensing (non-blocking RMS) |
| Current Sensor | `<ACS712-driver.h>` | AC current sensing (blocking RMS, time-sliced) |

---

### Constants

#### Pin Assignments
| Constant | Value | Description |
|:---------|:------|:------------|
| `SERVO_PIN` | `3` | PWM output to MG996R continuous rotation servo |
| `BUZZER_PIN` | `5` | Piezo buzzer for keypress feedback and alarms |
| `SERVO_POT_PIN` | `A2` | Servo internal potentiometer (used for homing only) |
| `VIN_SENSOR_PIN` | `A3` | ZMPT101B #1 — reads generator input voltage |
| `VOUT_SENSOR_PIN` | `A0` | ZMPT101B #2 — reads Variac output voltage |
| `CSM_OUT_PIN` | `A1` | ACS712 20A — reads output current |

#### Physical Limits
| Constant | Value | Description |
|:---------|:------|:------------|
| `VARIAC_MAX_V` | `250` | Maximum Variac dial position (0–250V range) |
| `POT_HOME_VALUE` | `5` | ADC reading at the 0V physical stop. Adjust after bench testing |

#### EEPROM Addresses
| Constant | Value | Description |
|:---------|:------|:------------|
| `EEPROM_TOLERANCE_ADDR` | `0` | Byte 0: stores tolerance (1–99, `0xFF` = use default) |
| `EEPROM_TARGET_ADDR` | `1` | Byte 1: stores target voltage (0–250, `0xFF` = none) |

#### Servo Pulse Timing
| Constant | Value | Description |
|:---------|:------|:------------|
| `PULSE_DURATION_MS` | `80` | How long the servo spins per pulse (milliseconds) |
| `SETTLE_DURATION_MS` | `150` | Wait time after a pulse for mechanics to settle |
| `HOME_SPEED` | `1300` | Servo microseconds for homing (slow CCW) |

#### Regulation
| Constant | Value | Description |
|:---------|:------|:------------|
| `DEFAULT_TOLERANCE_V` | `2` | Default ±2V tolerance if no EEPROM value |
| `STALL_TIMEOUT` | `3000` | If Vout doesn't change for 3s while servo is active → alarm |
| `MIN_VIN_V` | `5` | Minimum Vin to attempt regulation (below = generator off) |
| `VOUT_AVG_SAMPLES` | `10` | Moving average window size for Vout smoothing |
| `CONFIRM_DURATION_MS` | `3000` | Vout must stay outside tolerance this long before servo acts |

#### Sensor Calibration
| Constant | Value | Description |
|:---------|:------|:------------|
| `ACS712_SENSITIVITY` | `0.100` | V/A for ACS712-20A model |
| `ZMPT_SENSITIVITY` | `500.0` | ZMPT101B sensitivity factor |
| `ZMPT_FREQUENCY` | `60.0` | AC frequency in Hz |

---

### Enums

#### `UIState` — What the LCD is showing
```cpp
enum UIState {
  HOME,             // Main menu: [A] Configure / [B] View Status
  CONFIGURE,        // Sub-menu: [A] Set Voltage / [B] Tolerance
  INPUT_TARGET,     // Numeric input for target voltage (0–250)
  INPUT_TOLERANCE,  // Numeric input for tolerance (1–99)
  VIEW_STATUS,      // Sub-menu: [A] Input / [B] Output
  VIEW_INPUT,       // Live display: Vin
  VIEW_OUTPUT       // Live display: Vout, Current, Power, Alarms
};
```

#### `RegulatorState` — What the regulation system is doing
```cpp
enum RegulatorState {
  REG_IDLE,           // No target set — servo does nothing
  REG_ACTIVE,         // Actively monitoring and correcting Vout
  REG_UNDER_VOLTAGE,  // Variac maxed out — Vin too low to reach target
  REG_OVER_VOLTAGE    // Variac at minimum — Vin too high
};
```

#### `ServoState` — Pulse-and-wait FSM
```cpp
enum ServoState {
  SERVO_IDLE,       // Not moving — within tolerance
  SERVO_PULSING,    // Spinning for PULSE_DURATION_MS
  SERVO_SETTLING    // Stopped — waiting SETTLE_DURATION_MS before re-reading
};
```

---

### Member Variables

#### Hardware References
| Type | Name | Description |
|:-----|:-----|:------------|
| `Keypad&` | `k` | Reference to the keypad object |
| `LiquidCrystal_I2C&` | `lcd` | Reference to the LCD object |
| `Servo` | `variacServo` | Servo object for PWM control |
| `ZMPT101B` | `vinSensor` | Voltage sensor on A3 (input) |
| `ZMPT101B` | `voutSensor` | Voltage sensor on A0 (output) |
| `ACS712` | `currentSensor` | Current sensor on A1 |

#### State Tracking
| Type | Name | Description |
|:-----|:-----|:------------|
| `UIState` | `uiState` | Current LCD screen |
| `RegulatorState` | `regState` | Current regulation mode |
| `ServoState` | `servoState` | Current pulse-and-wait phase |

#### Servo Control
| Type | Name | Description |
|:-----|:-----|:------------|
| `bool` | `servoAttached` | Whether servo PWM signal is active |
| `int` | `pulseDirection` | `+1` = CW (increase), `-1` = CCW (decrease) |
| `unsigned long` | `servoTimer` | Timestamp for pulse/settle timing |

#### Regulation
| Type | Name | Description |
|:-----|:-----|:------------|
| `int` | `targetVoltage` | User-set target (`-1` = none) |
| `int` | `toleranceV` | Acceptable ± deviation from target |
| `float` | `currentVin` | Latest Vin reading |
| `float` | `currentVout` | Smoothed Vout (moving average) |
| `float` | `rawVout` | Raw Vout before averaging (for debug) |
| `float` | `currentAmps` | Latest current reading |
| `float` | `currentWatts` | Calculated power (Vout × Amps) |

#### Stall Detection
| Type | Name | Description |
|:-----|:-----|:------------|
| `float` | `lastVout` | Previous Vout for change detection |
| `unsigned long` | `lastVoutChangeTime` | When Vout last changed by >1V |

#### Voltage Smoothing
| Type | Name | Description |
|:-----|:-----|:------------|
| `float[10]` | `voutSamples` | Circular buffer for moving average |
| `byte` | `voutSampleIdx` | Current write position in buffer |
| `float` | `smoothedVout` | Computed average (same as `currentVout`) |

#### Confirmation Window
| Type | Name | Description |
|:-----|:-----|:------------|
| `unsigned long` | `outOfToleranceStart` | When Vout first left tolerance (`0` = timer inactive) |
| `bool` | `isConfirmed` | `true` = confirmation period elapsed, servo can act |

#### UI
| Type | Name | Description |
|:-----|:-----|:------------|
| `String` | `inputBuffer` | Digits typed by user (target or tolerance) |
| `byte` | `isLineFull` | LCD line counter for auto-clear |
| `bool` | `backlightOn` | LCD backlight state |

#### Buzzer
| Type | Name | Description |
|:-----|:-----|:------------|
| `unsigned long` | `buzzerOnMillis` | When buzzer was turned on |
| `bool` | `buzzerActive` | Whether buzzer is currently sounding |

#### LCD Throttle
| Type | Name | Description |
|:-----|:-----|:------------|
| `unsigned long` | `lastLCDTick` | Timestamp of last LCD/debug update (500ms interval) |

---

### Public Methods

| Method | Description |
|:-------|:------------|
| `Machine(Keypad&, LiquidCrystal_I2C&)` | Constructor — initializes all members |
| `void Initialize()` | Hardware setup, EEPROM load, sensor calibration, homing |
| `void runState()` | Main tick — sensors, regulation, UI (called every `loop()`) |
| `void keypadPress()` | Reads keypad, routes key to appropriate handler |

### Private Methods

| Method | Description |
|:-------|:------------|
| `displayLCD(bool, byte, byte, String)` | Write text to LCD with optional `clear` |
| `displayLCD(byte, byte, String)` | Write text to LCD with auto-clear when both lines are full |
| `processTargetInput(char)` | Handle digit input, confirm, cancel for target voltage |
| `processToleranceInput(char)` | Handle digit input, confirm, cancel for tolerance |
| `servoStart(int)` | Attach servo and write microseconds |
| `servoStop()` | Write 1500µs (stop), delay 5ms, detach |
| `homeToZero()` | Spin CCW until POT ≤ `POT_HOME_VALUE` (blocking, 15s timeout) |
| `regulateLoop()` | Vout closed-loop: confirmation → pulse → settle → check |
| `checkAlarms()` | Detect stall → trigger UNDER_V or OVER_V alarm |
| `checkAlarmRecovery()` | Monitor Vin and auto-recover from alarm states |
| `readCurrent()` | Call `currentSensor.readCurrentAC(60)` (blocks ~20ms) |
| `printDebug()` | Print 10-column fixed-width debug line to Serial |

---

## 📄 `Machine.cpp` — Implementation

### Constructor (Line 6–45)

Initializes all member variables to safe defaults:
- `targetVoltage = -1` (no target)
- `toleranceV = DEFAULT_TOLERANCE_V` (2V)
- All sensor values to `0`
- `voutSamples[]` zeroed out
- Confirmation timer reset
- Buzzer inactive

Sensor objects are initialized via the member initializer list:
```cpp
vinSensor(VIN_SENSOR_PIN, ZMPT_FREQUENCY)     // A3, 60Hz
voutSensor(VOUT_SENSOR_PIN, ZMPT_FREQUENCY)    // A0, 60Hz
currentSensor(CSM_OUT_PIN, 5.0, 1023)           // A1, 5V ref, 10-bit ADC
```

---

### `Initialize()` (Line 50–105)

Boot sequence executed once in `setup()`:

```
1. Pin modes (buzzer = OUTPUT, POT = INPUT)
2. LCD init + backlight + "System starting!" message
3. ZMPT101B sensitivity set (both sensors)
4. ACS712 sensitivity set + calibrate() (auto-zeros with no load)
5. Load tolerance from EEPROM byte 0
6. Load target from EEPROM byte 1
   → If valid (0–250): auto-start regulation (REG_ACTIVE)
7. homeToZero() → servo spins CCW until POT ≤ POT_HOME_VALUE
8. 1 second delay
9. Show HOME screen
```

> ⚠️ **Important:** `currentSensor.calibrate()` must run with **no load** connected to get accurate zero-point.

---

### `displayLCD()` (Line 110–128)

Two overloaded helpers for LCD output:

**Version 1:** `displayLCD(bool clear, byte col, byte line, String text)`
- If `clear = true` → clears entire LCD first
- Sets cursor and prints text

**Version 2:** `displayLCD(byte col, byte line, String text)`
- Auto-clears when both lines have been written (`isLineFull >= 2`)
- Prevents stale text from lingering

---

### `servoStart()` / `servoStop()` (Line 133–148)

**`servoStart(int speed)`**
- Attaches servo to `SERVO_PIN` if not already attached
- Writes microseconds: `1500` = stop, `>1500` = CW, `<1500` = CCW

**`servoStop()`**
- Writes `1500` (stop command)
- Waits 5ms for the command to register
- **Detaches** servo to prevent MG996R drift from unbalanced internal resistors

---

### `homeToZero()` (Line 153–196)

**Purpose:** Move servo to the physical 0V position on boot.

**How it works:**
1. Read POT on A2
2. If already ≤ `POT_HOME_VALUE` → skip (already at 0V)
3. Display "Homing to 0V..." on LCD
4. Spin CCW at `HOME_SPEED` (1300µs = slow)
5. Continuously read POT until it reaches `POT_HOME_VALUE`
6. **15-second timeout safety** — if POT never reaches target, stop and show error

> This is the **only blocking function** in the system (besides `readCurrent()`). It runs once during boot.

---

### `readCurrent()` (Line 201–203)

Calls `currentSensor.readCurrentAC(60)` from the ACS712-driver library.
- Blocks for ~20ms (one full 60Hz AC cycle) to compute true RMS
- Only called every 500ms (time-sliced in `runState()`)
- Returns current in Amperes

---

### `regulateLoop()` (Line 209–286)

The core regulation engine. Uses a **3-state FSM** (pulse-and-wait):

```
               ┌──────────────────────────────────────┐
               │                                      │
               ▼                                      │
         ┌──────────┐                                 │
         │SERVO_IDLE│ ──error > tol for 3s──▶ ┌──────────────┐
         └──────────┘                         │SERVO_SETTLING│
               ▲                              └──────────────┘
               │                                 │         │
        within tolerance                   still off?    within tol?
               │                                 │         │
               │                                 ▼         │
               │                          ┌──────────────┐ │
               └──────────────────────────│SERVO_PULSING │ │
                                          └──────────────┘ │
                                                           │
                                          reset & idle ◀───┘
```

**Step-by-step:**

1. **Safety check:** If `currentVin < MIN_VIN_V` (5V) → stop servo, return
2. **Calculate error:** `error = targetVoltage - currentVout`
3. **SERVO_IDLE:**
   - If `|error| > toleranceV`:
     - Start confirmation timer (if not started)
     - If timer ≥ `CONFIRM_DURATION_MS` → transition to `SERVO_SETTLING`
   - If `|error| ≤ toleranceV` → reset timer (within tolerance)
4. **SERVO_SETTLING:**
   - Stop servo, wait `SETTLE_DURATION_MS` (150ms)
   - Re-read error
   - If within tolerance → go to `SERVO_IDLE`, reset confirmation
   - If still off → calculate **proportional speed**:
     - `|error| > 30V` → speed = 200 (fast)
     - `|error| > 10V` → speed = 100 (medium)
     - `|error| ≤ 10V` → speed = 50 (slow/fine)
   - Set direction: `error > 0` → CW (+), `error < 0` → CCW (-)
   - Pulse servo → transition to `SERVO_PULSING`
5. **SERVO_PULSING:**
   - After `PULSE_DURATION_MS` (80ms) → stop, transition to `SERVO_SETTLING`

---

### `checkAlarms()` (Line 291–319)

Monitors for **stalled** conditions — the servo is pulsing but Vout isn't changing.

**Logic:**
1. If `|currentVout - lastVout| > 1.0` → Vout is changing, update timestamp
2. If servo is active AND Vout hasn't changed for `STALL_TIMEOUT` (3s):
   - `pulseDirection == +1` → was trying to increase → `REG_UNDER_VOLTAGE`
   - `pulseDirection == -1` → was trying to decrease → `REG_OVER_VOLTAGE`
   - Buzzer sounds, serial alarm message

---

### `checkAlarmRecovery()` (Line 321–338)

Auto-recovery from alarm states — called every loop when in alarm.

**Under Voltage Recovery:**
```
maxAchievable = currentVin × (250 / 220)
If maxAchievable >= targetVoltage → RECOVERED → REG_ACTIVE
```

**Over Voltage Recovery:**
```
If currentVout <= targetVoltage + toleranceV → RECOVERED → REG_ACTIVE
```

---

### `processTargetInput()` (Line 344–392)

Handles keypad input when on the `INPUT_TARGET` screen.

| Key | Action |
|:----|:-------|
| `0-9` | Append digit to `inputBuffer` (max 3 digits) |
| `*` | Remove last digit (backspace) |
| `A` | Confirm: validate 0–250, save to EEPROM, start regulation |
| `D` | Cancel: clear buffer, go back to CONFIGURE |

After each key, redraws the LCD with current input and positions the blinking cursor.

---

### `processToleranceInput()` (Line 397–441)

Same pattern as target input, but:
- Max 2 digits
- Valid range: 1–99
- Saves to `EEPROM_TOLERANCE_ADDR` on confirm
- Returns to CONFIGURE (not VIEW_STATUS)

---

### `keypadPress()` (Line 446–543)

Routes keypad input based on current `uiState`.

**Global handler:** `#` toggles LCD backlight (works from any screen, doesn't trigger buzzer).

**Per-state routing:** Each `uiState` has its own key handler within a `switch` block. Navigation follows this tree:

```
HOME
├─ A → CONFIGURE
│       ├─ A → INPUT_TARGET
│       │       ├─ A → Confirm → VIEW_STATUS
│       │       └─ D → Cancel → CONFIGURE
│       ├─ B → INPUT_TOLERANCE
│       │       ├─ A → Confirm → CONFIGURE
│       │       └─ D → Cancel → CONFIGURE
│       └─ D → HOME
└─ B → VIEW_STATUS
        ├─ A → VIEW_INPUT
        │       └─ D → VIEW_STATUS
        ├─ B → VIEW_OUTPUT
        │       └─ D → VIEW_STATUS
        └─ D → HOME
```

Every keypress triggers a **buzzer click** (100ms) for tactile feedback.

---

### `runState()` (Line 548–649)

The main tick function, called every `loop()` iteration. Divided into 4 sections:

#### Section 1: Hardware Ticks
- **Buzzer auto-off:** If buzzer has been on for ≥ 100ms → turn off

#### Section 2: Sensor Ticks (Non-blocking)
- **ADC MUX priming:** Dummy `analogRead()` before each sensor update to prevent cross-talk
- **Vin:** `vinSensor.update()` → `currentVin = vinSensor.getVoltage()`
- **Vout:** Raw reading → circular buffer → 10-sample average → `currentVout`
- **Current:** Time-sliced every 500ms (blocks ~20ms for one AC cycle)
- **Power:** `currentWatts = currentVout × currentAmps`

#### Section 3: Regulation Ticks
- If `REG_ACTIVE` → call `regulateLoop()` + `checkAlarms()`
- If `REG_UNDER_VOLTAGE` or `REG_OVER_VOLTAGE` → call `checkAlarmRecovery()`

#### Section 4: UI Ticks (throttled to every 500ms)
- **Serial debug** → `printDebug()` (10-column fixed-width output)
- **LCD updates** → only for live screens (`VIEW_INPUT` and `VIEW_OUTPUT`)
- Static menus (HOME, CONFIGURE, etc.) don't need refreshing

---

### `printDebug()` (Line 654–738)

Prints a fixed-width columnar debug line to Serial every 500ms.

**Format:**
```
Vin: 220V | Vout:  24V | V_avg:  25V | I: 300mA | POT: 342 | Tgt:  25V | Tol:+- 1V | Reg:ACTIVE  | Srv:IDLE   | Cfm:---
```

**Implementation details:**
- Uses `sprintf()` with `%4d` format specifiers for right-justified fixed-width numbers
- Current auto-switches between `A` and `mA` when below 1A
- Enum states are mapped to fixed-width strings with trailing spaces
- Confirmation timer shows `elapsed/total` (e.g., `1.5s/3.0s`) or `---` when inactive
- All string literals wrapped in `F()` macro to save SRAM (stored in flash)

---

## 🔄 Execution Flow

### Every `loop()` Iteration (~1ms)
```
loop()
 ├─ keypadPress()           ← scan keypad, route to handler
 └─ runState()
      ├─ [1] Buzzer auto-off check
      ├─ [2] Sensor reads
      │     ├─ Vin (non-blocking)
      │     ├─ Vout (non-blocking + moving average)
      │     └─ Current (every 500ms, blocks 20ms)
      ├─ [3] Regulation
      │     ├─ REG_ACTIVE → regulateLoop() + checkAlarms()
      │     └─ ALARM state → checkAlarmRecovery()
      └─ [4] UI (every 500ms)
            ├─ printDebug() → Serial
            └─ LCD update (VIEW_INPUT or VIEW_OUTPUT only)
```

### Timing Budget (per loop)
| Task | Frequency | Duration |
|:-----|:----------|:---------|
| Keypad scan | Every loop | ~50µs |
| ZMPT101B update (×2) | Every 250µs | ~100µs |
| ACS712 read | Every 500ms | ~20ms (blocking) |
| Regulation FSM | Every loop | ~10µs |
| Serial debug | Every 500ms | ~1ms (at 115200 baud) |
| LCD update | Every 500ms | ~2ms |
