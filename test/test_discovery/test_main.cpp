// Host-side tests for the discovery registration payload.
// Run with:  pio test -e native

#include <unity.h>

#include <string.h>

#include "DiscoveryPayload.h"

using Discovery::Info;

namespace {

Info sampleInfo() {
  return {"living-room", "192.168.1.42", 7777, "CC:50:E3:3C:E9:03", 2, "0.3.0", 4242};
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_payload_matches_documented_shape() {
  char out[256];
  const int n = Discovery::buildPayload(sampleInfo(), out, sizeof(out));

  const char* expected =
      "{\"name\":\"living-room\",\"ip\":\"192.168.1.42\",\"port\":7777,"
      "\"mac\":\"CC:50:E3:3C:E9:03\",\"protocol\":2,\"firmware\":\"0.3.0\",\"uptime_s\":4242}";
  TEST_ASSERT_EQUAL_INT(static_cast<int>(strlen(expected)), n);
  TEST_ASSERT_EQUAL_STRING(expected, out);
}

void test_default_style_name_fits_comfortably() {
  Info info = sampleInfo();
  info.name = "neoncore-3ce903";
  char out[256];
  TEST_ASSERT_TRUE(Discovery::buildPayload(info, out, sizeof(out)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(out, "\"name\":\"neoncore-3ce903\""));
}

void test_strings_are_escaped() {
  Info info = sampleInfo();
  info.name = "kid\"s \\ room\n";
  char out[256];
  TEST_ASSERT_TRUE(Discovery::buildPayload(info, out, sizeof(out)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(out, "\"name\":\"kid\\\"s \\\\ room\\u000a\""));
}

void test_null_strings_become_empty() {
  Info info = sampleInfo();
  info.name = nullptr;
  char out[256];
  TEST_ASSERT_TRUE(Discovery::buildPayload(info, out, sizeof(out)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(out, "\"name\":\"\""));
}

void test_too_small_buffer_is_rejected() {
  char out[40];
  TEST_ASSERT_EQUAL_INT(-1, Discovery::buildPayload(sampleInfo(), out, sizeof(out)));
  TEST_ASSERT_EQUAL_INT(-1, Discovery::buildPayload(sampleInfo(), out, 0));
  TEST_ASSERT_EQUAL_INT(-1, Discovery::buildPayload(sampleInfo(), nullptr, 100));
}

void test_exact_fit_boundary() {
  char big[256];
  const int n = Discovery::buildPayload(sampleInfo(), big, sizeof(big));
  TEST_ASSERT_TRUE(n > 0);

  // Capacity n is one byte short of the terminator; n + 1 fits exactly.
  char tight[256];
  TEST_ASSERT_EQUAL_INT(-1, Discovery::buildPayload(sampleInfo(), tight, n));
  TEST_ASSERT_EQUAL_INT(n, Discovery::buildPayload(sampleInfo(), tight, n + 1));
  TEST_ASSERT_EQUAL_STRING(big, tight);
}

void test_numbers_use_full_range() {
  Info info = sampleInfo();
  info.port = 65535;
  info.uptimeSeconds = 4294967295u;
  info.protocolVersion = 255;
  char out[256];
  TEST_ASSERT_TRUE(Discovery::buildPayload(info, out, sizeof(out)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(out, "\"port\":65535"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"uptime_s\":4294967295"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"protocol\":255"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_payload_matches_documented_shape);
  RUN_TEST(test_default_style_name_fits_comfortably);
  RUN_TEST(test_strings_are_escaped);
  RUN_TEST(test_null_strings_become_empty);
  RUN_TEST(test_too_small_buffer_is_rejected);
  RUN_TEST(test_exact_fit_boundary);
  RUN_TEST(test_numbers_use_full_range);
  return UNITY_END();
}
