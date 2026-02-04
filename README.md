# bbx-game-timer

Arduino Nano (ATmega328P protoboard) board game turn timer with MAX7219 8x8 LED matrix, buzzer, button, external LED, and battery monitoring.

## Features
- **Setup Mode (boot):** Tap to cycle timer duration (15, 30, 45, 60, 90, 120, 150, 180, 240, 300 seconds), long-press (3s) to confirm and advance to display mode selection. Tap cycle modes (Numeric, Progress Bar, Sand), long-press to start countdown.
- **Numeric Mode:** Displays SS (≤90s) or Xm (&gt;90s, e.g. &#39;2m&#39;).
- **Progress Bar Mode:** 4×8 pixel bar fills left-to-right (32 pixels total).
- **Sand Mode:** 32 upper pixels (rows 0-3) fall randomly to lower rows (4 steps/grain = 128 total steps).
- **Controls:** Tap (short press/release): Cycle setup or reset/restart timer. Long-press (3s immediate): Confirm setup or reset from expired.
- **Expired:** Buzzer beeps 1s (1kHz), LED flashes every 1s. Tap ignored; long-press resets.
- **Battery Monitor:** Continuous check; &lt;2.8V → stops all, shows blinking battery icon.
- **Single Button:** Software debounced, no extra hardware debounce needed.

## Hardware Requirements
- Arduino Nano protoboard (ATmega328P)
- MAX7219 8×8 LED matrix module
- Large tactile/push button
- Piezo buzzer (passive)
- External LED (any color) + 330Ω current-limiting resistor
- 2× AA alkaline battery holder with power switch
- Buck-boost DC-DC converter (input 1.8-5.5V, output fixed 5V, e.g. MT3608 or TP4056-based)
- Resistors: 470kΩ (1%), 1MΩ (1%) for battery divider; 330Ω (LED); optional 10kΩ button pull-up
- Jumper wires, protoboard/perfboard for assembly
- 0.1µF ceramic cap optional across battery divider for ADC noise reduction

## Power Supply
- 2× AA batteries (~2.0V low - 3.3V fresh) → power switch → buck-boost input.
- Buck-boost output 5V → Arduino Nano **5V pin** (provides stable 4-5.5V for MAX7219).
- Common GND.
- Low power: divider ~2µA draw.

## Wiring Table
| Component          | Connection Details                                      | Arduino Pin | Passives/Notes                          |
|--------------------|--------------------------------------------------------|-------------|-----------------------------------------|
| **MAX7219 VCC**    | Power supply                                            | **5V**     | Requires 4-5.5V                        |
| **MAX7219 GND**    | Ground                                                  | **GND**    | Common ground                          |
| **MAX7219 DIN**    | Data input                                              | **D12**    |                                        |
| **MAX7219 CS**     | Chip select/load                                        | **D10**    |                                        |
| **MAX7219 CLK**    | Clock                                                   | **D11**    |                                        |
| **Button**         | One leg to pin, other to GND                            | **D2**     | INPUT_PULLUP enabled; opt 10kΩ pull-up |
| **Buzzer +**       | Positive to pin, negative to GND                        | **D3**     | PWM tone(); passive piezo              |
| **LED Anode**      | Anode → resistor → pin, cathode to GND                  | **D4**     | 330Ω series resistor                   |
| **Battery Monitor**| Batt+ → 470kΩ → A0; A0 → 1MΩ → GND                      | **A0**     | Voltage divider (1.47x ratio)          |
| **Buck-Boost**     | 5V output to 5V pin; GND common                         | **5V/GND** | Stable 5V from 2-3.3V batt             |

## ASCII Wiring Diagram
```
  +----- 2xAA Battery Pack -----+
  | + -------------------------|------- [Power Switch] ------- [Buck-Boost DC-DC]
  | - -------------------------|                                   |
  +----------------------------+                                   | 5V out
                                       |                           |
                                       | GND ---------------------- Nano GND --- MAX7219 GND
                                                             |     |     |         |         |
                                                             |     |     |         |         +--- Buzzer - 
  Nano A0 &lt;--- [470kΩ] --- Battery + --- [1MΩ] --- GND      |     |     |         |
                                                             |     |     |         +--- [330Ω] --- LED Anode --- LED Cathode --- GND
                                                             |     |     |
                                                             |     |     +--- D4 (LED)
                                                             |     |
                                                             |     +--- D3 (Buzzer +)
                                                             |
  Nano D12 -------------------------------------------------- MAX DIN
       D11 -------------------------------------------------- CLK
       D10 -------------------------------------------------- CS
       D2  -------------------------------------------------- Button leg1
Button leg2 ------------------------------------------------ GND
                                                             |
                                                             +--- 5V pin &lt;--- Buck 5V out
```

## Software Setup & Upload
1. Download Arduino IDE: https://www.arduino.cc/en/software
2. Install library: **Tools → Manage Libraries → &quot;LedControl&quot; by Eberhard Fahle**
3. Open `bbx-game-timer.ino`
4. **Board:** Arduino Nano  
   **Processor:** ATmega328P (Old Bootloader)
5. Select port, **Verify** (Ctrl+R), **Upload** (Ctrl+U)

## Usage Guide
### Setup (Power On)
- **Tap button:** Cycle timer values (flashing 2-digits: 15,30,...,300s)
- **Long-press (3s):** Confirm → Cycle display modes (icons: N/P/S flashing)
- **Long-press:** Start countdown in selected mode/time

### During Countdown
- **Display** updates realtime per mode
- **Tap:** Immediately reset to full time &amp; restart
- **Time reaches 0:** Buzzer 1s beep + LED blink (1Hz); tap ignored

### Expired State
- LED blinks; display off
- **Long-press:** Reset &amp; restart full timer

### Low Battery
- Auto-detect &lt;2.8V: Shows blinking battery icon, stops timer/buzzer
- Power cycle to reset (replace batteries)

## Testing Checklist
1. **Power on:** Full matrix flash → setup time display
2. **Setup:** Tap cycles 15-300 flash underline; long → modes N/P/S blink
3. **Start:** Long-press → countdown (observe mode)
4. **Numeric:** SS → Xm transition
5. **Progbar:** Smooth L-R fill
6. **Sand:** Grains fall from top → bottom over time
7. **Tap reset:** Immediate refill/restart
8. **Expired:** Beep + blink; long reset
9. **Battery:** Serial shows V; &lt;2.8V → icon blink
10. **Serial Monitor (9600 baud):** Debug states/voltages

## Code Overview
- **State Machine:** SELECT_TIME → SELECT_MODE → RUNNING → (EXPIRED | LOW_BATT)
- **Button:** Edge-detect press/release duration (software debounce)
- **Sand Anim:** Bitmask rows; 128 steps (total/128 ms tick), random col drop
- **Battery:** Avg 16 ADC reads every 10s
- **Memory:** PROGMEM for TIMES; byte arrays efficient
- **No external libs beyond LedControl**

## Troubleshooting
- No display: Check 5V to MAX VCC, wiring D10-12
- Button sticky: Verify INPUT_PULLUP, GND
- No tone: PWM pin3 ok?
- ADC wrong: Calib divider ratio
- Sand stuck: randomSeed ok

## Next Steps / Enhancements
- Multi-turn (extra button)
- Brightness auto (batt-based)
- Sleep mode low power
- Multiple matrices (hh:mm)
- EEPROM save prefs

GNU License - Enjoy!
