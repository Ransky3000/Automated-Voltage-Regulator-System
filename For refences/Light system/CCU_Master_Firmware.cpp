#include "CCU_Master_Firmware.h"

// Hardware Mapping Constants
#define BUZZER_PIN A1
const uint8_t relayPins[] = {12, 13, A0}; // D12, D13, A0

const unsigned int DEADBAND = 500;
const unsigned long TIMEOUT_MS = 10000;
const unsigned long POLL_INTERVAL = 300;

// Constructor
CCU_Master_Firmware::CCU_Master_Firmware(Keypad& kRef, LiquidCrystal_I2C& lcdRef, SoftwareSerial& radRef)
  : k(kRef), lcd(lcdRef), rad(radRef) {
  
  currentState = INIT;
  isLineFull = 0;
  buzzerOnMillis = 0;
  buzzerActive = false;

  isTimerActive = false;
  timerDurationMillis = 0;
  timerStartTime = 0;
  roomA_lux_threshold = 0; // Default off
  
  inputBuffer = "";

  lastRFPollTime = 0;
  currentTargetSensor = 0;

  // Default all relays to Auto mode
  relayMode[0] = 'A';
  relayMode[1] = 'A';
  relayMode[2] = 'A';
  selectedRelay = -1;

  // Initialize all 6 sensor trackers
  for(int i = 0; i < 6; i++) {
    sensors[i].lux = 0;
    sensors[i].lastUpdateTime = 0;
    sensors[i].isConnected = false;
    sensors[i].relayState = false; // Relays start OFF
  }
}

void CCU_Master_Firmware::Initialize() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  for(int i=0; i<3; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  lcd.init();
  lcd.backlight();

  displayLCD(true, 0, 0, "CCU System");
  displayLCD(false, 0, 1, "Initializing...");

  rad.begin(9600);

  delay(2000);
  
  currentState = HOME;
  displayLCD(true, 0, 0, "[A] Room A");
  displayLCD(false, 0, 1, "[B] Room B");
}

void CCU_Master_Firmware::displayLCD(byte col, byte line, String sentence) {
  if (isLineFull == 2) lcd.clear();
  lcd.setCursor(col, line);
  lcd.print(sentence);
  isLineFull++;
}

void CCU_Master_Firmware::displayLCD(bool clear, byte col, byte line, String sentence) {
  if (clear) {
    lcd.clear();
    lcd.setCursor(col, line);
    lcd.print(sentence);
    isLineFull = 1; 
  } else {
    lcd.setCursor(col, line);
    lcd.print(sentence);
    isLineFull++;
  }
}

// ---------------------------------------------------------
// --- Core Logic Methods ---
// ---------------------------------------------------------

void CCU_Master_Firmware::formatTimeLCD(unsigned long timeLeftInMillis, byte col, byte row) {
  unsigned long totalSeconds = timeLeftInMillis / 1000;
  int h = totalSeconds / 3600;
  int m = (totalSeconds % 3600) / 60;
  int s = totalSeconds % 60;

  lcd.setCursor(col, row);
  if(h < 10) lcd.print("0");
  lcd.print(h);
  lcd.print(":");
  if(m < 10) lcd.print("0");
  lcd.print(m);
  lcd.print(":");
  if(s < 10) lcd.print("0");
  lcd.print(s);
}

void CCU_Master_Firmware::pollSensorsRF() {
  if (millis() - lastRFPollTime >= POLL_INTERVAL) {
    lastRFPollTime = millis();
    
    // 1. Process previous response (if available) before transmitting next
    // The Custom Protocol: [0xAA] [Master_ID] [CMD_REPORT_DATA] [Data_H] [Data_L]
    if (rad.available() >= 5) {
      if (rad.read() == 0xAA) {
        if (rad.read() == 0x00) { // Addressed to Master
          if (rad.read() == 0x05) { // CMD_REPORT_DATA
            byte dataH = rad.read();
            byte dataL = rad.read();
            unsigned int luxVal = (dataH << 8) | dataL;

            // Save to the sensor struct we just queried!
            sensors[currentTargetSensor].lux = luxVal;
            sensors[currentTargetSensor].lastUpdateTime = millis();
            sensors[currentTargetSensor].isConnected = true;
          }
        }
      }
      // Flush any lingering garbage bytes to prevent desync
      while(rad.available() > 0) rad.read(); 
    }

    // 2. Select next Target
    // "Smart" target cycling to save RF bandwidth
    int maxTargetSensor = (currentState == VIEW_SENSOR_B) ? 5 : 2; 

    currentTargetSensor++;
    if (currentTargetSensor > maxTargetSensor) {
         currentTargetSensor = 0; 
    }
    
    // 3. Transmit Request (IDs map 0x01-0x06 based on index)
    byte targetID = currentTargetSensor + 1;
    rad.write((uint8_t)0xAA); // SOF
    rad.write((uint8_t)targetID); // Target
    rad.write((uint8_t)0x04); // CMD_READ_LUX
    rad.write((uint8_t)0x00); // Padding Data_H
    rad.write((uint8_t)0x00); // Padding Data_L
  }
}

