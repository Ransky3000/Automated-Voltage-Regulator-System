#include "Machine.h"

// ═══════════════════════════════════════════════════════
//  Constructor
// ═══════════════════════════════════════════════════════
Machine::Machine(Keypad& kRef, LiquidCrystal_I2C& lcdRef)
  : k(kRef), lcd(lcdRef),
    vinSensor(VIN_SENSOR_PIN, ZMPT_FREQUENCY),
    voutSensor(VOUT_SENSOR_PIN, ZMPT_FREQUENCY),
    currentSensor(CSM_OUT_PIN, 5.0, 1023)
{
  uiState = HOME;
  regState = REG_IDLE;
  servoState = SERVO_IDLE;

  servoAttached = false;
  pulseDirection = 0;
  servoTimer = 0;

  targetVoltage = -1;
  toleranceV = DEFAULT_TOLERANCE_V;
  currentVin = 0;
  currentVout = 0;
  currentAmps = 0;
  currentWatts = 0;

  lastVout = 0;
  lastVoutChangeTime = 0;

  inputBuffer = "";
  isLineFull = 0;
  backlightOn = true;

  buzzerOnMillis = 0;
  buzzerActive = false;
  lastLCDTick = 0;
}

// ═══════════════════════════════════════════════════════
//  Initialize
// ═══════════════════════════════════════════════════════
void Machine::Initialize() {
  // Pin setup
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(SERVO_POT_PIN, INPUT);

  // LCD
  lcd.init();
  lcd.backlight();
  displayLCD(true, 0, 0, "System");
  displayLCD(false, 0, 1, "starting!");

  // ZMPT101B Sensors
  vinSensor.setSensitivity(ZMPT_SENSITIVITY);
  voutSensor.setSensitivity(ZMPT_SENSITIVITY);

  // ACS712 Current Sensor
  currentSensor.setSensitivity(ACS712_SENSITIVITY);
  currentSensor.calibrate();  // Auto-zero with no load
  Serial.println(F("ACS712 calibrated"));

  // Load saved tolerance from EEPROM
  byte savedTol = EEPROM.read(EEPROM_TOLERANCE_ADDR);
  if (savedTol >= 1 && savedTol <= 99) {
    toleranceV = savedTol;
    Serial.print(F("Loaded tolerance: "));
    Serial.println(toleranceV);
  } else {
    toleranceV = DEFAULT_TOLERANCE_V;
    Serial.println(F("No saved tolerance. Default: 2V"));
  }

  // Home servo to 0V position
  homeToZero();

  delay(1000);

  // Show Home Screen
  uiState = HOME;
  displayLCD(true, 0, 0, "[A] Configure");
  displayLCD(false, 0, 1, "[B] View Status");
}

// ═══════════════════════════════════════════════════════
//  LCD Display Helpers
// ═══════════════════════════════════════════════════════
void Machine::displayLCD(bool clear, byte col, byte line, String text) {
  if (clear) {
    lcd.clear();
    isLineFull = 0;
  }
  lcd.setCursor(col, line);
  lcd.print(text);
  isLineFull++;
}

void Machine::displayLCD(byte col, byte line, String text) {
  if (isLineFull >= 2) {
    lcd.clear();
    isLineFull = 0;
  }
  lcd.setCursor(col, line);
  lcd.print(text);
  isLineFull++;
}

// ═══════════════════════════════════════════════════════
//  Servo Helpers (Ported from ServoAutoPosition_test)
// ═══════════════════════════════════════════════════════
void Machine::servoStart(int speed) {
  if (!servoAttached) {
    variacServo.attach(SERVO_PIN);
    servoAttached = true;
  }
  variacServo.writeMicroseconds(speed);
}

void Machine::servoStop() {
  if (servoAttached) {
    variacServo.writeMicroseconds(1500);
    delay(5);
    variacServo.detach();
    servoAttached = false;
  }
}

// ═══════════════════════════════════════════════════════
//  Home to 0V using POT feedback
// ═══════════════════════════════════════════════════════
void Machine::homeToZero() {
  int potVal = analogRead(SERVO_POT_PIN);

  // Already at 0V position?
  if (potVal <= POT_HOME_VALUE) {
    Serial.println(F("Already at 0V position."));
    return;
  }

  // Show homing status on LCD
  displayLCD(true, 0, 0, "Homing to 0V...");
  displayLCD(false, 0, 1, "Please wait");
  Serial.println(F("Homing servo to 0V..."));

  // Spin CCW slowly until POT reaches 0V stop
  servoStart(HOME_SPEED);

  unsigned long homeStart = millis();
  const unsigned long HOME_TIMEOUT = 15000; // 15s max homing time

  while (true) {
    potVal = analogRead(SERVO_POT_PIN);

    // Reached 0V position
    if (potVal <= POT_HOME_VALUE) {
      servoStop();
      Serial.print(F("Homed! POT: "));
      Serial.println(potVal);
      displayLCD(true, 0, 0, "Homed to 0V");
      delay(500);
      return;
    }

    // Timeout safety
    if (millis() - homeStart > HOME_TIMEOUT) {
      servoStop();
      Serial.println(F("HOME TIMEOUT! Could not reach 0V."));
      displayLCD(true, 0, 0, "Home failed!");
      displayLCD(false, 0, 1, "Check servo");
      delay(2000);
      return;
    }
  }
}

