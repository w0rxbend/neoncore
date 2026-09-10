// Host-side tests for the wire protocol: checksum, response builder, and the
// byte-at-a-time frame parser. Run with:  pio test -e native

#include <unity.h>

#include <string.h>

#include "MatrixProtocol.h"

using MatrixProtocol::FrameParser;
using MatrixProtocol::Status;
using Result = FrameParser::Result;

namespace {

// Builds a well-formed frame into `out` and returns its size.
uint16_t buildFrame(uint8_t command, const uint8_t* payload, uint8_t length, uint8_t* out) {
  out[0] = MatrixProtocol::kMagic0;
  out[1] = MatrixProtocol::kMagic1;
  out[2] = MatrixProtocol::kVersion;
  out[3] = command;
  out[4] = length;
  for (uint8_t i = 0; i < length; ++i) {
    out[5 + i] = payload[i];
  }
  const uint16_t size = MatrixProtocol::kHeaderSize + length;
  out[size] = MatrixProtocol::checksum(out, size);
  return size + 1;
}

// Feeds a whole buffer and returns the last result.
Result feed(FrameParser& parser, const uint8_t* data, uint16_t size) {
  Result last = Result::kNeedMore;
  for (uint16_t i = 0; i < size; ++i) {
    last = parser.push(data[i]);
  }
  return last;
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- checksum / response -------------------------------------------------

void test_checksum_is_xor_of_bytes() {
  const uint8_t data[] = {0x4C, 0x4D, 0x02, 0x03, 0x01, 0x0C};
  uint8_t expected = 0;
  for (uint8_t b : data) expected ^= b;
  TEST_ASSERT_EQUAL_UINT8(expected, MatrixProtocol::checksum(data, sizeof(data)));
  TEST_ASSERT_EQUAL_UINT8(0, MatrixProtocol::checksum(data, 0));
}

void test_response_frame_is_well_formed() {
  uint8_t response[MatrixProtocol::kResponseSize];
  MatrixProtocol::buildResponse(Status::kInvalidArgument, response);

  TEST_ASSERT_EQUAL_UINT8(MatrixProtocol::kMagic0, response[0]);
  TEST_ASSERT_EQUAL_UINT8(MatrixProtocol::kMagic1, response[1]);
  TEST_ASSERT_EQUAL_UINT8(MatrixProtocol::kVersion, response[2]);
  TEST_ASSERT_EQUAL_UINT8(MatrixProtocol::kResponseCommand, response[3]);
  TEST_ASSERT_EQUAL_UINT8(0x06, response[4]);
  TEST_ASSERT_EQUAL_UINT8(MatrixProtocol::checksum(response, 5), response[5]);
}

// --- parser: happy paths -------------------------------------------------

void test_parses_ping_frame() {
  FrameParser parser;
  uint8_t frame[MatrixProtocol::kMaxFrameSize];
  const uint16_t size = buildFrame(0x00, nullptr, 0, frame);

  TEST_ASSERT_EQUAL_UINT16(6, size);
  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, frame, size));
  TEST_ASSERT_EQUAL_UINT8(0x00, parser.command());
  TEST_ASSERT_EQUAL_UINT8(0, parser.payloadLength());
}

void test_parses_frame_with_payload() {
  FrameParser parser;
  uint8_t frame[MatrixProtocol::kMaxFrameSize];
  const uint8_t payload[] = {0x0C};
  const uint16_t size = buildFrame(0x03, payload, 1, frame);

  // Every byte before the last must ask for more.
  for (uint16_t i = 0; i < size - 1; ++i) {
    TEST_ASSERT_EQUAL(Result::kNeedMore, parser.push(frame[i]));
    TEST_ASSERT_TRUE(parser.inProgress());
  }
  TEST_ASSERT_EQUAL(Result::kFrameReady, parser.push(frame[size - 1]));
  TEST_ASSERT_EQUAL_UINT8(0x03, parser.command());
  TEST_ASSERT_EQUAL_UINT8(1, parser.payloadLength());
  TEST_ASSERT_EQUAL_UINT8(0x0C, parser.payload()[0]);
}

void test_parses_max_payload() {
  FrameParser parser;
  uint8_t payload[MatrixProtocol::kMaxPayloadSize];
  for (uint16_t i = 0; i < sizeof(payload); ++i) payload[i] = static_cast<uint8_t>(i);
  uint8_t frame[MatrixProtocol::kMaxFrameSize];
  const uint16_t size = buildFrame(0x7F, payload, 255, frame);

  TEST_ASSERT_EQUAL_UINT16(MatrixProtocol::kMaxFrameSize, size);
  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, frame, size));
  TEST_ASSERT_EQUAL_UINT8(255, parser.payloadLength());
  TEST_ASSERT_EQUAL_UINT8(254, parser.payload()[254]);
}

