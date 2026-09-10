#include "DiscoveryPayload.h"

#include <stdio.h>
#include <string.h>

namespace Discovery {

namespace {

// Appends `text` to the buffer, escaping for a JSON string body. Returns
// false when the buffer would overflow. `pos` is advanced on success.
bool appendEscaped(const char* text, char* out, size_t capacity, size_t& pos) {
  static const char kHex[] = "0123456789abcdef";

  for (const char* p = text; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    char tmp[6];
    size_t n = 0;

    if (c == '"' || c == '\\') {
      tmp[0] = '\\';
      tmp[1] = static_cast<char>(c);
      n = 2;
    } else if (c < 0x20) {
      tmp[0] = '\\';
      tmp[1] = 'u';
      tmp[2] = '0';
      tmp[3] = '0';
      tmp[4] = kHex[c >> 4];
      tmp[5] = kHex[c & 0x0F];
      n = 6;
    } else {
      tmp[0] = static_cast<char>(c);
      n = 1;
    }

    if (pos + n >= capacity) {
      return false;
    }
    memcpy(out + pos, tmp, n);
    pos += n;
  }
  return true;
}

bool appendRaw(const char* text, char* out, size_t capacity, size_t& pos) {
  const size_t n = strlen(text);
  if (pos + n >= capacity) {
    return false;
  }
  memcpy(out + pos, text, n);
  pos += n;
  return true;
}

bool appendString(const char* key, const char* value, bool first, char* out, size_t capacity,
                  size_t& pos) {
  return appendRaw(first ? "\"" : ",\"", out, capacity, pos) &&
         appendRaw(key, out, capacity, pos) && appendRaw("\":\"", out, capacity, pos) &&
         appendEscaped(value ? value : "", out, capacity, pos) &&
         appendRaw("\"", out, capacity, pos);
}

bool appendNumber(const char* key, uint32_t value, char* out, size_t capacity, size_t& pos) {
  char num[12];
  snprintf(num, sizeof(num), "%lu", static_cast<unsigned long>(value));
  return appendRaw(",\"", out, capacity, pos) && appendRaw(key, out, capacity, pos) &&
         appendRaw("\":", out, capacity, pos) && appendRaw(num, out, capacity, pos);
}

}  // namespace

int buildPayload(const Info& info, char* out, size_t capacity) {
  if (out == nullptr || capacity == 0) {
    return -1;
  }

  size_t pos = 0;
  const bool ok = appendRaw("{", out, capacity, pos) &&
                  appendString("name", info.name, true, out, capacity, pos) &&
                  appendString("ip", info.ip, false, out, capacity, pos) &&
                  appendNumber("port", info.port, out, capacity, pos) &&
                  appendString("mac", info.mac, false, out, capacity, pos) &&
                  appendNumber("protocol", info.protocolVersion, out, capacity, pos) &&
                  appendString("firmware", info.firmware, false, out, capacity, pos) &&
                  appendNumber("uptime_s", info.uptimeSeconds, out, capacity, pos) &&
                  appendRaw("}", out, capacity, pos);

  if (!ok) {
    return -1;
  }

  out[pos] = '\0';
  return static_cast<int>(pos);
}

}  // namespace Discovery
