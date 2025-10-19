# Olimex Pico2‑XL Wiring Guide

This document summarizes the suggested wiring for the Olimex Pico2‑XL board in this project, covering the SPI OLED and a PCM5102 DAC. Pin choices avoid reserved/used GPIOs as requested.

Reserved/avoid:
- GPIO0–3 (UART / I2C1)
- GPIO24–25 (SD_DATO / USER LED)
- GPIO40–47 (ADC)

The rest are considered available.

## OLED (SPI, SSD1309 128x64)

Firmware mapping (SPI0):
- SCK: GPIO6 (`OLED_SCK`)
- MOSI (SDA): GPIO7 (`OLED_MOSI`)
- DC: GPIO32 (`OLED_DC`)
- RST: GPIO33 (`OLED_RST`)
- CS: tie to GND by default (no CS pin used by code); optional `OLED_CS` = GPIO5
- MISO: GPIO4 exists on header but is unused by the OLED

Connect your OLED pins as follows:
- CS → GND (or GPIO5 if you want to drive CS in software)
- DC → GPIO32
- RES → GPIO33
- SDA → GPIO7
- SCLK → GPIO6
- VDD → 3V3
- VSS → GND

Notes:
- The Screen driver initializes SPI0 with `SPI.setSCK()`/`SPI.setTX()`; MISO not used.
- If you want to use CS, switch the U8g2 constructor to a variant that takes CS and wire CS → GPIO5.

## PCM5100 DAC (I2S audio output)

PCM5100 top pins: `SCK  BCK  DIN  LRCK  GND  VIN`

- SCK (MCLK): Not required by PCM5100 in standard I2S mode; leave unconnected.
- BCK → GPIO10
- DIN → GPIO12
- LRCK → GPIO11
- GND → GND
- VIN → 5V (check your module; many accept 3.3–5V and have onboard regulator; if 3.3V‑only, use 3V3)

Module label mapping (common silkscreen variants):
- WSEL = WS = LRCK → connect to GPIO11
- BCK = BCLK → connect to GPIO10
- DIN = SD = DATA → connect to GPIO12
- MCK = SCK = MCLK → not required; leave unconnected
- MU = XSMT (mute) → tie to 3V3 to enable outputs, or to a spare GPIO for software mute
- FM = FMT (format) → tie to GND for I2S format (typical); other settings select left-justified
- FIL = FLT (filter) → leave default (pulled down = sharp); tie to 3V3 for slow roll‑off
- DE = DEMP (de‑emphasis) → leave default (off)
- 3v → module’s 3.3V rail (from onboard regulator); do not power the module from here

Side pins (labeled may vary): `FLT  DEMP  XSMT  FMT  A3V3  AGND  ROUT  AGND  LROUT`

- FLT: Optional filter; leave default (usually pulled down to GND). Low selects the “sharp” roll‑off filter (default); pull high for the “slow/soft” roll‑off. The pin has a weak internal pulldown on PCM5102A and many modules also add an external pulldown, so you typically don’t need to wire it.
- DEMP: De‑emphasis; leave default (unused)
- XSMT: Output mute; tie HIGH to enable outputs, or leave to on‑board pull‑ups (typical modules already pull this high). If needed, you may wire to a spare GPIO to control mute.
- FMT: Format select; leave default (I2S)
- A3V3: Analog 3.3V (provided on module); leave as is
- AGND: Analog ground; ensure good ground connection
- ROUT/LROUT: Audio outputs to your amplifier/input

Firmware configuration (set in board header):
- PIN_BCLK = 10
- PIN_LRCK = 11
- PIN_DATA = 12

These pins satisfy the RP2040 I2S requirement that WS (LRCK) is adjacent to BCK (either BCK+1 or BCK‑1). The pins are defined per-board and consumed by the audio core.

## Trigger button and status LEDs

Pins used (per `olimex_pico2xl.h` and UI code):
- Trigger button: GPIO13 (`TRIG_IN`)
- Trigger LED: GPIO14 (`LED_TRIGGER`)
- Clipping LED: GPIO15 (`LED_CLIPPING`)

Trigger button wiring (active‑low, internal pull‑up):
- Connect a momentary push button between GPIO13 and GND.
- No external resistor is required; the firmware enables the internal pull‑up and treats LOW as pressed.
- Do not connect the button to 5V; RP2350 GPIO is 3.3V only.

- Status LEDs wiring (active‑high):
- Each LED requires a series resistor (typically 330Ω–1kΩ).
- Connect LED anode (+) through the resistor to the GPIO pin.
	- Trigger LED anode → resistor → GPIO14.
	- Clipping LED anode → resistor → GPIO15.
- Connect LED cathode (−) to GND.
- The firmware drives these pins HIGH to turn LEDs on (default JLed polarity).

## Front‑panel LEDs (tab/page indicators)

Pins used (per `olimex_pico2xl.h`):
- LEDS group: GPIO26, GPIO27, GPIO28, GPIO29

Wiring (active‑high):
- Each LED needs a series resistor (330Ω–1kΩ).
- Connect each LED anode (+) through its resistor to the corresponding GPIO.
- Connect the cathodes (−) to GND.

## Tab buttons (4)

Pins used (per `olimex_pico2xl.h`):
- TAB_BUTTONS: GPIO34, GPIO35, GPIO36, GPIO37

Wiring (active‑low, internal pull‑ups):
- Connect each button between its GPIO and GND.
- No external resistor is required; firmware enables the pull‑up.

## Conflicts and ranges

- We avoided GPIO0–3, 8–11, 24–25, and 40–47.
- SPI OLED uses 6/7 (+ optional 5 for CS, 4 available as MISO if needed).
- I2S uses 10/11/12.
- Trigger/LEDs use 13/14/15 (button to GND, LEDs active‑high with resistors).
- Front‑panel LEDS use 26/27/28/29; TAB buttons use 34/35/36/37.
- Other front‑panel IO in `olimex_pico2xl.h` is mapped outside these to avoid clashes.

## Power and grounding

- Ensure OLED VDD matches 3V3; many SSD1306 boards tolerate 3.3V.
- PCM5100 VIN commonly expects 5V and creates on‑board 3.3V analog rails; verify your module. Tie all grounds together.
