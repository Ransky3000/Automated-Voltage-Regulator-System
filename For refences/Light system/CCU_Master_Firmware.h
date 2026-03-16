#ifndef CCU_MASTER_FIRMWARE_H
#define CCU_MASTER_FIRMWARE_H

#include <Arduino.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// Definition for hardware mapping
#define BUZZER_PIN A1
#define RELAY_1 D12
#define RELAY_2 D13
#define RELAY_3 A0

// State Machine Enums based on User Flow
enum SystemState {
  INIT,
  HOME,
  MENU_ROOM_A,
  MENU_ROOM_B,
  SET_TIMER_LUX,
  INPUT_TIMER,
  INPUT_THRESHOLD,
  MENU_STATUS_A,
  VIEW_TIME_LEFT,
  VIEW_SENSOR_A,
  VIEW_SENSOR_B,
  MANUAL_RELAY
};

// Struct to efficiently track the 6 remote sensors
struct SensorNode {
  unsigned int lux;
  unsigned long lastUpdateTime;
  bool isConnected;
  bool relayState;
};

class CCU_Master_Firmware {
  private:
    Keypad& k;
    LiquidCrystal_I2C& lcd;
    SoftwareSerial& rad; // HC-12 Radio Transceiver

    SystemState currentState;
    byte isLineFull;

    // Buzzer Feedback
    unsigned long buzzerOnMillis;
    bool buzzerActive;

    // System Timer & Logic
    bool isTimerActive;
    unsigned long timerDurationMillis;
    unsigned long timerStartTime;
    unsigned int roomA_lux_threshold;

    // Manual Relay Override
    char relayMode[3];    // 'A' = Auto, '0' = Force OFF, '1' = Force ON
    int selectedRelay;    // Which relay is being edited (0-2), -1 = none
    
    // UI Input Buffers
    String inputBuffer;

    // Sensor Tracking array (0-2 = Room A, 3-5 = Room B)
    SensorNode sensors[6]; 
    
    // RF Polling Control
    unsigned long lastRFPollTime;
    int currentTargetSensor; // Cycles 0 to 5

    // Internal Logic Helpers
    void processTimerInput(char key);
    void processThresholdInput(char key);
    void updateRelays();
    void pollSensorsRF();
    void checkSensorTimeouts();
    void formatTimeLCD(unsigned long timeLeftInMillis, byte col, byte row);
    void processRelayInput(char key);
    void drawRelayScreen();

  public:
    CCU_Master_Firmware(Keypad& kRef, LiquidCrystal_I2C& lcdRef, SoftwareSerial& radRef);

    void Initialize();
    void displayLCD(bool clear, byte col, byte line, String sentence);
    void displayLCD(byte col, byte line, String sentence);
    
    void runState();
    void keypadPress();
};

#endif
