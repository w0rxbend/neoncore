# neoncore TCP Protocol

All communication is over a persistent TCP connection on port **7777**.

---

## Frame format

Every message (command and response) uses the same envelope:

```text
Offset  Size  Field
0       1     Magic byte 0  — 0x4C ('L')
1       1     Magic byte 1  — 0x4D ('M')
2       1     Version       — 0x01
3       1     Command / response code
4       1     Payload length (0–255 bytes)
5       N     Payload
5+N     1     XOR checksum  — XOR of all preceding bytes
```

Checksum is XOR over all bytes from offset 0 up to (but not including) the checksum byte itself.

---

## Response frame

After every command the device sends a 6-byte response:

```text
Offset  Size  Field
0       1     0x4C
1       1     0x4D
2       1     0x01
3       1     0x80  (response marker)
4       1     Status code (see below)
5       1     XOR checksum
```

### Status codes

| Code | Name                 | Meaning                          |
|------|----------------------|----------------------------------|
| 0x00 | OK                   | Command accepted and applied     |
| 0x01 | BAD_MAGIC            | First two bytes were not LM      |
| 0x02 | UNSUPPORTED_VERSION  | Version byte != 0x01             |
| 0x03 | UNKNOWN_COMMAND      | Command code not implemented     |
| 0x04 | INVALID_LENGTH       | Wrong payload length or bad arg  |
| 0x05 | CHECKSUM_MISMATCH    | Frame failed XOR check           |

---

## Commands

### 0x00 — PING

Payload: none (0 bytes)
Checks connectivity without changing the display.

### 0x01 — CLEAR

Payload: none (0 bytes)
Turns all LEDs off.

### 0x02 — SET_BRIGHTNESS

Payload: `brightness` (1 byte, 0–255)
Sets the global brightness scale. Default is 40.

### 0x03 — FILL

Payload: `r g b` (3 bytes)
Fills the entire matrix with one colour.

### 0x04 — SET_PIXEL

Payload: `x y r g b` (5 bytes)
Sets a single logical pixel. `x` and `y` are 0-based; (0,0) is top-left.

### 0x05 — SET_FRAME

Payload: 48 bytes — 16 RGB triples in physical LED chain order.
Replaces the full display in one shot.

### 0x06 — SET_PANEL_ENABLED

Payload: `enabled` (1 byte — 0 = off, 1 = on)
Blanks or restores the display without clearing stored colour data.

### 0x07 — SET_STATIC_COLOR

Payload: `r g b` (3 bytes)
Fills the matrix with a colour and holds it (same as FILL but persists across reconnects via the effect engine).

### 0x08 — SET_PRESET_EFFECT

Payload: `effectId intervalMsL intervalMsH r g b` (6 bytes)
`intervalMs` is little-endian 16-bit milliseconds per animation step.

| effectId | Name          |
|----------|---------------|
| 0        | Stop effect   |
| 1        | Chase         |
| 2        | Color wipe    |
| 3        | Blink         |
| 4        | Wave          |
| 5        | Rain          |
| 6        | Meteor        |
| 7        | Rainbow       |
| 8        | Breathing     |
| 9        | Scanner       |
| 10       | Sparkle       |
| 11       | Fire          |
| 12       | Matrix rain   |
| 13       | Ripple        |
| 14       | Theater chase |
| 15       | Twinkle       |
| 16       | Comet         |
| 17       | Plasma        |
| 18       | Diagonal      |
| 19       | Border chase  |
| 20       | Heartbeat     |
| 21       | Pulse wipe    |
| 22       | Confetti      |

### 0x09 — UPLOAD_CUSTOM_FRAME

Payload: `frameIndex frameCount delayMsL delayMsH` + 48 RGB bytes (52 bytes total)
Uploads one frame of a custom animation (max 8 frames). Send all frames in any order; playback starts automatically when the last frame arrives.

### 0x0A — STOP_EFFECT

