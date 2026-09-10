#include "TcpMatrixServer.h"

#include <esp_wifi.h>
#include <lwip/sockets.h>

#include <cstring>

namespace {

const char* configuredWifiSsid() {
  return WIFI_SSID;
}

const char* configuredWifiPassword() {
  return WIFI_PASSWORD;
}

const char* configuredApPassword() {
  return WIFI_AP_PASSWORD;
}

void scanNetworks() {
  Serial.println("Scanning for networks...");
  const int count = WiFi.scanNetworks();
  if (count == 0) {
    Serial.println("No networks found");
    return;
  }
  for (int i = 0; i < count; ++i) {
    Serial.print("  [");
    Serial.print(i + 1);
    Serial.print("] SSID: \"");
    Serial.print(WiFi.SSID(i));
    Serial.print("\"  RSSI: ");
    Serial.print(WiFi.RSSI(i));
    Serial.print(" dBm  CH: ");
    Serial.print(WiFi.channel(i));
    Serial.print("  ENC: ");
    Serial.println(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured");
  }
  WiFi.scanDelete();
}

void printMacAddress() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  for (int i = 0; i < 6; ++i) {
    if (i) Serial.print(":");
    if (mac[i] < 0x10) Serial.print("0");
    Serial.print(mac[i], HEX);
  }
  Serial.println();
}

const char* commandName(uint8_t command) {
  switch (static_cast<MatrixProtocol::Command>(command)) {
    case MatrixProtocol::Command::kPing:
      return "PING";
    case MatrixProtocol::Command::kSetBrightness:
      return "SET_BRIGHTNESS";
    case MatrixProtocol::Command::kSetPanelEnabled:
      return "SET_PANEL_ENABLED";
    case MatrixProtocol::Command::kSetAqiStatus:
      return "SET_AQI_STATUS";
    default:
      return "UNKNOWN";
  }
}

const char* statusName(MatrixProtocol::Status status) {
  switch (status) {
    case MatrixProtocol::Status::kOk:
      return "OK";
    case MatrixProtocol::Status::kBadMagic:
      return "BAD_MAGIC";
    case MatrixProtocol::Status::kUnsupportedVersion:
      return "UNSUPPORTED_VERSION";
    case MatrixProtocol::Status::kUnknownCommand:
      return "UNKNOWN_COMMAND";
    case MatrixProtocol::Status::kInvalidLength:
      return "INVALID_LENGTH";
    case MatrixProtocol::Status::kChecksumMismatch:
      return "CHECKSUM_MISMATCH";
    case MatrixProtocol::Status::kInvalidArgument:
      return "INVALID_ARGUMENT";
  }
  return "?";
}

void logInstruction(uint8_t command, uint8_t payloadLength, const IPAddress& remoteIp,
                    uint16_t remotePort, MatrixProtocol::Status result) {
  Serial.print("Instruction: ");
  Serial.print(commandName(command));
  Serial.print(" (0x");
  Serial.print(command, HEX);
  Serial.print("), len=");
  Serial.print(payloadLength);
  Serial.print(", from ");
  Serial.print(remoteIp);
  Serial.print(":");
  Serial.print(remotePort);
  Serial.print(" -> ");
  Serial.println(statusName(result));
}

}  // namespace

TcpMatrixServer::TcpMatrixServer(LedMatrixController& matrix)
    : matrix_(matrix),
      display_(),
      discovery_(),
      server_(AppConfig::kTcpPort),
      client_(),
      parser_(),
      serverStarted_(false),
      stationConnected_(false),
      lastWifiRetryMs_(0),
      lastClientActivityMs_(0),
      lastClientByteMs_(0) {}

void TcpMatrixServer::begin() {
  display_.begin(millis());
  startWifi();
  ensureServerRunning();
  if (hasStationCredentials()) {
    discovery_.begin();
  }
}

void TcpMatrixServer::loop() {
  const uint32_t nowMs = millis();
  updateWifi();
  ensureServerRunning();
  discovery_.loop(nowMs);
  acceptClientIfPending();
  readClientBytes();
  expireStalledFrame(nowMs);
  updateDisplay(nowMs);
}

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------

void TcpMatrixServer::startWifi() {
  if (!hasStationCredentials()) {
    startAccessPoint();
    return;
  }

  btStop();
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);

  applyMacOverrideIfConfigured();
  printMacAddress();
  applyStaticIpIfConfigured();

  Serial.print("Target SSID: \"");
  Serial.print(configuredWifiSsid());
  Serial.println("\"");

  if (AppConfig::kScanNetworksOnBoot) {
    scanNetworks();
  }

  beginStationConnect();
  Serial.println("Connecting to Wi-Fi (non-blocking)");
}

