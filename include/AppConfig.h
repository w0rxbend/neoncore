#pragma once

#include <Arduino.h>

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

// Optional static IP. Define all four in creds.h to skip DHCP entirely.
// Leave undefined to use DHCP (default).
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

// Give USB power, the external LED supply, and the ESP8266 radio a moment to
// settle before LEDs and Wi-Fi start drawing burst current.
constexpr uint32_t kBootSettleDelayMs = 2000;

// Network behavior. Empty WIFI_SSID falls back to AP mode.
constexpr uint16_t kTcpPort = 7777;
constexpr char kAccessPointSsid[] = "led-matrix";
constexpr uint32_t kStationConnectTimeoutMs = 60000;
constexpr uint32_t kWifiRetryIntervalMs = 10000;
constexpr uint32_t kServerHealthCheckIntervalMs = 5000;
constexpr uint8_t kMaxCustomFrames = 8;
constexpr uint16_t kDefaultPresetIntervalMs = 140;
constexpr uint16_t kMinEffectFrameDelayMs = 20;

// Standby: if no kSetAqiStatus is received within this window the display
// returns to the white-blue breathing animation.
constexpr uint32_t kAqiStandbyTimeoutMs = 60000;

}  // namespace AppConfig
