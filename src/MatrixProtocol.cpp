#include "MatrixProtocol.h"

namespace MatrixProtocol {

uint8_t checksum(const uint8_t* data, uint16_t length) {
  uint8_t result = 0;

  for (uint16_t byteIndex = 0; byteIndex < length; byteIndex++) {
    result ^= data[byteIndex];
  }

  return result;
}

void buildResponse(Status status, uint8_t* out) {
  out[0] = kMagic0;
  out[1] = kMagic1;
  out[2] = kVersion;
  out[3] = kResponseCommand;
  out[4] = static_cast<uint8_t>(status);
  out[5] = checksum(out, kResponseSize - 1);
}

FrameParser::FrameParser() : buffer_(), index_(0), expectedSize_(0), error_(Status::kOk), frameReady_(false) {}

void FrameParser::reset() {
  index_ = 0;
  expectedSize_ = 0;
  frameReady_ = false;
}

FrameParser::Result FrameParser::fail(Status status) {
  error_ = status;
  reset();
  return Result::kError;
}

FrameParser::Result FrameParser::push(uint8_t value) {
  if (frameReady_) {
    // The previous frame has been consumed; start fresh.
    reset();
  }

  if (index_ == 0 && value != kMagic0) {
    return fail(Status::kBadMagic);
  }

  if (index_ == 1 && value != kMagic1) {
    return fail(Status::kBadMagic);
  }

  if (index_ == 2 && value != kVersion) {
    return fail(Status::kUnsupportedVersion);
  }

  buffer_[index_] = value;
  index_++;

  if (index_ == kHeaderSize) {
    // payloadLength is a single byte so it can never exceed kMaxPayloadSize.
    expectedSize_ = kHeaderSize + buffer_[kLengthOffset] + kChecksumSize;
  }

  if (expectedSize_ == 0 || index_ < expectedSize_) {
    return Result::kNeedMore;
  }

  const uint8_t received = buffer_[expectedSize_ - 1];
  const uint8_t expected = checksum(buffer_, expectedSize_ - 1);
  if (received != expected) {
    return fail(Status::kChecksumMismatch);
  }

  frameReady_ = true;
  return Result::kFrameReady;
}

}  // namespace MatrixProtocol
