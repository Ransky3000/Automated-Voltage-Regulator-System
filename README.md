# Automated Voltage Regulator System

This is the main application firmware for the Automated Voltage Regulator System.

## Dependencies

This project requires the following libraries:
- **ZMPT101B_DRIVER**: Custom library for AC Voltage sensing.
  - Make sure this library is installed in your Arduino `libraries` folder.

## Directory Structure

- `src/`: Core Application Logic.
    - `Machine.h` / `Machine.cpp`: State Machine implementation.
- `tests/`: Project sketches.
    - `__main__/`: The primary application.
    - `ZMPT101B_test/`: Integration test using the external driver library.
- `docs/`: Documentation and simulation files.

## Usage

1. Open `tests/__main__/__main__.ino` in Arduino IDE.
2. Select your board and port.
3. Upload to the device.

## Development

- Implement core system logic in `src/Machine.cpp`.
- Use `tests` to verify subsystems.