// ═══════════════════════════════════════════════════════
//  Current Sensor (ACS712 20A via ACS712-driver library)
// ═══════════════════════════════════════════════════════
float Machine::readCurrent() {
  // Time-sliced AC reading: blocks ~20ms but only called every 500ms
  return currentSensor.readCurrentAC(60);
}

// ═══════════════════════════════════════════════════════
//  Regulation Loop (Vout Closed-Loop, Pulse-and-Wait)
// ═══════════════════════════════════════════════════════
void Machine::regulateLoop() {
  // Skip regulation if no AC input
  if (currentVin < MIN_VIN_V) {
    servoStop();
    servoState = SERVO_IDLE;
    return;
  }

  int error = targetVoltage - (int)currentVout;
  int absError = abs(error);

  switch (servoState) {

    case SERVO_IDLE:
      if (absError > toleranceV) {
        // Vout drifted outside tolerance — need to move
        servoState = SERVO_SETTLING;
        servoTimer = millis();
      }
      break;

    case SERVO_SETTLING:
      servoStop();
      if (millis() - servoTimer >= SETTLE_DURATION_MS) {
        // Re-read error after settling
        error = targetVoltage - (int)currentVout;
        absError = abs(error);

        if (absError <= toleranceV) {
          // Within tolerance — stay idle
          servoStop();
          servoState = SERVO_IDLE;
        } else {
          // Calculate proportional speed based on Vout error
          int speed;
          if (absError > 30) speed = 200;
          else if (absError > 10) speed = 100;
          else speed = 50;

          int servoCmd;
          if (error > 0) {
            servoCmd = 1500 + speed;
            pulseDirection = 1;   // CW = increase voltage
          } else {
            servoCmd = 1500 - speed;
            pulseDirection = -1;  // CCW = decrease voltage
          }

          servoStart(servoCmd);
          servoState = SERVO_PULSING;
          servoTimer = millis();
        }
      }
      break;

    case SERVO_PULSING:
      // Pulse timer expired — stop and settle
      if (millis() - servoTimer >= PULSE_DURATION_MS) {
        servoStop();
        servoState = SERVO_SETTLING;
        servoTimer = millis();
      }
      break;
  }
}

// ═══════════════════════════════════════════════════════
//  Alarm Detection & Recovery
// ═══════════════════════════════════════════════════════
void Machine::checkAlarms() {
  // Track if Vout is changing
  if (abs(currentVout - lastVout) > 1.0) {
    lastVout = currentVout;
    lastVoutChangeTime = millis();
  }

  // If servo has been trying but Vout hasn't changed for STALL_TIMEOUT
  if (servoState != SERVO_IDLE && (millis() - lastVoutChangeTime > STALL_TIMEOUT)) {
    servoStop();
    servoState = SERVO_IDLE;

    if (pulseDirection == 1) {
      // Was trying to increase → hit MAX → under voltage
      regState = REG_UNDER_VOLTAGE;
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerOnMillis = millis();
      buzzerActive = true;
      Serial.println(F("ALARM: UNDER_VOLTAGE - Variac maxed out"));
    } else if (pulseDirection == -1) {
      // Was trying to decrease → hit MIN → over voltage
      regState = REG_OVER_VOLTAGE;
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerOnMillis = millis();
      buzzerActive = true;
      Serial.println(F("ALARM: OVER_VOLTAGE - Variac at minimum"));
    }
  }
}

void Machine::checkAlarmRecovery() {
  if (regState == REG_UNDER_VOLTAGE) {
    // Check if Vin has improved enough
    // Max achievable Vout = Vin * (250 / 220)
    float maxAchievable = currentVin * (250.0 / 220.0);
    if (maxAchievable >= targetVoltage) {
      regState = REG_ACTIVE;
      lastVoutChangeTime = millis(); // Reset stall timer
      Serial.println(F("RECOVERED from UNDER_VOLTAGE"));
    }
  } else if (regState == REG_OVER_VOLTAGE) {
    // Over voltage recovery: if currentVout dropped back into range
    if (currentVout <= targetVoltage + toleranceV) {
      regState = REG_ACTIVE;
      lastVoutChangeTime = millis();
      Serial.println(F("RECOVERED from OVER_VOLTAGE"));
    }
  }
}