void CCU_Master_Firmware::checkSensorTimeouts() {
    for (int i = 0; i < 6; i++) {
        // If we haven't heard from a sensor in 10 seconds, mark it offline.
        // It keeps its last known Relay state for safety.
        if (millis() - sensors[i].lastUpdateTime > TIMEOUT_MS) {
            sensors[i].isConnected = false;
        }
    }
}

void CCU_Master_Firmware::updateRelays() {
    for (int i = 0; i < 3; i++) {
        // --- Manual Override Modes ---
        if (relayMode[i] == '0') {
            digitalWrite(relayPins[i], LOW);
            sensors[i].relayState = false;
            continue;
        }
        if (relayMode[i] == '1') {
            digitalWrite(relayPins[i], HIGH);
            sensors[i].relayState = true;
            continue;
        }

        // --- Auto Mode ('A') ---
        // If Global Timer expired, shut down
        if (!isTimerActive) {
            digitalWrite(relayPins[i], LOW);
            sensors[i].relayState = false;
            continue;
        }

        if (!sensors[i].isConnected) continue; // Skip if sensor offline

        if (sensors[i].lux > roomA_lux_threshold) {
             digitalWrite(relayPins[i], LOW); 
             sensors[i].relayState = false;
        } else if (sensors[i].lux <= (roomA_lux_threshold - DEADBAND)) {
             digitalWrite(relayPins[i], HIGH);
             sensors[i].relayState = true;
        }
    }
}

void CCU_Master_Firmware::drawRelayScreen() {
    String r1 = String(relayMode[0]);
    String r2 = String(relayMode[1]);
    String r3 = String(relayMode[2]);

    displayLCD(true, 0, 0, "R1:" + r1 + "   R2:" + r2 + "    ");
    displayLCD(false, 0, 1, "R3:" + r3 + "   [1-3]?  ");
}

void CCU_Master_Firmware::processRelayInput(char key) {
    if (key == 'D') {
        lcd.noBlink();
        selectedRelay = -1;
        currentState = HOME;
        displayLCD(true, 0, 0, "[A] Room A      ");
        displayLCD(false, 0, 1, "[B] Room B      ");
    } else if (selectedRelay >= 0 && (key == '0' || key == '1' || key == 'A')) {
        // A relay is already selected, so set its mode
        lcd.noBlink();
        relayMode[selectedRelay] = key;
        selectedRelay = -1;
        drawRelayScreen();
    } else if (selectedRelay < 0 && (key == '1' || key == '2' || key == '3')) {
        // No relay selected yet, so pick one
        selectedRelay = key - '1'; // Maps '1'->0, '2'->1, '3'->2
        displayLCD(true, 0, 0, "Relay " + String((int)(selectedRelay + 1)));
        displayLCD(false, 0, 1, "0/1/A?          ");
        lcd.blink();
    }
}

// ---------------------------------------------------------
// --- The Central State Machine Tick ---
// ---------------------------------------------------------

