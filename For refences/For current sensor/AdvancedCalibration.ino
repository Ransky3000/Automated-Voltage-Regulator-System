/*
   -------------------------------------------------------------------------------------
   ACS712-driver
   Arduino library for ACS712 Current Sensor
   -------------------------------------------------------------------------------------
   
   This example file shows how to calibrate the sensor and optionally store the calibration
   value in EEPROM, and also how to change the value manually.
   Refined to match the style of common HX711 calibration sketches for familiarity.
*/

#include <ACS712-driver.h>
#include <EEPROM.h>

// Pins
ACS712 sensor(A0, 5.0, 1023);

const int calVal_eepromAdress_Zero = 0;
const int calVal_eepromAdress_Sens = 4;
unsigned long t = 0;

void setup() {
  Serial.begin(9600); delay(10);
  Serial.println();
  Serial.println("Starting...");

  // Default sensitivity for 5A model as a starting point
  sensor.setSensitivity(0.185); 
  
  // Attempt to load from EEPROM on startup?
  // Uncomment the next lines if you want auto-load on boot
  /*
  int zero; float sens;
  EEPROM.get(calVal_eepromAdress_Zero, zero);
  EEPROM.get(calVal_eepromAdress_Sens, sens);
  // Basic validation
  if(sens > 0.001) {
    sensor.setZeroPoint(zero);
    sensor.setSensitivity(sens);
  }
  */

  Serial.println("Startup is complete");
  Serial.println("Send 't' to Tare, 'r' to Calibrate, 'c' to Edit Manual.");
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 500; 

  // Check for new data/start next conversion:
  if (sensor.update()) newDataReady = true;

  // Get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = sensor.getAmps();
      Serial.print("Sensor output val: ");
      Serial.print(i, 4); // Show 4 decimal places
      Serial.println(" A");
      newDataReady = 0;
      t = millis();
    }
  }

  // Receive command from serial terminal
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    
    // Stop printing while we handle commands
    if (inByte == 't') {
        Serial.println("Taring...");
        sensor.calibrate(); 
        Serial.println("Tare complete. Press 'enter' to continue measuring.");
        
        while(Serial.available() > 0) Serial.read(); // Flush
        while(!Serial.available()); // Wait for user info
        while(Serial.available() > 0) Serial.read(); // Flush again
    }
    else if (inByte == 'r') calibrate(); 
    else if (inByte == 'c') changeSavedCalFactor(); 
    
    // Flush buffer
    while(Serial.available() > 0 && Serial.read() != '\n');
    t = millis(); // Reset timer so we don't print immediately
  }
}

void calibrate() {
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Ensure NO current is flowing through the sensor.");
  Serial.println("Send 't' from serial monitor to set the tare offset.");

  boolean _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') {
        sensor.calibrate();
        Serial.println("Tare complete");
        _resume = true;
      }
      // Flush
      while(Serial.available() > 0 && Serial.read() != '\n'); 
    }
  }

  Serial.println("Now, apply a KNOWN current (e.g. 1000.0 for 1A).");
  Serial.println("Then send the value of this current in mA (milliAmps) from serial monitor.");

  float known_current_mA = 0;
  _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0) {
      known_current_mA = Serial.parseFloat();
      if (known_current_mA != 0) {
        Serial.print("Known current is: ");
        Serial.print(known_current_mA);
        Serial.println(" mA");
        _resume = true;
      }
      while(Serial.available() > 0 && Serial.read() != '\n'); 
    }
  }

  // Calculate new sensitivity
  // We need to know what the voltage is reading NOW vs the Zero Point
  // We take a MASSIVE amount of samples to ensure the reference is rock solid
  long adcSum = 0;
  int sample_count = 1000;
  for(int i=0; i<sample_count; i++) {
      adcSum += analogRead(A0);
      delay(1); // Small delay to allow ADC charge and accumulation
  }
  float avgAdc = adcSum / (float)sample_count;
  
  // V = (ADC/1023)*5.0
  // V_zero = (Zero/1023)*5.0
  // Diff = V - V_zero
  // Sensitivity = Diff / Current (Amps)
  
  float voltage = (avgAdc / 1023.0) * 5.0;
  float zeroVolts = (sensor.getZeroPoint() / 1023.0) * 5.0;
  float diff = voltage - zeroVolts;
  
  // Convert mA to Amps for calculation
  float known_current_Amps = known_current_mA / 1000.0;
  
  float newSensitivity = diff / known_current_Amps;
  
  // Ensure positive
  if (newSensitivity < 0) newSensitivity = -newSensitivity;

  Serial.print("New sensitivity has been set to: ");
  Serial.print(newSensitivity, 4);
  Serial.println(" V/A");
  
  sensor.setSensitivity(newSensitivity);

  Serial.print("Save this value to EEPROM? y/n");

  _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
        float zero = sensor.getZeroPoint(); // Now float
        EEPROM.put(calVal_eepromAdress_Zero, zero);
        EEPROM.put(calVal_eepromAdress_Sens, newSensitivity);
        Serial.println("Values saved to EEPROM.");
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
      while(Serial.available() > 0 && Serial.read() != '\n'); 
    }
  }

  Serial.println("End calibration");
  Serial.println("***");
}

void changeSavedCalFactor() {
  float oldSensitivity = sensor.getSensitivity();
  boolean _resume = false;
  Serial.println("***");
  Serial.print("Current sensitivity is: ");
  Serial.println(oldSensitivity, 4);
  Serial.println("Now, send the new value from serial monitor, i.e. 0.185");
  
  float newSensitivity;
  while (_resume == false) {
    if (Serial.available() > 0) {
      newSensitivity = Serial.parseFloat();
      if (newSensitivity != 0) {
        Serial.print("New sensitivity is: ");
        Serial.println(newSensitivity, 4);
        sensor.setSensitivity(newSensitivity);
        _resume = true;
      }
      while(Serial.available() > 0 && Serial.read() != '\n'); 
    }
  }
  
  _resume = false;
  Serial.print("Save this value to EEPROM? y/n");
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
        float zero = sensor.getZeroPoint(); // float
        EEPROM.put(calVal_eepromAdress_Zero, zero);
        EEPROM.put(calVal_eepromAdress_Sens, newSensitivity);
        Serial.println("Values saved to EEPROM");
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
      while(Serial.available() > 0 && Serial.read() != '\n'); 
    }
  }
  Serial.println("End change value");
  Serial.println("***");
}
