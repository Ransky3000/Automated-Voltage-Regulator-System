#ifndef MACHINE_H
#define MACHINE_H

#include <Arduino.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>
#include <ZMPT101B_DRIVER.h>

// ─── Pin Assignments ─────────────────────────────────
#define SERVO_PIN       3
#define BUZZER_PIN      5
#define SERVO_POT_PIN   A2
#define VIN_SENSOR_PIN  A3
#define VOUT_SENSOR_PIN A0
#define CSM_OUT_PIN     A1

// ─── Physical Limits ─────────────────────────────────
#define POT_MIN         10
#define POT_MAX         670
#define VARIAC_MAX_V    250

// ─── EEPROM Calibration Constants ────────────────────
#define CAL_STEP_V      5
#define CAL_POINTS      51    // (250 / 5) + 1
#define EEPROM_MAGIC    0xCA

// ─── Servo Pulse Timing ──────────────────────────────
#define PULSE_DURATION_MS   80
#define SETTLE_DURATION_MS  150

// ─── Regulation ──────────────────────────────────────
#define DEADBAND_V      2
#define STALL_TIMEOUT   3000  // ms without Vout change = stalled

// ─── ACS712 20A ──────────────────────────────────────
// Sensitivity: 100 mV/A, Zero-current voltage: VCC/2 (2.5V)
#define ACS712_SENSITIVITY  0.100f  // V per Amp
#define ACS712_ZERO_POINT   2.5f    // Volts at 0A

// ─── ZMPT101B ────────────────────────────────────────
#define ZMPT_SENSITIVITY    500.0f
#define ZMPT_FREQUENCY      60.0f

// ─── UI State (what the LCD shows) ───────────────────
enum UIState {
  HOME,           // [A] Set Voltage  / [B] View Status
  INPUT_TARGET,   // Set target output / ___V
  VIEW_STATUS,    // [A] Input  / [B] Output
  VIEW_INPUT,     // Voltage: <Vin>V
  VIEW_OUTPUT     // V:<Vout>  I:<A> / P:<W>
};

// ─── Regulator State (what the servo is doing) ───────
enum RegulatorState {
  REG_IDLE,           // No target set yet
  REG_ACTIVE,         // Actively regulating
  REG_UNDER_VOLTAGE,  // Variac maxed out, Vin too low
  REG_OVER_VOLTAGE    // Variac at minimum, Vin too high
};

// ─── Servo State (pulse-and-wait FSM) ────────────────
enum ServoState {
  SERVO_IDLE,
  SERVO_PULSING,
  SERVO_SETTLING
};

class Machine {
  private:
    // Hardware References
    Keypad& k;
    LiquidCrystal_I2C& lcd;
    Servo variacServo;
    ZMPT101B vinSensor;
    ZMPT101B voutSensor;

    // ── State Tracking ──
    UIState uiState;
    RegulatorState regState;
    ServoState servoState;

    // ── Servo Control ──
    bool servoAttached;
    int pulseDirection;         // +1 = CW (increase), -1 = CCW (decrease)
    unsigned long servoTimer;
    uint16_t calPotValues[CAL_POINTS];
    bool hasCalibration;

    // ── Regulation ──
    int targetVoltage;          // -1 = no target
    float currentVin;
    float currentVout;
    float currentAmps;
    float currentWatts;

    // ── Stall Detection ──
    float lastVout;
    unsigned long lastVoutChangeTime;

    // ── UI ──
    String inputBuffer;
    byte isLineFull;

    // ── Buzzer ──
    unsigned long buzzerOnMillis;
    bool buzzerActive;

    // ── LCD Throttle ──
    unsigned long lastLCDTick;

    // ── Private Helpers ──
    // LCD
    void displayLCD(bool clear, byte col, byte line, String text);
    void displayLCD(byte col, byte line, String text);
    
    // Input processing
    void processTargetInput(char key);

    // Servo helpers
    void servoStart(int speed);
    void servoStop();
    
    // Control loop
    void regulateLoop();
    void checkAlarms();
    void checkAlarmRecovery();
    
    // Calibration
    int potToVoltage(int potVal);
    void loadCalibration();

    // Sensor
    float readCurrent();

  public:
    Machine(Keypad& kRef, LiquidCrystal_I2C& lcdRef);
    void Initialize();
    void runState();
    void keypadPress();
};

#endif