void CCU_Master_Firmware::runState() {
  // 1. Hardware Ticks
  if (buzzerActive && (millis() - buzzerOnMillis >= 100)) {
    digitalWrite(BUZZER_PIN, LOW);   
    buzzerActive = false;
  }

  // 2. CCU Operations Ticks
  pollSensorsRF();
  checkSensorTimeouts();
  updateRelays();

  // 3. UI Ticks (Screen Updates)
  unsigned long currentMillis = millis();
  static unsigned long lastLCDTick = 0;

  switch(currentState) {
    case HOME:
      break;

    case VIEW_TIME_LEFT:
      if (currentMillis - lastLCDTick > 500) {
        lastLCDTick = currentMillis;
        // Live Countdown
        if (isTimerActive) {
          unsigned long elapsed = currentMillis - timerStartTime;
          if(elapsed < timerDurationMillis){
             unsigned long remaining = timerDurationMillis - elapsed;
             formatTimeLCD(remaining, 4, 0); 
          } else {
             isTimerActive = false;
             lcd.setCursor(4, 0); lcd.print("00:00:00");
          }
        } else {
             lcd.setCursor(4, 0); lcd.print("00:00:00");
        }
      }
      break;

    case VIEW_SENSOR_A:
      if (currentMillis - lastLCDTick > 500) {
        lastLCDTick = currentMillis;
        
        // Live Sensor Draw
        String s1 = sensors[0].isConnected ? String(sensors[0].lux) : "err";
        String s2 = sensors[1].isConnected ? String(sensors[1].lux) : "err";
        String s3 = sensors[2].isConnected ? String(sensors[2].lux) : "err";
        
        // Pad for neatness
        while(s1.length() < 3) s1 += " ";
        while(s2.length() < 3) s2 += " ";
        while(s3.length() < 3) s3 += " ";

        lcd.setCursor(0, 0);
        lcd.print("S1:" + s1 + " S2:" + s2 + " ");
        lcd.setCursor(0, 1);
        lcd.print("S3:" + s3 + "           ");
      }
      break;

    case VIEW_SENSOR_B:
       if (currentMillis - lastLCDTick > 500) {
        lastLCDTick = currentMillis;

        String s4 = sensors[3].isConnected ? String(sensors[3].lux) : "err";
        String s5 = sensors[4].isConnected ? String(sensors[4].lux) : "err";
        String s6 = sensors[5].isConnected ? String(sensors[5].lux) : "err";
        
        while(s4.length() < 3) s4 += " ";
        while(s5.length() < 3) s5 += " ";
        while(s6.length() < 3) s6 += " ";

        lcd.setCursor(0, 0); lcd.print("S4:" + s4 + "  S5:" + s5 + "  ");
        lcd.setCursor(0, 1); lcd.print("S6:" + s6 + "           ");
       }
       break;

    default:
      break;
  }
}

// ---------------------------------------------------------
// --- Keypad UI Handlers  ---
// ---------------------------------------------------------

void CCU_Master_Firmware::processTimerInput(char key) {
    if (isDigit(key) && inputBuffer.length() < 6) {
        inputBuffer += key;
    } else if (key == '*' && inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1); 
    } 
    
    // Commit the Timer and Proceed to Threshold
    else if (key == 'A' && inputBuffer.length() == 6) {
        String hStr = inputBuffer.substring(0,2);
        String mStr = inputBuffer.substring(2,4);
        String sStr = inputBuffer.substring(4,6);
        
        long inputHours = hStr.toInt();
        long inputMins = mStr.toInt();
        long inputSecs = sStr.toInt();
        
        timerDurationMillis = (inputHours * 3600000UL) + (inputMins * 60000UL) + (inputSecs * 1000UL);
        
        lcd.noBlink();
        displayLCD(true, 0, 0, hStr + "h/" + mStr + "m/" + sStr + "s");
        delay(1500);

        inputBuffer = "";
        currentState = INPUT_THRESHOLD;
        displayLCD(true, 0, 0, "Set threshold   ");
        displayLCD(false, 0, 1, "                ");
        lcd.setCursor(0, 1);
        lcd.blink();
        return; // Break out of this method so we don't redraw the timer format below
    } else if (key == 'D') {
        lcd.noBlink();
        inputBuffer = "";
        currentState = MENU_ROOM_A;
        displayLCD(true, 0, 0, "[A] Time and LUX");
        displayLCD(false, 0, 1, "[B] Check status");
        return; 
    }

    // Redraw LCD Auto-Formatting (e.g., 01/36/25)
    // We start with 4 spaces to physically overwrite the leftover "[B] " text
    String displayStr = "    ";
    
    for(int i = 0; i < inputBuffer.length(); i++) {
        displayStr += inputBuffer[i];
        if (i == 1 || i == 3) displayStr += "/";
    }
    // Pad remaining space with underscores
    while(displayStr.length() < 12) { // 4 spaces + 8 timer chars = 12 total
        displayStr += "_";
    }
    // Fully pad the rest of the 16-char line with spaces
    while(displayStr.length() < 16) displayStr += " ";

    lcd.setCursor(0, 1); 
    lcd.print(displayStr);

    // Position blinking cursor at next digit slot
    int cursorCol = 4 + inputBuffer.length();
    if (inputBuffer.length() >= 2) cursorCol++; // account for first slash
    if (inputBuffer.length() >= 4) cursorCol++; // account for second slash
    lcd.setCursor(cursorCol, 1);
}

