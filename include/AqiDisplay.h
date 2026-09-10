#pragma once

#include <stdint.h>

#include "AppConfig.h"
#include "MatrixLayout.h"

// The AQI contract, rendered. This module has no hardware dependency: it
// turns (status code, time) into a 48-byte physical-order RGB frame, so the
// whole visual contract can be unit-tested on the host.
namespace Aqi {

constexpr uint8_t kStatusCount = 13;

struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

enum class Pattern : uint8_t {
  kStatic,     // Full matrix, primary colour, no animation
  kBreathing,  // Full matrix, primary colour, brightness cycles
  kBlink,      // Full matrix, primary colour on/off
  kInner,      // Inner 2x2 only, primary colour, static
  kDualZone,   // Perimeter primary static, inner 2x2 secondary breathing
  kAlternate,  // Full matrix alternating primary/secondary
};

struct Visual {
  Pattern pattern;
  Rgb primary;
  Rgb secondary;
};

// Contract lookup. Returns false for codes outside 0x00..0x0C.
bool lookupVisual(uint8_t status, Visual& out);

// White-blue breathing shown when no data has arrived or it has gone stale.
Visual standbyVisual();

// Drives the display through time. Call setStatus() from the protocol
// handler and render() from the main loop.
class Display {
 public:
  Display();

  // Enters standby immediately (used at boot). No transition.
  void begin(uint32_t nowMs);

  // Applies a status code. Returns false and changes nothing if the code is
  // outside the contract. A changed status starts a transition animation;
  // the same status repeated only refreshes the standby timeout.
  bool setStatus(uint8_t status, uint32_t nowMs);

  // Renders the frame for `nowMs` into `frame` (MatrixLayout::kFrameBytes
  // bytes, physical order). Applies the standby timeout. Returns true when
  // the frame differs from the previous render, so the caller can skip
  // pushing identical data to the LEDs.
  bool render(uint32_t nowMs, uint8_t* frame);

  bool inStandby() const { return standby_; }
  bool inTransition() const { return transitioning_; }
  bool hasStatus() const { return !standby_; }
  uint8_t currentStatus() const { return status_; }

  // Total transition length: wipe across every LED, then a solid hold.
  static constexpr uint32_t kTransitionWipeMs =
      static_cast<uint32_t>(AppConfig::kAqiTransitionWipeStepMs) * AppConfig::kLedCount;
  static constexpr uint32_t kTransitionTotalMs =
      kTransitionWipeMs + AppConfig::kAqiTransitionHoldMs;

 private:
  void startTransition(const Visual& to, uint32_t nowMs);
  void renderSteady(uint32_t elapsedMs, uint8_t* frame) const;
  void renderTransition(uint32_t elapsedMs, uint8_t* frame) const;

  Visual visual_;
  uint8_t status_;
  bool standby_;
  bool transitioning_;
  uint32_t effectStartMs_;
  uint32_t lastStatusMs_;

  // Frame shown at the moment a transition started; the wipe paints the
  // new colour over it.
  uint8_t transitionBase_[MatrixLayout::kFrameBytes];

  uint8_t lastFrame_[MatrixLayout::kFrameBytes];
  bool hasLastFrame_;
};

}  // namespace Aqi
