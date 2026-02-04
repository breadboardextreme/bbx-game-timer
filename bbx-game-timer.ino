#include <LedControl.h>

// Hardware pins
#define BTN_PIN 2
#define BUZZ_PIN 3
#define LED_PIN 4
#define DIN_PIN 12
#define CLK_PIN 11
#define CS_PIN 10
#define BATT_PIN A0

// Timing constants
#define LONG_PRESS_MS 3000
#define BATT_CHECK_MS 10000
#define LOW_BATT_TH 2.8f  // Voltage threshold

// Available timer durations in seconds
const int TIMES[] PROGMEM = {15,30,45,60,90,120,150,180,240,300};
const int NUM_TIMES = 10;

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

// Global state variables
State currentState = SELECT_TIME;
int selectedTimeIdx = 1;  // Default 30s (index 1)
DisplayMode selectedMode = NUMERIC;
unsigned long totalMs;
unsigned long startMs;
unsigned long remainingMs;
unsigned long pressStartMs = 0;
bool buttonPressed = false;
unsigned long lastBattMs = 0;
float battV = 3.2f;
unsigned long lastStepMs = 0;
unsigned long tickMs;
byte sandRows[8];  // Bitmask for sand simulation (bit 7 = col 0 left)

// MAX7219 instance
LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 1);

// 8x8 font for digits 0-9 and 'm' (index 10)
byte font[11][8] = {
  {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}, // 0
  {0x08,0x18,0x28,0x08,0x08,0x08,0x3E,0x00}, // 1
  {0x3C,0x42,0x02,0x1C,0x20,0x42,0x3E,0x00}, // 2
  {0x3E,0x02,0x1C,0x02,0x02,0x42,0x3C,0x00}, // 3
  {0x04,0x0C,0x14,0x24,0x3E,0x04,0x04,0x00}, // 4
  {0x3E,0x20,0x3C,0x02,0x02,0x42,0x3C,0x00}, // 5
  {0x1C,0x20,0x3C,0x22,0x42,0x42,0x3C,0x00}, // 6
  {0x3E,0x02,0x04,0x08,0x10,0x10,0x10,0x00}, // 7
  {0x1C,0x22,0x22,0x1C,0x22,0x22,0x1C,0x00}, // 8
  {0x1C,0x22,0x22,0x1E,0x02,0x42,0x3C,0x00}, // 9
  {0x00,0x00,0x18,0x24,0x42,0x00,0x00,0x00}  // m (simple)
};

void setup() {
  // Seed random for sand mode
  randomSeed(analogRead(0));

  // Initialize serial for debug (optional)
  Serial.begin(9600);
  Serial.println(F("BBX Game Timer v1.0 - Boot"));

  // Pin modes
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  // Initialize MAX7219
  lc.shutdown(0, false);
  lc.setIntensity(0, 8);  // Max brightness
  lc.clearDisplay(0);

  // Boot test: all LEDs on briefly
  for (byte r = 0; r < 8; r++) {
    lc.setRow(0, r, 0xFF);
  }
  delay(500);
  lc.clearDisplay(0);

  Serial.println(F("Ready - Tap button to select time"));
}

void loop() {
  unsigned long now = millis();

  // Always handle button input
  handleButton(now);

  // Periodic battery check
  if (now - lastBattMs > BATT_CHECK_MS) {
    battV = readBatteryVoltage();
    lastBattMs = now;
    if (battV < LOW_BATT_TH && currentState != LOW_BATT) {
      currentState = LOW_BATT;
      Serial.print(F("Low battery: ")); Serial.println(battV, 2);
    }
  }

  // State machine
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
        digitalWrite(BUZZ_PIN, HIGH);  // Active buzzer DC on
        delay(1000);
        digitalWrite(BUZZ_PIN, LOW);   // Off
        Serial.println(F("Time expired"));
      } else {
        updateDisplay(now);
      }
      break;
    case EXPIRED:
      // Flash LED every second
      digitalWrite(LED_PIN, (now / 1000) % 2);
      break;
    case LOW_BATT:
      drawBatteryLow(now);
      break;
  }

  delay(20);  // Main loop rate for responsiveness
}

