#include <LedControl.h>

// Hardware pins
#define BTN_PIN 4
#define BUZZ_PIN 3
#define LED_PIN 2
#define DIN_PIN 12
#define CLK_PIN 11
#define CS_PIN 10
#define BATT_PIN A0

// Timing constants
#define LONG_PRESS_MS 3000
#define BATT_CHECK_MS 10000
#define LOW_BATT_TH 2.8f  // Voltage threshold

// Display related
#define HIGH_INTENSITY 12 // Max = 15
#define LOW_INTENSITY 2

// Available timer durations in seconds
//const int TIMES[] PROGMEM = {15,30,45,60,90,120,150,180,240,300};
const int TIMES[] = {15,30,45,60,90,120,180,240,300};
const int NUM_TIMES = 9;

// Application states
enum State {
  SELECT_TIME,
  SELECT_MODE,
  RUNNING,
  EXPIRED,
  LOW_BATT
};

// Display modes
enum DisplayMode {
  NUMERIC,
  PROGBAR,
  SAND
};

// Function prototypes
void drawTwoDigits(int leftDig, int rightDig);
void drawDigitLeft(int dig);
void drawMRight();
void initSand();
void moveOneGrain();
void drawBatteryLow(unsigned long now);
void drawSetupTime(int secs);
void drawSetupMode(DisplayMode mode);

// Global state variables
State currentState = SELECT_TIME;
int selectedTimeIdx = 1;  // Default 30s (index 1)
DisplayMode selectedMode = NUMERIC;
long totalMs;
long startMs;
long remainingMs;
long pressStartMs = 0;
bool buttonPressed = false;
long lastBattMs = 0;
float battV = 3.2f;
long lastStepMs = 0;
long tickMs;
byte sandRows[8];  // Bitmask for sand simulation (bit 7 = col 0 left)

// MAX7219 instance
LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 1);

// Compact 3x5 font (cols0-2 left, col3 blank, cols4-6 right)
byte font[11][8] = {
  {0x07,0x05,0x05,0x05,0x07,0x00,0x00,0x00}, // 0
  {0x02,0x06,0x02,0x02,0x07,0x00,0x00,0x00}, // 1
  {0x07,0x01,0x07,0x04,0x07,0x00,0x00,0x00}, // 2
  {0x07,0x01,0x07,0x01,0x07,0x00,0x00,0x00}, // 3
  {0x05,0x05,0x07,0x01,0x01,0x00,0x00,0x00}, // 4
  {0x07,0x04,0x07,0x01,0x07,0x00,0x00,0x00}, // 5
  {0x07,0x04,0x07,0x05,0x07,0x00,0x00,0x00}, // 6
  {0x07,0x01,0x02,0x02,0x02,0x00,0x00,0x00}, // 7
  {0x07,0x05,0x07,0x05,0x07,0x00,0x00,0x00}, // 8
  {0x07,0x05,0x07,0x01,0x07,0x00,0x00,0x00}, // 9
  {0x02,0x02,0x04,0x00,0x00,0x00,0x00,0x00}  // prime (4col low bits)
};
#define CHAR_PRIME 10

byte reverseBits(byte b) {
  b = ((b >> 1) & 0x55) | ((b << 1) & 0xAA);
  b = ((b >> 2) & 0x33) | ((b << 2) & 0xCC);
  b = ((b >> 4) & 0x0F) | ((b << 4) & 0xF0);
  return b;
}

void setup() {
  randomSeed(analogRead(0));
  Serial.begin(9600);
  Serial.println(F("BBX Game Timer v1.0 - Boot"));

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  lc.shutdown(0, false);
  lc.setIntensity(0, 8);
  lc.clearDisplay(0);

  // All on test
  for (byte r = 0; r < 8; r++) lc.setRow(0, r, 0xFF);
  delay(500);
  lc.clearDisplay(0);

  // "42" test
  // Serial.println(F("Test 42"));
  // drawTwoDigits(4, 2);
  // delay(3000);
  // lc.clearDisplay(0);

  Serial.println(F("Ready"));
}

void loop() {
  unsigned long now = millis();

  handleButton(now);

  // Battery check
  if (now - lastBattMs > BATT_CHECK_MS) {
    battV = readBatteryVoltage();
    lastBattMs = now;
    if (battV > 0.8f && battV < LOW_BATT_TH && currentState != LOW_BATT) {
      currentState = LOW_BATT;
      Serial.print(F("Low batt: ")); Serial.println(battV, 2);
    }
    Serial.print(F("Batt: ")); Serial.println(battV, 2);
  }

  switch (currentState) {
    case SELECT_TIME:
      drawSetupTime(TIMES[selectedTimeIdx]);
      break;
    case SELECT_MODE:
      drawSetupMode(selectedMode);
      break;
    case RUNNING:
      remainingMs = totalMs - (now - startMs);
      if (remainingMs <= 0) {
        currentState = EXPIRED;
        digitalWrite(BUZZ_PIN, HIGH);
        delay(1000);
        digitalWrite(BUZZ_PIN, LOW);
        Serial.println(F("Expired"));
      } else {
        updateDisplay(now);
      }
      break;
    case EXPIRED:
      digitalWrite(LED_PIN, (now / 1000) % 2);
      break;
    case LOW_BATT:
      drawBatteryLow(now);
      break;
  }

  delay(20);
}

float readBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(BATT_PIN);
    delayMicroseconds(10);
  }
  float adcAvg = sum / 16.0f;
  float vadc = (adcAvg / 1023.0f) * 5.0f;
  return vadc * 1.47f;
}

