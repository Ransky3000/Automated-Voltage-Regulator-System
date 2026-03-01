const int SERVO_POT_PIN = A2;

// Calibration points: Variac dial positions to record
const int NUM_POINTS = 11;
const int dialPositions[NUM_POINTS] = {0, 25, 50, 75, 100, 125, 150, 175, 200, 225, 250};
int potReadings[NUM_POINTS];

int currentStep = 0;
bool calibrationDone = false;

void setup() {
  Serial.begin(9600);
  pinMode(SERVO_POT_PIN, INPUT);
  
  Serial.println("========================================");
  Serial.println("   SERVO POTENTIOMETER CALIBRATION");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Instructions:");
  Serial.println("1. Detach the servo from Variac (or keep it stopped).");
  Serial.println("2. Manually turn the Variac dial to the position shown.");
  Serial.println("3. Press ENTER to record the A2 reading.");
  Serial.println("4. Repeat for all 11 positions.");
  Serial.println();
  printCurrentInstruction();
}

void printCurrentInstruction() {
  Serial.print(">> Turn Variac dial to [ ");
  Serial.print(dialPositions[currentStep]);
  Serial.print("V ] then press ENTER (");
  Serial.print(currentStep + 1);
  Serial.print("/");
  Serial.print(NUM_POINTS);
  Serial.println(")");
  
  // Show live A2 reading so they can see it settle
  Serial.println("   (Watch live A2 value below, wait for it to settle)");
}

void printResults() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("   CALIBRATION COMPLETE!");
  Serial.println("========================================");
  Serial.println();
  
  // Print as a readable table
  Serial.println("Variac (V)  |  Pot (A2)");
  Serial.println("------------|----------");
  for (int i = 0; i < NUM_POINTS; i++) {
    Serial.print("   ");
    if (dialPositions[i] < 100) Serial.print(" ");
    if (dialPositions[i] < 10)  Serial.print(" ");
    Serial.print(dialPositions[i]);
    Serial.print("      |    ");
    if (potReadings[i] < 100) Serial.print(" ");
    if (potReadings[i] < 10)  Serial.print(" ");
    Serial.println(potReadings[i]);
  }
  
  // Print as copy-paste code for Machine.h
  Serial.println();
  Serial.println("--- COPY-PASTE CODE FOR YOUR PROJECT ---");
  Serial.println();
  Serial.print("const int CAL_POINTS = ");
  Serial.print(NUM_POINTS);
  Serial.println(";");
  
  Serial.print("const int CAL_VOLTAGE[CAL_POINTS] = {");
  for (int i = 0; i < NUM_POINTS; i++) {
    Serial.print(dialPositions[i]);
    if (i < NUM_POINTS - 1) Serial.print(", ");
  }
  Serial.println("};");
  
  Serial.print("const int CAL_POT[CAL_POINTS]     = {");
  for (int i = 0; i < NUM_POINTS; i++) {
    Serial.print(potReadings[i]);
    if (i < NUM_POINTS - 1) Serial.print(", ");
  }
  Serial.println("};");
  
  Serial.println();
  Serial.println("--- END OF CALIBRATION ---");
}

void loop() {
  // Read the current pot value
  int rawPotValue = analogRead(SERVO_POT_PIN);
  
  if (!calibrationDone) {
    // Show live A2 value every 200ms so user can see it settle
    static unsigned long lastLive = 0;
    if (millis() - lastLive > 200) {
      Serial.print("   Live A2: ");
      Serial.println(rawPotValue);
      lastLive = millis();
    }
    
    // Wait for ENTER key to record
    if (Serial.available() > 0) {
      // Clear input buffer
      while (Serial.available() > 0) Serial.read();
      
      // Record this point (take average of 10 readings for stability)
      long sum = 0;
      for (int i = 0; i < 10; i++) {
        sum += analogRead(SERVO_POT_PIN);
        delay(5);
      }
      potReadings[currentStep] = sum / 10;
      
      Serial.print("   >> RECORDED: Variac ");
      Serial.print(dialPositions[currentStep]);
      Serial.print("V = Pot ");
      Serial.println(potReadings[currentStep]);
      Serial.println();
      
      currentStep++;
      
      if (currentStep >= NUM_POINTS) {
        calibrationDone = true;
        printResults();
      } else {
        printCurrentInstruction();
      }
    }
  }
}
