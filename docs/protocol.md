# neoncore TCP Protocol — version 2

All communication is over TCP on port **7777**. The device is an
air-quality indicator: the protocol lets a sender tell it which of 13
status levels to show, plus three device controls. There is no general
pixel or animation access; what appears on the matrix is fully determined
by the status table below.

---

## Frame format

Every message (command and response) uses the same envelope:

```text
Offset  Size  Field
0       1     Magic byte 0  — 0x4C ('L')
1       1     Magic byte 1  — 0x4D ('M')
2       1     Version       — 0x02
3       1     Command / response code
4       1     Payload length (0–255 bytes)
5       N     Payload
5+N     1     XOR checksum  — XOR of all preceding bytes
```

Checksum is XOR over all bytes from offset 0 up to (but not including) the
checksum byte itself.

Frames may be pipelined on one connection; each is answered in order.

---

## Response frame

After every command the device sends a 6-byte response:

```text
Offset  Size  Field
0       1     0x4C
1       1     0x4D
2       1     0x02
3       1     0x80  (response marker)
4       1     Status code (see below)
5       1     XOR checksum
```

### Status codes

| Code | Name                 | Meaning                                              |
|------|----------------------|------------------------------------------------------|
| 0x00 | OK                   | Command accepted and applied                         |
| 0x01 | BAD_MAGIC            | First two bytes were not `LM`                        |
| 0x02 | UNSUPPORTED_VERSION  | Version byte != 0x02                                 |
| 0x03 | UNKNOWN_COMMAND      | Command code not in this protocol                    |
| 0x04 | INVALID_LENGTH       | Payload length is wrong for the command              |
| 0x05 | CHECKSUM_MISMATCH    | Frame failed XOR check                               |
| 0x06 | INVALID_ARGUMENT     | Length was right but a value is outside the contract |

Framing errors (0x01, 0x02, 0x05) reset the parser; the next byte is
treated as the start of a new frame. A `BAD_MAGIC` response is sent for
every stray byte received while no frame is in progress.

---

## Commands

### 0x00 — PING

Payload: none (0 bytes)
Connectivity check. Does not change the display. Also refreshes the
client idle timer (see *Connection rules*).

### 0x01 — SET_BRIGHTNESS

Payload: `brightness` (1 byte, 0–255)
Global brightness scale. Default is 40. Persists until reboot.

### 0x02 — SET_PANEL_ENABLED

Payload: `enabled` (1 byte — 0 = off, non-zero = on)
Blanks or restores the display. The status and animations keep running
underneath, so re-enabling shows the current state immediately.

### 0x03 — SET_AQI_STATUS

Payload: `status` (1 byte, 0x00–0x0C)

Sets the air-quality status. Any other value returns `INVALID_ARGUMENT`
and changes nothing.

- **Changed status**: the device plays a transition, then settles into the
  pattern for the new status. The new colour wipes across the matrix from
  the top-right corner (one LED every 40 ms, 640 ms total), holds solid for
  600 ms, then the steady pattern begins. Roughly 1.25 s in all.
- **Same status repeated**: treated as a heartbeat. The standby timer is
  refreshed and nothing animates.
- A status arriving mid-transition restarts the wipe from whatever is
  currently on the matrix.

If no `SET_AQI_STATUS` arrives for **60 seconds** the device transitions
back to standby. The first status after standby always animates, even if
it is the same code as before.

| Code | Name                  | Pattern                         | Colour       | Animation               |
|------|-----------------------|---------------------------------|--------------|-------------------------|
| 0x00 | Excellent             | Inner 2×2 only                  | Green        | Static                  |
| 0x01 | Good                  | Full matrix                     | Green        | Static                  |
| 0x02 | Good (degrading)      | Full matrix                     | Green        | Breathing               |
| 0x03 | Moderate              | Full matrix                     | Yellow       | Static                  |
| 0x04 | Moderate (degrading)  | Perimeter yellow + inner orange | Dual         | Inner 2×2 breathing     |
| 0x05 | Poor                  | Full matrix                     | Orange       | Static                  |
| 0x06 | Poor (degrading)      | Full matrix                     | Orange       | Breathing               |
| 0x07 | Unhealthy             | Full matrix                     | Red          | Static                  |
| 0x08 | Unhealthy (degrading) | Full matrix                     | Red          | Breathing               |
| 0x09 | Very Unhealthy        | Full matrix                     | Purple       | Static                  |
| 0x0A | Very Unhealthy (deg)  | Full matrix                     | Purple       | Breathing               |
| 0x0B | Hazardous             | Full matrix                     | Purple       | Blink 500 ms            |
| 0x0C | Extreme               | Full matrix                     | Purple ↔ Red | Fast alternating 300 ms |

