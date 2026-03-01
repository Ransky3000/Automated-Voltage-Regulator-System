#include <Servo.h>

Servo myServo;

const int SERVO_PIN = 3;
const int SERVO_POT_PIN = A2;
bool servoAttached = false;

// Known calibration (SAME as ServoMap_test.ino)
const int POT_MIN = 10;
const int POT_MAX = 670;
const int VARIAC_MIN_V = 0;
const int VARIAC_MAX_V = 250;

// Target voltage (set via Serial)
int targetVoltage = -1; // -1 means no target set yet
const int DEADBAND_V = 2;

// Pulse-and-wait timing
const unsigned long PULSE_DURATION_MS = 80;   // Longer spin per pulse = faster travel
const unsigned long SETTLE_DURATION_MS = 150;  // Less wait between pulses

enum ControlState {
  IDLE,
  PULSING,
  SETTLING
};

ControlState controlState = IDLE;
unsigned long stateTimer = 0;
int pulseDirection = 0; // +1 = Forward/Increase, -1 = Reverse/Decrease

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

void setup() {
  Serial.begin(9600);
  pinMode(SERVO_POT_PIN, INPUT);
  
  Serial.println("--- Variac Auto-Positioning (Pulse & Wait + Detach) ---");
  Serial.println("Type a target voltage (0 to 250) and press Enter.");
  Serial.println("-------------------------------------------------------");
}

void loop() {
  // 1. Read Serial for a new target voltage
  if (Serial.available() > 0) {
    int inputVal = Serial.parseInt();
    while(Serial.available() > 0) Serial.read();

    if (inputVal >= 0 && inputVal <= 250) {
      targetVoltage = inputVal;
      controlState = SETTLING;
      stateTimer = millis();
      Serial.print(">>> New Target Set: ");
      Serial.print(targetVoltage);
      Serial.println(" V");
    } else {
      Serial.println("Error: Target must be between 0 and 250.");
    }
  }

  // 2. Read physical position
  int rawPotValue = analogRead(SERVO_POT_PIN);
  int constrainedPot = constrain(rawPotValue, POT_MIN, POT_MAX);
  int currentVoltage = map(constrainedPot, POT_MIN, POT_MAX, VARIAC_MIN_V, VARIAC_MAX_V);

  // 3. Pulse-and-Wait Control Loop
  switch (controlState) {
    
    case IDLE:
      // Servo is detached. Monitor for drift.
      if (targetVoltage >= 0) {
        int absError = abs(targetVoltage - currentVoltage);
        if (absError > DEADBAND_V) {
          // Position drifted! Re-engage.
          controlState = SETTLING;
          stateTimer = millis();
        }
      }
      break;

    case SETTLING:
      // Servo is detached, waiting for physical settling
      servoStop();
      
      if (millis() - stateTimer >= SETTLE_DURATION_MS) {
        int error = targetVoltage - currentVoltage;
        int absError = abs(error);
        
        if (absError <= DEADBAND_V) {
          // At target! Detach and go idle.
          servoStop();
          controlState = IDLE;
          Serial.print("=== AT TARGET: ");
          Serial.print(currentVoltage);
          Serial.println(" V ===");
        } else {
          // Need to move
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
          
          // Safety limits: only block if heading TOWARD the limit
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
          
          // Start pulse
          servoStart(servoCmd);
          controlState = PULSING;
          stateTimer = millis();
        }
      }
      break;

    case PULSING:
      // Servo is spinning — CHECK LIMITS IN REAL TIME!
      // Only stop if heading TOWARD the limit (not away from it)
      if (pulseDirection == 1 && constrainedPot >= POT_MAX) {
        servoStop();
        controlState = IDLE;
        Serial.println("!!! EMERGENCY STOP: Hit MAX limit during pulse !!!");
        break;
      }
      if (pulseDirection == -1 && constrainedPot <= POT_MIN) {
        servoStop();
        controlState = IDLE;
        Serial.println("!!! EMERGENCY STOP: Hit MIN limit during pulse !!!");
        break;
      }
      
      if (millis() - stateTimer >= PULSE_DURATION_MS) {
        // Pulse done! Stop and settle
        servoStop();
        controlState = SETTLING;
        stateTimer = millis();
      }
      break;
  }

  // 4. Status Print
  static unsigned long lastPrint = 0;
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
  }
}
