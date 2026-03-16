#include "CCU_Master_Firmware.h"
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// --- System Configuration & Pin Maps ---

// 1. Keypad Definitions (Pins D4-D11)
#define ROWS 4
#define COLS 4

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {4, 5, 6, 7}; // Per your Hardware Map
byte colPins[COLS] = {8, 9, 10, 11}; // Per your Hardware Map

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// 2. LCD Definitions (Hardware I2C -> A4, A5)
#define lcdColumns 16
#define lcdRows 2
#define lcdAddress 0x27

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(lcdAddress, lcdColumns, lcdRows);

// 3. HC-12 Radio Definitions (Pins D2, D3)
const int hcRxPin = 2; // Connect to HC-12 TX
const int hcTxPin = 3; // Connect to HC-12 RX

SoftwareSerial hcSerial(hcRxPin, hcTxPin);

// --- Object Instantiation ---
// Inject all hardware dependencies down into the CCU Firmware Logic
CCU_Master_Firmware ccu(keypad, lcd, hcSerial);

void setup() {
  // Initialize PC Hardware Serial Console for Debugging (D0, D1)
  Serial.begin(9600);
  Serial.println("\n--- Master CCU Boot Sequence ---");
  
  // Pass execution control to the Object State Machine
  ccu.Initialize();
}

void loop() {
  ccu.keypadPress();
  ccu.runState();
}