// ═══════════════════════════════════════════════════════
//  Input Target Processing
// ═══════════════════════════════════════════════════════
void Machine::processTargetInput(char key) {
  if (isDigit(key) && inputBuffer.length() < 3) {
    // Max 3 digits (0-250)
    inputBuffer += key;
  } else if (key == '*' && inputBuffer.length() > 0) {
    // Backspace
    inputBuffer.remove(inputBuffer.length() - 1);
  } else if (key == 'A' && inputBuffer.length() > 0) {
    // Confirm
    lcd.noBlink();
    int val = inputBuffer.toInt();
    if (val >= 0 && val <= 250) {
      targetVoltage = val;
      regState = REG_ACTIVE;
      servoState = SERVO_SETTLING;
      servoTimer = millis();
      lastVoutChangeTime = millis();

      inputBuffer = "";
      uiState = VIEW_STATUS;
      displayLCD(true, 0, 0, "[A] Input");
      displayLCD(false, 0, 1, "[B] Output");
      return;
    } else {
      displayLCD(true, 0, 0, "Invalid! 0-250");
      delay(1000);
      inputBuffer = "";
    }
  } else if (key == 'D') {
    // Cancel
    lcd.noBlink();
    inputBuffer = "";
    uiState = CONFIGURE;
    displayLCD(true, 0, 0, "[A] Set Voltage");
    displayLCD(false, 0, 1, "[B] Tolerance");
    return;
  }

  // Redraw: show typed digits, pad with spaces, position blink cursor
  String display = inputBuffer;
  while (display.length() < 16) display += " ";

  lcd.setCursor(0, 0);
  lcd.print("Set target      ");
  lcd.setCursor(0, 1);
  lcd.print(display);
  lcd.setCursor(inputBuffer.length(), 1);
}

// ═══════════════════════════════════════════════════════
//  Input Tolerance Processing
// ═══════════════════════════════════════════════════════
void Machine::processToleranceInput(char key) {
  if (isDigit(key) && inputBuffer.length() < 2) {
    // Max 2 digits (1-99)
    inputBuffer += key;
  } else if (key == '*' && inputBuffer.length() > 0) {
    // Backspace
    inputBuffer.remove(inputBuffer.length() - 1);
  } else if (key == 'A' && inputBuffer.length() > 0) {
    // Confirm
    lcd.noBlink();
    int val = inputBuffer.toInt();
    if (val >= 1 && val <= 99) {
      toleranceV = val;
      EEPROM.write(EEPROM_TOLERANCE_ADDR, (byte)toleranceV);

      inputBuffer = "";
      uiState = CONFIGURE;
      displayLCD(true, 0, 0, "[A] Set Voltage");
      displayLCD(false, 0, 1, "[B] Tolerance");
      return;
    } else {
      displayLCD(true, 0, 0, "Invalid! 1-99");
      delay(1000);
      inputBuffer = "";
    }
  } else if (key == 'D') {
    // Cancel
    lcd.noBlink();
    inputBuffer = "";
    uiState = CONFIGURE;
    displayLCD(true, 0, 0, "[A] Set Voltage");
    displayLCD(false, 0, 1, "[B] Tolerance");
    return;
  }

  // Redraw: show typed digits, pad with spaces, position blink cursor
  String display = inputBuffer;
  while (display.length() < 16) display += " ";

  lcd.setCursor(0, 0);
  lcd.print("Set tolerance   ");
  lcd.setCursor(0, 1);
  lcd.print(display);
  lcd.setCursor(inputBuffer.length(), 1);
}

