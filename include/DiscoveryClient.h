#pragma once

#include <Arduino.h>

#include "AppConfig.h"

// Registers the device with an HTTP discovery service so senders can find
// it by name instead of by hard-coded address.
//
// The HTTP request is blocking, so it runs on its own FreeRTOS task. The
// main loop only decides *when* to register (link up, periodic refresh,
// retry after failure) and pokes the task; the task performs the request
// and publishes the outcome through volatile flags. No shared mutable
// buffers cross the boundary: the task snapshots everything it needs from
// WiFi at the start of each attempt.
//
// Compiled to a no-op unless DISCOVERY_URL is defined (see AppConfig.h).
class DiscoveryClient {
 public:
  DiscoveryClient();

  // Starts the worker task. Safe to call when discovery is disabled.
  void begin();

  // Link-state hooks from the Wi-Fi manager.
  void onNetworkUp();
  void onNetworkDown();

  // Call every loop pass. Never blocks.
  void loop(uint32_t nowMs);

  bool enabled() const;
  bool isRegistered() const { return registered_; }

 private:
  void requestRegistration();
  static void taskEntry(void* self);
  void taskLoop();
  bool registerOnce();

  volatile bool networkUp_;
  volatile bool inFlight_;
  volatile bool registered_;
  volatile bool attemptedSinceLinkUp_;
  volatile uint32_t lastAttemptMs_;
  void* taskHandle_;
};
