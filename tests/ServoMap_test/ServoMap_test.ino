#include <Servo.h>

Servo myServo;

const int SERVO_PIN = 3;
const int SERVO_POT_PIN = A2;

int currentVal = 1500; 

// Known calibration from previous test
const int POT_MIN = 15;
const int POT_MAX = 650;

// Target Variac Voltage output limits
const int VARIAC_MIN_V = 0;
const int VARIAC_MAX_V = 250;

void setup() {
  Serial.begin(9600);
  pinMode(SERVO_POT_PIN, INPUT);
  myServo.attach(SERVO_PIN);
  
  // Initialize to stop position
  myServo.writeMicroseconds(currentVal);
  
  Serial.println("--- Servo Mapping Test ---");
  Serial.println("Send commands to spin servo and watch the Mapped Voltage.");
  Serial.println("  'f' = spin CCW (Forward)");
  Serial.println("  'r' = spin CW (Reverse)");
  Serial.println("--------------------------");
}

void loop() {
  // 1. Read the physical position of the Variac dial via A2
  int rawPotValue = analogRead(SERVO_POT_PIN);
  
  // Constrain just in case the pot drifts slightly below 0 or above 670
  int constrainedPot = constrain(rawPotValue, POT_MIN, POT_MAX);
  
  // 2. Map the 0-670 range to the 0-250V range.
  // NOTE: If CW (Reverse) increases the voltage, and CW increases the A2 number, 
  // then 0 = 0V and 670 = 250V. 
  // If spinning to 250V actually makes A2 drop to 0, you would swap the last two numbers:
  // map(constrainedPot, POT_MIN, POT_MAX, VARIAC_MAX_V, VARIAC_MIN_V)
  int estimatedVolts = map(constrainedPot, POT_MIN, POT_MAX, VARIAC_MIN_V, VARIAC_MAX_V);
  
  // 3. Handle commands
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    if (cmd == '\n' || cmd == '\r') return;

    switch (cmd) {
      case 'f': // CCW (Forward)
        if (currentVal == 1700) {
          currentVal = 1500;
        } else {
          currentVal = 1700;
        }
        break;
      case 'r': // CW (Reverse)
        if (currentVal == 1300) {
          currentVal = 1500;
        } else {
          currentVal = 1300;
        }
        break;
    }
  }

  // == NEW ALARM LIMIT SAFETY LOGIC ==
  // Prevent spinning if we've hit the physical Variac stops
  if (currentVal == 1300 && constrainedPot <= POT_MIN) {
     // Trying to reverse past zero? Stop!
     currentVal = 1500; 
  }
  else if (currentVal == 1700 && constrainedPot >= POT_MAX) {
     // Trying to forward past 250V? Stop!
     currentVal = 1500; 
  }

  // Apply the (potentially overridden) speed setting
  myServo.writeMicroseconds(currentVal);
  
  // 4. Print output every 500ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 3) {
    Serial.print("Servo: ");
    if (currentVal == 1700) Serial.print("CCW (f)");
    else if (currentVal == 1300) Serial.print("CW (r) ");
    else Serial.print("STOP   ");
    
    Serial.print(" | Raw A2: ");
    Serial.print(rawPotValue);
    Serial.print(" | Estimated Variac V: ");
    Serial.print(estimatedVolts);
    Serial.println(" V");
    
    lastPrint = millis();
  }
}