Colours (RGB before brightness scaling): green (0, 180, 0), yellow
(255, 210, 0), orange (255, 100, 0), red (255, 0, 0), purple (140, 0, 140),
extreme's alternate red (220, 0, 0).

Breathing is a 16-step brightness cycle at 80 ms per step (1.28 s per
cycle). It never goes fully dark and never reaches the full static
brightness, so it reads as "alive" rather than blinking.

**Standby** (boot, no data, or 60 s timeout): soft white-blue breathing,
RGB (150, 180, 255), same cadence.

### Threshold mapping (sender side)

The device does not know about sensor values. The sender maps a reading to
a code; this is the mapping the codes were designed for:

| Code | Name                  | CO₂ (ppm)   | PM2.5 (µg/m³) |
|------|-----------------------|-------------|---------------|
| 0x00 | Excellent             | 0–400       | 0–2           |
| 0x01 | Good                  | 400–600     | 2–5           |
| 0x02 | Good (degrading)      | 600–800     | 5–9           |
| 0x03 | Moderate              | 800–1 000   | 9–15          |
| 0x04 | Moderate (degrading)  | 1 000–1 250 | 15–25         |
| 0x05 | Poor                  | 1 250–1 500 | 25–35.4       |
| 0x06 | Poor (degrading)      | 1 500–1 750 | 35.4–45       |
| 0x07 | Unhealthy             | 1 750–2 000 | 45–55.4       |
| 0x08 | Unhealthy (degrading) | 2 000–2 500 | 55.4–75       |
| 0x09 | Very Unhealthy        | 2 500–3 000 | 75–125        |
| 0x0A | Very Unhealthy (deg)  | 3 000–4 000 | 125–200       |
| 0x0B | Hazardous             | 4 000–5 000 | 200–300       |
| 0x0C | Extreme               | > 5 000     | > 300         |

---

## Connection rules

- **One client at a time, newest wins.** A new TCP connection replaces the
  current one; the old socket is closed. A sender that vanished without
  closing can therefore never block a fresh sender.
- **TCP keepalive** is enabled on the accepted socket (10 s idle, 5 s
  interval, 3 probes), so a dead peer is detected in about 25 s even
  without a new connection.
- **Idle timeout.** A client that completes no frame for 90 s is dropped.
  A long-lived sender should send `PING` or a status at least that often.
- **Frame timeout.** If a frame stops arriving part-way for 2 s, the
  partial bytes are discarded and the parser resynchronises.
- Both "connect per update" and "one persistent connection" are supported.

---

## Finding the device

The TCP contract above assumes the sender knows the device's address. With
`DISCOVERY_URL` set in `creds.h`, the device announces itself instead: every
time Wi-Fi comes up it POSTs this JSON to the URL, and refreshes it every
5 minutes (retrying every 30 s after a failure):

```json
{"name":"living-room","ip":"192.168.1.42","port":7777,
 "mac":"CC:50:E3:3C:E9:03","protocol":2,"firmware":"0.3.0","uptime_s":4242}
```

Headers: `Content-Type: application/json`, `User-Agent: neoncore/<firmware>`,
and `Authorization: Bearer <DISCOVERY_TOKEN>` when a token is configured. Any
2xx response counts as success. `name` defaults to `neoncore-` followed by
the last three MAC bytes in lowercase hex. Senders then look the device up by
name and connect to `ip:port`. `tools/discovery_server.py` is a reference
registry that accepts these registrations and serves `GET /devices`.

This is a convenience for locating the device; it is not part of the TCP
protocol and a device with discovery disabled behaves identically on port
7777.

---

## Security model

There is no authentication or encryption. Anyone who can reach port 7777
can change the display. This is intended for a trusted home LAN only. Do
not expose the port to the internet. The fallback access point is open
unless `WIFI_AP_PASSWORD` is set in `creds.h`.

---

## Python client example

```python
import socket

VERSION = 0x02
CMD_SET_AQI_STATUS = 0x03

def checksum(data):
    r = 0
    for b in data:
        r ^= b
    return r

def build_frame(cmd, payload=b''):
    header = b'\x4C\x4D' + bytes([VERSION, cmd, len(payload)]) + payload
    return header + bytes([checksum(header)])

def send_aqi_status(host, port, status):
    with socket.create_connection((host, port), timeout=5) as s:
        s.sendall(build_frame(CMD_SET_AQI_STATUS, bytes([status])))
        resp = s.recv(6)
        return resp[4] == 0x00   # True = OK

send_aqi_status('192.168.1.42', 7777, 0x01)  # Good
```

`tools/client.py` is a complete command-line client and `tools/aqi_example.py`
cycles through all 13 levels.
