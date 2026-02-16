#include <ZMPT101B_DRIVER.h>

// Sensor on Pin Ax, Frequency 60Hz
ZMPT101B voltageSensor(A2, 50.0);

void setup() {
  Serial.begin(115200);
  
  // Set sensitivity based on your calibration (e.g., 500.0)
  voltageSensor.setSensitivity(500.0f);
  
  Serial.println("Non-Blocking ZMPT101B Test Started");
}

void loop() {
  // 1. CRITICAL: Keep the driver running!
  // Call this as frequently as possible for accurate sampling.
  voltageSensor.update();

  // 2. Read the value periodically (without blocking)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 300) {
    float voltage = voltageSensor.getVoltage();
    Serial.print("Voltage: ");
    Serial.println(voltage);
    lastPrint = millis();
  }
  
  // You can do other things here without pausing the sensor!
}