void TcpMatrixServer::applyStaticIpIfConfigured() {
#if defined(STATIC_IP) && defined(STATIC_GATEWAY) && defined(STATIC_SUBNET)
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;

  bool ok = ip.fromString(STATIC_IP) && gateway.fromString(STATIC_GATEWAY) &&
            subnet.fromString(STATIC_SUBNET);
#if defined(STATIC_DNS)
  ok = ok && dns.fromString(STATIC_DNS);
#else
  dns = gateway;
#endif

  if (!ok) {
    Serial.println("Static IP config is invalid, falling back to DHCP");
    return;
  }

  if (!WiFi.config(ip, gateway, subnet, dns)) {
    Serial.println("WiFi.config() failed, falling back to DHCP");
    return;
  }

  Serial.print("Static IP: ");
  Serial.println(ip);
#endif
}

void TcpMatrixServer::applyMacOverrideIfConfigured() {
#if defined(WIFI_MAC_OVERRIDE)
  uint8_t mac[6] = WIFI_MAC_OVERRIDE;
  const esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, mac);
  if (err != ESP_OK) {
    Serial.print("MAC override failed, esp_err=");
    Serial.println(err);
  }
#endif
}

void TcpMatrixServer::beginStationConnect() {
  lastWifiRetryMs_ = millis();
  WiFi.begin(configuredWifiSsid(), configuredWifiPassword());
}

void TcpMatrixServer::startAccessPoint() {
  WiFi.mode(WIFI_AP);

  const char* password = configuredApPassword();
  if (strlen(password) >= 8) {
    WiFi.softAP(AppConfig::kAccessPointSsid, password);
    Serial.print("AP SSID (WPA2): ");
  } else {
    if (strlen(password) > 0) {
      Serial.println("WIFI_AP_PASSWORD is shorter than 8 characters, starting an open AP");
    }
    WiFi.softAP(AppConfig::kAccessPointSsid);
    Serial.print("AP SSID (open): ");
  }

  Serial.println(AppConfig::kAccessPointSsid);
  printNetworkAddress();
}

void TcpMatrixServer::updateWifi() {
  if (!hasStationCredentials()) {
    return;
  }

  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected) {
    if (!stationConnected_) {
      stationConnected_ = true;
      Serial.println("Wi-Fi connected");
      printNetworkAddress();
      discovery_.onNetworkUp();
    }
    return;
  }

  if (stationConnected_) {
    stationConnected_ = false;
    Serial.println("Wi-Fi disconnected");
    discovery_.onNetworkDown();
  }

  stopServer();

  const uint32_t nowMs = millis();
  if (nowMs - lastWifiRetryMs_ < AppConfig::kWifiRetryIntervalMs) {
    return;
  }

  Serial.println("Wi-Fi not connected, retrying");
  beginStationConnect();
}

bool TcpMatrixServer::hasStationCredentials() const {
  return strlen(configuredWifiSsid()) > 0;
}

bool TcpMatrixServer::networkIsReady() const {
  if (!hasStationCredentials()) {
    return WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
  }

  return WiFi.status() == WL_CONNECTED;
}

IPAddress TcpMatrixServer::currentIpAddress() const {
  if (!hasStationCredentials()) {
    return WiFi.softAPIP();
  }

  return WiFi.localIP();
}

void TcpMatrixServer::printNetworkAddress() const {
  Serial.print("Device IP: ");
  Serial.println(currentIpAddress());
}

// ---------------------------------------------------------------------------
// TCP listener and client
// ---------------------------------------------------------------------------

void TcpMatrixServer::ensureServerRunning() {
  if (!networkIsReady()) {
    stopServer();
    return;
  }

  if (!serverStarted_) {
    startServer();
  }
}

void TcpMatrixServer::startServer() {
  server_.begin();
  server_.setNoDelay(true);
  serverStarted_ = true;

  Serial.print("TCP server started on ");
  Serial.print(currentIpAddress());
  Serial.print(":");
  Serial.println(AppConfig::kTcpPort);
}

void TcpMatrixServer::stopServer() {
  if (!serverStarted_) {
    return;
  }

  if (client_) {
    dropClient("server stopping");
  }

  server_.stop();
  serverStarted_ = false;
  Serial.println("TCP server stopped");
}

void TcpMatrixServer::acceptClientIfPending() {
  if (!serverStarted_) {
    return;
  }

  // One client at a time, newest wins. A stale connection whose peer
  // vanished is therefore never able to block a fresh sender.
  if (!server_.hasClient()) {
    return;
  }

  if (client_ && client_.connected()) {
    dropClient("replaced by new connection");
  }

  WiFiClient newClient = server_.accept();
  if (!newClient) {
    return;
  }

  client_ = newClient;
  client_.setNoDelay(true);
  enableKeepAlive();
  parser_.reset();

  const uint32_t nowMs = millis();
  lastClientActivityMs_ = nowMs;
  lastClientByteMs_ = nowMs;

  Serial.print("TCP client connected from ");
  Serial.println(client_.remoteIP());
}