/**
 * Read battery voltage via divider on A0
 * Vbat = Vadc * (R1 + R2) / R2 , R1=470k, R2=1M, ratio=1.47
 */
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

/**
 * Handle button press/release with debounce and timing
 */
void handleButton(unsigned long now) {
  bool btnLow = (digitalRead(BTN_PIN) == LOW);

  if (!buttonPressed && btnLow) {
    // Press start
    pressStartMs = now;
    buttonPressed = true;
  } else if (buttonPressed && !btnLow) {
    // Release
    unsigned long pressDur = now - pressStartMs;
    if (pressDur < LONG_PRESS_MS) {
      onTap();
    }
    // Long press handled on detect
    buttonPressed = false;
  } else if (buttonPressed && (now - pressStartMs >= LONG_PRESS_MS)) {
    // Long press detected (consume immediately)
    onLongPress();
    buttonPressed = false;
  }
}

/**
 * Action on short tap (cycle/reset)
 */
void onTap() {
  switch (currentState) {
    case SELECT_TIME:
      selectedTimeIdx = (selectedTimeIdx + 1) % NUM_TIMES;
      break;
    case SELECT_MODE:
      selectedMode = static_cast<DisplayMode>((static_cast<int>(selectedMode) + 1) % 3);
      break;
    case RUNNING:
      // Reset timer and restart
      totalMs = pgm_read_dword(&TIMES[selectedTimeIdx]) * 1000UL;
      startMs = millis();
      initSand();
      break;
    // EXPIRED & LOW_BATT ignore tap
  }
}

/**
 * Action on long press (confirm/advance)
 */
void onLongPress() {
  switch (currentState) {
    case SELECT_TIME:
      // Confirm time, go to mode select
      currentState = SELECT_MODE;
      break;
    case SELECT_MODE:
      // Confirm mode, start running
      totalMs = pgm_read_dword(&TIMES[selectedTimeIdx]) * 1000UL;
      startMs = millis();
      initSand();
      currentState = RUNNING;
      Serial.print(F("Start: ")); Serial.print(totalMs/1000); Serial.print("s "); Serial.println(selectedMode);
      break;
    case EXPIRED:
      // Reset and restart
      totalMs = pgm_read_dword(&TIMES[selectedTimeIdx]) * 1000UL;
      startMs = millis();
      initSand();
      currentState = RUNNING;
      Serial.println(F("Long press reset"));
      break;
    // RUNNING & LOW_BATT ignore
  }
}

/**
 * Draw selected time in setup (flashing underline)
 */
void drawSetupTime(int secs) {
  lc.clearDisplay(0);
  int tens = secs / 10;
  int ones = secs % 10;
  drawTwoDigits(tens, ones);
  // Flashing underline
  if ((millis() / 1000) % 2 == 0) {
    lc.setRow(0, 7, 0xFF);
  }
}

/**
 * Draw selected mode icon in setup (flashing)
 */
void drawSetupMode(DisplayMode mode) {
  lc.clearDisplay(0);
  // Simple mode icons (large letters)
  byte modeIcons[3][8] = {
    {0x7F, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00},  // N
    {0x10, 0x10, 0x10, 0x10, 0x1F, 0x00, 0x00, 0x00},  // P (bar)
    {0x08, 0x14, 0x22, 0x41, 0x00, 0x00, 0x00, 0x00}   // S
  };
  for (int r = 0; r < 8; r++) {
    lc.setRow(0, r, modeIcons[static_cast<int>(mode)][r]);
  }
  // Flash entire display
  static unsigned long flashMs = 0;
  if ((millis() / 500) % 2 == 0) {
    lc.setIntensity(0, 2);  // Dim
  } else {
    lc.setIntensity(0, 15); // Bright
  }
}

/**
 * Update display based on mode and remaining time
 */
