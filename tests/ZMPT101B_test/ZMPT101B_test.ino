#include <ZMPT101B-driver.h>

ZMPT101B sensor(A0);

unsigned long lastPrint = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Calibrating Zero Point...");
  int zero = sensor.calibrateZeroPoint();
  Serial.print("Zero Point: ");
  Serial.println(zero);
  
  // calibrate this against a multimeter
  sensor.setSensitivity(1.0); 
}

void loop() {
  // MUST call this frequently for non-blocking sampling
  sensor.update();

  // Print results every 500ms without blocking
  if (millis() - lastPrint > 500) {
    float voltage = sensor.getVoltageAC();
    
    // For Serial Plotter:
    Serial.print("Voltage:");
    Serial.println(voltage);
    
    lastPrint = millis();
  }
}
