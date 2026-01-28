# bbx-game-timer

Arduino board game turn timer using ATmega328P + MAX7219-driven 8x8 LED matrix.

## Hardware Requirements
- Arduino Uno (or compatible ATmega328P board)
- MAX7219 8x8 LED matrix module
- Push button + 10kΩ pull-up resistor (or use INPUT_PULLUP)
- Jumper wires
- Optional: Buzzer on pin 3 for time-up alert

## Wiring
| MAX7219 | Arduino Pin |
|---------|-------------|
| VCC     | 5V          |
| GND     | GND         |
| DIN     | 12          |
| CS      | 10          |
| CLK     | 11          |

| Button  | Arduino Pin |
|---------|-------------|
| One leg | 2 (digital) |
| Other   | GND         |

## Software Setup
1. Install Arduino IDE (https://www.arduino.cc/en/software).
2. Library: Tools > Manage Libraries > Search "LedControl" by Eberhard Fahle > Install.

## Building & Uploading
1. Open `bbx_game_timer.ino` in Arduino IDE.
2. Select Board: Arduino Uno.
3. Select Port: Your board's port (Tools > Port).
4. Verify (Ctrl+R), then Upload (Ctrl+U).

## Testing
1. Power on: Matrix lights all LEDs briefly, then clears.
2. Open Serial Monitor (9600 baud): See startup message.
3. Press button (pin 2): Starts 30s countdown (displays two-digit seconds on matrix).
4. Press again to pause/resume.
5. Time up (00): Matrix flashes full bright, resets.
6. Idle: Matrix off.

## Next Steps
- Add multi-player turns.
- Buzzer integration.
- Adjustable time limits via buttons/potentiometer.
- More matrices for better display.
