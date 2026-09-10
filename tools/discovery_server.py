#!/usr/bin/env python3
"""Reference discovery registry for neoncore devices. Standard library only.

Devices POST their registration JSON to /register; senders GET /devices to
find them. Registrations expire if not refreshed (the firmware refreshes
every 5 minutes; the default TTL here is 15).

    python3 tools/discovery_server.py --port 8787 [--token SECRET]

    curl http://localhost:8787/devices
    curl http://localhost:8787/devices/living-room

Then in creds.h on the device:

    #define DISCOVERY_URL "http://<this host>:8787/register"
"""

import argparse
import json
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

REQUIRED_FIELDS = ("name", "ip", "port")

_lock = threading.Lock()
_devices: dict[str, dict] = {}
_token: str | None = None
_ttl_seconds = 900


def _now() -> float:
    return time.time()


def _prune() -> None:
    cutoff = _now() - _ttl_seconds
    stale = [name for name, d in _devices.items() if d["last_seen"] < cutoff]
    for name in stale:
        del _devices[name]


def _public(d: dict) -> dict:
    out = dict(d)
    out["age_s"] = int(_now() - d["last_seen"])
    out["last_seen"] = time.strftime("%Y-%m-%dT%H:%M:%S%z", time.localtime(d["last_seen"]))
    return out


class Handler(BaseHTTPRequestHandler):
    server_version = "neoncore-discovery/1.0"

    def _send(self, code: int, body: dict | list) -> None:
        data = json.dumps(body, indent=2).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _authorized(self) -> bool:
        if _token is None:
            return True
        return self.headers.get("Authorization", "") == f"Bearer {_token}"

    def do_POST(self) -> None:  # noqa: N802
        if self.path != "/register":
            self._send(404, {"error": "not found"})
            return
        if not self._authorized():
            self._send(401, {"error": "bad or missing bearer token"})
            return

        length = int(self.headers.get("Content-Length", "0"))
        try:
            payload = json.loads(self.rfile.read(length) or b"{}")
        except json.JSONDecodeError:
            self._send(400, {"error": "body is not JSON"})
            return

        missing = [f for f in REQUIRED_FIELDS if f not in payload]
        if missing:
            self._send(400, {"error": f"missing fields: {', '.join(missing)}"})
            return

        record = dict(payload)
        record["remote_addr"] = self.client_address[0]
        record["last_seen"] = _now()

        with _lock:
            _prune()
            is_new = payload["name"] not in _devices
            _devices[payload["name"]] = record

        print(f"{'registered' if is_new else 'refreshed '} {payload['name']:<24} "
              f"{payload['ip']}:{payload['port']}  (from {self.client_address[0]})",
              flush=True)
        self._send(200, {"ok": True, "name": payload["name"], "ttl_s": _ttl_seconds})

    def do_GET(self) -> None:  # noqa: N802
        with _lock:
            _prune()
            if self.path == "/devices":
                self._send(200, [_public(d) for d in _devices.values()])
                return
            if self.path.startswith("/devices/"):
                name = self.path[len("/devices/"):]
                if name in _devices:
                    self._send(200, _public(_devices[name]))
                else:
                    self._send(404, {"error": f"no device named {name!r}"})
                return
        self._send(404, {"error": "try GET /devices or POST /register"})

    def log_message(self, fmt, *args):  # quieter than the default
        pass


def main() -> None:
    global _token, _ttl_seconds

    parser = argparse.ArgumentParser(description="neoncore discovery registry")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8787)
    parser.add_argument("--token", help="require this bearer token on /register")
    parser.add_argument("--ttl", type=int, default=900,
                        help="seconds before an unrefreshed device is dropped (default 900)")
    args = parser.parse_args()

    _token = args.token
    _ttl_seconds = args.ttl

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"neoncore discovery registry on http://{args.host}:{args.port}  "
          f"(POST /register, GET /devices, ttl {args.ttl}s"
          f"{', bearer token required' if args.token else ''})", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nbye", file=sys.stderr)


if __name__ == "__main__":
    main()
