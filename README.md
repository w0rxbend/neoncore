# neoncore

**A wireless ambient air-quality indicator built on ESP32 + WS2812B.**
Place it anywhere in the room — completely independent of the sensor that feeds it.

---

## The idea

[AirGradient ONE](https://www.airgradient.com) is a fantastic open-source air-quality monitor.
Its built-in LED bar is great, but it lives on the device itself — usually tucked in a corner, plugged into power, hidden behind a monitor.

**neoncore** solves that. It is a secondary display — a 4×4 WS2812B LED matrix driven by an ESP32 — that receives an air-quality status over Wi-Fi TCP and lights up with a distinct colour + animation pattern for every quality level. Put the sensor where it works best. Put the indicator where you actually see it.

```text
  AirGradient ONE          your sender                 neoncore device
  ─────────────────        ────────────────────        ──────────────────────
  measures CO₂, PM2.5  →   reads local API        →    ESP32 + 4×4 WS2812B
  exposes local API        maps value → status         shows colour pattern
```

The device is deliberately dumb: it knows nothing about sensors or thresholds. It exposes a tiny TCP contract of 13 status codes and renders them. The sender (a script on a Pi, a Home Assistant automation, anything that can open a socket) decides which code to send. That sender is out of scope for this repo.

---

## Features

- **13 distinct visual states** — each a unique combination of pattern, colour, and animation, readable at a glance without a legend
- **Transition on change** — when the status changes, the new colour wipes across the matrix and holds before settling, so a change is noticeable even from the corner of your eye
- **Standby animation** — soft white-blue breathing when no data is received; tells you the device is online and waiting
- **60-second timeout** — automatically returns to standby if the sender stops pushing updates
- **Robust connection handling** — newest connection wins, TCP keepalive, idle and partial-frame timeouts; a crashed sender can never wedge the device
- **Non-blocking boot** — standby starts immediately while Wi-Fi connects in the background
- **Static IP / MAC override / AP fallback** — configurable from one credentials header
- **Host-side tests** — the protocol parser and the whole visual contract are unit-tested on your PC, no hardware needed

---

## Hardware

| Component                 | Notes                                               |
| ------------------------- | --------------------------------------------------- |
| ESP32 DevKit v1 (WROOM32) | Any standard 30-pin or 38-pin variant               |
| WS2812B 4×4 LED matrix    | 16-pixel panel, 5 V                                 |
| 5 V / 2 A power supply    | Shared for ESP32 and LEDs                           |
| 470 Ω resistor            | In series with the data line                        |
| 1 000 µF capacitor        | Across the LED strip power rails (recommended)      |

### Wiring

```text
5V supply (+) ──────────────────────────── WS2812B VCC
                                             │
5V supply (−) ──────────────────────────── WS2812B GND
      │
      └─── ESP32 GND

ESP32 GPIO4 ──── 470 Ω ──── WS2812B DIN
```

> **Note:** Do not power the LED strip from the ESP32 3.3 V or 5 V pin. Use a dedicated 5 V rail — 16 LEDs at full brightness can draw over 900 mA.

The firmware disables the ESP32 brown-out detector by default because cheap USB supplies trip it during Wi-Fi bursts. See `kDisableBrownoutDetector` in `include/AppConfig.h` for the trade-off.

---

## Quick start

### 1. Configure credentials

Copy the example credentials file and fill in your Wi-Fi details:

```bash
cp include/creds.example.h include/creds.h
```

```cpp
// include/creds.h
#define WIFI_SSID     "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
```

Optional settings, all in the same file:

```cpp
// Static IP (skips DHCP). DNS is optional and defaults to the gateway.
#define STATIC_IP      "192.168.1.42"
#define STATIC_GATEWAY "192.168.1.1"
#define STATIC_SUBNET  "255.255.255.0"
#define STATIC_DNS     "192.168.1.1"

// Replace the factory MAC (locally-administered address).
#define WIFI_MAC_OVERRIDE {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}

// Password for the fallback access point (8+ chars). Open AP if unset.
#define WIFI_AP_PASSWORD "changeme123"

// List nearby networks on the serial console at boot (adds a few seconds).
#define WIFI_SCAN_ON_BOOT 1
```

If `WIFI_SSID` is left empty the device starts its own access point named `led-matrix` instead.

### 2. Flash

```bash
pio run --target upload
```

### 3. Verify

Open the serial monitor at 115200 baud. A successful boot looks like:

```text
neoncore ESP32 WS2812B AQI indicator
Protocol version: 2
LED data pin: GPIO4
Matrix: 4x4
Boot settle delay ms: 2000
Startup animation: running
MAC: CC:50:E3:3C:E9:03
Target SSID: "YourNetwork"
Connecting to Wi-Fi (non-blocking)
Wi-Fi connected
Device IP: 192.168.1.42
TCP server started on 192.168.1.42:7777
```

The matrix begins a soft white-blue breathing animation — standby mode — as soon as the startup sweep finishes, before Wi-Fi is up.

### 4. Test with the Python client

```bash
# Ping
python tools/client.py --host 192.168.1.42 ping

# Set a status by name or code
python tools/client.py --host 192.168.1.42 aqi moderate
python tools/client.py --host 192.168.1.42 aqi 0x0C

# Brightness and blanking
python tools/client.py --host 192.168.1.42 brightness 80
python tools/client.py --host 192.168.1.42 panel off

# Cycle through all 13 levels, 3 seconds each, on one connection
python tools/aqi_example.py --host 192.168.1.42 --hold 3
```

### 5. Run the host-side tests

```bash
pio test -e native
```

This compiles the protocol parser and the AQI display logic for your PC and checks framing, error handling, every status's pattern and colour, transition timing, heartbeat behaviour, and the standby timeout.

---

## Status reference

The 13 levels cover the full AirGradient colour scale with enough granularity to distinguish a reading that is just entering a new band from one that is deep inside it.

| Code   | Name                  | Pattern                         | Colour       | Animation               | CO₂ (ppm)   | PM2.5 (µg/m³) |
| ------ | --------------------- | ------------------------------- | ------------ | ----------------------- | ----------- | ------------- |
| `0x00` | Excellent             | Inner 2×2 only                  | Green        | Static                  | 0–400       | 0–2           |
| `0x01` | Good                  | Full matrix                     | Green        | Static                  | 400–600     | 2–5           |
| `0x02` | Good (degrading)      | Full matrix                     | Green        | Breathing               | 600–800     | 5–9           |
| `0x03` | Moderate              | Full matrix                     | Yellow       | Static                  | 800–1 000   | 9–15          |
| `0x04` | Moderate (degrading)  | Perimeter yellow + inner orange | Dual         | Inner 2×2 breathing     | 1 000–1 250 | 15–25         |
| `0x05` | Poor                  | Full matrix                     | Orange       | Static                  | 1 250–1 500 | 25–35.4       |
| `0x06` | Poor (degrading)      | Full matrix                     | Orange       | Breathing               | 1 500–1 750 | 35.4–45       |
| `0x07` | Unhealthy             | Full matrix                     | Red          | Static                  | 1 750–2 000 | 45–55.4       |
| `0x08` | Unhealthy (degrading) | Full matrix                     | Red          | Breathing               | 2 000–2 500 | 55.4–75       |
| `0x09` | Very Unhealthy        | Full matrix                     | Purple       | Static                  | 2 500–3 000 | 75–125        |
| `0x0A` | Very Unhealthy (deg)  | Full matrix                     | Purple       | Breathing               | 3 000–4 000 | 125–200       |
| `0x0B` | Hazardous             | Full matrix                     | Purple       | Blink 500 ms            | 4 000–5 000 | 200–300       |
| `0x0C` | Extreme               | Full matrix                     | Purple ↔ Red | Fast alternating 300 ms | > 5 000     | > 300         |

**Standby** — boot, no data, or 60-second timeout — soft white-blue breathing.

**On change** — the new colour wipes in from the top-right corner (640 ms), holds solid (600 ms), then the steady pattern above begins. Repeating the same code is a heartbeat and does not animate.

Visual language:

- **Static** — reading is stable and comfortably inside the band
- **Breathing** — reading is in the upper portion of the band, approaching the next threshold
- **Blink / alternating** — dangerous, demands immediate attention

---

## TCP protocol

Full specification: [`docs/protocol.md`](docs/protocol.md). Version 2 has four commands:

| Code   | Command             | Payload            |
| ------ | ------------------- | ------------------ |
| `0x00` | PING                | none               |
| `0x01` | SET_BRIGHTNESS      | 1 byte, 0–255      |
| `0x02` | SET_PANEL_ENABLED   | 1 byte, 0 = off    |
| `0x03` | SET_AQI_STATUS      | 1 byte, 0x00–0x0C  |

Minimal sender:

```python
import socket

def checksum(data):
    r = 0
    for b in data:
        r ^= b
    return r

def send_aqi(host, port, status):
    frame = b'\x4C\x4D\x02\x03\x01' + bytes([status])
    frame += bytes([checksum(frame)])
    with socket.create_connection((host, port), timeout=5) as s:
        s.sendall(frame)
        return s.recv(6)[4] == 0x00  # True = OK

send_aqi('192.168.1.42', 7777, 0x01)  # Good
```

**Security:** there is no authentication. Anyone on the LAN can drive the display. Keep port 7777 inside your home network.

---

## Project structure

```text
neoncore/
├── include/
│   ├── AppConfig.h            pins, matrix size, timing and network constants
│   ├── MatrixProtocol.h       frame format, commands, status codes, FrameParser
│   ├── AqiDisplay.h           the visual contract: status → frame over time
│   ├── MatrixLayout.h         serpentine wiring order
│   ├── LedMatrixController.h  WS2812B driver wrapper
│   ├── TcpMatrixServer.h      Wi-Fi, TCP, command dispatch
│   └── creds.example.h        copy to creds.h and fill in Wi-Fi details
├── src/
│   ├── main.cpp               Arduino setup / loop
│   ├── TcpMatrixServer.cpp
│   ├── AqiDisplay.cpp         hardware-free, unit-tested
│   ├── MatrixProtocol.cpp     hardware-free, unit-tested
│   └── LedMatrixController.cpp
├── test/
│   ├── test_protocol/         parser and checksum tests
│   └── test_aqi/              contract, transition and timeout tests
├── tools/
│   ├── client.py              command-line client
│   └── aqi_example.py         cycles through all 13 levels
├── docs/
│   └── protocol.md            full TCP protocol specification
└── platformio.ini             esp32dev (firmware) and native (tests) envs
```

---

## Built with

- [PlatformIO](https://platformio.org) + Arduino framework for ESP32
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
- [Unity](https://github.com/ThrowTheSwitch/Unity) for host-side tests
- [AirGradient ONE](https://www.airgradient.com) as the intended air-quality data source

---

## License

MIT
