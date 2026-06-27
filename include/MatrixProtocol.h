#pragma once

#include <Arduino.h>

#include "AppConfig.h"

namespace MatrixProtocol {

// The TCP protocol is intentionally tiny:
//
//   'L' 'M' version command payloadLength payload checksum
//
// A fixed header and a 1-byte payload length keep the protocol tiny.
// Some commands need metadata, so the maximum payload is now 255 bytes.
constexpr uint8_t kMagic0 = 0x4C;
constexpr uint8_t kMagic1 = 0x4D;
constexpr uint8_t kVersion = 0x01;
constexpr uint8_t kResponseCommand = 0x80;
constexpr uint16_t kMaxPayloadSize = 255;
constexpr uint16_t kHeaderSize = 5;
constexpr uint16_t kChecksumSize = 1;
constexpr uint16_t kMaxFrameSize = kHeaderSize + kMaxPayloadSize + kChecksumSize;
constexpr uint16_t kResponseSize = 6;

enum class Command : uint8_t {
  // Connectivity check. Does not change the matrix.
  kPing = 0x00,

  // Clears all pixels to black.
  kClear = 0x01,

  // Updates global brightness with one payload byte: 0..255.
  kSetBrightness = 0x02,

  // Fills the matrix with one RGB triple.
  kFill = 0x03,

  // Sets one logical x/y pixel using payload: x, y, r, g, b.
  kSetPixel = 0x04,

  // Replaces the full physical LED buffer using 16 RGB triples.
  kSetFrame = 0x05,

  // Turns visible panel output off/on without clearing the stored image.
  kSetPanelEnabled = 0x06,

  // Sets a single static color and keeps matrix in static mode.
  kSetStaticColor = 0x07,

  // Applies one preset effect.
  kSetPresetEffect = 0x08,

  // Uploads one frame for the single custom animation slot.
  kUploadCustomFrame = 0x09,

  // Stops any running effect and returns control to direct commands.
  kStopEffect = 0x0A,

  // Sets the AirGradient air-quality status. Payload: 1 byte AqiStatus code.
  // Resets the standby timeout. If no kSetAqiStatus is received within
  // AppConfig::kAqiStandbyTimeoutMs the display returns to standby breathing.
  kSetAqiStatus = 0x0B,
};

// Air-quality status codes for kSetAqiStatus.
// Colors mirror the standard AirGradient ONE LED indicator scale.
// 13 visually distinct air-quality statuses for a 4×4 LED matrix.
// Each uses a unique combination of pattern, colour, and animation so it is
// immediately identifiable at a glance.
// The same code set covers both CO2 (ppm) and PM2.5 (µg/m³) — the sender
// maps the sensor reading to the appropriate code before transmitting.
enum class AqiStatus : uint8_t {
  // Green — Good air quality
  kExcellent          = 0x00,  // Inner 2×2 green static   — CO2 0–400    / PM2.5 0–2
  kGood               = 0x01,  // Full green static         — CO2 400–600  / PM2.5 2–5
  kGoodDegrading      = 0x02,  // Full green breathing      — CO2 600–800  / PM2.5 5–9

  // Yellow — Moderate
  kModerate           = 0x03,  // Full yellow static        — CO2 800–1000 / PM2.5 9–15
  kModerateDegrading  = 0x04,  // Perimeter yellow + inner 2×2 orange breathing
                               //                           — CO2 1000–1250/ PM2.5 15–25

  // Orange — Unhealthy for Sensitive Groups
  kPoor               = 0x05,  // Full orange static        — CO2 1250–1500/ PM2.5 25–35.4
  kPoorDegrading      = 0x06,  // Full orange breathing     — CO2 1500–1750/ PM2.5 35.4–45

  // Red — Unhealthy
  kUnhealthy          = 0x07,  // Full red static           — CO2 1750–2000/ PM2.5 45–55.4
  kUnhealthyDegrading = 0x08,  // Full red breathing        — CO2 2000–2500/ PM2.5 55.4–75

  // Purple — Very Unhealthy
  kVeryUnhealthy      = 0x09,  // Full purple static        — CO2 2500–3000/ PM2.5 75–125
  kVeryUnhealthyDeg   = 0x0A,  // Full purple breathing     — CO2 3000–4000/ PM2.5 125–200

  // Hazardous
  kHazardous          = 0x0B,  // Full purple blink         — CO2 4000–5000/ PM2.5 200–300
  kExtreme            = 0x0C,  // Full purple/red fast alternating blink — CO2 >5000 / PM2.5 >300
};

enum class Status : uint8_t {
  // Command was valid and has been applied.
  kOk = 0x00,

  // The first two bytes were not 'L' and 'M'.
  kBadMagic = 0x01,

  // The client is speaking another protocol version.
  kUnsupportedVersion = 0x02,

  // Header was valid, but command id is not implemented.
  kUnknownCommand = 0x03,

  // Command exists, but payload length or coordinate range is wrong.
  kInvalidLength = 0x04,

  // Frame arrived but did not pass the XOR checksum.
  kChecksumMismatch = 0x05,
};

// Simple corruption check for short LAN/AP packets. This is not cryptographic;
// it only catches malformed or truncated frames before applying LED changes.
uint8_t checksum(const uint8_t* data, uint16_t length);

}  // namespace MatrixProtocol
