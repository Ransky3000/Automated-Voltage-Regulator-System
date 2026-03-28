#include "Machine.h"
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>
#include <ZMPT101B_DRIVER.h>

// ─── Keypad Configuration ────────────────────────────
#define ROWS 4
#define COLS 4

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {6, 7, 8, 9};     // Keypad Rows
byte colPins[COLS] = {10, 11, 12, 13};  // Keypad Cols

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ─── LCD Configuration ───────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── Machine Object ──────────────────────────────────
Machine machine(keypad, lcd);

void setup() {
  Serial.begin(9600);
  Serial.println("--- AVR System Boot ---");
  machine.Initialize();
}

void loop() {
  machine.keypadPress();
  machine.runState();
}
