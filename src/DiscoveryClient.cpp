#include "DiscoveryClient.h"

#include <string.h>

#include "DiscoveryPayload.h"
#include "MatrixProtocol.h"

#if defined(NEONCORE_DISCOVERY)
#include <HTTPClient.h>
#include <WiFi.h>
#if defined(DISCOVERY_HTTPS)
#include <WiFiClientSecure.h>
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace {

#if defined(NEONCORE_DISCOVERY)
constexpr const char* kDiscoveryUrl = DISCOVERY_URL;
constexpr const char* kDiscoveryToken = DISCOVERY_TOKEN;
constexpr const char* kConfiguredName = DISCOVERY_DEVICE_NAME;
constexpr uint32_t kTaskStackBytes = 12288;
constexpr UBaseType_t kTaskPriority = 1;

bool isHttps(const char* url) {
  return strncmp(url, "https://", 8) == 0;
}

// "neoncore-3ce903" from the last three MAC bytes, unless a name is set.
void deviceName(char* out, size_t capacity) {
  if (kConfiguredName[0] != '\0') {
    strncpy(out, kConfiguredName, capacity - 1);
    out[capacity - 1] = '\0';
    return;
  }

  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(out, capacity, "neoncore-%02x%02x%02x", mac[3], mac[4], mac[5]);
}
#endif

}  // namespace

DiscoveryClient::DiscoveryClient()
    : networkUp_(false),
      inFlight_(false),
      registered_(false),
      attemptedSinceLinkUp_(false),
      lastAttemptMs_(0),
      taskHandle_(nullptr) {}

bool DiscoveryClient::enabled() const {
#if defined(NEONCORE_DISCOVERY)
  return true;
#else
  return false;
#endif
}

void DiscoveryClient::begin() {
#if defined(NEONCORE_DISCOVERY)
  Serial.print("Discovery: enabled, service ");
  Serial.println(kDiscoveryUrl);

  TaskHandle_t handle = nullptr;
  const BaseType_t created = xTaskCreatePinnedToCore(
      &DiscoveryClient::taskEntry, "discovery", kTaskStackBytes, this, kTaskPriority, &handle,
      tskNO_AFFINITY);
  if (created != pdPASS) {
    Serial.println("Discovery: failed to start worker task, registration disabled");
    return;
  }
  taskHandle_ = handle;
#else
  Serial.println("Discovery: disabled (DISCOVERY_URL not set)");
#endif
}

void DiscoveryClient::onNetworkUp() {
  networkUp_ = true;
  registered_ = false;
  attemptedSinceLinkUp_ = false;
}

void DiscoveryClient::onNetworkDown() {
  networkUp_ = false;
  registered_ = false;
}

void DiscoveryClient::loop(uint32_t nowMs) {
  if (taskHandle_ == nullptr || !networkUp_ || inFlight_) {
    return;
  }

  if (!attemptedSinceLinkUp_) {
    requestRegistration();
    return;
  }

  const uint32_t interval = registered_ ? AppConfig::kDiscoveryRefreshIntervalMs
                                        : AppConfig::kDiscoveryRetryIntervalMs;
  if (nowMs - lastAttemptMs_ >= interval) {
    requestRegistration();
  }
}

void DiscoveryClient::requestRegistration() {
#if defined(NEONCORE_DISCOVERY)
  inFlight_ = true;
  attemptedSinceLinkUp_ = true;
  lastAttemptMs_ = millis();
  xTaskNotifyGive(static_cast<TaskHandle_t>(taskHandle_));
#endif
}

void DiscoveryClient::taskEntry(void* self) {
  static_cast<DiscoveryClient*>(self)->taskLoop();
}

void DiscoveryClient::taskLoop() {
#if defined(NEONCORE_DISCOVERY)
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const bool ok = registerOnce();
    registered_ = ok;
    inFlight_ = false;
  }
#endif
}

bool DiscoveryClient::registerOnce() {
#if defined(NEONCORE_DISCOVERY)
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  char name[48];
  deviceName(name, sizeof(name));
  const String ip = WiFi.localIP().toString();
  const String mac = WiFi.macAddress();

  const Discovery::Info info = {
      name,
      ip.c_str(),
      AppConfig::kTcpPort,
      mac.c_str(),
      MatrixProtocol::kVersion,
      AppConfig::kFirmwareVersion,
      static_cast<uint32_t>(millis() / 1000),
  };

  char payload[AppConfig::kDiscoveryPayloadCapacity];
  const int length = Discovery::buildPayload(info, payload, sizeof(payload));
  if (length < 0) {
    Serial.println("Discovery: payload does not fit, check DISCOVERY_DEVICE_NAME length");
    return false;
  }

  const bool https = isHttps(kDiscoveryUrl);
#if defined(DISCOVERY_HTTPS)
  // The registry is expected to be a LAN service; certificate validation
  // would need a CA bundle that most home setups do not have.
  WiFiClientSecure secure;
  secure.setInsecure();
  WiFiClient plain;
  WiFiClient& client = https ? static_cast<WiFiClient&>(secure) : plain;
#else
  if (https) {
    Serial.println("Discovery: DISCOVERY_URL is https but DISCOVERY_HTTPS is not defined");
    return false;
  }
  WiFiClient client;
#endif

  HTTPClient http;
  http.setConnectTimeout(AppConfig::kDiscoveryHttpTimeoutMs);
  http.setTimeout(AppConfig::kDiscoveryHttpTimeoutMs);
  http.setReuse(false);

  if (!http.begin(client, kDiscoveryUrl)) {
    Serial.println("Discovery: invalid DISCOVERY_URL");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", String("neoncore/") + AppConfig::kFirmwareVersion);
  if (kDiscoveryToken[0] != '\0') {
    http.addHeader("Authorization", String("Bearer ") + kDiscoveryToken);
  }

  const int code = http.POST(reinterpret_cast<uint8_t*>(payload), static_cast<size_t>(length));
  http.end();

  if (code >= 200 && code < 300) {
    Serial.print("Discovery: registered ");
    Serial.print(name);
    Serial.print(" at ");
    Serial.print(ip);
    Serial.print(" (HTTP ");
    Serial.print(code);
    Serial.println(")");
    return true;
  }

  Serial.print("Discovery: registration failed (");
  if (code < 0) {
    Serial.print(HTTPClient::errorToString(code));
  } else {
    Serial.print("HTTP ");
    Serial.print(code);
  }
  Serial.print("), retry in ");
  Serial.print(AppConfig::kDiscoveryRetryIntervalMs / 1000);
  Serial.println(" s");
  return false;
#else
  return false;
#endif
}
