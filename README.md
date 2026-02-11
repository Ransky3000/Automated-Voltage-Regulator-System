# Automated Voltage Regulator System

This is an Arduino Library for the Automated Voltage Regulator System project.

## Directory Structure

- `src/`: Shared library source code (.h and .cpp files).
- `tests/`: Independent sketches that use the library.
    - `__main__/`: The primary application sketch.
    - `ZMPT101B_test/`: Test sketch for the ZMPT101B sensor.
- `docs/`: Project documentation and simulation files.

## Usage

This project is structured as an Arduino Library. To use it:
1. Ensure this folder is located in your Arduino `libraries` folder.
2. Restart the Arduino IDE.
3. Open sketches from the `tests/` directory (e.g., `tests/__main__/__main__.ino`).
4. You can include shared code in your sketches using `#include <YourHeader.h>`.

## Development

- Add reusable logic to `src/`.
- Create new test sketches in `tests/` to verify components.
