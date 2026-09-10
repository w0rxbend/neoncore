#pragma once

#include <stdint.h>

#include "AppConfig.h"

// Wire protocol for the neoncore device. This header has no hardware
// dependency so the parser can be unit-tested on the host.
namespace MatrixProtocol {

// Every frame, command or response, uses the same envelope:
//
//   'L' 'M' version command payloadLength payload checksum
//
// A fixed 5-byte header, up to 255 payload bytes, and a 1-byte XOR checksum
// over everything before it.
constexpr uint8_t kMagic0 = 0x4C;
constexpr uint8_t kMagic1 = 0x4D;
constexpr uint8_t kVersion = 0x02;
constexpr uint8_t kResponseCommand = 0x80;
constexpr uint16_t kMaxPayloadSize = 255;
constexpr uint16_t kHeaderSize = 5;
constexpr uint16_t kChecksumSize = 1;
constexpr uint16_t kMaxFrameSize = kHeaderSize + kMaxPayloadSize + kChecksumSize;
constexpr uint16_t kResponseSize = 6;

// Header byte offsets.
constexpr uint16_t kCommandOffset = 3;
constexpr uint16_t kLengthOffset = 4;
constexpr uint16_t kPayloadOffset = 5;

// Protocol version 2 is deliberately small: the device is an air-quality
// indicator, not a general LED canvas. The display content is fully
// determined by the AQI status; the other commands are device controls.
enum class Command : uint8_t {
  // Connectivity check. Payload: none. Does not change the display.
  kPing = 0x00,

  // Global brightness. Payload: 1 byte, 0..255.
  kSetBrightness = 0x01,

  // Blank or restore the panel without forgetting the current state.
  // Payload: 1 byte, 0 = off, non-zero = on.
  kSetPanelEnabled = 0x02,

  // Air-quality status. Payload: 1 byte AqiStatus code.
  // Resets the standby timeout. A changed status plays a transition
  // animation before settling; the same status repeated is a heartbeat.
  kSetAqiStatus = 0x03,
};

// Air-quality status codes for kSetAqiStatus. The same code set covers both
// CO2 (ppm) and PM2.5 (µg/m³); the sender maps the reading to a code.
enum class AqiStatus : uint8_t {
  kExcellent          = 0x00,  // Inner 2x2 green, static
  kGood               = 0x01,  // Full green, static
  kGoodDegrading      = 0x02,  // Full green, breathing
  kModerate           = 0x03,  // Full yellow, static
  kModerateDegrading  = 0x04,  // Yellow perimeter, orange inner 2x2 breathing
  kPoor               = 0x05,  // Full orange, static
  kPoorDegrading      = 0x06,  // Full orange, breathing
  kUnhealthy          = 0x07,  // Full red, static
  kUnhealthyDegrading = 0x08,  // Full red, breathing
  kVeryUnhealthy      = 0x09,  // Full purple, static
  kVeryUnhealthyDeg   = 0x0A,  // Full purple, breathing
  kHazardous          = 0x0B,  // Full purple, blink 500 ms
  kExtreme            = 0x0C,  // Full purple/red alternating 300 ms
};
constexpr uint8_t kAqiStatusCount = 13;

enum class Status : uint8_t {
  // Command was valid and has been applied.
  kOk = 0x00,

  // The first two bytes were not 'L' and 'M'.
  kBadMagic = 0x01,

  // The client is speaking another protocol version.
  kUnsupportedVersion = 0x02,

  // Header was valid, but command id is not implemented.
  kUnknownCommand = 0x03,

  // Command exists, but payload length is wrong for it.
  kInvalidLength = 0x04,

  // Frame arrived but did not pass the XOR checksum.
  kChecksumMismatch = 0x05,

  // Payload length was right but a value is outside the contract, for
  // example an AQI status code above 0x0C.
  kInvalidArgument = 0x06,
};

// Simple corruption check for short LAN/AP packets. This is not cryptographic;
// it only catches malformed or truncated frames before applying changes.
uint8_t checksum(const uint8_t* data, uint16_t length);

// Builds the 6-byte response frame for a status code into `out`.
void buildResponse(Status status, uint8_t* out);

// Byte-at-a-time frame parser. Feed it every received byte; when push()
// returns kFrameReady the accessors describe the complete frame (checksum
// already verified). When it returns kError, error() says what went wrong
// and the parser has already reset itself.
class FrameParser {
 public:
  enum class Result : uint8_t {
    kNeedMore,
    kFrameReady,
    kError,
  };

  FrameParser();

  void reset();
  Result push(uint8_t value);

  // True while a frame is partially buffered.
  bool inProgress() const { return index_ > 0; }

  // Valid after kError.
  Status error() const { return error_; }

  // Valid after kFrameReady.
  uint8_t command() const { return buffer_[kCommandOffset]; }
  uint8_t payloadLength() const { return buffer_[kLengthOffset]; }
  const uint8_t* payload() const { return &buffer_[kPayloadOffset]; }

 private:
  Result fail(Status status);

  uint8_t buffer_[kMaxFrameSize];
  uint16_t index_;
  uint16_t expectedSize_;
  Status error_;
  bool frameReady_;
};

}  // namespace MatrixProtocol
