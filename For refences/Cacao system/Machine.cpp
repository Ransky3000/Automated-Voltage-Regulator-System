#include "Machine.h"

//Constructor Related
Machine::Machine(Keypad &kRef, LiquidCrystal_I2C &lcdRef, HX711_ADC &LoadCellRef) : k(kRef), lcd(lcdRef), LoadCell(LoadCellRef){
  
  currentState = MENU;
  isRelay = false;
  calibrationFactor = 1.0;
  newCal = 0;
  t = 0;
  countDown = 3;

  inputHours = 0;
  inputMins = 0;
  inputSecs = 0;
  targetDuration = 0;
  processStartTime = 0;
  stopTimer = 0;         // <--- FIX: Initialize timer
  isServoOpen = false;

  inputBuffer = "";
  isLineFull = 0;
  buzzerOnMillis = 0;
  buzzerActive = false;
}

void Machine::Initialize(){
  pinMode(2,OUTPUT);
  digitalWrite(2,LOW);
  pinMode(3,OUTPUT);
  digitalWrite(3,LOW);
  pinMode(A0,OUTPUT);
  digitalWrite(A0,LOW);
  pinMode(A1,OUTPUT);
  digitalWrite(A1,HIGH);
  
  valveServo.attach(A3);
  valveServo.write(0); 
  isServoOpen = false;

  lcd.init();
  lcd.backlight();
  
  displayLCD(true,0,0,"System");
  displayLCD(false,0,1,"starting!");

  delay(3000);
  LoadCell.begin();
  LoadCell.start(2000, true); 
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check HX711 wiring and pins.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("HX711 Error!");
    lcd.setCursor(0, 1);
    lcd.print("Check wiring");
    while(1); 
  } else {
    Serial.println("HX711 initialization complete.");
    if(EEPROM.read(0) != 0xFF){
      EEPROM.get(0, calibrationFactor); 
    }
    LoadCell.setCalFactor(calibrationFactor);
    LoadCell.tare();
  }
  
  currentState = MENU;
  displayMainMenu();
}

void Machine::displayLCD(byte col, byte line, String sentence){
  if(isLineFull == 2) lcd.clear();
  lcd.setCursor(col, line);
  lcd.print(sentence);
  isLineFull++;
}
void Machine::displayLCD(bool clear,byte col, byte line, String sentence){
  if(clear == true){
    lcd.clear();
    lcd.setCursor(col, line);
    lcd.print(sentence);
  }else if(clear == false){
    lcd.setCursor(col, line);
    lcd.print(sentence);
  }else{
    Serial.println("Invalid display parameter");
  }
}

void Machine::formatTimeLCD(unsigned long timeLeftInMillis){
  unsigned long totalSeconds = timeLeftInMillis / 1000;
  int h = totalSeconds / 3600;
  int m = (totalSeconds % 3600) / 60;
  int s = totalSeconds % 60;

  lcd.setCursor(0,0);
  lcd.print("Time: ");
  if(h < 10) lcd.print("0");
  lcd.print(h);
  lcd.print(":");
  if(m < 10) lcd.print("0");
  lcd.print(m);
  lcd.print(":");
  if(s < 10) lcd.print("0");
  lcd.print(s);
}

void Machine::showScale(){
  if (LoadCell.update()) {
    if (millis() > t + 20) {
      float scale = LoadCell.getData(); 
      lcd.setCursor(0,1);
      lcd.print("Mass: ");
      lcd.print(scale, 1); 
      lcd.print(" g    "); 
      t = millis(); 
    }
  }
}

void Machine::newCalibration(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place nothing.");
  lcd.setCursor(0, 1);
  lcd.print("Press 'A' to tare.");
  
  while (true) {
    char key = k.getKey();
    if (key == 'A') {
      LoadCell.tare();
      break;
    }
  }
  displayLCD(true,0,0,"Now place know");
  displayLCD(false,0,1,"Mass: ");

  currentState = SAVE_CAL;
  inputBuffer = "";
}

void Machine::displayMainMenu(){
  displayLCD(true,0,0,"[1]Set Time");
  displayLCD(false,0,1,"[2]Check scale");
}

void Machine::displayScaleOptions(){
  displayLCD(true,0,0,"[1] Calibrate");
  displayLCD(false,0,1,"[2] Set zero");
}

bool Machine::getRelay(){
  return isRelay;
}

void Machine::offRelay(){
  isRelay = false;
}