void test_parses_pipelined_frames() {
  FrameParser parser;
  uint8_t first[MatrixProtocol::kMaxFrameSize];
  uint8_t second[MatrixProtocol::kMaxFrameSize];
  const uint8_t brightness = 42;
  const uint16_t firstSize = buildFrame(0x01, &brightness, 1, first);
  const uint16_t secondSize = buildFrame(0x00, nullptr, 0, second);

  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, first, firstSize));
  TEST_ASSERT_EQUAL_UINT8(0x01, parser.command());
  TEST_ASSERT_EQUAL_UINT8(42, parser.payload()[0]);

  // Caller resets between frames.
  parser.reset();
  TEST_ASSERT_FALSE(parser.inProgress());
  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, second, secondSize));
  TEST_ASSERT_EQUAL_UINT8(0x00, parser.command());
}

void test_next_byte_after_ready_frame_starts_new_frame_without_reset() {
  FrameParser parser;
  uint8_t frame[MatrixProtocol::kMaxFrameSize];
  const uint16_t size = buildFrame(0x00, nullptr, 0, frame);

  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, frame, size));
  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, frame, size));
}

// --- parser: error paths -------------------------------------------------

void test_bad_first_magic_byte_reports_error_and_keeps_sync() {
  FrameParser parser;
  TEST_ASSERT_EQUAL(Result::kError, parser.push(0x00));
  TEST_ASSERT_EQUAL(Status::kBadMagic, parser.error());
  TEST_ASSERT_FALSE(parser.inProgress());

  uint8_t frame[MatrixProtocol::kMaxFrameSize];
  const uint16_t size = buildFrame(0x00, nullptr, 0, frame);
  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, frame, size));
}

void test_bad_second_magic_byte_resets() {
  FrameParser parser;
  TEST_ASSERT_EQUAL(Result::kNeedMore, parser.push(MatrixProtocol::kMagic0));
  TEST_ASSERT_EQUAL(Result::kError, parser.push(0xFF));
  TEST_ASSERT_EQUAL(Status::kBadMagic, parser.error());
  TEST_ASSERT_FALSE(parser.inProgress());
}

void test_wrong_version_is_rejected() {
  FrameParser parser;
  parser.push(MatrixProtocol::kMagic0);
  parser.push(MatrixProtocol::kMagic1);
  TEST_ASSERT_EQUAL(Result::kError, parser.push(MatrixProtocol::kVersion + 1));
  TEST_ASSERT_EQUAL(Status::kUnsupportedVersion, parser.error());
  TEST_ASSERT_FALSE(parser.inProgress());
}

void test_checksum_mismatch_is_rejected_and_resets() {
  FrameParser parser;
  uint8_t frame[MatrixProtocol::kMaxFrameSize];
  const uint8_t payload[] = {0x05};
  const uint16_t size = buildFrame(0x03, payload, 1, frame);
  frame[size - 1] ^= 0x01;

  TEST_ASSERT_EQUAL(Result::kError, feed(parser, frame, size));
  TEST_ASSERT_EQUAL(Status::kChecksumMismatch, parser.error());
  TEST_ASSERT_FALSE(parser.inProgress());

  // A good frame right after is parsed normally.
  frame[size - 1] ^= 0x01;
  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, frame, size));
}

void test_reset_discards_partial_frame() {
  FrameParser parser;
  parser.push(MatrixProtocol::kMagic0);
  parser.push(MatrixProtocol::kMagic1);
  parser.push(MatrixProtocol::kVersion);
  TEST_ASSERT_TRUE(parser.inProgress());
  parser.reset();
  TEST_ASSERT_FALSE(parser.inProgress());

  uint8_t frame[MatrixProtocol::kMaxFrameSize];
  const uint16_t size = buildFrame(0x00, nullptr, 0, frame);
  TEST_ASSERT_EQUAL(Result::kFrameReady, feed(parser, frame, size));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_checksum_is_xor_of_bytes);
  RUN_TEST(test_response_frame_is_well_formed);
  RUN_TEST(test_parses_ping_frame);
  RUN_TEST(test_parses_frame_with_payload);
  RUN_TEST(test_parses_max_payload);
  RUN_TEST(test_parses_pipelined_frames);
  RUN_TEST(test_next_byte_after_ready_frame_starts_new_frame_without_reset);
  RUN_TEST(test_bad_first_magic_byte_reports_error_and_keeps_sync);
  RUN_TEST(test_bad_second_magic_byte_resets);
  RUN_TEST(test_wrong_version_is_rejected);
  RUN_TEST(test_checksum_mismatch_is_rejected_and_resets);
  RUN_TEST(test_reset_discards_partial_frame);
  return UNITY_END();
}
