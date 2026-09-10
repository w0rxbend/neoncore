#pragma once

#include <stdint.h>

#include "AppConfig.h"

// Physical wiring of the 4x4 panel. Pure functions, no hardware dependency,
// shared by the LED driver and the AQI renderer so there is exactly one
// definition of the serpentine order.
namespace MatrixLayout {

constexpr uint16_t kFrameBytes = AppConfig::kLedCount * 3;

// Converts logical x/y (0,0 = top-left) into the LED chain index.
// The panel is wired in serpentine rows:
//   row 0: left -> right
//   row 1: right -> left
//   row 2: left -> right
//   row 3: right -> left
// Invalid coordinates return AppConfig::kLedCount, which is out of range.
inline uint16_t logicalToPhysical(uint8_t x, uint8_t y) {
  if (x >= AppConfig::kMatrixWidth || y >= AppConfig::kMatrixHeight) {
    return AppConfig::kLedCount;
  }

  if (y % 2 == 0) {
    return static_cast<uint16_t>(y * AppConfig::kMatrixWidth + x);
  }

  return static_cast<uint16_t>(y * AppConfig::kMatrixWidth + (AppConfig::kMatrixWidth - 1 - x));
}

// Writes one logical pixel into a physical-order RGB frame buffer.
inline void setFramePixel(uint8_t* frame, uint8_t x, uint8_t y, uint8_t red, uint8_t green,
                          uint8_t blue) {
  const uint16_t physicalIndex = logicalToPhysical(x, y);
  if (physicalIndex >= AppConfig::kLedCount) {
    return;
  }

  const uint16_t base = physicalIndex * 3;
  frame[base] = red;
  frame[base + 1] = green;
  frame[base + 2] = blue;
}

}  // namespace MatrixLayout
