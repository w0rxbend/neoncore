#include "LedMatrixController.h"

LedMatrixController::LedMatrixController()
    : pixels_(AppConfig::kLedCount, AppConfig::kLedPin, NEO_GRB + NEO_KHZ800),
      frameRgb_(),
      enabled_(true) {}

void LedMatrixController::begin() {
  pixels_.begin();
  pixels_.setBrightness(AppConfig::kDefaultBrightness);
  clear();
}

void LedMatrixController::clear() {
  memset(frameRgb_, 0, sizeof(frameRgb_));
  render();
}

void LedMatrixController::setBrightness(uint8_t brightness) {
  pixels_.setBrightness(brightness);
  render();
}

void LedMatrixController::setEnabled(bool enabled) {
  enabled_ = enabled;
  render();
}

void LedMatrixController::fill(uint8_t red, uint8_t green, uint8_t blue) {
  for (uint16_t ledIndex = 0; ledIndex < AppConfig::kLedCount; ledIndex++) {
    const uint16_t offset = ledIndex * 3;
    frameRgb_[offset] = red;
    frameRgb_[offset + 1] = green;
    frameRgb_[offset + 2] = blue;
  }

  render();
}

bool LedMatrixController::setPixel(uint8_t x, uint8_t y, uint8_t red, uint8_t green, uint8_t blue) {
  const uint16_t ledIndex = MatrixLayout::logicalToPhysical(x, y);
  if (ledIndex >= AppConfig::kLedCount) {
    return false;
  }

  const uint16_t offset = ledIndex * 3;
  frameRgb_[offset] = red;
  frameRgb_[offset + 1] = green;
  frameRgb_[offset + 2] = blue;
  render();
  return true;
}

bool LedMatrixController::setPhysicalFrame(const uint8_t* rgbFrame, uint16_t length) {
  if (length != MatrixLayout::kFrameBytes) {
    return false;
  }

  memcpy(frameRgb_, rgbFrame, sizeof(frameRgb_));
  render();
  return true;
}

void LedMatrixController::render() {
  for (uint16_t ledIndex = 0; ledIndex < AppConfig::kLedCount; ledIndex++) {
    const uint16_t offset = ledIndex * 3;

    if (!enabled_) {
      pixels_.setPixelColor(ledIndex, 0);
      continue;
    }

    pixels_.setPixelColor(
        ledIndex, pixels_.Color(frameRgb_[offset], frameRgb_[offset + 1], frameRgb_[offset + 2]));
  }

  pixels_.show();
}
