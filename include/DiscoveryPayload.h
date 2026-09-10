#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds the JSON document the device POSTs to the discovery service.
// Hardware-free so the exact wire format is pinned by host tests.
namespace Discovery {

struct Info {
  const char* name;
  const char* ip;
  uint16_t port;
  const char* mac;
  uint8_t protocolVersion;
  const char* firmware;
  uint32_t uptimeSeconds;
};

// Writes a single-line JSON object into `out` (NUL-terminated). Returns the
// number of bytes written excluding the terminator, or -1 if `capacity` is
// too small; on -1 the buffer contents are unspecified. String fields are
// escaped: quote, backslash and control characters.
int buildPayload(const Info& info, char* out, size_t capacity);

}  // namespace Discovery
