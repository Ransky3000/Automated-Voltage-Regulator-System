# Automated Voltage Regulator System

This is an Arduino Library for the Automated Voltage Regulator System project.

## Directory Structure

- `src/`: Shared library source code (.h and .cpp files).
- `tests/`: Independent sketches that use the library.
    - `__main__/`: The primary application sketch.
    - `ZMPT101B_test/`: Test sketch for the ZMPT101B sensor.
- `docs/`: Project documentation and simulation files.

## ZMPT101B Driver Usage

The library includes a hybrid ZMPT101B AC Voltage Sensor driver.

### 1. Blocking Mode (Simpler)
Use `getRmsVoltage()` to get a reading. Note that this pauses execution for ~17ms.
```cpp
ZMPT101B voltageSensor(A0, 60.0);
float voltage = voltageSensor.getRmsVoltage();
```

### 2. Non-Blocking Mode (High Performance)
Use `update()` in `loop()` to sample in the background (4kHz rate).
```cpp
ZMPT101B voltageSensor(A0, 60.0);

void loop() {
    voltageSensor.update(); // Call frequently!
    
    // Read anytime without delay
    float voltage = voltageSensor.getVoltage();
}
```

## Development

- Add reusable logic to `src/`.
- Create new test sketches in `tests/` to verify components.