void handleButton(unsigned long now) {
  bool btnLow = (digitalRead(BTN_PIN) == LOW);

  if (!buttonPressed && btnLow) {
    pressStartMs = now;
    buttonPressed = true;
  } else if (buttonPressed && !btnLow) {
    unsigned long pressDur = now - pressStartMs;
    if (pressDur < LONG_PRESS_MS) onTap();
    buttonPressed = false;
  } else if (buttonPressed && (now - pressStartMs >= LONG_PRESS_MS)) {
    onLongPress();
    buttonPressed = false;
  }
}

void onTap() {
  switch (currentState) {
    case SELECT_TIME:
      selectedTimeIdx = (selectedTimeIdx + 1) % NUM_TIMES;
      break;
    case SELECT_MODE:
      selectedMode = DisplayMode((int(selectedMode) + 1) % 3);
      break;
    case RUNNING:
      totalMs = (unsigned long) TIMES[selectedTimeIdx] * 1000UL;
      startMs = millis();
      initSand();
      break;
  }
}

void onLongPress() {
  switch (currentState) {
    case SELECT_TIME:
      currentState = SELECT_MODE;
      break;
    case SELECT_MODE:
      totalMs = (unsigned long)TIMES[selectedTimeIdx] * 1000UL;
      startMs = millis();
      initSand();
      currentState = RUNNING;
      Serial.print(F("Start ")); Serial.print(totalMs/1000); Serial.print("s mode "); Serial.println(selectedMode);
      break;
    case EXPIRED:
      totalMs = (unsigned long)TIMES[selectedTimeIdx] * 1000UL;
      startMs = millis();
      initSand();
      currentState = RUNNING;
      break;
  }
}

void drawSetupTime(int secs) {
  lc.clearDisplay(0);
  if (secs <= 90) {
    int tens = secs / 10;
    int ones = secs % 10;
    drawTwoDigits(tens, ones);
  } else {
    int mins = secs / 60;
    drawTwoDigits(mins, CHAR_PRIME);
  }
  lc.setIntensity(0, (millis() / 500) % 2 ? HIGH_INTENSITY : LOW_INTENSITY);
//  if ((millis() / 1000) % 2 == 0) lc.setRow(0, 7, 0xFF);
}

void drawSetupMode(DisplayMode mode) {
  lc.clearDisplay(0);
  byte modeIcons[3][8] = {    
    {0x24, 0x24, 0xFF, 0x24, 0x24, 0xFF, 0x24, 0x24},  // #
    {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00},  // horiz rect
    {0xFF, 0x7E, 0x3C, 0x18, 0x18, 0x3C, 0x7E, 0xFF}   // hourglass
  };
  for (int r = 0; r < 8; r++) {
    lc.setRow(0, 7-r, reverseBits(modeIcons[int(mode)][r]));
  }
  lc.setIntensity(0, (millis() / 500) % 2 ? HIGH_INTENSITY : LOW_INTENSITY);
}

void updateDisplay(unsigned long now) {
  lc.clearDisplay(0);
  lc.setIntensity(0, 8);

  switch (selectedMode) {
    case NUMERIC:
      int secs = remainingMs / 1000UL;
      if (secs <= 90) {
        int tens = secs / 10;
        int ones = secs % 10;
        drawTwoDigits(tens, ones);
      } else {
        int mins = secs / 60;
        drawDigitLeft(mins);
        drawMRight();
      }
      break;
    case PROGBAR:
      unsigned long elapsedMs = totalMs - remainingMs;
      int pixels = (elapsedMs * 32UL) / totalMs;
      int fullCols = pixels / 4;
      int remPx = pixels % 4;
      for (int c = 0; c < fullCols; c++) {
        for (int rr = 0; rr < 4; rr++) {
          lc.setLed(0, 2 + rr, c, true);
        }
      }
      for (int rr = 0; rr < remPx; rr++) {
        lc.setLed(0, 2 + rr, fullCols, true);
      }
      break;
    case SAND:
      if (now - lastStepMs >= tickMs) {
        moveOneGrain();
        lastStepMs = now;
      }
      for (int r = 0; r < 8; r++) {
        lc.setRow(0, r, sandRows[r]);
      }
      break;
  }
}

void drawTwoDigits(int leftDig, int rightDig) {
  for (int row = 0; row < 8; row++) {
    byte bits = ((font[leftDig][row] & 0x07) << 4) | ((font[rightDig][row] & 0x07));
    lc.setRow(0, 7-row, reverseBits(bits));
  }
}

void drawDigitLeft(int dig) {
  for (int row = 0; row < 8; row++) {
    byte bits = (font[dig][row] & 0x07) << 4;
    lc.setRow(0, 7-row, reverseBits(bits));
  }
}

void drawMRight() {
  for (int row = 0; row < 8; row++) {
    byte bits = font[10][row] & 0x07;
    lc.setRow(0, 7-row, reverseBits(bits));
  }
}

void initSand() {
  tickMs = totalMs / 128UL;
  lastStepMs = millis();
  memset(sandRows, 0, sizeof(sandRows));
  for (int r = 0; r < 4; r++) sandRows[r] = 0xFF;
}

void moveOneGrain() {
  int col = random(8);
  byte mask = 1 << (7 - col);
  for (int row = 3; row >= 0; row--) {
    if (sandRows[row] & mask) {
      sandRows[row] &= ~mask;
      sandRows[row + 1] |= mask;
      return;
    }
  }
}

void drawBatteryLow(unsigned long now) {
  lc.clearDisplay(0);
  byte battIcon[8] = {0x3C,0x42,0x81,0x81,0x81,0x42,0x3C,0x00};
  for (int r = 0; r < 8; r++) lc.setRow(0, r, battIcon[r]);
  lc.setIntensity(0, (now / 500) % 2 ? 15 : 2);
}