Payload: none (0 bytes)
Stops any running animation and returns to direct control mode.

### 0x0B — SET_AQI_STATUS *(AirGradient integration)*

Payload: `status` (1 byte)

Sets the air-quality status. The display immediately switches to the corresponding colour and animation. If no `SET_AQI_STATUS` command is received for **60 seconds** the display returns automatically to the standby breathing animation.

13 visually distinct statuses. Each uses a unique pattern + colour + animation so it is immediately identifiable without a legend.

| Code | Name                  | Pattern                         | Color  | Animation               |
|------|-----------------------|---------------------------------|--------|-------------------------|
| 0x00 | Excellent             | Inner 2×2 only                  | Green  | Static                  |
| 0x01 | Good                  | Full matrix                     | Green  | Static                  |
| 0x02 | Good (degrading)      | Full matrix                     | Green  | Slow breathing          |
| 0x03 | Moderate              | Full matrix                     | Yellow | Static                  |
| 0x04 | Moderate (degrading)  | Perimeter yellow + inner orange | Dual   | Inner 2×2 breathing     |
| 0x05 | Poor                  | Full matrix                     | Orange | Static                  |
| 0x06 | Poor (degrading)      | Full matrix                     | Orange | Slow breathing          |
| 0x07 | Unhealthy             | Full matrix                     | Red    | Static                  |
| 0x08 | Unhealthy (degrading) | Full matrix                     | Red    | Slow breathing          |
| 0x09 | Very Unhealthy        | Full matrix                     | Purple | Static                  |
| 0x0A | Very Unhealthy (deg)  | Full matrix                     | Purple | Slow breathing          |
| 0x0B | Hazardous             | Full matrix                     | Purple | Blink 500 ms            |
| 0x0C | Extreme               | Full matrix                     | Dual   | Fast alternating 300 ms |

**Standby** (no data / timeout): soft white-blue breathing — RGB (150, 180, 255).

### Threshold mapping

| Code | Name                 | CO2 (ppm)   | PM2.5 (µg/m³) |
|------|----------------------|-------------|---------------|
| 0x00 | Excellent            | 0–400       | 0–2           |
| 0x01 | Good                 | 400–600     | 2–5           |
| 0x02 | Good (degrading)     | 600–800     | 5–9           |
| 0x03 | Moderate             | 800–1 000   | 9–15          |
| 0x04 | Moderate (degrading) | 1 000–1 250 | 15–25         |
| 0x05 | Poor                 | 1 250–1 500 | 25–35.4       |
| 0x06 | Poor (degrading)     | 1 500–1 750 | 35.4–45       |
| 0x07 | Unhealthy            | 1 750–2 000 | 45–55.4       |
| 0x08 | Unhealthy (degrading)| 2 000–2 500 | 55.4–75       |
| 0x09 | Very Unhealthy       | 2 500–3 000 | 75–125        |
| 0x0A | Very Unhealthy (deg) | 3 000–4 000 | 125–200       |
| 0x0B | Hazardous            | 4 000–5 000 | 200–300       |
| 0x0C | Extreme              | > 5 000     | > 300         |

---

## Standby behaviour

On boot and after any 60-second gap in `SET_AQI_STATUS` messages the device plays a **soft white-blue breathing** animation (RGB 150, 180, 255 at 80 ms/step). This signals that the display is online and waiting for air-quality data.

---

## Python client example

```python
import socket

def checksum(data):
    r = 0
    for b in data: r ^= b
    return r

def build_frame(cmd, payload=b''):
    header = b'\x4C\x4D\x01' + bytes([cmd, len(payload)]) + payload
    return header + bytes([checksum(header)])

def send_aqi_status(host, port, status):
    with socket.create_connection((host, port), timeout=5) as s:
        s.sendall(build_frame(0x0B, bytes([status])))
        resp = s.recv(6)
        print('status:', hex(resp[4]))

# Example: report Good air quality
send_aqi_status('192.168.1.192', 7777, 0x00)
```
