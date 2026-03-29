/*
  NonBlockingAC.ino - AC Current Reading with Responsiveness
  
  AC reading inherently *blocks* for ~20ms (one full wave cycle).
  We cannot make it mathematically non-blocking without complex interrupts.
  
  HOWEVER, we can make your main loop Responsive by using "Time Slicing":
  - Only read current once every 500ms (or 1s).
  - The rest of the time, the loop runs free (blinking LED, reading buttons, etc).
  - You only "lose" 20ms of responsiveness every 500ms, which is unnoticeable.
*/

#include <ACS712-driver.h>

ACS712 sensor(A0, 5.0, 1023);

unsigned long lastCurrentRead = 0;
unsigned long lastBlink = 0;
bool ledState = false;

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  sensor.setSensitivity(0.185); // Adjust as needed
  sensor.calibrate();
}

void loop() {
  unsigned long now = millis();

  // TASK 1: Read AC Current (Runs rarely, costs 20ms)
  if (now - lastCurrentRead >= 500) {
    float amps = sensor.readCurrentAC(60);
    
    Serial.print("AC Current: ");
    Serial.println(amps);
    
    lastCurrentRead = now;
  }
  
  // TASK 2: High Speed Task (Blinking LED)
  // This runs freely 96% of the time.
  if (now - lastBlink > 100) { // Blink fast (10Hz)
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
    lastBlink = now;
  }
}
