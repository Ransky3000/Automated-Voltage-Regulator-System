#ifndef MACHINE_H
#define MACHINE_H

#include <Arduino.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>
#include <ZMPT101B_DRIVER.h>
#include <ACS712-driver.h>

// ─── Pin Assignments ─────────────────────────────────
#define SERVO_PIN       3
#define BUZZER_PIN      5
#define SERVO_POT_PIN   A2    // Kept for debug only (not used in control)
#define VIN_SENSOR_PIN  A3
#define VOUT_SENSOR_PIN A0
#define CSM_OUT_PIN     A1

// ─── Physical Limits ─────────────────────────────────
#define VARIAC_MAX_V    250
#define POT_HOME_VALUE  5    // POT reading at 0V physical stop (adjust after testing)

// ─── EEPROM ──────────────────────────────────────────
#define EEPROM_TOLERANCE_ADDR  0  // Tolerance stored at byte 0
#define EEPROM_TARGET_ADDR     1  // Target voltage stored at byte 1

// ─── Servo Pulse Timing ──────────────────────────────
#define PULSE_DURATION_MS   80
#define SETTLE_DURATION_MS  150
#define HOME_SPEED          1300  // Servo speed for homing (CCW, slow)

// ─── Regulation ──────────────────────────────────────
#define DEFAULT_TOLERANCE_V  2     // Default ±2V if user hasn't set one
#define STALL_TIMEOUT   3000  // ms without Vout change = stalled
#define MIN_VIN_V       5     // Minimum Vin to start regulating
#define VOUT_AVG_SAMPLES     10    // Moving average window size
#define CONFIRM_DURATION_MS  1000  // 3s confirmation before servo acts

// ─── ACS712 20A ──────────────────────────────────────
#define ACS712_SENSITIVITY  0.100f  // V/A for 20A model

// ─── ZMPT101B ────────────────────────────────────────
#define ZMPT_SENSITIVITY    500.0f
#define ZMPT_FREQUENCY      60.0f

// ─── UI State (what the LCD shows) ───────────────────
enum UIState {
  HOME,             // [A] Configure    / [B] View Status
  CONFIGURE,        // [A] Set Voltage  / [B] Set Tolerance
  INPUT_TARGET,     // Set target output / ___V
  INPUT_TOLERANCE,  // Set tolerance    / __V
  VIEW_STATUS,      // [A] Input  / [B] Output
  VIEW_INPUT,       // Voltage: <Vin>V
  VIEW_OUTPUT       // V:<Vout>  I:<A> / P:<W>
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
    ACS712 currentSensor;

    // ── State Tracking ──
    UIState uiState;
    RegulatorState regState;
    ServoState servoState;

    // ── Servo Control ──
    bool servoAttached;
    int pulseDirection;         // +1 = CW (increase), -1 = CCW (decrease)
    unsigned long servoTimer;

    // ── Regulation ──
    int targetVoltage;          // -1 = no target
    int toleranceV;             // User-configurable ± tolerance (default 2V)
    float currentVin;
    float currentVout;
    float rawVout;              // Pre-averaged Vout for debug
    float currentAmps;
    float currentWatts;

    // ── Stall Detection ──
    float lastVout;
    unsigned long lastVoutChangeTime;

    // ── Voltage Smoothing ──
    float voutSamples[VOUT_AVG_SAMPLES];
    byte voutSampleIdx;
    float smoothedVout;

    // ── Confirmation Window ──
    unsigned long outOfToleranceStart;
    bool isConfirmed;

    // ── UI ──
    String inputBuffer;
    byte isLineFull;
    bool backlightOn;

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
    void processToleranceInput(char key);

    // Servo helpers
    void servoStart(int speed);
    void servoStop();
    void homeToZero();
    
    // Control loop (Vout closed-loop)
    void regulateLoop();
    void checkAlarms();
    void checkAlarmRecovery();

    // Sensor
    float readCurrent();

    // Debug
    void printDebug();

  public:
    Machine(Keypad& kRef, LiquidCrystal_I2C& lcdRef);
    void Initialize();
    void runState();
    void keypadPress();
};

#endif
