#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "AqiDisplay.h"
#include "LedMatrixController.h"
#include "MatrixProtocol.h"

// Coordinates Wi-Fi, the TCP listener, protocol parsing, and the AQI display.
//
// This class owns all network state. The main Arduino loop only calls loop();
// this class decides whether to reconnect Wi-Fi, (re)start the listener,
// accept or drop a client, read bytes, dispatch commands, and refresh LEDs.
// Nothing in loop() blocks.
class TcpMatrixServer {
 public:
  explicit TcpMatrixServer(LedMatrixController& matrix);

  // Configures Wi-Fi, starts the (non-blocking) connect, and enters standby.
  void begin();

  // Must be called repeatedly from Arduino loop().
  void loop();

 private:
  // Wi-Fi.
  void startWifi();
  void applyStaticIpIfConfigured();
  void applyMacOverrideIfConfigured();
  void beginStationConnect();
  void startAccessPoint();
  void updateWifi();
  bool hasStationCredentials() const;
  bool networkIsReady() const;
  IPAddress currentIpAddress() const;
  void printNetworkAddress() const;

  // TCP listener and client lifecycle.
  void ensureServerRunning();
  void startServer();
  void stopServer();
  void acceptClientIfPending();
  void dropClient(const char* reason);
  void enableKeepAlive();
  void readClientBytes();
  void expireStalledFrame(uint32_t nowMs);

  // Protocol.
  MatrixProtocol::Status applyCommand(uint8_t command, const uint8_t* payload, uint8_t length);
  void sendStatus(MatrixProtocol::Status status);

  // Display.
  void updateDisplay(uint32_t nowMs);

  LedMatrixController& matrix_;
  Aqi::Display display_;

  WiFiServer server_;
  WiFiClient client_;
  MatrixProtocol::FrameParser parser_;

  bool serverStarted_;
  bool stationConnected_;
  uint32_t lastWifiRetryMs_;
  uint32_t lastClientActivityMs_;
  uint32_t lastClientByteMs_;
};
