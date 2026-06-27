# neoncore

**A wireless ambient air-quality indicator built on ESP32 + WS2812B.**  
Place it anywhere in the room — completely independent of the sensor that feeds it.

---

## The idea

[AirGradient ONE](https://www.airgradient.com) is a fantastic open-source air-quality monitor.  
Its built-in LED bar is great, but it lives on the device itself — usually tucked in a corner, plugged into power, hidden behind a monitor.

**neoncore** solves that. It is a secondary display — a 4×4 WS2812B LED matrix driven by an ESP32 — that receives air-quality status over Wi-Fi TCP and lights up with a distinct colour + animation pattern for every quality level. Put the sensor where it works best (good airflow, stable surface). Put the indicator where you actually see it: on your desk, on a shelf, on the wall.

```text
  AirGradient ONE          neoncore scraper          neoncore device
  ─────────────────        ────────────────────       ──────────────────────
  measures CO₂, PM2.5  →  reads local API      →     ESP32 + 4×4 WS2812B
  exposes local API        maps value → status        shows colour pattern
```

---

## Features

- **13 distinct visual states** — each a unique combination of pattern, colour, and animation, readable at a glance without any legend
- **Standby animation** — soft white-blue breathing when no data is received; tells you the device is online and waiting
- **60-second timeout** — automatically returns to standby if the scraper stops pushing updates
- **Dual-zone patterns** — e.g. yellow perimeter with a breathing orange centre to show a transitioning reading
- **Full TCP protocol** — send any status, control individual pixels, run preset effects, or push custom animation frames
- **AP fallback** — if no Wi-Fi credentials are configured, the device creates its own access point

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

Optional — static IP (skips DHCP, recommended for reliable operation):

```cpp
#define STATIC_IP      "192.168.1.42"
#define STATIC_GATEWAY "192.168.1.1"
#define STATIC_SUBNET  "255.255.255.0"
```

### 2. Flash

```bash
pio run --target upload
```

### 3. Verify

Open the serial monitor at 115200 baud. A successful boot looks like:

```text
ESP32 WS2812B TCP matrix controller
LED data pin: GPIO4
Matrix: 4x4
Boot settle delay ms: 2000
MAC: CC:50:E3:3C:E9:03
Target SSID: "YourNetwork"
Scanning for networks...
  [1] SSID: "YourNetwork"  RSSI: -42 dBm  CH: 6  ENC: secured
Connecting to Wi-Fi............
Wi-Fi connected, IP: 192.168.1.42
TCP server started on 192.168.1.42:7777
```

The matrix begins a soft white-blue breathing animation — standby mode.

### 4. Test with the Python client

```bash
# Ping
python tools/client.py --host 192.168.1.42 ping

# Cycle through all 13 AQI status levels (2 seconds each)
python tools/aqi_example.py --host 192.168.1.42 --hold 2
```

---

## AirGradient integration

> **The scraper client is coming soon.**

The integration works in two parts:

**1. neoncore (this repo)** — firmware running on the ESP32. Exposes a TCP socket on port 7777 and waits for status commands.

**2. neoncore-scraper (to be provided)** — a lightweight service that:

- Polls the AirGradient ONE local API for CO₂ and PM2.5 readings
- Maps the reading to one of 13 status codes
- Sends a `SET_AQI_STATUS` TCP command to the neoncore device every 30 seconds

Once the scraper is running the indicator updates automatically and returns to standby if the scraper stops.

---

## Status reference

The 13 levels cover the full AirGradient colour scale with enough granularity to distinguish a reading that is just entering a new band from one that is deep inside it.

| Code   | Name                  | Pattern                         | Color         | Animation               | CO₂ (ppm)   | PM2.5 (µg/m³) |
| ------ | --------------------- | ------------------------------- | ------------- | ----------------------- | ----------- | ------------- |
| `0x00` | Excellent             | Inner 2×2 only                  | Green         | Static                  | 0–400       | 0–2           |
| `0x01` | Good                  | Full matrix                     | Green         | Static                  | 400–600     | 2–5           |
| `0x02` | Good (degrading)      | Full matrix                     | Green         | Breathing               | 600–800     | 5–9           |
| `0x03` | Moderate              | Full matrix                     | Yellow        | Static                  | 800–1 000   | 9–15          |
| `0x04` | Moderate (degrading)  | Perimeter yellow + inner orange | Dual          | Inner 2×2 breathing     | 1 000–1 250 | 15–25         |
| `0x05` | Poor                  | Full matrix                     | Orange        | Static                  | 1 250–1 500 | 25–35.4       |
| `0x06` | Poor (degrading)      | Full matrix                     | Orange        | Breathing               | 1 500–1 750 | 35.4–45       |
| `0x07` | Unhealthy             | Full matrix                     | Red           | Static                  | 1 750–2 000 | 45–55.4       |
| `0x08` | Unhealthy (degrading) | Full matrix                     | Red           | Breathing               | 2 000–2 500 | 55.4–75       |
| `0x09` | Very Unhealthy        | Full matrix                     | Purple        | Static                  | 2 500–3 000 | 75–125        |
| `0x0A` | Very Unhealthy (deg)  | Full matrix                     | Purple        | Breathing               | 3 000–4 000 | 125–200       |
| `0x0B` | Hazardous             | Full matrix                     | Purple        | Blink 500 ms            | 4 000–5 000 | 200–300       |
| `0x0C` | Extreme               | Full matrix                     | Purple ↔ Red  | Fast alternating 300 ms | > 5 000     | > 300         |

**Standby** — no data received or 60-second timeout — soft white-blue breathing.

Visual language:

- **Static** — reading is stable and comfortably inside the band
- **Breathing** — reading is in the upper portion of the band, approaching the next threshold
- **Blink / alternating** — dangerous, demands immediate attention

---

## TCP Protocol

Full protocol documentation: [`docs/protocol.md`](docs/protocol.md)

Quick reference — send `SET_AQI_STATUS` (command `0x0B`):

```python
import socket

def checksum(data):
    r = 0
    for b in data: r ^= b
    return r

def send_aqi(host, port, status):
    payload = bytes([status])
    frame = b'\x4C\x4D\x01\x0B' + bytes([len(payload)]) + payload
    frame += bytes([checksum(frame)])
    with socket.create_connection((host, port), timeout=5) as s:
        s.sendall(frame)
        return s.recv(6)[4] == 0x00  # True = OK

send_aqi('192.168.1.42', 7777, 0x01)  # Good
```

The protocol also supports direct pixel control, full-frame pushes, 22 preset animations, and custom multi-frame animations — see [`docs/protocol.md`](docs/protocol.md) for the complete command set.

---

## Project structure

```text
neoncore/
├── include/
│   ├── AppConfig.h            pin, matrix size, timing constants
│   ├── MatrixProtocol.h       TCP protocol + AqiStatus enum
│   ├── TcpMatrixServer.h      main server class
│   ├── LedMatrixController.h
│   └── creds.example.h        copy to creds.h and fill in Wi-Fi details
├── src/
│   ├── main.cpp               Arduino setup / loop
│   ├── TcpMatrixServer.cpp    Wi-Fi, TCP, protocol parsing, LED effects
│   ├── LedMatrixController.cpp
│   └── MatrixProtocol.cpp     XOR checksum
├── tools/
│   ├── client.py              general-purpose CLI client
│   └── aqi_example.py         cycles through all 13 AQI levels
├── docs/
│   └── protocol.md            full TCP protocol specification
└── platformio.ini
```

---

## Built with

- [PlatformIO](https://platformio.org) + Arduino framework for ESP32
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
- [AirGradient ONE](https://www.airgradient.com) as the air-quality data source

---

## License

MIT