void CCU_Master_Firmware::processThresholdInput(char key) {
    if (isDigit(key) && inputBuffer.length() < 5) { // Max 65535, so 5 digits
        inputBuffer += key;
    } else if (key == '*' && inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1); 
    } else if (key == 'A' && inputBuffer.length() > 0) {
        lcd.noBlink();
        roomA_lux_threshold = inputBuffer.toInt();
        
        // Start the freshly minted timer too!
        isTimerActive = true;
        timerStartTime = millis();

        displayLCD(true, 0, 0, "LUX set " + String(roomA_lux_threshold));
        delay(1500);

        inputBuffer = "";
        currentState = MENU_ROOM_A;
        displayLCD(true, 0, 0, "[A] Time and LUX");
        displayLCD(false, 0, 1, "[B] Check status");
        return;
    } else if (key == 'D') {
        lcd.noBlink();
        inputBuffer = "";
        currentState = MENU_ROOM_A;
        displayLCD(true, 0, 0, "[A] Time and LUX");
        displayLCD(false, 0, 1, "[B] Check status");
        return;
    }

    // Redraw Threshold UI
    String displayStr = inputBuffer;
    // Pad rest of the line with spaces to erase "[B] "
    while(displayStr.length() < 16) displayStr += " "; 
    
    lcd.setCursor(0, 1); 
    lcd.print(displayStr);

    // Position blinking cursor at next digit slot
    lcd.setCursor(inputBuffer.length(), 1);
}

void CCU_Master_Firmware::keypadPress() {
  char key = k.getKey();

  if (key) {
    digitalWrite(BUZZER_PIN, HIGH);               
    buzzerOnMillis = millis();           
    buzzerActive = true; 

    switch (currentState) {
      case HOME:
        if (key == 'A') {
          currentState = MENU_ROOM_A;
          displayLCD(true, 0, 0, "[A] Time and LUX");
          displayLCD(false, 0, 1, "[B] Check status");
        } else if (key == 'B') {
          currentState = VIEW_SENSOR_B;
          displayLCD(true, 0, 0, "S4:---  S5:---  ");
          displayLCD(false, 0, 1, "S6:---          ");
        } else if (key == 'C') {
          currentState = MANUAL_RELAY;
          selectedRelay = -1;
          drawRelayScreen();
        }
        break;

      case MENU_ROOM_A:
        if (key == 'A') {
          currentState = INPUT_TIMER;
          inputBuffer = "";
          displayLCD(true, 0, 0, "Set Timer(H/M/S)");
          displayLCD(false, 0, 1, "    __/__/__    ");
          lcd.setCursor(4, 1);
          lcd.blink();
        } else if (key == 'B') {
          currentState = MENU_STATUS_A;
          displayLCD(true, 0, 0, "[A] Check timer "); 
          displayLCD(false, 0, 1, "[B] View sensor ");
        } else if (key == 'D') {
          currentState = HOME;
          displayLCD(true, 0, 0, "[A] Room A");
          displayLCD(false, 0, 1, "[B] Room B");
        }
        break;

      case MENU_STATUS_A:
        if (key == 'A') {
          currentState = VIEW_TIME_LEFT;
          displayLCD(true, 0, 0, "    00:00:00    "); // Initial placeholder
          displayLCD(false, 0, 1, "                "); 
        } else if (key == 'B') {
          currentState = VIEW_SENSOR_A;
          displayLCD(true, 0, 0, "S1:--- S2:---   ");
          displayLCD(false, 0, 1, "S3:---          ");
        } else if (key == 'D') {
          currentState = MENU_ROOM_A;
          displayLCD(true, 0, 0, "[A] Time and LUX");
          displayLCD(false, 0, 1, "[B] Check status");
        }
        break;

      case INPUT_TIMER:
        processTimerInput(key);
        break;

      case INPUT_THRESHOLD:
        processThresholdInput(key);
        break;

      case VIEW_TIME_LEFT:
        if (key == 'C') {
          isTimerActive = false;
          timerDurationMillis = 0;
          displayLCD(true, 0, 0, "Timer cancelled!");
          displayLCD(false, 0, 1, "                ");
          delay(1500);
          currentState = MENU_STATUS_A;
          displayLCD(true, 0, 0, "[A] Check timer "); 
          displayLCD(false, 0, 1, "[B] View sensor ");
        } else if (key == 'D') {
          currentState = MENU_STATUS_A;
          displayLCD(true, 0, 0, "[A] Check timer "); 
          displayLCD(false, 0, 1, "[B] View sensor ");
        }
        break;

      case VIEW_SENSOR_A:
        if (key == 'D') {
          currentState = MENU_STATUS_A;
          displayLCD(true, 0, 0, "[A] Check timer "); 
          displayLCD(false, 0, 1, "[B] View sensor ");
        }
        break;

      case VIEW_SENSOR_B:
        if (key == 'D') {
          currentState = HOME;
          displayLCD(true, 0, 0, "[A] Room A");
          displayLCD(false, 0, 1, "[B] Room B");
        }
        break;

      case MANUAL_RELAY:
        processRelayInput(key);
        break;

      default:
        break;
    }
  }
}
