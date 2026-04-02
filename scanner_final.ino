#include <SPI.h>
#define MFRC522_SPICLOCK 1000000U
#include <MFRC522.h>
#include <LiquidCrystal.h>

// =====================================================
// RC522 RFID Reader
// =====================================================
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);

// =====================================================
// LCD1602 (parallel mode)
// RS, E, D4, D5, D6, D7
// =====================================================
LiquidCrystal lcd(2, 3, 4, 6, 7, 8);

// =====================================================
// Outputs
// =====================================================
#define GREEN_LED A0
#define RED_LED   A1
#define BUZZER    A2
#define RELAY_PIN A3

// =====================================================
// Relay / Lock settings
// =====================================================
// Your relay is active LOW if the relay LED turns on when the pin is LOW.
// Keep this as true unless testing proves otherwise.
const bool RELAY_ACTIVE_LOW = true;

// Time door stays unlocked after authorised access
const unsigned long UNLOCK_MS = 3000;

// =====================================================
// Security / reliability settings
// =====================================================
const int MAX_FAILED_ATTEMPTS = 3;
const unsigned long LOCKOUT_MS = 10000;       // 10 seconds
const unsigned long READER_RESET_MS = 15000;  // Reinitialise RFID reader if idle too long

// =====================================================
// System state
// =====================================================
int failedAttempts = 0;
bool isLockedOut = false;
unsigned long lockoutUntil = 0;
unsigned long lastSuccessfulReadMs = 0;

// =====================================================
// Authorised UIDs
// =====================================================
// Current authorised card: 16 73 8A 40
byte authorisedUIDs[][4] = {
  {0x16, 0x73, 0x8A, 0x40}
};

const int AUTHORISED_UID_COUNT = sizeof(authorisedUIDs) / sizeof(authorisedUIDs[0]);

// =====================================================
// Relay control
// =====================================================
void relayOn() {
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
}

void relayOff() {
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
}

// Confirmed behaviour from your testing:
// relay ON  -> lock unlocked
// relay OFF -> lock locked
void lockDoor() {
  relayOff();
}

void unlockDoor() {
  relayOn();
}

void unlockDoorTimed(unsigned long durationMs) {
  unlockDoor();
  delay(durationMs);
  lockDoor();
}

// =====================================================
// Output helper functions
// =====================================================
void allOutputsOff() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  noTone(BUZZER);
}

void showIdleScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan RFID tag");
  lcd.setCursor(0, 1);
  lcd.print("Door locked");
}

void showGrantedScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access Granted");
  lcd.setCursor(0, 1);
  lcd.print("Door unlocked");
}

void showDeniedScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access Denied");
  lcd.setCursor(0, 1);
  lcd.print("Try again");
}

void showLockoutScreen(unsigned long remainingSec) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM LOCKED");
  lcd.setCursor(0, 1);
  lcd.print("Wait ");
  lcd.print(remainingSec);
  lcd.print(" sec");
}

void successFeedback() {
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);

  tone(BUZZER, 2000);
  delay(150);
  noTone(BUZZER);

  delay(300);
  digitalWrite(GREEN_LED, LOW);
}

void deniedFeedback() {
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);

  tone(BUZZER, 500);
  delay(300);
  noTone(BUZZER);

  delay(300);
  digitalWrite(RED_LED, LOW);
}

void lockoutFeedback() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED, HIGH);
    tone(BUZZER, 800);
    delay(150);

    digitalWrite(RED_LED, LOW);
    noTone(BUZZER);
    delay(150);
  }
}

// =====================================================
// RFID helper functions
// =====================================================
void printUIDToSerial() {
  Serial.print("UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

bool isAuthorised() {
  // This project currently expects 4-byte UIDs
  if (mfrc522.uid.size != 4) {
    return false;
  }

  for (int user = 0; user < AUTHORISED_UID_COUNT; user++) {
    bool match = true;
    for (byte i = 0; i < 4; i++) {
      if (mfrc522.uid.uidByte[i] != authorisedUIDs[user][i]) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }

  return false;
}

void triggerLockout() {
  isLockedOut = true;
  lockoutUntil = millis() + LOCKOUT_MS;

  Serial.println("LOCKOUT TRIGGERED: 3 consecutive failed attempts.");
  showLockoutScreen(LOCKOUT_MS / 1000);
  lockoutFeedback();
  lockDoor();
}

void resetReaderIfNeeded() {
  if (millis() - lastSuccessfulReadMs > READER_RESET_MS) {
    mfrc522.PCD_Init();
    Serial.println("RC522 reinitialised.");
    lastSuccessfulReadMs = millis();
  }
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(9600);
  SPI.begin();

  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  allOutputsOff();

  // Start system in locked state
  lockDoor();

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RFID Entry");
  lcd.setCursor(0, 1);
  lcd.print("Initialising");
  delay(1200);

  mfrc522.PCD_Init();
  lastSuccessfulReadMs = millis();

  Serial.println("RFID Access Control System Ready");
  Serial.println("Authorised UID: 16 73 8A 40");
  Serial.println("Lockout after 3 consecutive failed attempts.");
  Serial.println("Door starts LOCKED.");

  showIdleScreen();
}

// =====================================================
// Main loop
// =====================================================
void loop() {
  // ---------------------------
  // Lockout mode
  // ---------------------------
  if (isLockedOut) {
    if (millis() >= lockoutUntil) {
      isLockedOut = false;
      failedAttempts = 0;
      Serial.println("Lockout ended. System reset.");
      lockDoor();
      showIdleScreen();
      allOutputsOff();
    } else {
      static unsigned long lastLockoutUpdate = 0;
      if (millis() - lastLockoutUpdate > 1000) {
        lastLockoutUpdate = millis();
        unsigned long remaining = (lockoutUntil - millis()) / 1000;
        showLockoutScreen(remaining);
      }
      return;
    }
  }

  // ---------------------------
  // Keep RC522 stable
  // ---------------------------
  resetReaderIfNeeded();

  // ---------------------------
  // Wait for card
  // ---------------------------
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  lastSuccessfulReadMs = millis();
  printUIDToSerial();

  // ---------------------------
  // Check authorisation
  // ---------------------------
  if (isAuthorised()) {
    Serial.println("Access Granted");
    failedAttempts = 0;

    showGrantedScreen();
    successFeedback();

    // Unlock briefly, then relock
    unlockDoorTimed(UNLOCK_MS);

  } else {
    failedAttempts++;
    Serial.print("Access Denied (failed attempts: ");
    Serial.print(failedAttempts);
    Serial.println(")");

    showDeniedScreen();
    deniedFeedback();
    lockDoor();

    if (failedAttempts >= MAX_FAILED_ATTEMPTS) {
      triggerLockout();
    }
  }

  // ---------------------------
  // End RFID communication
  // ---------------------------
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(500);

  if (!isLockedOut) {
    lockDoor();
    showIdleScreen();
  }
}