void TcpMatrixServer::dropClient(const char* reason) {
  if (client_) {
    client_.stop();
  }
  parser_.reset();
  Serial.print("TCP client dropped: ");
  Serial.println(reason);
}

void TcpMatrixServer::enableKeepAlive() {
  int enable = 1;
  int idle = AppConfig::kTcpKeepAliveIdleSec;
  int interval = AppConfig::kTcpKeepAliveIntervalSec;
  int count = AppConfig::kTcpKeepAliveCount;

  client_.setSocketOption(SO_KEEPALIVE, reinterpret_cast<char*>(&enable), sizeof(enable));
  client_.setOption(TCP_KEEPIDLE, &idle);
  client_.setOption(TCP_KEEPINTVL, &interval);
  client_.setOption(TCP_KEEPCNT, &count);
}

void TcpMatrixServer::readClientBytes() {
  if (!client_) {
    return;
  }

  if (!client_.connected()) {
    dropClient("peer closed");
    client_ = WiFiClient();
    return;
  }

  const uint32_t nowMs = millis();

  if (nowMs - lastClientActivityMs_ >= AppConfig::kClientIdleTimeoutMs) {
    dropClient("idle timeout");
    client_ = WiFiClient();
    return;
  }

  uint16_t budget = AppConfig::kMaxClientBytesPerLoop;
  while (budget > 0 && client_.available() > 0) {
    budget--;
    lastClientByteMs_ = nowMs;

    const uint8_t value = static_cast<uint8_t>(client_.read());
    const MatrixProtocol::FrameParser::Result result = parser_.push(value);

    if (result == MatrixProtocol::FrameParser::Result::kError) {
      sendStatus(parser_.error());
      continue;
    }

    if (result != MatrixProtocol::FrameParser::Result::kFrameReady) {
      continue;
    }

    lastClientActivityMs_ = nowMs;
    const uint8_t command = parser_.command();
    const uint8_t length = parser_.payloadLength();
    const MatrixProtocol::Status status = applyCommand(command, parser_.payload(), length);
    logInstruction(command, length, client_.remoteIP(), client_.remotePort(), status);
    sendStatus(status);
    parser_.reset();
  }
}

void TcpMatrixServer::expireStalledFrame(uint32_t nowMs) {
  if (!parser_.inProgress()) {
    return;
  }

  if (nowMs - lastClientByteMs_ < AppConfig::kFrameTimeoutMs) {
    return;
  }

  Serial.println("Partial frame timed out, parser reset");
  parser_.reset();
}

// ---------------------------------------------------------------------------
// Protocol
// ---------------------------------------------------------------------------

MatrixProtocol::Status TcpMatrixServer::applyCommand(uint8_t command, const uint8_t* payload,
                                                     uint8_t length) {
  using MatrixProtocol::Command;
  using MatrixProtocol::Status;

  switch (static_cast<Command>(command)) {
    case Command::kPing:
      return length == 0 ? Status::kOk : Status::kInvalidLength;

    case Command::kSetBrightness:
      if (length != 1) {
        return Status::kInvalidLength;
      }
      matrix_.setBrightness(payload[0]);
      return Status::kOk;

    case Command::kSetPanelEnabled:
      if (length != 1) {
        return Status::kInvalidLength;
      }
      matrix_.setEnabled(payload[0] != 0);
      return Status::kOk;

    case Command::kSetAqiStatus:
      if (length != 1) {
        return Status::kInvalidLength;
      }
      return display_.setStatus(payload[0], millis()) ? Status::kOk : Status::kInvalidArgument;
  }

  return Status::kUnknownCommand;
}

void TcpMatrixServer::sendStatus(MatrixProtocol::Status status) {
  if (!client_ || !client_.connected()) {
    return;
  }

  uint8_t response[MatrixProtocol::kResponseSize];
  MatrixProtocol::buildResponse(status, response);
  client_.write(response, MatrixProtocol::kResponseSize);
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void TcpMatrixServer::updateDisplay(uint32_t nowMs) {
  const bool wasStandby = display_.inStandby();

  uint8_t frame[MatrixLayout::kFrameBytes];
  if (display_.render(nowMs, frame)) {
    matrix_.setPhysicalFrame(frame, sizeof(frame));
  }

  if (!wasStandby && display_.inStandby()) {
    Serial.println("AQI data timeout, returning to standby");
  }
}