// ═══════════════════════════════════════════════════════
//  Keypad Press Handler
// ═══════════════════════════════════════════════════════
void Machine::keypadPress() {
  char key = k.getKey();

  if (key) {
    // '#' toggles backlight globally (works from any screen)
    if (key == '#') {
      backlightOn = !backlightOn;
      if (backlightOn) lcd.backlight();
      else lcd.noBacklight();
      return; // Don't process further
    }

    // Buzzer click feedback
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOnMillis = millis();
    buzzerActive = true;

    switch (uiState) {

      case HOME:
        if (key == 'A') {
          uiState = CONFIGURE;
          displayLCD(true, 0, 0, "[A] Set Voltage");
          displayLCD(false, 0, 1, "[B] Tolerance");
        } else if (key == 'B') {
          uiState = VIEW_STATUS;
          displayLCD(true, 0, 0, "[A] Input");
          displayLCD(false, 0, 1, "[B] Output");
        }
        break;

      case CONFIGURE:
        if (key == 'A') {
          uiState = INPUT_TARGET;
          inputBuffer = "";
          displayLCD(true, 0, 0, "Set target      ");
          displayLCD(false, 0, 1, "                ");
          lcd.setCursor(0, 1);
          lcd.blink();
        } else if (key == 'B') {
          uiState = INPUT_TOLERANCE;
          inputBuffer = "";
          displayLCD(true, 0, 0, "Set tolerance   ");
          displayLCD(false, 0, 1, "                ");
          lcd.setCursor(0, 1);
          lcd.blink();
        } else if (key == 'D') {
          uiState = HOME;
          displayLCD(true, 0, 0, "[A] Configure");
          displayLCD(false, 0, 1, "[B] View Status");
        }
        break;

      case INPUT_TARGET:
        processTargetInput(key);
        break;

      case INPUT_TOLERANCE:
        processToleranceInput(key);
        break;

      case VIEW_STATUS:
        if (key == 'A') {
          uiState = VIEW_INPUT;
          displayLCD(true, 0, 0, "Voltage:        ");
          displayLCD(false, 0, 1, "                ");
        } else if (key == 'B') {
          uiState = VIEW_OUTPUT;
          displayLCD(true, 0, 0, "                ");
          displayLCD(false, 0, 1, "                ");
        } else if (key == 'D') {
          uiState = HOME;
          displayLCD(true, 0, 0, "[A] Configure");
          displayLCD(false, 0, 1, "[B] View Status");
        }
        break;

      case VIEW_INPUT:
        if (key == 'D') {
          uiState = VIEW_STATUS;
          displayLCD(true, 0, 0, "[A] Input");
          displayLCD(false, 0, 1, "[B] Output");
        }
        break;

      case VIEW_OUTPUT:
        if (key == 'D') {
          uiState = VIEW_STATUS;
          displayLCD(true, 0, 0, "[A] Input");
          displayLCD(false, 0, 1, "[B] Output");
        }
        break;

      default:
        break;
    }
  }
}

// ═══════════════════════════════════════════════════════
//  Run State (called every loop iteration)
// ═══════════════════════════════════════════════════════
void Machine::runState() {

  // ── 1. Hardware Ticks ──────────────────────────────
  // Buzzer auto-off
  if (buzzerActive && (millis() - buzzerOnMillis >= 100)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }

  // ── 2. Sensor Ticks (Non-blocking) ─────────────────
  vinSensor.update();
  voutSensor.update();
  currentVin = vinSensor.getVoltage();
  currentVout = voutSensor.getVoltage();

  // ACS712: time-sliced read (blocks ~20ms, only every 500ms)
  static unsigned long lastCurrentRead = 0;
  if (millis() - lastCurrentRead >= 500) {
    currentAmps = readCurrent();
    currentWatts = currentVout * currentAmps;
    lastCurrentRead = millis();
  }

  // ── 3. Regulation Ticks ────────────────────────────
  if (regState == REG_ACTIVE) {
    regulateLoop();
    checkAlarms();
  } else if (regState == REG_UNDER_VOLTAGE || regState == REG_OVER_VOLTAGE) {
    checkAlarmRecovery();
  }

  // ── 4. UI Ticks (throttled to prevent flicker) ─────
  unsigned long now = millis();
  if (now - lastLCDTick < 500) return;
  lastLCDTick = now;

  switch (uiState) {

    case VIEW_INPUT:
    {
      // Line 1: "Voltage: <Vin>V"
      lcd.setCursor(0, 0);
      lcd.print("Voltage: ");
      lcd.print((int)currentVin);
      lcd.print("V   ");  // Trailing spaces to clear old chars
      // Line 2: blank
      lcd.setCursor(0, 1);
      lcd.print("                ");
      break;
    }

    case VIEW_OUTPUT:
    {
      // Line 1: "V: <Vout>V I:<Amps>A"
      lcd.setCursor(0, 0);
      lcd.print("V:");
      lcd.print((int)currentVout);
      lcd.print("V ");
      lcd.print("I:");
      lcd.print(currentAmps, 1);
      lcd.print("A  ");

      // Line 2: "P: <Watts>W" + alarm indicator
      lcd.setCursor(0, 1);
      lcd.print("P:");
      lcd.print((int)currentWatts);
      lcd.print("W ");

      // Show alarm on this screen only
      if (regState == REG_UNDER_VOLTAGE) {
        lcd.print("LOW!  ");
      } else if (regState == REG_OVER_VOLTAGE) {
        lcd.print("HIGH! ");
      } else {
        lcd.print("      ");
      }
      break;
    }

    default:
      // HOME, INPUT_TARGET, VIEW_STATUS: static menus, no live update needed
      break;
  }
}
