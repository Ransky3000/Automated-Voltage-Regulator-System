#include <Servo.h>
#include <EEPROM.h>

Servo myServo;

const int SERVO_PIN = 3;
const int SERVO_POT_PIN = A2;
bool servoAttached = false;

// Known limits for raw pot readings
const int POT_MIN = 5;
const int POT_MAX = 670;
const int VARIAC_MIN_V = 0;
const int VARIAC_MAX_V = 250;

// Calibration Constants
const int CAL_STEP_V = 5;                           // 5V increments
const int CAL_POINTS = (250 / CAL_STEP_V) + 1;      // 51 points total
const int EEPROM_MAGIC_BYTE = 0xCA;                 // Valid calibration flag
uint16_t calPotValues[CAL_POINTS];
bool hasCalibration = false;

// Target voltage (set via Serial)
int targetVoltage = -1; // -1 means no target set yet
const int DEADBAND_V = 1;

// Pulse-and-wait timing
const unsigned long PULSE_DURATION_MS = 80;   
const unsigned long SETTLE_DURATION_MS = 150; 

enum ControlState {
  IDLE,
  PULSING,
  SETTLING,
  CALIBRATING     // System is in calibration mode
};

ControlState controlState = IDLE;
unsigned long stateTimer = 0;
int pulseDirection = 0; // +1 = Forward/Increase, -1 = Reverse/Decrease

// Calibration state logic
int calCurrentStep = 0;

void servoStart(int speed) {
  if (!servoAttached) {
    myServo.attach(SERVO_PIN);
    servoAttached = true;
  }
  myServo.writeMicroseconds(speed);
}

void servoStop() {
  if (servoAttached) {
    myServo.writeMicroseconds(1500);
    delay(5); // Brief stop signal before detaching
    myServo.detach();
    servoAttached = false;
  }
}

// Loads 51-point lookup table from EEPROM
void loadCalibration() {
  if (EEPROM.read(0) == EEPROM_MAGIC_BYTE && EEPROM.read(1) == CAL_POINTS) {
    hasCalibration = true;
    for (int i = 0; i < CAL_POINTS; i++) {
      int addr = 2 + (i * 2);
      calPotValues[i] = EEPROM.read(addr) | (EEPROM.read(addr + 1) << 8);
    }
    Serial.println("\n>>> SUCCESS: 51-Point EEPROM Calibration Loaded <<<");
  } else {
    hasCalibration = false;
    Serial.println("\n!!! NO CALIBRATION FOUND. Using linear map(). Type 'cal' to calibrate. !!!");
  }
}

// Saves 51-point lookup table to EEPROM
void saveCalibration() {
  EEPROM.write(0, EEPROM_MAGIC_BYTE);
  EEPROM.write(1, CAL_POINTS);
  for (int i = 0; i < CAL_POINTS; i++) {
    int addr = 2 + (i * 2);
    EEPROM.write(addr, calPotValues[i] & 0xFF);         // Low byte
    EEPROM.write(addr + 1, (calPotValues[i] >> 8) & 0xFF); // High byte
  }
  hasCalibration = true;
  Serial.println("\n>>> SUCCESS: Calibration saved to EEPROM! <<<");
}

// Interpolates pot reading to physical voltage
int potToVoltage(int potVal) {
  if (!hasCalibration) {
    // Fallback if no calibration done yet
    return map(potVal, POT_MIN, POT_MAX, VARIAC_MIN_V, VARIAC_MAX_V);
  }
  
  // Handle out of bounds
  int minP = min(calPotValues[0], calPotValues[CAL_POINTS - 1]);
  int maxP = max(calPotValues[0], calPotValues[CAL_POINTS - 1]);
  if (potVal <= minP) return (calPotValues[0] < calPotValues[CAL_POINTS - 1]) ? VARIAC_MIN_V : VARIAC_MAX_V;
  if (potVal >= maxP) return (calPotValues[0] < calPotValues[CAL_POINTS - 1]) ? VARIAC_MAX_V : VARIAC_MIN_V;
  
  // Linear interpolation between the two closest recorded points
  for (int i = 0; i < CAL_POINTS - 1; i++) {
    int p1 = min(calPotValues[i], calPotValues[i + 1]);
    int p2 = max(calPotValues[i], calPotValues[i + 1]);
    
    if (potVal >= p1 && potVal <= p2) {
      int v1 = i * CAL_STEP_V;
      int v2 = (i + 1) * CAL_STEP_V;
      return map(potVal, calPotValues[i], calPotValues[i + 1], v1, v2);
    }
  }
  return VARIAC_MAX_V; // Fallback
}

