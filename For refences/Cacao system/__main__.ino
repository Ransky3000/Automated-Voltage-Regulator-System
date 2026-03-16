#include "Machine.h"
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711_ADC.h>
#include <EEPROM.h>
#include <Servo.h>


// --- System Configuration ---
// Keypad definitions
#define ROWS 4
#define COLS 4

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'.','0','#','D'}
};
// byte rowPins[ROWS] = {13, 12, 11, 10}; // Connect to the row pins of the keypad
// byte colPins[COLS] = {6 , 7 , 8 , 9}; // Connect to the column pins of the keypad

byte rowPins[ROWS] = {6 , 7 , 8 , 9}; 
byte colPins[COLS] = {10, 11, 12, 13}; 

// LCD definitions
#define lcdColumns 16
#define lcdRows 2
#define lcdAddress 0x27   // Common I2C address, change if needed

// HX711 definitions  
#define HX711_dout 4    // MCU > HX711 dout pin
#define HX711_sck 5     // MCU > HX711 sck pin 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(lcdAddress, lcdColumns, lcdRows);
HX711_ADC LoadCell = HX711_ADC(HX711_dout, HX711_sck);


Machine machine(keypad, lcd, LoadCell);
void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(57600);
  Serial.println("System starting...");

  machine.Initialize();
}

void loop() {
  machine.keypadPress();
  if(machine.getRelay()){
    digitalWrite(2,HIGH);
    //LED indicator
    digitalWrite(A0, HIGH);
    digitalWrite(A1,LOW);
  }else{
    //relay
    digitalWrite(2,LOW);
    //LED indicator
    digitalWrite(A0, LOW);
    digitalWrite(A1,HIGH);
  }
  machine.runState();
}