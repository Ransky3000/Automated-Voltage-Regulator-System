#include <Servo.h>

Servo myServo;
const int SERVO_PIN = 3;

// Typical MG996R 360-degree servo midpoint is around 1500us (microseconds).
// Some MG996R continuous rotation servos drift slightly, so you can use the +/-
// keys in this test to find your specific servo's exact "dead stop" value.
int currentVal = 1500; 

void setup() {
  Serial.begin(9600);
  myServo.attach(SERVO_PIN);
  
  // Initialize to stop position
  myServo.writeMicroseconds(currentVal);
  
  Serial.println("--- 360 Servo Tester ---");
  Serial.println("Send commands via Serial Monitor (Newline or No line ending)");
  Serial.println("  'f' = spin forward (CW)");
  Serial.println("  'r' = spin reverse (CCW)");
  Serial.println("  's' = stop");
  Serial.println("  '+' = increase value by 10 (tune forward speed)");
  Serial.println("  '-' = decrease value by 10 (tune reverse speed)");
  Serial.println("  '0' = exact 1500us (default stop)");
  Serial.println("------------------------");
  Serial.print("Current Value: ");
  Serial.println(currentVal);
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    // Ignore newline characters if the user has "Newline" selected
    if (cmd == '\n' || cmd == '\r') {
      return; 
    }

    switch (cmd) {
      case 'f':
        currentVal = 1700; // Full forward (usually 1600-2000)
        break;
      case 'r':
        currentVal = 1300; // Full reverse (usually 1000-1400)
        break;
      case 's':
      case '0':
        currentVal = 1500; // Neutral / Stop
        break;
      case '+':
        currentVal += 10;
        break;
      case '-':
        currentVal -= 10;
        break;
      default:
        Serial.println("Unknown command!");
        break;
    }

    // Constrain to typical servo pulse ranges
    currentVal = constrain(currentVal, 1000, 2000);
    
    myServo.writeMicroseconds(currentVal);
    
    Serial.print("Command: ");
    Serial.print(cmd);
    Serial.print(" | Sending Value: ");
    Serial.println(currentVal);
  }
}
