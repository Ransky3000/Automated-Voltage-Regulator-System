# 🍫 Automated Cacao Processor

**Operating Instructions & User Manual**

## ⚙️ System Overview

This system is designed to process cocoa beans for a specific duration while monitoring weight in real-time. It features a precise **Load Cell** for mass monitoring, a **Timer-Based Control System** for the wash cycle, and a **Servo-Controlled Valve** with a manual "Jog" discharge mode.

### 🎮 Control Interface Map

| Key           | Function                     | Description                                                                             |
| :------------ | :--------------------------- | :-------------------------------------------------------------------------------------- |
| **0-9** | **Numeric Input** 🔢   | Used to enter Time (Hours, Minutes, Seconds) and Calibration weights.                   |
| **A**   | **ENTER / CONFIRM** ✅ | Confirms inputs, starts processes, tares the scale, or opens the Valve.                 |
| **B**   | **VALVE MENU** 🦾      | Opens the Manual Valve Control menu.                                                    |
| **C**   | **MANUAL JOG** ✊      | **Hold-to-Run:** Activates motor *only* when valve is open (Dead Man's Switch). |
| **D**   | **CANCEL / STOP** 🛑   | Closes valve, returns to menu, or triggers**Emergency Stop**.                     |
| **1**   | **Option Select** ⬆️ | Selects the top option in a menu.                                                       |
| **2**   | **Option Select** ⬇️ | Selects the bottom option in a menu.                                                    |

### 🚦 Status Indicators

* 🟢 **Green LED (Pin A1):** **STANDBY** - System is idle and ready for input.
* 🔴 **Red LED (Pin A0):** **ACTIVE** - Process is running; Relay is ON (Motor Active).
* 🔊 **Buzzer:** Provides audio feedback on key presses and alarms.

---

## 🚀 Start-Up Procedure

1. **Preparation:** Ensure the scale platform/bucket is **empty** before powering on.
2. **Power On:** Connect the system to the power source.
3. **Initialization:**
   * The LCD will display `System starting!`.
   * The Green LED will light up.
   * **Auto-Tare:** The system automatically sets the current weight to 0.0g.
   * **Servo Reset:** The valve automatically moves to the **CLOSED (0°)** position.
4. **Ready:** After approx. 3 seconds, the **Main Menu** will appear.

---

## 🕹️ Operation Modes

### ⏱️ Mode A: Timer Operation (Set Time)

*Use this mode to run the motor/pump for a specific duration.*

1. **Select Mode:** From the Main Menu, press **1** (`[1]Set Time`).
2. **Set Hours:** Screen shows `Set Hours:` — Type the hours (e.g., `1`) and press **A**.
3. **Set Minutes:** Screen shows `Set Mins:` — Type the minutes (e.g., `30`) and press **A**.
4. **Set Seconds:** Screen shows `Set Secs:` — Type the seconds (e.g., `0`) and press **A**.
5. **Confirm Start:**
   * The screen shows a preview: `Begin process? / 01h 30m 00s`.
   * Press **A** to **START** (or press **D** to cancel).
6. **Running Process:**
   * **Countdown:** System counts `3... 2... 1...`.
   * **Action:** Relay turns **ON** (Motor Starts), Red LED turns **ON**.
   * **Display:**
     * Line 1: `Time` (counting down)
     * Line 2: `Mass` (live weight monitoring)
7. **Completion:**
   * When the timer hits `00:00:00`, the Relay and Motor cut off **instantly**.
   * Screen displays `Time Reached! / Done..`.
   * After 2 seconds, the system returns to the Main Menu.

### 🦾 Mode B: Manual Valve & Discharge (Servo + Jog)

*Use this feature to manually open the discharge valve and pulse the motor for cleaning/unclogging.*

1. **Access:** From the Main Menu, press **B**.
2. **Prompt:** Screen displays `Toggle Valve? / A:Yes D:Back`.
3. **Activate Mode:** Press **A** to Open Valve.
   * **Servo Action:** Valve moves to **OPEN (90°)**. 🔓
   * **Screen Update:** Displays `Valve OPEN! / Hold C-Run D-Esc`.
4. **Discharge Operation (Dead Man's Switch):**
   * **HOLD 'C':** Relay turns **ON** (Motor runs) while key is held. ⚡
   * **RELEASE 'C':** Relay turns **OFF** (Motor stops) immediately.
5. **Exit:** Press **D**.
   * **Servo Action:** Valve moves to **CLOSED (0°)**. 🔒
   * System returns to Main Menu.

### ⚖️ Mode C: Scale Maintenance (Check Scale)

*Use this mode to calibrate or zero the scale.*

1. **Select Mode:** From the Main Menu, press **2** (`[2] Check scale`).
2. **Select Option:**
   * Press **1** for `[Calibrate]`: Recalibrate the sensor using a known weight.
   * Press **2** for `[Set zero]`: Quickly reset the scale to 0.0g (Tare).

---

## 🛡️ Safety Features

### 🔒 Valve Interlock Logic

The system prevents the motor/pump from running in Manual Mode unless the discharge valve is explicitly **OPEN**. This prevents pressure buildup or mechanical stress from running the pump against a closed valve.

### 🛑 Dead Man's Switch

In Manual Mode (Mode B), the motor only runs while the operator physically holds the **'C'** key. If the operator releases the key or walks away, the machine stops instantly.

### ⚡ Emergency Stop

During any automated process (Mode A), pressing **D** triggers an immediate hard stop:

* Relay cuts power.
* Red LED turns off.
* Process state resets.

---

## 💡 Technical Specifications

*(For Engineering Reference)*

* **Processor:** Arduino Uno / ATmega328P
* **Motor Control:** Relay on **Pin D2** (Active HIGH)
* **Valve Control:** Servo Motor on **Pin A3** (PWM Signal)
* **Load Cell:** HX711 Interface on **Pins D4 (DT) & D5 (SCK)**
* **Safety Logic:** State Machine implementation restricts Relay activation to specific safe states (`RUN_PROCESS` or `VALVE_OPEN_STATE`).
