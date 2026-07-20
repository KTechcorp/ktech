#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

LiquidCrystal_I2C lcd(0x26, 20, 4);

#define SS_PIN   10
#define RST_PIN  9
MFRC522 rfid(SS_PIN, RST_PIN);

#define HALL_PIN   A0
#define BUZZER_PIN 4
#define BTN_NEXT   5
#define BTN_PREV   6
#define BTN_SELECT 7

const int MPU_ADDR = 0x68;
int16_t ax, ay, az, gx, gy, gz;

// --- STATE ---
bool bikeLocked = true;

float wheelCirc = 2.1;
float currentSpeed = 0;
float maxSpeed = 0;
float tripDistance = 0;

unsigned long lastPulseTime = 0;
unsigned long lastMovingUpdate = 0;
unsigned long movingSeconds = 0;

byte profileWeight = 70;

String directionState = "Stopped";

int menuIndex = 0;
int maxMenuIndex = 2;

unsigned long lastBtnTime = 0;
const unsigned long btnLockout = 120;

enum MenuState {
  MENU_NAV,
  MENU_WAIT_RELEASE,
  SCREEN_ACTIVE
};

MenuState menuState = MENU_NAV;

// --- RFID CONFIG ---
const byte RFID_BLOCK = 5;
byte unlockCode[4] = {0,0,0,0};
bool unlockCodeSet = false;

// --- ALARM SYSTEM ---
void alarmBlink() {
  // Blink LCD + ALARM text + buzzer
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(5,1);
  lcd.print("!!! ALARM !!!");
  beep(2500,200);
  delay(120);

  lcd.noBacklight();
  delay(120);
}

void antiTheftCheck() {
  if (!bikeLocked) return;

  float axG = ax / 16384.0;
  float ayG = ay / 16384.0;

  if (abs(axG) > 0.25 || abs(ayG) > 0.25) {
    alarmBlink();
  }
}

// --- UTILS ---
bool readButton(int pin) {
  return digitalRead(pin) == LOW;
}

void beep(int f, int d) {
  tone(BUZZER_PIN, f, d);
}

void readMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  ax = Wire.read() << 8 | Wire.read();
  ay = Wire.read() << 8 | Wire.read();
  az = Wire.read() << 8 | Wire.read();
  gx = Wire.read() << 8 | Wire.read();
  gy = Wire.read() << 8 | Wire.read();
  gz = Wire.read() << 8 | Wire.read();
}

void updateDirection() {
  if (currentSpeed <= 0.3) {
    directionState = "Stopped";
    return;
  }
  float gzDeg = gz / 131.0;
  if (gzDeg > 20) directionState = "Right";
  else if (gzDeg < -20) directionState = "Left";
  else directionState = "Straight";
}

void handleHall() {
  int val = analogRead(HALL_PIN);
  static int lastVal = 0;

  if (val > 600 && lastVal <= 600) {
    unsigned long now = millis();
    float dt = (now - lastPulseTime) / 1000.0;
    if (dt > 0.1) {
      currentSpeed = (wheelCirc / dt) * 3.6;
      if (currentSpeed > maxSpeed) maxSpeed = currentSpeed;
      tripDistance += wheelCirc;
    }
    lastPulseTime = now;
  }
  lastVal = val;

  if (currentSpeed > 0.3) {
    unsigned long now = millis();
    if (now - lastMovingUpdate >= 1000) {
      movingSeconds++;
      lastMovingUpdate = now;
    }
  } else {
    currentSpeed = 0;
  }
}

// --- RFID LOW LEVEL ---
bool authBlock(byte blockAddr) {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte trailerBlock = (blockAddr / 4) * 4 + 3;

  MFRC522::StatusCode status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    trailerBlock,
    &key,
    &(rfid.uid)
  );
  return status == MFRC522::STATUS_OK;
}

bool readRFIDBlock(byte blockAddr, byte *dataOut) {
  if (!authBlock(blockAddr)) return false;

  byte buffer[18];
  byte size = 18;

  if (rfid.MIFARE_Read(blockAddr, buffer, &size) != MFRC522::STATUS_OK)
    return false;

  for (int i = 0; i < 16; i++)
    dataOut[i] = buffer[i];

  return true;
}

bool writeRFIDBlock(byte blockAddr, byte *dataIn) {
  if (!authBlock(blockAddr)) return false;

  MFRC522::StatusCode status = rfid.MIFARE_Write(blockAddr, dataIn, 16);
  return status == MFRC522::STATUS_OK;
}

