# ESP32 WS2812 4x4 LED Matrix Controller

Firmware for an ESP32 NodeMCU-compatible board driving a 4x4 WS2812 LED matrix.

## Features

- Arduino + PlatformIO project
- Uses `Adafruit NeoPixel` for WS2812 control
- Configurable 4x4 matrix layout
- Simple serial commands for color and matrix testing

## Wiring

| ESP32 Pin | Matrix |
|-----------|--------|
| GPIO4     | DIN    |
| GND       | GND    |
| 5V / 3.3V | VCC    |

> Use a common ground between the ESP32 and the LED matrix power supply.

## Quick start

1. Build:
   ```bash
   pio run
   ```
2. Upload:
   ```bash
   pio run -t upload
   ```
3. Monitor:
   ```bash
   pio device monitor
   ```

## Controls

- `r` - red fill
- `g` - green fill
- `b` - blue fill
- `w` - white fill
- `o` - turn off
- `t` - test pixel pattern
