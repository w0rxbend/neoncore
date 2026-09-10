#pragma once

#include <stdint.h>

// Keep private Wi-Fi credentials outside git. PlatformIO automatically adds
// the include directory, so a local include/creds.h is enough.
#if __has_include("creds.h")
#include "creds.h"
#endif

#ifndef LED_PIN
#define LED_PIN 4
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// Optional password for the fallback access point. Leave empty for an open
// AP. WPA2 requires at least 8 characters; shorter values are ignored.
#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD ""
#endif

// Optional: list nearby networks on the serial console during boot. Useful
// when diagnosing a connection problem, but it adds a few seconds to boot.
#ifndef WIFI_SCAN_ON_BOOT
#define WIFI_SCAN_ON_BOOT 0
#endif

// Optional discovery registration. When DISCOVERY_URL is defined the device
// POSTs a JSON document describing itself (name, IP, port, MAC, protocol and
// firmware version) to that URL every time Wi-Fi comes up, and refreshes it
// periodically, so senders can look the device up instead of hard-coding an
// address. Leave undefined to disable (nothing is compiled in).
//   #define DISCOVERY_URL         "http://192.168.1.10:8787/register"
//   #define DISCOVERY_TOKEN       "optional-bearer-token"
//   #define DISCOVERY_DEVICE_NAME "living-room"   // default: neoncore-<mac tail>
//   #define DISCOVERY_HTTPS       1               // link TLS for an https:// URL
// Flash cost: plain-HTTP discovery adds about 175 KB (HTTPClient);
// DISCOVERY_HTTPS adds a further ~125 KB (mbedTLS). Leave it out for a
// plain-HTTP registry on the LAN.
#if defined(DISCOVERY_URL)
#define NEONCORE_DISCOVERY 1
#endif

#ifndef DISCOVERY_TOKEN
#define DISCOVERY_TOKEN ""
#endif

#ifndef DISCOVERY_DEVICE_NAME
#define DISCOVERY_DEVICE_NAME ""
#endif

// Optional static IP. Define STATIC_IP, STATIC_GATEWAY and STATIC_SUBNET in
// creds.h to skip DHCP. STATIC_DNS is optional and defaults to the gateway.
// Leave undefined to use DHCP (default). With discovery enabled DHCP is the
// recommended mode: the device announces whatever address it was given.
//   #define STATIC_IP      "192.168.1.123"
//   #define STATIC_GATEWAY "192.168.1.1"
//   #define STATIC_SUBNET  "255.255.255.0"
//   #define STATIC_DNS     "8.8.8.8"

// Optional MAC address override. Define in creds.h to replace the burned-in
// MAC. Use a locally-administered address (first byte 0x02..0x0E even).
// Leave undefined to use the factory MAC.
//   #define WIFI_MAC_OVERRIDE {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}

namespace AppConfig {

// Hardware layout for the current ESP32 + WS2812B 4x4 build.
constexpr uint8_t kLedPin = LED_PIN;
constexpr uint8_t kMatrixWidth = 4;
constexpr uint8_t kMatrixHeight = 4;
constexpr uint16_t kLedCount = kMatrixWidth * kMatrixHeight;

// 16 LEDs draw significantly less peak current than a full 8x8, so a moderate
// default brightness is safe on USB power.
constexpr uint8_t kDefaultBrightness = 40;

// Give USB power, the external LED supply, and the ESP32 radio a moment to
// settle before LEDs and Wi-Fi start drawing burst current.
constexpr uint32_t kBootSettleDelayMs = 2000;

// The ESP32 brown-out detector resets the chip when VDD dips below ~2.4 V.
// Cheap USB supplies and the Wi-Fi radio's transmit bursts can trip it even
// when the board is otherwise fine, so it is disabled by default. The cost is
// that a genuinely sagging supply corrupts state instead of resetting; if
// you see garbage on the LEDs or random hangs, re-enable it and fix the
// power supply.
constexpr bool kDisableBrownoutDetector = true;

// Reported in the discovery registration and on the serial banner.
constexpr char kFirmwareVersion[] = "0.3.0";

// Network behavior. Empty WIFI_SSID falls back to AP mode.
constexpr uint16_t kTcpPort = 7777;
constexpr char kAccessPointSsid[] = "led-matrix";
constexpr bool kScanNetworksOnBoot = WIFI_SCAN_ON_BOOT != 0;

// Wi-Fi station connect is non-blocking. If the link is not up after this
// interval, WiFi.begin() is issued again.
constexpr uint32_t kWifiRetryIntervalMs = 15000;

// One TCP client at a time. A new connection replaces the current one.
// A client that sends no complete frame for this long is dropped; send PING
// to keep a long-lived connection open.
constexpr uint32_t kClientIdleTimeoutMs = 90000;

// A frame whose bytes stop arriving mid-way is discarded after this long so
// the parser can re-synchronise on the next frame.
constexpr uint32_t kFrameTimeoutMs = 2000;

// Upper bound on bytes drained from the socket per loop pass, so a chatty
// client cannot starve the animation engine.
constexpr uint16_t kMaxClientBytesPerLoop = 256;

// TCP keepalive on the accepted client so a peer that vanished without a
// FIN (host power loss, cable pull) is detected within roughly
// idle + interval * count seconds.
constexpr int kTcpKeepAliveIdleSec = 10;
constexpr int kTcpKeepAliveIntervalSec = 5;
constexpr int kTcpKeepAliveCount = 3;

// Discovery registration cadence. A successful registration is refreshed
// every kDiscoveryRefreshIntervalMs so the registry can expire devices that
// went away; a failed one is retried every kDiscoveryRetryIntervalMs. Both
// timers restart whenever Wi-Fi reconnects, because DHCP may have handed
// out a new address.
constexpr uint32_t kDiscoveryRefreshIntervalMs = 300000;
constexpr uint32_t kDiscoveryRetryIntervalMs = 30000;
constexpr uint16_t kDiscoveryHttpTimeoutMs = 5000;
constexpr uint16_t kDiscoveryPayloadCapacity = 256;

// AQI presentation timing. Breathing uses a 16-step table, so a full
// inhale/exhale cycle is 16 * step (1.28 s at 80 ms). Standby breathes at
// the same rate.
constexpr uint16_t kAqiBreathingStepMs = 80;
constexpr uint16_t kAqiBlinkIntervalMs = 500;
constexpr uint16_t kAqiAlternateIntervalMs = 300;

// Transition played when the status changes: the new colour wipes across
// the matrix one LED per step, then holds solid, then the steady pattern
// for the new status begins. Total = 16 * wipe step + hold.
constexpr uint16_t kAqiTransitionWipeStepMs = 40;
constexpr uint16_t kAqiTransitionHoldMs = 600;

// Standby: if no kSetAqiStatus is received within this window the display
// returns to the white-blue breathing animation.
constexpr uint32_t kAqiStandbyTimeoutMs = 60000;

}  // namespace AppConfig
