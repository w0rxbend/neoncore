#!/usr/bin/env python3
"""Simple test client for the neoncore TCP matrix server."""

import argparse
import socket
import struct
import sys

HOST = "192.168.1.100"
PORT = 7777

MAGIC = b"\x4C\x4D"
VERSION = 0x01

CMD_PING = 0x00
CMD_CLEAR = 0x01
CMD_SET_BRIGHTNESS = 0x02
CMD_FILL = 0x03
CMD_SET_PIXEL = 0x04
CMD_SET_STATIC_COLOR = 0x07
CMD_SET_PRESET_EFFECT = 0x08
CMD_STOP_EFFECT = 0x0A

STATUS_NAMES = {
    0x00: "OK",
    0x01: "BAD_MAGIC",
    0x02: "UNSUPPORTED_VERSION",
    0x03: "UNKNOWN_COMMAND",
    0x04: "INVALID_LENGTH",
    0x05: "CHECKSUM_MISMATCH",
}

EFFECT_NAMES = {
    1: "chase", 2: "color_wipe", 3: "blink", 4: "wave", 5: "rain",
    6: "meteor", 7: "rainbow", 8: "breathing", 9: "scanner", 10: "sparkle",
    11: "fire", 12: "matrix_rain", 13: "ripple", 14: "theater_chase",
    15: "twinkle", 16: "comet", 17: "plasma", 18: "diagonal",
    19: "border_chase", 20: "heartbeat", 21: "pulse_wipe", 22: "confetti",
}


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
    response = sock.recv(6)
    if len(response) < 6:
        print("ERROR: short response")
        return -1
    status = response[4]
    name = STATUS_NAMES.get(status, f"0x{status:02X}")
    print(f"  -> {name}")
    return status


def connect(host: str, port: int) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect((host, port))
    return sock


def cmd_ping(args):
    with connect(args.host, args.port) as s:
        print("PING")
        send(s, build_frame(CMD_PING))


def cmd_clear(args):
    with connect(args.host, args.port) as s:
        print("CLEAR")
        send(s, build_frame(CMD_CLEAR))


def cmd_fill(args):
    r, g, b = args.color
    with connect(args.host, args.port) as s:
        print(f"FILL rgb({r},{g},{b})")
        send(s, build_frame(CMD_FILL, bytes([r, g, b])))


def cmd_brightness(args):
    with connect(args.host, args.port) as s:
        print(f"SET_BRIGHTNESS {args.value}")
        send(s, build_frame(CMD_SET_BRIGHTNESS, bytes([args.value])))


def cmd_pixel(args):
    x, y, r, g, b = args.x, args.y, *args.color
    with connect(args.host, args.port) as s:
        print(f"SET_PIXEL ({x},{y}) rgb({r},{g},{b})")
        send(s, build_frame(CMD_SET_PIXEL, bytes([x, y, r, g, b])))


def cmd_effect(args):
    effect_id = args.id
    interval = args.interval
    r, g, b = args.color
    payload = bytes([effect_id]) + struct.pack("<H", interval) + bytes([r, g, b])
    name = EFFECT_NAMES.get(effect_id, f"effect_{effect_id}")
    with connect(args.host, args.port) as s:
        print(f"SET_PRESET_EFFECT {name} interval={interval}ms rgb({r},{g},{b})")
        send(s, build_frame(CMD_SET_PRESET_EFFECT, payload))


def cmd_stop(args):
    with connect(args.host, args.port) as s:
        print("STOP_EFFECT")
        send(s, build_frame(CMD_STOP_EFFECT))


def parse_color(value: str):
    parts = value.split(",")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("color must be r,g,b (e.g. 255,0,0)")
    return [int(p) for p in parts]


def main():
    parser = argparse.ArgumentParser(description="neoncore TCP matrix client")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", type=int, default=PORT)
    sub = parser.add_subparsers(dest="cmd", required=True)

    sub.add_parser("ping")
    sub.add_parser("clear")
    sub.add_parser("stop")

    p = sub.add_parser("fill")
    p.add_argument("color", type=parse_color, help="r,g,b")

    p = sub.add_parser("brightness")
    p.add_argument("value", type=int, help="0-255")

    p = sub.add_parser("pixel")
    p.add_argument("x", type=int)
    p.add_argument("y", type=int)
    p.add_argument("color", type=parse_color, help="r,g,b")

    p = sub.add_parser("effect")
    p.add_argument("id", type=int, help=f"1-22  ({', '.join(f'{k}={v}' for k,v in EFFECT_NAMES.items())})")
    p.add_argument("--interval", type=int, default=140, help="frame delay ms (default 140)")
    p.add_argument("--color", type=parse_color, default=[255, 255, 255], help="r,g,b (default 255,255,255)")

    args = parser.parse_args()

    dispatch = {
        "ping": cmd_ping,
        "clear": cmd_clear,
        "fill": cmd_fill,
        "brightness": cmd_brightness,
        "pixel": cmd_pixel,
        "effect": cmd_effect,
        "stop": cmd_stop,
    }

    try:
        dispatch[args.cmd](args)
    except (ConnectionRefusedError, TimeoutError, OSError) as e:
        print(f"Connection error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