// --- WRONG CARD SCREEN ---
void wrongCardScreen() {
  lcd.clear();
  lcd.setCursor(3,1);
  lcd.print("NOT CORRECT");
  lcd.setCursor(6,2);
  lcd.print("CARD");
  beep(400,200);
  delay(900);
}

// --- LOCK / UNLOCK ---
void unlockBike() {
  bikeLocked = false;
  lcd.clear();
  lcd.setCursor(5,1);
  lcd.print("Dismater");
  lcd.setCursor(4,2);
  lcd.print("Bike Unlocked");
  beep(1500,150);
  delay(600);
}

void lockBike() {
  bikeLocked = true;
  lcd.clear();
  lcd.setCursor(5,1);
  lcd.print("Dismater");
  lcd.setCursor(4,2);
  lcd.print("Bike Locked");
  beep(900,150);
  delay(600);
}

// --- RFID DETECT ANIM ---
void rfidDetectedAnim() {
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(15,1);
    lcd.write(byte(6));
    delay(120);
    lcd.setCursor(15,1);
    lcd.print(" ");
    delay(120);
  }
}

// --- RFID TOGGLE ---
void handleRFIDToggle() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  byte data[16];
  if (!readRFIDBlock(RFID_BLOCK, data)) {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  if (data[0] != 'B') {
    wrongCardScreen();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  if (!unlockCodeSet) {
    for (int i = 0; i < 4; i++) unlockCode[i] = data[i+1];
    unlockCodeSet = true;
  }

  for (int i = 0; i < 4; i++) {
    if (data[i+1] != unlockCode[i]) {
      wrongCardScreen();
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      return;
    }
  }

  profileWeight = data[5];

  rfidDetectedAnim();

  if (bikeLocked) unlockBike();
  else            lockBike();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// --- CREATE PROFILE CARD ---
void createProfileCard() {
  lcd.clear();
  lcd.setCursor(3,0); lcd.print("Dismater");
  lcd.setCursor(1,1); lcd.print("Scan new card");
  lcd.setCursor(1,2); lcd.print("for profile+code");

  while (true) {
    if (!rfid.PICC_IsNewCardPresent()) continue;
    if (!rfid.PICC_ReadCardSerial()) continue;

    for (int i = 0; i < 4; i++) {
      unlockCode[i] = (byte)random(1, 255);
    }
    unlockCodeSet = true;

    byte blockData[16];
    blockData[0] = 'B';
    blockData[1] = unlockCode[0];
    blockData[2] = unlockCode[1];
    blockData[3] = unlockCode[2];
    blockData[4] = unlockCode[3];
    blockData[5] = profileWeight;
    for (int i = 6; i < 16; i++) blockData[i] = 0x00;

    if (!writeRFIDBlock(RFID_BLOCK, blockData)) {
      lcd.clear();
      lcd.setCursor(2,1); lcd.print("WRITE FAILED");
      beep(400,200);
      delay(1000);
    } else {
      lcd.clear();
      lcd.setCursor(3,1); lcd.print("CARD PROGRAMMED");
      rfidDetectedAnim();
      delay(800);
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    break;
  }
}

// --- BOOT ANIM ---
void slideLogo() {
  lcd.clear();
  String title = "Dismater";

  for (int pos = -10; pos <= 6; pos++) {
    lcd.clear();
    lcd.setCursor(pos, 0);
    lcd.print(title);
    delay(60);
  }
}

void bootProgress() {
  lcd.setCursor(2,3);
  lcd.print("[              ]");

  for (int i = 0; i < 14; i++) {
    lcd.setCursor(3 + i, 3);
    lcd.print((char)255);
    beep(1000 + (i * 40), 40);
    delay(80);
  }
}

void showDismaterBoot() {
  lcd.clear();

  slideLogo();

  String tagline = "Bike Beter";
  for (int i = 0; i <= tagline.length(); i++) {
    lcd.setCursor(4,1);
    lcd.print(tagline.substring(0, i));
    delay(90);
  }

  bootProgress();

  lcd.clear();
  lcd.setCursor(4,2);
  lcd.print("Ready...");
  delay(600);
}

// --- SCREENS ---
void screenLocked() {
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("Dismater");
  lcd.setCursor(3,2);
  lcd.print("Scan card to");
  lcd.setCursor(6,3);
  lcd.print("unlock/lock");
}

void screenSpeed() {
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("Dismater");

  lcd.setCursor(0,1);
  lcd.print("Spd: ");
  lcd.print(currentSpeed,1);

  lcd.setCursor(0,2);
  lcd.print("Max: ");
  lcd.print(maxSpeed,1);

  lcd.setCursor(0,3);
  lcd.print("Dir: ");
  lcd.print(directionState);
}

void screenTripMax() {
  lcd.clear();
  lcd.setCursor(4,0); lcd.print("Dismater");

  lcd.setCursor(0,1);
  lcd.print("Trip: ");
  lcd.print(tripDistance/1000.0,2);

  lcd.setCursor(0,2);
  lcd.print("Max: ");
  lcd.print(maxSpeed,1);

  lcd.setCursor(0,3);
  lcd.print("Wt: ");
  lcd.print(profileWeight);
  lcd.print("kg");
}

void screenSetWeight() {
  lcd.clear();
  lcd.setCursor(4,0); lcd.print("Dismater");
  lcd.setCursor(0,1); lcd.print("SET WEIGHT");

  while (true) {
    lcd.setCursor(0,2);
    lcd.print("Weight: ");
    lcd.print(profileWeight);
    lcd.print("kg   ");

    if (readButton(BTN_NEXT)) {
      profileWeight++;
      beep(1200,100);
      delay(150);
    }

    if (readButton(BTN_PREV)) {
      if (profileWeight > 1) profileWeight--;
      beep(900,100);
      delay(150);
    }

    if (readButton(BTN_SELECT)) {
      beep(1500,150);
      break;
    }

    delay(50);
  }
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("Dismater");

  lcd.setCursor(0,1);
  lcd.print("Speed");
  lcd.setCursor(19,1);
  if (menuIndex == 0) lcd.write(byte(5));

  lcd.setCursor(0,2);
  lcd.print("Trip/Max");
  lcd.setCursor(19,2);
  if (menuIndex == 1) lcd.write(byte(5));

  lcd.setCursor(0,3);
  lcd.print("Weight");
  lcd.setCursor(19,3);
  if (menuIndex == 2) lcd.write(byte(5));
}

// --- BUTTONS ---
void handleButtons() {
  if (bikeLocked) return;

  unsigned long now = millis();
  if (now - lastBtnTime < btnLockout) return;

  if (readButton(BTN_NEXT)) {
    lastBtnTime = now;
    if (menuState == MENU_NAV) {
      menuIndex++;
      if (menuIndex > maxMenuIndex) menuIndex = 0;
      beep(1200,100);
      showMenu();
    }
    return;
  }

  if (readButton(BTN_PREV)) {
    lastBtnTime = now;
    if (menuState == MENU_NAV) {
      menuIndex--;
      if (menuIndex < 0) menuIndex = maxMenuIndex;
      beep(900,100);
      showMenu();
    }
    return;
  }

  if (readButton(BTN_SELECT)) {
    lastBtnTime = now;
    beep(1500,120);

    if (menuState == MENU_NAV) {
      menuState = MENU_WAIT_RELEASE;
    }
    else if (menuState == SCREEN_ACTIVE) {
      menuState = MENU_NAV;
      showMenu();
    }
    return;
  }

  if (menuState == MENU_WAIT_RELEASE && !readButton(BTN_SELECT)) {
    menuState = SCREEN_ACTIVE;
    lcd.clear();
  }
}

// --- SETUP ---
void setup() {
  randomSeed(analogRead(A1));

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(HALL_PIN, INPUT);

  Wire.begin();
  lcd.begin();
  lcd.backlight();

  SPI.begin();
  rfid.PCD_Init();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  showDismaterBoot();

  bikeLocked = true;
  menuState = MENU_NAV;
  menuIndex = 0;

  screenLocked();
}

// --- LOOP ---
void loop() {
  readMPU();
  updateDirection();
  handleHall();

  handleRFIDToggle();

  if (bikeLocked) {
    antiTheftCheck();
    screenLocked();
    return;
  }

  lcd.backlight();

  handleButtons();

  if (menuState == MENU_NAV) {
    showMenu();
  }
  else if (menuState == SCREEN_ACTIVE) {
    if (menuIndex == 0) screenSpeed();
    if (menuIndex == 1) screenTripMax();
    if (menuIndex == 2) {
      screenSetWeight();
      menuState = MENU_NAV;
      showMenu();
    }
  }

  delay(40);
}
