#include &lt;LedControl.h&gt;

// Pin definitions for MAX7219
// DIN = 12, CLK = 11, CS = 10
LedControl lc = LedControl(12, 11, 10, 1);

// Simple 8x8 font for digits 0-9 (columns)
byte font[10][8] = {
  {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}, // 0
  {0x08,0x18,0x28,0x08,0x08,0x08,0x3E,0x00}, // 1
  {0x3C,0x42,0x02,0x1C,0x20,0x42,0x3E,0x00}, // 2
  {0x3E,0x02,0x1C,0x02,0x02,0x42,0x3C,0x00}, // 3
  {0x04,0x0C,0x14,0x24,0x3E,0x04,0x04,0x00}, // 4
  {0x3E,0x20,0x3C,0x02,0x02,0x42,0x3C,0x00}, // 5
  {0x1C,0x20,0x3C,0x22,0x42,0x42,0x3C,0x00}, // 6
  {0x3E,0x02,0x04,0x08,0x10,0x10,0x10,0x00}, // 7
  {0x1C,0x22,0x22,0x1C,0x22,0x22,0x1C,0x00}, // 8
  {0x1C,0x22,0x22,0x1E,0x02,0x42,0x3C,0x00}  // 9
};

// Function to display a digit at position (0=left, 1=right)
void displayDigit(int pos, int digit) {
  for (int i = 0; i &lt; 8; i++) {
    lc.setRow(0, pos * 4 + i, font[digit][i]); // Adjust for 2 digits on 8x8
  }
}

unsigned long startTime;
int timeLeft = 30; // 30 seconds for test
bool timerRunning = false;

void setup() {
  Serial.begin(9600);
  lc.shutdown(0, false);
  lc.setIntensity(0, 8);
  lc.clearDisplay(0);
  
  // Test pattern: all on briefly
  lc.setRow(0, 0, 0xFF);
  lc.setRow(0, 1, 0xFF);
  lc.setRow(0, 2, 0xFF);
  lc.setRow(0, 3, 0xFF);
  lc.setRow(0, 4, 0xFF);
  lc.setRow(0, 5, 0xFF);
  lc.setRow(0, 6, 0xFF);
  lc.setRow(0, 7, 0xFF);
  delay(1000);
  lc.clearDisplay(0);
  
  Serial.println(&quot;BBX Game Timer Test - Press button on pin 2 to start/pause&quot;);
  pinMode(2, INPUT_PULLUP); // Button to start/pause
  startTime = millis();
}

void loop() {
  if (digitalRead(2) == LOW) { // Button pressed
    delay(50); // debounce
    if (digitalRead(2) == LOW) {
      timerRunning = !timerRunning;
      if (timerRunning) startTime = millis();
      while (digitalRead(2) == LOW); // wait release
    }
  }
  
  if (timerRunning) {
    unsigned long elapsed = (millis() - startTime) / 1000;
    int displayTime = timeLeft - elapsed;
    if (displayTime &lt;= 0) {
      // Time up - buzzer or flash
      for (int i = 0; i &lt; 10; i++) {
        lc.clearDisplay(0);
        delay(100);
        for (int r = 0; r &lt; 8; r++) lc.setRow(0, r, 0xFF);
        delay(100);
      }
      timerRunning = false;
      timeLeft = 30; // reset
    } else {
      // Display two digits: tens and units
      int tens = displayTime / 10;
      int units = displayTime % 10;
      lc.clearDisplay(0);
      // Display tens (left 4 cols) and units (right 4 cols) - simplified
      for (int col = 0; col &lt; 8; col++) {
        byte left = font[tens][col];
        byte right = font[units][col];
        lc.setRow(0, col, (left &gt;&gt; 4) | (right &lt;&lt; 4)); // Pack into 8 bits
      }
    }
  } else {
    lc.clearDisplay(0);
  }
  
  delay(100);
}