void updateDisplay(unsigned long now) {
  float progress = 1.0f - static_cast<float>(remainingMs) / totalMs;  // 0.0 start, 1.0 end

  lc.clearDisplay(0);
  lc.setIntensity(0, 8);

  switch (selectedMode) {
    case NUMERIC: {
      int secs = remainingMs / 1000;
      if (secs <= 90) {
        // Two digits seconds
        int tens = secs / 10;
        int ones = secs % 10;
        drawTwoDigits(tens, ones);
      } else {
        // Minutes + "m"
        int mins = secs / 60;
        drawDigitLeft(mins);
        drawMRight();
      }
      break;
    }
    case PROGBAR: {
      // 4 high x 8 wide = 32 pixels, left to right, top to bottom in col
      int pixels = static_cast<int>(progress * 32);
      int fullCols = pixels / 4;
      int remPx = pixels % 4;
      // Rows 2-5 for bar
      for (int c = 0; c < fullCols; c++) {
        for (int rr = 0; rr < 4; rr++) {
          lc.setLed(0, 2 + rr, c, true);
        }
      }
      for (int rr = 0; rr < remPx; rr++) {
        lc.setLed(0, 2 + rr, fullCols, true);
      }
      break;
    }
    case SAND: {
      // Animate one grain per tick
      if (now - lastStepMs >= tickMs) {
        moveOneGrain();
        lastStepMs = now;
      }
      // Draw sand grid
      for (int r = 0; r < 8; r++) {
        lc.setRow(0, r, sandRows[r]);
      }
      break;
    }
  }
}

/**
 * Pack two 4-bit nibble digits into rows (left digit bits7-4, right 3-0)
 */
void drawTwoDigits(int leftDig, int rightDig) {
  for (int row = 0; row < 8; row++) {
    byte leftNib = font[leftDig][row] >> 4;
    byte rightNib = font[rightDig][row] & 0x0F;
    lc.setRow(0, row, (leftNib << 4) | rightNib);
  }
}

/**
 * Draw single digit left side (cols 0-3)
 */
void drawDigitLeft(int dig) {
  for (int row = 0; row < 8; row++) {
    byte nib = font[dig][row] >> 4;
    lc.setRow(0, row, nib << 4);
  }
}

/**
 * Draw 'm' right side (cols 4-7)
 */
void drawMRight() {
  for (int row = 0; row < 8; row++) {
    byte nib = font[10][row] & 0x0F;
    lc.setRow(0, row, nib);
  }
}

/**
 * Initialize sand display (upper 4 rows full)
 */
void initSand() {
  tickMs = totalMs / 128UL;
  lastStepMs = millis();
  memset(sandRows, 0, sizeof(sandRows));
  for (int r = 0; r < 4; r++) {
    sandRows[r] = 0xFF;  // All pixels on
  }
}

/**
 * Move one grain down from upper to lower (random column)
 */
void moveOneGrain() {
  int col = random(8);
  byte mask = 1 << (7 - col);  // Bit 7 = col 0 (left)
  // Find highest grain in upper half (bottom-up priority)
  for (int row = 3; row >= 0; row--) {
    if (sandRows[row] & mask) {
      sandRows[row] &= ~mask;     // Clear current
      sandRows[row + 1] |= mask;   // Set below
      return;
    }
  }
}

/**
 * Draw static low battery icon (outline + low level bar), blinking intensity
 */
void drawBatteryLow(unsigned long now) {
  lc.clearDisplay(0);
  // Battery outline + low level (adjust bars for level if desired)
  byte battIcon[8] = {
    0x3C,  // 01111100 top
    0x42,  // 01000010 sides
    0x81,  // 10000001 side + low bar start
    0x81,  // low bar
    0x81,  // low bar
    0x42,  // sides
    0x3C,  // bottom
    0x00
  };
  for (int r = 0; r < 8; r++) {
    lc.setRow(0, r, battIcon[r]);
  }
  // Blink intensity
  lc.setIntensity(0, ((now / 500) % 2) ? 15 : 2);
}
