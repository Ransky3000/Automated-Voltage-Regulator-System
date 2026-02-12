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
  Serial.println("Blocking ZMPT101B Test Started");
}

void loop() {
  // Blocking Mode (Standard)
  // This function will pause execution for ~1 cycle (16.7ms at 60Hz) to read the RMS voltage.
  float voltage = voltageSensor.getRmsVoltage();
  
  Serial.println(voltage);
  delay(300);
}