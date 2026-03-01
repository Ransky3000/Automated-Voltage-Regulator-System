#include <Servo.h>

Servo myServo;

const int SERVO_PIN = 3;
const int SERVO_POT_PIN = A2;

// The neutral/stop value for the modified MG996R
int currentVal = 1500; 

void setup() {
  Serial.begin(9600);
  pinMode(SERVO_POT_PIN, INPUT);
  myServo.attach(SERVO_PIN);
  
  // Initialize to stop position
  myServo.writeMicroseconds(currentVal);
  
  Serial.println("--- Servo + Potentiometer Feedback Test ---");
  Serial.println("Send commands to spin servo. Watch the Potentiometer (A2) reading change.");
  Serial.println("  'f' = spin forward (CW)");
  Serial.println("  'r' = spin reverse (CCW)");
  Serial.println("  's' = stop");
  Serial.println("-------------------------------------------");
}

void loop() {
  // 1. Read the physical position of the servo's output gear
  int potValue = analogRead(SERVO_POT_PIN);
  
  // 2. Handle serial commands to move the servo
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    if (cmd == '\n' || cmd == '\r') return;

    switch (cmd) {
      case 'f':
        if (currentVal == 1700) {
          currentVal = 1500; // Toggle OFF
        } else {
          currentVal = 1700; // Toggle ON (Forward)
        }
        break;
      case 'r':
        if (currentVal == 1300) {
          currentVal = 1500; // Toggle OFF
        } else {
          currentVal = 1300; // Toggle ON (Reverse)
        }
        break;
    }
    
    myServo.writeMicroseconds(currentVal);
  }
  
  // 3. Print the status every 250ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 250) {
    Serial.print("Servo Cmd: ");
    Serial.print(currentVal);
    Serial.print(" (");
    if (currentVal > 1500) Serial.print("FWD ");
    else if (currentVal < 1500) Serial.print("REV ");
    else Serial.print("STOP");
    Serial.print(") | Potentiometer A2: ");
    Serial.println(potValue);
    
    lastPrint = millis();
  }
}