void Machine::toggleServo(){
  if(isServoOpen){
     valveServo.write(0); 
     isServoOpen = false;
     displayLCD(true,0,0,"Valve CLOSED!");
  } else {
     valveServo.write(90); 
     isServoOpen = true;
     displayLCD(true,0,0,"Valve OPENED!");
  }
  delay(1500); 
}

void Machine::runState(){
  LoadCell.update();
  
  switch(currentState){
    case RUN_PROCESS:
      // Scope block to keep variables local
      {
        unsigned long currentMillis = millis();
        unsigned long elapsed = currentMillis - processStartTime;
        
        if(elapsed < targetDuration){
          unsigned long remaining = targetDuration - elapsed;
          static unsigned long lastLCD = 0;
          if(currentMillis - lastLCD > 200){
             formatTimeLCD(remaining);
             lastLCD = currentMillis;
          }
          showScale(); 
        } else {
          // TIME UP
          isRelay = false;        
          digitalWrite(2, LOW);   
          digitalWrite(A0, LOW);  
          
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Time Reached!");
          lcd.setCursor(0,1);
          lcd.print("Done.");
          Serial.println("Timer Finished. Waiting 2s..."); 
          
          stopTimer = millis(); // Set class member timer
          currentState = STOP_COUNTDOWN; 
        }
      }
      break;
      
    case CANCEL_PROCESS:
      digitalWrite(2, LOW); 
      digitalWrite(A0, LOW);
      lcd.clear();           
      displayLCD(false,0,0,"Process");
      displayLCD(false,0,1,"Canceled!");
      delay(1500);    
      isRelay = false;  
      currentState = MENU;
      displayMainMenu();
      break;
      
    case CALIBRATE:
      while(countDown > 0){
        displayLCD(true,0,0,"Remove loads");
        displayLCD(false,0,1,"On scale ");
        lcd.print(countDown);
        delay(1000); 
        countDown--;
        if (buzzerActive && (millis() - buzzerOnMillis >= 140)) {
          digitalWrite(3, LOW);   
          buzzerActive = false;
        }
      }
      countDown = 3;
      newCalibration();
      break;
      
    case SET_ZERO:
      showScale();
      break;
      
    case STOP_COUNTDOWN:
      // FIX: Use Class Member timer
      if(millis() - stopTimer > 2000) {
        Serial.println("Returning to Menu"); // Debug to Terminal
        
        isRelay = false; 
        digitalWrite(2, LOW);
        digitalWrite(A0, LOW);
        
        currentState = MENU;
        displayMainMenu();
      }
      break;
      
    default:
      break;
  }
  
  if (buzzerActive && (millis() - buzzerOnMillis >= 140)) {
    digitalWrite(3, LOW);   
    buzzerActive = false;
  }
}

