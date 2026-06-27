#include <Arduino.h>

#include "AppConfig.h"
#include "LedMatrixController.h"
#include "TcpMatrixServer.h"

LedMatrixController ledMatrix;
TcpMatrixServer tcpServer(ledMatrix);

namespace {

void runStartupAnimation() {
  Serial.println("Startup animation: running");

  ledMatrix.clear();
  delay(100);

  // Pixel sweep: one bright white pixel moves across the panel a few times.
  constexpr uint8_t sweepRepeats = 2;
  constexpr uint16_t sweepDelayMs = 35;

  for (uint8_t repeat = 0; repeat < sweepRepeats; ++repeat) {
    for (uint8_t logicalIndex = 0; logicalIndex < AppConfig::kLedCount; ++logicalIndex) {
      const uint8_t x = logicalIndex % AppConfig::kMatrixWidth;
      const uint8_t y = logicalIndex / AppConfig::kMatrixWidth;

      ledMatrix.clear();
      ledMatrix.setPixel(x, y, 255, 255, 255);
      delay(sweepDelayMs);
    }
  }

  ledMatrix.fill(0, 255, 0);
  delay(180);
  ledMatrix.clear();
  delay(80);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("ESP8266 WS2812B TCP matrix controller");
  Serial.println("LED data pin: D2 / GPIO4");
  Serial.print("Matrix: ");
  Serial.print(AppConfig::kMatrixWidth);
  Serial.print("x");
  Serial.println(AppConfig::kMatrixHeight);
  Serial.print("Boot settle delay ms: ");
  Serial.println(AppConfig::kBootSettleDelayMs);

  delay(AppConfig::kBootSettleDelayMs);

  ledMatrix.begin();
  runStartupAnimation();
  tcpServer.begin();
}

void loop() {
  tcpServer.loop();
}
