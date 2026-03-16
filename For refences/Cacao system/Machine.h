#ifndef MACHINE_H
#define MACHINE_H

#include <Arduino.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711_ADC.h>
#include <EEPROM.h>
#include <Servo.h> 

enum SystemState {
  MENU,
  SERVO_MENU,
  CHECK_SCALE,
  CALIBRATE,
  CALIBRATE_KNOWN_MASS,
  SAVE_CAL,
  SET_ZERO,
  INPUT_HOURS,
  INPUT_MINS,
  INPUT_SECS,
  BEGIN_PROCESS,
  RUN_PROCESS,
  STOP_COUNTDOWN,
  CANCEL_PROCESS,
  VALVE_OPEN_STATE
};

class Machine{
  private:
    Keypad k;
    LiquidCrystal_I2C lcd;
    HX711_ADC LoadCell;
    Servo valveServo; 

    //State machine related
    SystemState currentState;

    // Relay flag
    bool isRelay;

    //LoadCell related
    float calibrationFactor;
    float newCal;
    unsigned long t;
    byte countDown;
    
    //Time Process related
    long inputHours;
    long inputMins;
    long inputSecs;
    unsigned long targetDuration; 
    unsigned long processStartTime;
    unsigned long stopTimer; // <--- FIX: Moved Timer to Class Level
    bool isServoOpen; 

    //Keypad related
    String inputBuffer;
    byte isLineFull;

    //Buzzer 
    unsigned long buzzerOnMillis;
    bool buzzerActive;


  public:
    Machine();
    Machine(Keypad &k, LiquidCrystal_I2C &lcd, HX711_ADC &LoadCell);

    void Initialize();
    void displayMainMenu();
    void displayScaleOptions();
    void runState();
    bool getRelay();
    void offRelay();
    void displayLCD(bool clear, byte col, byte line, String sentence);
    void displayLCD(byte col, byte line, String sentence);
    void formatTimeLCD(unsigned long timeLeft); 
    void keypadPress();
    void showScale();
    void newCalibration();
    void toggleServo();

};

#endif