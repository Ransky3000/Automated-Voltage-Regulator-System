#include "Machine.h"

// ═══════════════════════════════════════════════════════
//  Constructor
// ═══════════════════════════════════════════════════════
Machine::Machine(Keypad& kRef, LiquidCrystal_I2C& lcdRef)
  : k(kRef), lcd(lcdRef),
    vinSensor(VIN_SENSOR_PIN, ZMPT_FREQUENCY),
    voutSensor(VOUT_SENSOR_PIN, ZMPT_FREQUENCY)
{
  uiState = HOME;
  regState = REG_IDLE;
  servoState = SERVO_IDLE;

  servoAttached = false;
  pulseDirection = 0;
  servoTimer = 0;
  hasCalibration = false;

  targetVoltage = -1;
  currentVin = 0;
  currentVout = 0;
  currentAmps = 0;
  currentWatts = 0;

  lastVout = 0;
  lastVoutChangeTime = 0;

  inputBuffer = "";
  isLineFull = 0;

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
  pinMode(CSM_OUT_PIN, INPUT);

  // LCD
  lcd.init();
  lcd.backlight();
  displayLCD(true, 0, 0, "System");
  displayLCD(false, 0, 1, "starting!");

  // ZMPT101B Sensors
  vinSensor.setSensitivity(ZMPT_SENSITIVITY);
  voutSensor.setSensitivity(ZMPT_SENSITIVITY);

  // Load EEPROM calibration for servo
  loadCalibration();

  delay(2000);

  // Show Home Screen
  uiState = HOME;
  displayLCD(true, 0, 0, "[A] Set Voltage");
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
//  EEPROM Calibration
// ═══════════════════════════════════════════════════════
void Machine::loadCalibration() {
  if (EEPROM.read(0) == EEPROM_MAGIC && EEPROM.read(1) == CAL_POINTS) {
    hasCalibration = true;
    for (int i = 0; i < CAL_POINTS; i++) {
      int addr = 2 + (i * 2);
      calPotValues[i] = EEPROM.read(addr) | (EEPROM.read(addr + 1) << 8);
    }
    Serial.println(F("EEPROM Calibration Loaded (51 points)"));
  } else {
    hasCalibration = false;
    Serial.println(F("No EEPROM calibration. Using linear map()."));
  }
}

int Machine::potToVoltage(int potVal) {
  if (!hasCalibration) {
    return map(potVal, POT_MIN, POT_MAX, 0, VARIAC_MAX_V);
  }

  // Handle out of bounds
  int minP = min(calPotValues[0], calPotValues[CAL_POINTS - 1]);
  int maxP = max(calPotValues[0], calPotValues[CAL_POINTS - 1]);
  if (potVal <= minP) return (calPotValues[0] < calPotValues[CAL_POINTS - 1]) ? 0 : VARIAC_MAX_V;
  if (potVal >= maxP) return (calPotValues[0] < calPotValues[CAL_POINTS - 1]) ? VARIAC_MAX_V : 0;

  // Linear interpolation between closest calibrated points
  for (int i = 0; i < CAL_POINTS - 1; i++) {
    int p1 = min(calPotValues[i], calPotValues[i + 1]);
    int p2 = max(calPotValues[i], calPotValues[i + 1]);

    if (potVal >= p1 && potVal <= p2) {
      int v1 = i * CAL_STEP_V;
      int v2 = (i + 1) * CAL_STEP_V;
      return map(potVal, calPotValues[i], calPotValues[i + 1], v1, v2);
    }
  }
  return VARIAC_MAX_V;
}

// ═══════════════════════════════════════════════════════
//  Current Sensor (ACS712 20A)
// ═══════════════════════════════════════════════════════
float Machine::readCurrent() {
  int raw = analogRead(CSM_OUT_PIN);
  float voltage = (raw / 1023.0) * 5.0;
  float amps = (voltage - ACS712_ZERO_POINT) / ACS712_SENSITIVITY;
  return abs(amps); // We only care about magnitude
}

// ═══════════════════════════════════════════════════════
//  Regulation Loop (Pulse-and-Wait)
// ═══════════════════════════════════════════════════════
void Machine::regulateLoop() {
  int rawPot = analogRead(SERVO_POT_PIN);
  int constrainedPot = constrain(rawPot, POT_MIN, POT_MAX);
  int dialPosition = potToVoltage(constrainedPot);

  // Calculate required rotary position from Vin and target
  // Formula: Required Rotary = Vout_target × 220 / Vin
  // But we also use the Vout feedback to refine
  int error = targetVoltage - (int)currentVout;
  int absError = abs(error);

  switch (servoState) {

    case SERVO_IDLE:
      if (absError > DEADBAND_V) {
        // Drift detected — need to move
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

        if (absError <= DEADBAND_V) {
          // At target!
          servoStop();
          servoState = SERVO_IDLE;
        } else {
          // Calculate proportional speed
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

          // Direction-aware safety limits
          if (pulseDirection == -1 && constrainedPot <= POT_MIN) {
            servoStop();
            servoState = SERVO_IDLE;
            break;
          }
          if (pulseDirection == 1 && constrainedPot >= POT_MAX) {
            servoStop();
            servoState = SERVO_IDLE;
            break;
          }

          servoStart(servoCmd);
          servoState = SERVO_PULSING;
          servoTimer = millis();
        }
      }
      break;

    case SERVO_PULSING:
      // Real-time safety during pulse
      if (pulseDirection == 1 && constrainedPot >= POT_MAX) {
        servoStop();
        servoState = SERVO_IDLE;
        break;
      }
      if (pulseDirection == -1 && constrainedPot <= POT_MIN) {
        servoStop();
        servoState = SERVO_IDLE;
        break;
      }
      // Pulse timer expired
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
    if (currentVout <= targetVoltage + DEADBAND_V) {
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
    int val = inputBuffer.toInt();
    if (val >= 0 && val <= 250) {
      targetVoltage = val;
      regState = REG_ACTIVE;
      servoState = SERVO_SETTLING;
      servoTimer = millis();
      lastVoutChangeTime = millis(); // Reset stall detection

      inputBuffer = "";
      uiState = VIEW_STATUS;
      displayLCD(true, 0, 0, "[A] Input");
      displayLCD(false, 0, 1, "[B] Output");
      return;
    } else {
      // Invalid range
      displayLCD(true, 0, 0, "Invalid! 0-250");
      delay(1000);
      inputBuffer = "";
    }
  } else if (key == 'D') {
    // Cancel
    inputBuffer = "";
    uiState = HOME;
    displayLCD(true, 0, 0, "[A] Set Voltage");
    displayLCD(false, 0, 1, "[B] View Status");
    return;
  }

  // Redraw input screen
  String display = inputBuffer;
  while (display.length() < 3) display += "_";
  display += "V";
  // Pad to 16 characters
  while (display.length() < 16) display += " ";

  lcd.setCursor(0, 0);
  lcd.print("Set target      ");
  lcd.setCursor(0, 1);
  lcd.print(display);
}

// ═══════════════════════════════════════════════════════
//  Keypad Press Handler
// ═══════════════════════════════════════════════════════
void Machine::keypadPress() {
  char key = k.getKey();

  if (key) {
    // Buzzer click feedback
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOnMillis = millis();
    buzzerActive = true;

    switch (uiState) {

      case HOME:
        if (key == 'A') {
          uiState = INPUT_TARGET;
          inputBuffer = "";
          displayLCD(true, 0, 0, "Set target      ");
          displayLCD(false, 0, 1, "___V            ");
        } else if (key == 'B') {
          uiState = VIEW_STATUS;
          displayLCD(true, 0, 0, "[A] Input");
          displayLCD(false, 0, 1, "[B] Output");
        }
        break;

      case INPUT_TARGET:
        processTargetInput(key);
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
          displayLCD(true, 0, 0, "[A] Set Voltage");
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
  currentAmps = readCurrent();
  currentWatts = currentVout * currentAmps;

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
