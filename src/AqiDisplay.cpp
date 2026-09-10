#include "AqiDisplay.h"

#include <string.h>

namespace Aqi {

namespace {

constexpr Rgb kGreen = {0, 180, 0};
constexpr Rgb kYellow = {255, 210, 0};
constexpr Rgb kOrange = {255, 100, 0};
constexpr Rgb kRed = {255, 0, 0};
constexpr Rgb kPurple = {140, 0, 140};
constexpr Rgb kExtremeRed = {220, 0, 0};
constexpr Rgb kStandbyBlue = {150, 180, 255};
constexpr Rgb kBlack = {0, 0, 0};

// The contract, indexed by status code.
constexpr Visual kVisuals[kStatusCount] = {
    {Pattern::kInner, kGreen, kBlack},         // 0x00 Excellent
    {Pattern::kStatic, kGreen, kBlack},        // 0x01 Good
    {Pattern::kBreathing, kGreen, kBlack},     // 0x02 Good (degrading)
    {Pattern::kStatic, kYellow, kBlack},       // 0x03 Moderate
    {Pattern::kDualZone, kYellow, kOrange},    // 0x04 Moderate (degrading)
    {Pattern::kStatic, kOrange, kBlack},       // 0x05 Poor
    {Pattern::kBreathing, kOrange, kBlack},    // 0x06 Poor (degrading)
    {Pattern::kStatic, kRed, kBlack},          // 0x07 Unhealthy
    {Pattern::kBreathing, kRed, kBlack},       // 0x08 Unhealthy (degrading)
    {Pattern::kStatic, kPurple, kBlack},       // 0x09 Very Unhealthy
    {Pattern::kBreathing, kPurple, kBlack},    // 0x0A Very Unhealthy (degrading)
    {Pattern::kBlink, kPurple, kBlack},        // 0x0B Hazardous
    {Pattern::kAlternate, kPurple, kExtremeRed},  // 0x0C Extreme
};

// Brightness scale per breathing step. Never fully off, never full
// brightness, so a breathing colour reads as "alive" rather than blinking.
constexpr uint8_t kBreathingSteps[16] = {10, 18, 28, 42, 60, 84, 112, 150,
                                         190, 150, 112, 84, 60, 42, 28, 18};

// Wipe order for transitions: top-right corner first, spreading left, then
// row by row downward. Mirrors the AirGradient ONE bar that grows leftward
// as air quality worsens.
constexpr uint8_t kWipeX[AppConfig::kLedCount] = {3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0};
constexpr uint8_t kWipeY[AppConfig::kLedCount] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};

uint8_t scaled(uint8_t value, uint8_t scale) {
  return static_cast<uint8_t>((static_cast<uint16_t>(value) * scale) / 255);
}

Rgb scaledRgb(const Rgb& colour, uint8_t scale) {
  return {scaled(colour.r, scale), scaled(colour.g, scale), scaled(colour.b, scale)};
}

void fillFrame(uint8_t* frame, const Rgb& colour) {
  for (uint16_t led = 0; led < AppConfig::kLedCount; ++led) {
    frame[led * 3] = colour.r;
    frame[led * 3 + 1] = colour.g;
    frame[led * 3 + 2] = colour.b;
  }
}

void paintInner(uint8_t* frame, const Rgb& colour) {
  for (uint8_t y = 1; y <= 2; ++y) {
    for (uint8_t x = 1; x <= 2; ++x) {
      MatrixLayout::setFramePixel(frame, x, y, colour.r, colour.g, colour.b);
    }
  }
}

void paintPerimeter(uint8_t* frame, const Rgb& colour) {
  for (uint8_t x = 0; x < AppConfig::kMatrixWidth; ++x) {
    MatrixLayout::setFramePixel(frame, x, 0, colour.r, colour.g, colour.b);
    MatrixLayout::setFramePixel(frame, x, AppConfig::kMatrixHeight - 1, colour.r, colour.g,
                                colour.b);
  }
  for (uint8_t y = 1; y < AppConfig::kMatrixHeight - 1; ++y) {
    MatrixLayout::setFramePixel(frame, 0, y, colour.r, colour.g, colour.b);
    MatrixLayout::setFramePixel(frame, AppConfig::kMatrixWidth - 1, y, colour.r, colour.g,
                                colour.b);
  }
}

uint8_t breathingScale(uint32_t elapsedMs) {
  return kBreathingSteps[(elapsedMs / AppConfig::kAqiBreathingStepMs) % 16];
}

}  // namespace

bool lookupVisual(uint8_t status, Visual& out) {
  if (status >= kStatusCount) {
    return false;
  }
  out = kVisuals[status];
  return true;
}

