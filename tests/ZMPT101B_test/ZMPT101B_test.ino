/**
 * This program shows you how to use the basics of this library.
*/

#include <ZMPT101B-driver.h>

#define SENSITIVITY 500.0f

// ZMPT101B sensor output connected to analog pin A0
// and the voltage source frequency is 50 Hz.
ZMPT101B voltageSensor(A0, 60.0);

void setup() {
  Serial.begin(115200);
  // Change the sensitivity value based on value you got from the calibrate
  // example.
  voltageSensor.setSensitivity(SENSITIVITY);
}

void loop() {
  
  // ----------------------------------------------------
  // OPTION 1: Non-Blocking (Recommended)
  // Keeps the loop running fast.
  // ----------------------------------------------------
  voltageSensor.update(); // Call this frequently!
  
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 300) {
    float voltage = voltageSensor.getVoltage();
    Serial.print("Non-Blocking: ");
    Serial.println(voltage);
    lastPrint = millis();
  }

  // ----------------------------------------------------
  // OPTION 2: Blocking (Simple)
  // Pauses code for ~20ms to measure.
  // Uncomment below to use:
  // ----------------------------------------------------
  /*
  float voltage = voltageSensor.getRmsVoltage();
  Serial.print("Blocking: ");
  Serial.println(voltage);
  delay(300);
  */
}