void Machine::keypadPress(){
  if (currentState == VALVE_OPEN_STATE) {
    if (k.getKeys()) {
      bool c_held = false;
      bool d_pressed = false;

      for (int i = 0; i < LIST_MAX; i++) {
        if (k.key[i].kstate == PRESSED || k.key[i].kstate == HOLD) {
          if (k.key[i].kchar == 'C') c_held = true;
          if (k.key[i].kchar == 'D') d_pressed = true;
        }
      }

      if (c_held) isRelay = true;
      else isRelay = false;

      if (d_pressed) {
        valveServo.write(0);
        isServoOpen = false;
        isRelay = false;
        currentState = MENU;
        displayMainMenu();
      }
    }
    return; // Skip standard getKey logic
  }

  char key = k.getKey();

  if (key) {
    digitalWrite(3, HIGH);               
    buzzerOnMillis = millis();           
    buzzerActive = true;                 

    switch(currentState){
      case MENU:
          if (key == '1') {
            currentState = INPUT_HOURS;
            inputBuffer = "";
            displayLCD(true,0,0,"Set Hours:");
            displayLCD(false,0,1,"0");
          } else if (key == '2') {
            currentState = CHECK_SCALE;
            inputBuffer = "";
            displayScaleOptions();
          } else if (key == 'B') {
            currentState = SERVO_MENU;
            displayLCD(true,0,0,"Toggle Valve?");
            displayLCD(false,0,1,"A:Yes D:Back");
          }
          break;
      
      case SERVO_MENU:
          if(key == 'A'){
             // Enter Manual Valve Mode
             valveServo.write(90); 
             isServoOpen = true;
             currentState = VALVE_OPEN_STATE;
             displayLCD(true,0,0,"Valve OPEN!");
             displayLCD(false,0,1,"Hold C-Run D-Esc");
          } else if(key == 'D'){
             currentState = MENU;
             displayMainMenu(); 
          }
          break;

      case INPUT_HOURS:
          if (isDigit(key)) {
            inputBuffer += key;
            displayLCD(false,0,1,"                ");
            displayLCD(false,0,1,inputBuffer);
          } else if (key == 'A') {
            inputHours = inputBuffer.toInt();
            inputBuffer = "";
            currentState = INPUT_MINS;
            displayLCD(true,0,0,"Set Mins:");
            displayLCD(false,0,1,"0");
          } else if(key == 'D'){
            currentState = MENU;
            displayMainMenu();
          }
          break;

      case INPUT_MINS:
          if (isDigit(key)) {
            inputBuffer += key;
            displayLCD(false,0,1,"                ");
            displayLCD(false,0,1,inputBuffer);
          } else if (key == 'A') {
            inputMins = inputBuffer.toInt();
            inputBuffer = "";
            currentState = INPUT_SECS;
            displayLCD(true,0,0,"Set Secs:");
            displayLCD(false,0,1,"0");
          } else if(key == 'D'){
            currentState = MENU;
            displayMainMenu();
          }
          break;

      case INPUT_SECS:
          if (isDigit(key)) {
            inputBuffer += key;
            displayLCD(false,0,1,"                ");
            displayLCD(false,0,1,inputBuffer);
          } else if (key == 'A') {
            inputSecs = inputBuffer.toInt();
            targetDuration = (inputHours * 3600000UL) + (inputMins * 60000UL) + (inputSecs * 1000UL);
            inputBuffer = "";
            currentState = BEGIN_PROCESS;
            displayLCD(true,0,0,"Begin process?");
            lcd.setCursor(0,1);
            lcd.print(inputHours); lcd.print("h ");
            lcd.print(inputMins); lcd.print("m ");
            lcd.print(inputSecs); lcd.print("s");
          } else if(key == 'D'){
            currentState = MENU;
            displayMainMenu();
          }
          break;

      case BEGIN_PROCESS:
          if(key == 'A'){
            displayLCD(true,0,0,"Starting in...");
            countDown = 3;
            while(countDown > 0){
              displayLCD(false,0,1, String(countDown));
              delay(1000); 
              countDown--;
              if (buzzerActive && (millis() - buzzerOnMillis >= 140)) {
                digitalWrite(3, LOW);   
                buzzerActive = false;
              }
            }
            isRelay = true;
            processStartTime = millis();
            currentState = RUN_PROCESS;
            lcd.clear(); 
          }else if(key == 'D'){
            inputBuffer = "";
            currentState = MENU;
            displayMainMenu();
          }
          break;
          
      case RUN_PROCESS:
          if(key == 'D'){
            currentState = CANCEL_PROCESS;
            isRelay = false;
          }
          break;
          
      case CHECK_SCALE:
          if (key == '1') {
            currentState =  CALIBRATE;
            inputBuffer = "";
          } else if (key == '2') {
            currentState = SET_ZERO;
            inputBuffer = "";
            displayLCD(true,0,0,"A- to scale zero");
            displayLCD(false,0,1,"<Current scale>");
          }else if(key == 'D'){
            inputBuffer = "";
            currentState = MENU;
            displayMainMenu();
          }
          break;
          
      case CALIBRATE:
          displayLCD(true,0,0,"Processing...");
          delay(1500);

      case SAVE_CAL:
          if (isDigit(key) || key == '.') {
            inputBuffer += key;
            displayLCD(false,0,1,"                ");
            displayLCD(false,0,1,inputBuffer);
          }else if (key == 'A') {
            LoadCell.refreshDataSet();
            float knownMass = inputBuffer.toFloat();
            calibrationFactor = LoadCell.getNewCalibration(knownMass);
            EEPROM.put(0, calibrationFactor); 
            inputBuffer = "";
            displayLCD(true,0,0,"Scale ");
            displayLCD(true,0,1,"Calibrated!");
            delay(1500);
            currentState = MENU;
            displayMainMenu();
            break;
          }else if(key == 'D'){
            inputBuffer = "";
            currentState = MENU;
            displayLCD(true,0,0,"Calibration");
            displayLCD(false,0,1,"Canceled!");
            delay(1500);
            displayMainMenu();
          }
          break;
          
      case SET_ZERO:
          if(key == 'A'){
            inputBuffer = "";
            LoadCell.tareNoDelay();
          }else if(key == 'D'){
            inputBuffer = "";
            currentState = MENU;
            displayMainMenu();
          }
      default:
          break;
    }
  }
}