Visual standbyVisual() {
  return {Pattern::kBreathing, kStandbyBlue, kBlack};
}

Display::Display()
    : visual_(standbyVisual()),
      status_(0xFF),
      standby_(true),
      transitioning_(false),
      effectStartMs_(0),
      lastStatusMs_(0),
      transitionBase_(),
      lastFrame_(),
      hasLastFrame_(false) {}

void Display::begin(uint32_t nowMs) {
  visual_ = standbyVisual();
  status_ = 0xFF;
  standby_ = true;
  transitioning_ = false;
  effectStartMs_ = nowMs;
  lastStatusMs_ = nowMs;
  hasLastFrame_ = false;
}

bool Display::setStatus(uint8_t status, uint32_t nowMs) {
  Visual next;
  if (!lookupVisual(status, next)) {
    return false;
  }

  lastStatusMs_ = nowMs;

  if (!standby_ && status == status_) {
    // Heartbeat: same reading, nothing to animate.
    return true;
  }

  status_ = status;
  standby_ = false;
  startTransition(next, nowMs);
  return true;
}

void Display::startTransition(const Visual& to, uint32_t nowMs) {
  if (hasLastFrame_) {
    memcpy(transitionBase_, lastFrame_, sizeof(transitionBase_));
  } else {
    memset(transitionBase_, 0, sizeof(transitionBase_));
  }

  visual_ = to;
  transitioning_ = true;
  effectStartMs_ = nowMs;
}

bool Display::render(uint32_t nowMs, uint8_t* frame) {
  if (!standby_ && nowMs - lastStatusMs_ >= AppConfig::kAqiStandbyTimeoutMs) {
    standby_ = true;
    status_ = 0xFF;
    startTransition(standbyVisual(), nowMs);
  }

  uint32_t elapsedMs = nowMs - effectStartMs_;

  if (transitioning_ && elapsedMs >= kTransitionTotalMs) {
    // Hand over to the steady pattern with its phase starting at the exact
    // end of the transition, so timing stays deterministic.
    transitioning_ = false;
    effectStartMs_ += kTransitionTotalMs;
    elapsedMs -= kTransitionTotalMs;
  }

  if (transitioning_) {
    renderTransition(elapsedMs, frame);
  } else {
    renderSteady(elapsedMs, frame);
  }

  const bool changed =
      !hasLastFrame_ || memcmp(frame, lastFrame_, MatrixLayout::kFrameBytes) != 0;
  if (changed) {
    memcpy(lastFrame_, frame, MatrixLayout::kFrameBytes);
    hasLastFrame_ = true;
  }
  return changed;
}

void Display::renderTransition(uint32_t elapsedMs, uint8_t* frame) const {
  if (elapsedMs >= kTransitionWipeMs) {
    fillFrame(frame, visual_.primary);
    return;
  }

  memcpy(frame, transitionBase_, MatrixLayout::kFrameBytes);
  const uint32_t lit = elapsedMs / AppConfig::kAqiTransitionWipeStepMs + 1;
  for (uint32_t i = 0; i < lit && i < AppConfig::kLedCount; ++i) {
    MatrixLayout::setFramePixel(frame, kWipeX[i], kWipeY[i], visual_.primary.r,
                                visual_.primary.g, visual_.primary.b);
  }
}

void Display::renderSteady(uint32_t elapsedMs, uint8_t* frame) const {
  switch (visual_.pattern) {
    case Pattern::kStatic:
      fillFrame(frame, visual_.primary);
      return;

    case Pattern::kBreathing:
      fillFrame(frame, scaledRgb(visual_.primary, breathingScale(elapsedMs)));
      return;

    case Pattern::kBlink: {
      const bool on = (elapsedMs / AppConfig::kAqiBlinkIntervalMs) % 2 == 0;
      fillFrame(frame, on ? visual_.primary : kBlack);
      return;
    }

    case Pattern::kInner:
      fillFrame(frame, kBlack);
      paintInner(frame, visual_.primary);
      return;

    case Pattern::kDualZone:
      fillFrame(frame, kBlack);
      paintPerimeter(frame, visual_.primary);
      paintInner(frame, scaledRgb(visual_.secondary, breathingScale(elapsedMs)));
      return;

    case Pattern::kAlternate: {
      const bool first = (elapsedMs / AppConfig::kAqiAlternateIntervalMs) % 2 == 0;
      fillFrame(frame, first ? visual_.primary : visual_.secondary);
      return;
    }
  }

  fillFrame(frame, kBlack);
}

}  // namespace Aqi
