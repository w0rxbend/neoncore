#!/usr/bin/env python3
"""Test client for the neoncore device (protocol v2).

Examples:
  python tools/client.py --host 192.168.1.42 ping
  python tools/client.py --host 192.168.1.42 brightness 60
  python tools/client.py --host 192.168.1.42 panel off
  python tools/client.py --host 192.168.1.42 aqi moderate
  python tools/client.py --host 192.168.1.42 aqi 0x0C
"""

import argparse
import socket
import sys

HOST = "192.168.1.100"
PORT = 7777

MAGIC = b"\x4C\x4D"
VERSION = 0x02
RESPONSE_SIZE = 6

CMD_PING = 0x00
CMD_SET_BRIGHTNESS = 0x01
CMD_SET_PANEL_ENABLED = 0x02
CMD_SET_AQI_STATUS = 0x03

STATUS_NAMES = {
    0x00: "OK",
    0x01: "BAD_MAGIC",
    0x02: "UNSUPPORTED_VERSION",
    0x03: "UNKNOWN_COMMAND",
    0x04: "INVALID_LENGTH",
    0x05: "CHECKSUM_MISMATCH",
    0x06: "INVALID_ARGUMENT",
}

AQI_STATUS_NAMES = {
    0x00: "excellent",
    0x01: "good",
    0x02: "good_degrading",
    0x03: "moderate",
    0x04: "moderate_degrading",
    0x05: "poor",
    0x06: "poor_degrading",
    0x07: "unhealthy",
    0x08: "unhealthy_degrading",
    0x09: "very_unhealthy",
    0x0A: "very_unhealthy_degrading",
    0x0B: "hazardous",
    0x0C: "extreme",
}
AQI_STATUS_BY_NAME = {name: code for code, name in AQI_STATUS_NAMES.items()}


def checksum(data: bytes) -> int:
    result = 0
    for b in data:
        result ^= b
    return result


def build_frame(command: int, payload: bytes = b"") -> bytes:
    header = MAGIC + bytes([VERSION, command, len(payload)]) + payload
    return header + bytes([checksum(header)])


def send(sock: socket.socket, frame: bytes) -> int:
    sock.sendall(frame)
    response = b""
    while len(response) < RESPONSE_SIZE:
        chunk = sock.recv(RESPONSE_SIZE - len(response))
        if not chunk:
            print("ERROR: connection closed before response")
            return -1
        response += chunk

    if response[:4] != MAGIC + bytes([VERSION, 0x80]):
        print(f"ERROR: unexpected response header {response.hex()}")
        return -1
    if checksum(response[:5]) != response[5]:
        print("ERROR: response checksum mismatch")
        return -1

    status = response[4]
    print(f"  -> {STATUS_NAMES.get(status, f'0x{status:02X}')}")
    return status


def connect(host: str, port: int) -> socket.socket:
    sock = socket.create_connection((host, port), timeout=5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return sock


def cmd_ping(args):
    with connect(args.host, args.port) as s:
        print("PING")
        return send(s, build_frame(CMD_PING))


def cmd_brightness(args):
    with connect(args.host, args.port) as s:
        print(f"SET_BRIGHTNESS {args.value}")
        return send(s, build_frame(CMD_SET_BRIGHTNESS, bytes([args.value])))


def cmd_panel(args):
    enabled = 1 if args.state == "on" else 0
    with connect(args.host, args.port) as s:
        print(f"SET_PANEL_ENABLED {args.state}")
        return send(s, build_frame(CMD_SET_PANEL_ENABLED, bytes([enabled])))


def cmd_aqi(args):
    code = args.status
    with connect(args.host, args.port) as s:
        name = AQI_STATUS_NAMES.get(code, f"unknown(0x{code:02X})")
        print(f"SET_AQI_STATUS 0x{code:02X} {name}")
        return send(s, build_frame(CMD_SET_AQI_STATUS, bytes([code])))


def parse_byte(value: str) -> int:
    number = int(value, 0)
    if not 0 <= number <= 255:
        raise argparse.ArgumentTypeError("value must be 0-255")
    return number


def parse_aqi(value: str) -> int:
    if value in AQI_STATUS_BY_NAME:
        return AQI_STATUS_BY_NAME[value]
    try:
        return parse_byte(value)
    except (ValueError, argparse.ArgumentTypeError):
        raise argparse.ArgumentTypeError(
            f"unknown status '{value}'; use a name ({', '.join(AQI_STATUS_BY_NAME)}) or a number"
        )


def main():
    parser = argparse.ArgumentParser(description="neoncore device client (protocol v2)")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", type=int, default=PORT)
    sub = parser.add_subparsers(dest="cmd", required=True)

    sub.add_parser("ping", help="connectivity check")

    p = sub.add_parser("brightness", help="set global brightness")
    p.add_argument("value", type=parse_byte, help="0-255")

    p = sub.add_parser("panel", help="blank or restore the panel")
    p.add_argument("state", choices=["on", "off"])

    p = sub.add_parser("aqi", help="set air-quality status")
    aqi_list = ", ".join(f"0x{k:02X}={v}" for k, v in AQI_STATUS_NAMES.items())
    p.add_argument("status", type=parse_aqi, help=f"name or code: {aqi_list}")

    args = parser.parse_args()

    dispatch = {
        "ping": cmd_ping,
        "brightness": cmd_brightness,
        "panel": cmd_panel,
        "aqi": cmd_aqi,
    }

    try:
        status = dispatch[args.cmd](args)
    except (ConnectionRefusedError, TimeoutError, OSError) as e:
        print(f"Connection error: {e}", file=sys.stderr)
        sys.exit(1)

    sys.exit(0 if status == 0 else 2)


if __name__ == "__main__":
    main()
