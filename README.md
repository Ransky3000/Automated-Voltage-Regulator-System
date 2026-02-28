# ⚡ Automated Voltage Regulator System

**Project Overview & Developer Guide**

## ⚙️ System Overview

This system is designed to automatically regulate AC voltage output using a ZMPT101B voltage sensor for real-time monitoring and a relay-based tap-changing mechanism for correction. The firmware follows a **State Machine** architecture, consistent with our previous project ([Automated Cacao Processor](Projects_we_made/Automated-Cacao-Processor/)).

## 📦 Dependencies

This project requires the following external libraries installed in your Arduino `libraries/` folder:

| Library | Purpose |
| :------ | :------ |
| **ZMPT101B_DRIVER** | Custom AC Voltage sensing (Blocking & Non-Blocking modes) |

## 📁 Directory Structure

```
Automated-Voltage-Regulator-System/
├── __main__/                       ← 🎯 Main application sketch
│   └── __main__.ino
├── src/                            ← 🧠 Core application logic
│   ├── Machine.h                   ← State Machine class definition
│   └── Machine.cpp                 ← State Machine implementation
├── tests/                          ← 🧪 Test & calibration sketches
│   ├── ZMPT101B_test/              ← Blocking voltage sensor test
│   └── Non-blocking_test/          ← Non-blocking voltage sensor test
├── docs/                           ← 📄 Documentation & simulations
│   ├── Datasheet/                  ← Component datasheets
│   ├── ZMPT101B_sensor_Simulation/ ← ZMPT101B Proteus simulation
│   └── Automated-Voltage-Regulator-System_Simulation/
│       └── *.PDF                   ← Circuit schematics (exported)
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
| :----- | :------ |
| `tests/ZMPT101B_test/` | Verify voltage readings using **blocking** mode |
| `tests/Non-blocking_test/` | Verify voltage readings using **non-blocking** mode (4kHz sampling) |

## 🛠️ Development

- Implement core regulation logic in `src/Machine.cpp`.
- Define system states in the `enum SystemState` inside `src/Machine.h`.
- Keep `__main__.ino` minimal — hardware wiring + `machine.runState()` calls only.
- Create new test sketches in `tests/` to verify subsystems.

## 💡 Technical Specifications

*(For Engineering Reference)*

* **Processor:** Arduino Uno / ATmega328P
* **Voltage Sensor:** ZMPT101B (AC Voltage Transformer)
* **Architecture:** State Machine (`Machine` class)