void setup() {
  Serial.begin(9600);
  pinMode(SERVO_POT_PIN, INPUT);
  
  Serial.println("=======================================================");
  Serial.println("   Variac Auto-Positioning (EEPROM Calibrated)         ");
  Serial.println("=======================================================");
  
  loadCalibration();
  
  Serial.println("\nCommands:");
  Serial.println("  0 to 250 : Set target voltage");
  Serial.println("  'cal'    : Start physical 51-point calibration wizard");
  Serial.println("-------------------------------------------------------");
}

void loop() {
  int rawPotValue = analogRead(SERVO_POT_PIN);
  int constrainedPot = constrain(rawPotValue, POT_MIN, POT_MAX);
  int currentVoltage = potToVoltage(constrainedPot);

  // 1. Read Serial for commands or targets
  if (Serial.available() > 0) {
    String inputStr = Serial.readStringUntil('\n');
    inputStr.trim(); // Remove whitespace/newline
    
    if (inputStr.length() == 0) return; // Ignore empty lines

    // --- CALIBRATION INTERFACE ---
    if (inputStr.equalsIgnoreCase("cal") && controlState != CALIBRATING) {
      controlState = CALIBRATING;
      calCurrentStep = 0;
      targetVoltage = -1; // Cancel any active targeting
      servoStop(); // Ensure servo goes limp so you can turn Variac by hand
      
      Serial.println("\n============================================");
      Serial.println("         CALIBRATION WIZARD STARTED");
      Serial.println("============================================");
      Serial.println("1. Turn the Variac knob physically by hand.");
      Serial.println("2. Align it perfectly with the 0V marking.");
      Serial.println("3. Type 'ok' and press Enter to lock it in.");
      Serial.println("Type 'cancel' anytime to abort.");
      Serial.println("--------------------------------------------");
    } 
    else if (controlState == CALIBRATING) {
      if (inputStr.equalsIgnoreCase("cancel")) {
        controlState = IDLE;
        loadCalibration(); // Reload existing from EEPROM just in case
        Serial.println("\n[!] Calibration cancelled. Returned to normal mode.");
      }
      else if (inputStr.equalsIgnoreCase("ok")) {
        // Average 10 readings of the POT for high stability
        long sum = 0;
        for (int i = 0; i < 10; i++) {
          sum += analogRead(SERVO_POT_PIN);
          delay(5);
        }
        calPotValues[calCurrentStep] = sum / 10;
        
        Serial.print("   >> Saved: ");
        Serial.print(calCurrentStep * CAL_STEP_V);
        Serial.print("V = Pot ");
        Serial.println(calPotValues[calCurrentStep]);
        
        calCurrentStep++;
        
        if (calCurrentStep < CAL_POINTS) {
          Serial.println();
          Serial.print("Now physically turn Variac to [ ");
          Serial.print(calCurrentStep * CAL_STEP_V);
          Serial.println("V ] and type 'ok'");
        } else {
          Serial.println("\n============================================");
          Serial.println("            CALIBRATION COMPLETE!");
          Serial.println("============================================");
          saveCalibration();
          controlState = IDLE;
        }
      } else {
         Serial.println("Invalid command. Type 'ok' to record, or 'cancel' to exit.");
      }
    } 
    // --- NORMAL OPERATION INTERFACE ---
    else if (controlState != CALIBRATING) {
      int inputVal = inputStr.toInt();
      // toInt() returns 0 for non-numbers, so handle "0" explicitly
      if (inputStr == "0" || inputVal > 0) {
        if (inputVal >= 0 && inputVal <= 250) {
          targetVoltage = inputVal;
          controlState = SETTLING; // Trigger movement assessment
          stateTimer = millis();
          Serial.print("\n>>> New Target Set: ");
          Serial.print(targetVoltage);
          Serial.println(" V");
        } else {
          Serial.println("Error: Target must be between 0 and 250.");
        }
      }
    }
  }

  // 2. Continuous State Machine
  if (controlState == CALIBRATING) {
    // Print live Pot value every 500ms so user can watch it settle
    static unsigned long lastCalPrint = 0;
    if (millis() - lastCalPrint > 500) {
      Serial.print("      Live POT: ");
      Serial.println(analogRead(SERVO_POT_PIN));
      lastCalPrint = millis();
    }
  } 
  else {
    // 3. Pulse-and-Wait Control Loop
    switch (controlState) {
      
      case IDLE:
        // Servo is detached. Monitor for drift.
        if (targetVoltage >= 0) {
          int absError = abs(targetVoltage - currentVoltage);
          if (absError > DEADBAND_V) {
            // Position drifted beyond deadband! Re-engage.
            controlState = SETTLING;
            stateTimer = millis();
          }
        }
        break;

      case SETTLING:
        servoStop(); // Servo is limp, waiting for mechanical settle
        
        if (millis() - stateTimer >= SETTLE_DURATION_MS) {
          int error = targetVoltage - currentVoltage;
          int absError = abs(error);
          
          if (absError <= DEADBAND_V) {
            // At target! Stay detached and go idle.
            servoStop();
            controlState = IDLE;
            Serial.print("\n=== AT TARGET: ");
            Serial.print(currentVoltage);
            Serial.println(" V ===");
          } else {
            // Calculate proportional speed
            int speed;
            if (absError > 30) {
              speed = 200;
            } else if (absError > 10) {
              speed = 100;
            } else {
              speed = 50;
            }
            
            int servoCmd;
            if (error > 0) {
              servoCmd = 1500 + speed;
              pulseDirection = 1;  // Going UP
            } else {
              servoCmd = 1500 - speed;
              pulseDirection = -1; // Going DOWN
            }
            
            // Limit checks
            if (pulseDirection == -1 && constrainedPot <= POT_MIN) {
              servoStop();
              controlState = IDLE;
              Serial.println("!!! HIT VARIAC MIN LIMIT !!!");
              break;
            }
            if (pulseDirection == 1 && constrainedPot >= POT_MAX) {
              servoStop();
              controlState = IDLE;
              Serial.println("!!! HIT VARIAC MAX LIMIT !!!");
              break;
            }
            
            servoStart(servoCmd);
            controlState = PULSING;
            stateTimer = millis();
          }
        }
        break;

      case PULSING:
        // CHECK LIMITS IN REAL TIME DURING FAST MOVEMENT
        if (pulseDirection == 1 && constrainedPot >= POT_MAX) {
          servoStop();
          controlState = IDLE;
          Serial.println("\n!!! EMERGENCY STOP: Hit MAX limit during pulse !!!");
          break;
        }
        if (pulseDirection == -1 && constrainedPot <= POT_MIN) {
          servoStop();
          controlState = IDLE;
          Serial.println("\n!!! EMERGENCY STOP: Hit MIN limit during pulse !!!");
          break;
        }
        
        // When pulse finishes, go back to settling
        if (millis() - stateTimer >= PULSE_DURATION_MS) {
          servoStop();
          controlState = SETTLING;
          stateTimer = millis();
        }
        break;
        
      case CALIBRATING:
        break; // Handled outside the switch
    }

    // 4. Status Print while NOT calibrating
    static unsigned long lastPrint = 0;
    static int lastPrintedV = -1;
    static ControlState lastPrintedState = IDLE;
    
    // Only print continuously if we're actively pulsing/settling to avoid Serial spam,
    // Or print once when we reach IDLE.
    if (controlState != IDLE || (controlState == IDLE && lastPrintedState != IDLE)) {
      if (millis() - lastPrint > 100) {
        Serial.print("State: ");
        if (controlState == PULSING) Serial.print("PULSE ");
        else if (controlState == SETTLING) Serial.print("SETTLE");
        else Serial.print("IDLE  ");
        
        Serial.print(" | Raw A2: ");
        Serial.print(rawPotValue);
        Serial.print(" | Current: ");
        Serial.print(currentVoltage);
        Serial.print("V | Target: ");
        if (targetVoltage >= 0) {
          Serial.print(targetVoltage);
          Serial.println("V");
        } else {
          Serial.println("---");
        }
        
        lastPrint = millis();
        lastPrintedState = controlState;
      }
    }
  }
}
