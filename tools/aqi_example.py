#!/usr/bin/env python3
"""
AQI status example — cycles through all AirGradient quality levels.
Usage: python tools/aqi_example.py --host 192.168.1.192
"""

import argparse
import socket
import time

HOST = "192.168.1.192"
PORT = 7777

AQI_EXCELLENT          = 0x00
AQI_GOOD               = 0x01
AQI_GOOD_DEG           = 0x02
AQI_MODERATE           = 0x03
AQI_MODERATE_DEG       = 0x04
AQI_POOR               = 0x05
AQI_POOR_DEG           = 0x06
AQI_UNHEALTHY          = 0x07
AQI_UNHEALTHY_DEG      = 0x08
AQI_VERY_UNHEALTHY     = 0x09
AQI_VERY_UNHEALTHY_DEG = 0x0A
AQI_HAZARDOUS          = 0x0B
AQI_EXTREME            = 0x0C

AQI_LABELS = {
    AQI_EXCELLENT:          "0x00 Excellent             inner 2x2 green static      CO2 0-400   / PM2.5 0-2",
    AQI_GOOD:               "0x01 Good                  full green static            CO2 400-600 / PM2.5 2-5",
    AQI_GOOD_DEG:           "0x02 Good (degrading)       full green breathing         CO2 600-800 / PM2.5 5-9",
    AQI_MODERATE:           "0x03 Moderate               full yellow static           CO2 800-1000/ PM2.5 9-15",
    AQI_MODERATE_DEG:       "0x04 Moderate (degrading)   perim yellow+inner orange   CO2 1000-1250/PM2.5 15-25",
    AQI_POOR:               "0x05 Poor                   full orange static           CO2 1250-1500/PM2.5 25-35",
    AQI_POOR_DEG:           "0x06 Poor (degrading)       full orange breathing        CO2 1500-1750/PM2.5 35-45",
    AQI_UNHEALTHY:          "0x07 Unhealthy              full red static              CO2 1750-2000/PM2.5 45-55",
    AQI_UNHEALTHY_DEG:      "0x08 Unhealthy (degrading)  full red breathing           CO2 2000-2500/PM2.5 55-75",
    AQI_VERY_UNHEALTHY:     "0x09 Very Unhealthy         full purple static           CO2 2500-3000/PM2.5 75-125",
    AQI_VERY_UNHEALTHY_DEG: "0x0A Very Unhealthy (deg)   full purple breathing        CO2 3000-4000/PM2.5 125-200",
    AQI_HAZARDOUS:          "0x0B Hazardous              full purple blink            CO2 4000-5000/PM2.5 200-300",
    AQI_EXTREME:            "0x0C Extreme                full purple/red alternating  CO2 >5000   / PM2.5 >300",
}


def checksum(data: bytes) -> int:
    result = 0
    for b in data:
        result ^= b
    return result


def build_frame(command: int, payload: bytes = b"") -> bytes:
    header = b"\x4C\x4D\x01" + bytes([command, len(payload)]) + payload
    return header + bytes([checksum(header)])


def set_aqi_status(host: str, port: int, status: int) -> bool:
    try:
        with socket.create_connection((host, port), timeout=5) as s:
            s.sendall(build_frame(0x0B, bytes([status])))
            resp = s.recv(6)
            return len(resp) == 6 and resp[4] == 0x00
    except OSError as e:
        print(f"  connection error: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="AQI status demo")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", type=int, default=PORT)
    parser.add_argument("--hold", type=float, default=3.0,
                        help="seconds to hold each status (default 3)")
    args = parser.parse_args()

    print(f"Connecting to {args.host}:{args.port}")
    print(f"Holding each status for {args.hold}s\n")

    statuses = [
        AQI_EXCELLENT,
        AQI_GOOD,
        AQI_GOOD_DEG,
        AQI_MODERATE,
        AQI_MODERATE_DEG,
        AQI_POOR,
        AQI_POOR_DEG,
        AQI_UNHEALTHY,
        AQI_UNHEALTHY_DEG,
        AQI_VERY_UNHEALTHY,
        AQI_VERY_UNHEALTHY_DEG,
        AQI_HAZARDOUS,
        AQI_EXTREME,
    ]

    for status in statuses:
        label = AQI_LABELS[status]
        print(f"  {label} ... ", end="", flush=True)
        ok = set_aqi_status(args.host, args.port, status)
        print("OK" if ok else "FAILED")
        time.sleep(args.hold)

    print("\nDone — display returns to standby after 60s with no updates.")


if __name__ == "__main__":
    main()
