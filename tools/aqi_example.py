#!/usr/bin/env python3
"""
AQI status demo: cycles through all 13 contract levels on one persistent
connection, so you can see the transition wipe between each pair of states.

Usage: python tools/aqi_example.py --host 192.168.1.42 --hold 3
"""

import argparse
import socket
import time

HOST = "192.168.1.192"
PORT = 7777

VERSION = 0x02
CMD_SET_AQI_STATUS = 0x03
RESPONSE_SIZE = 6

AQI_LEVELS = [
    (0x00, "Excellent",             "inner 2x2 green, static",           "CO2 0-400     / PM2.5 0-2"),
    (0x01, "Good",                  "full green, static",                "CO2 400-600   / PM2.5 2-5"),
    (0x02, "Good (degrading)",      "full green, breathing",             "CO2 600-800   / PM2.5 5-9"),
    (0x03, "Moderate",              "full yellow, static",               "CO2 800-1000  / PM2.5 9-15"),
    (0x04, "Moderate (degrading)",  "yellow perimeter, orange centre",   "CO2 1000-1250 / PM2.5 15-25"),
    (0x05, "Poor",                  "full orange, static",               "CO2 1250-1500 / PM2.5 25-35.4"),
    (0x06, "Poor (degrading)",      "full orange, breathing",            "CO2 1500-1750 / PM2.5 35.4-45"),
    (0x07, "Unhealthy",             "full red, static",                  "CO2 1750-2000 / PM2.5 45-55.4"),
    (0x08, "Unhealthy (degrading)", "full red, breathing",               "CO2 2000-2500 / PM2.5 55.4-75"),
    (0x09, "Very Unhealthy",        "full purple, static",               "CO2 2500-3000 / PM2.5 75-125"),
    (0x0A, "Very Unhealthy (deg)",  "full purple, breathing",            "CO2 3000-4000 / PM2.5 125-200"),
    (0x0B, "Hazardous",             "full purple, blink 500 ms",         "CO2 4000-5000 / PM2.5 200-300"),
    (0x0C, "Extreme",               "purple/red alternating 300 ms",     "CO2 >5000     / PM2.5 >300"),
]


def checksum(data: bytes) -> int:
    result = 0
    for b in data:
        result ^= b
    return result


def build_frame(command: int, payload: bytes = b"") -> bytes:
    header = b"\x4C\x4D" + bytes([VERSION, command, len(payload)]) + payload
    return header + bytes([checksum(header)])


def recv_exact(sock: socket.socket, size: int) -> bytes:
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("connection closed")
        data += chunk
    return data


def set_aqi_status(sock: socket.socket, status: int) -> bool:
    sock.sendall(build_frame(CMD_SET_AQI_STATUS, bytes([status])))
    resp = recv_exact(sock, RESPONSE_SIZE)
    return resp[4] == 0x00


def main():
    parser = argparse.ArgumentParser(description="AQI status demo")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", type=int, default=PORT)
    parser.add_argument("--hold", type=float, default=3.0,
                        help="seconds to hold each status (default 3)")
    args = parser.parse_args()

    print(f"Connecting to {args.host}:{args.port}")
    print(f"Holding each status for {args.hold}s\n")

    try:
        with socket.create_connection((args.host, args.port), timeout=5) as sock:
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            for code, name, visual, thresholds in AQI_LEVELS:
                print(f"  0x{code:02X} {name:<22} {visual:<34} {thresholds} ... ", end="", flush=True)
                print("OK" if set_aqi_status(sock, code) else "FAILED")
                time.sleep(args.hold)
    except OSError as e:
        print(f"\nconnection error: {e}")
        return

    print("\nDone. The display returns to standby 60 s after the last update.")


if __name__ == "__main__":
    main()
