// Host-side tests for the AQI contract and display timing.
// Run with:  pio test -e native

#include <unity.h>

#include <string.h>

#include "AqiDisplay.h"
#include "MatrixLayout.h"
#include "MatrixProtocol.h"

using Aqi::Display;
using Aqi::Pattern;
using Aqi::Rgb;
using Aqi::Visual;

namespace {

constexpr uint16_t kFrameBytes = MatrixLayout::kFrameBytes;
constexpr uint32_t kWipe = Display::kTransitionWipeMs;
constexpr uint32_t kTotal = Display::kTransitionTotalMs;

bool sameRgb(const Rgb& a, const Rgb& b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

Rgb pixelAt(const uint8_t* frame, uint8_t x, uint8_t y) {
  const uint16_t p = MatrixLayout::logicalToPhysical(x, y);
  return {frame[p * 3], frame[p * 3 + 1], frame[p * 3 + 2]};
}

bool allPixelsAre(const uint8_t* frame, const Rgb& colour) {
  for (uint16_t led = 0; led < AppConfig::kLedCount; ++led) {
    const Rgb px = {frame[led * 3], frame[led * 3 + 1], frame[led * 3 + 2]};
    if (!sameRgb(px, colour)) return false;
  }
  return true;
}

bool isBlack(const Rgb& c) {
  return c.r == 0 && c.g == 0 && c.b == 0;
}

bool isInner(uint8_t x, uint8_t y) {
  return x >= 1 && x <= 2 && y >= 1 && y <= 2;
}

Visual visualOf(uint8_t status) {
  Visual v;
  TEST_ASSERT_TRUE_MESSAGE(Aqi::lookupVisual(status, v), "status must be in contract");
  return v;
}

// Puts a display into the steady state of `status` at time `t0`, returning
// the time at which the steady pattern's phase starts.
uint32_t settle(Display& display, uint8_t status, uint32_t t0) {
  uint8_t frame[kFrameBytes];
  TEST_ASSERT_TRUE(display.setStatus(status, t0));
  display.render(t0 + kTotal, frame);
  TEST_ASSERT_FALSE(display.inTransition());
  return t0 + kTotal;
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- layout ---------------------------------------------------------------

void test_layout_is_serpentine() {
  TEST_ASSERT_EQUAL_UINT16(0, MatrixLayout::logicalToPhysical(0, 0));
  TEST_ASSERT_EQUAL_UINT16(3, MatrixLayout::logicalToPhysical(3, 0));
  TEST_ASSERT_EQUAL_UINT16(7, MatrixLayout::logicalToPhysical(0, 1));
  TEST_ASSERT_EQUAL_UINT16(4, MatrixLayout::logicalToPhysical(3, 1));
  TEST_ASSERT_EQUAL_UINT16(8, MatrixLayout::logicalToPhysical(0, 2));
  TEST_ASSERT_EQUAL_UINT16(15, MatrixLayout::logicalToPhysical(0, 3));
  TEST_ASSERT_EQUAL_UINT16(AppConfig::kLedCount, MatrixLayout::logicalToPhysical(4, 0));
  TEST_ASSERT_EQUAL_UINT16(AppConfig::kLedCount, MatrixLayout::logicalToPhysical(0, 4));
}

// --- contract table -------------------------------------------------------

void test_contract_has_exactly_13_statuses() {
  TEST_ASSERT_EQUAL_UINT8(13, Aqi::kStatusCount);
  TEST_ASSERT_EQUAL_UINT8(MatrixProtocol::kAqiStatusCount, Aqi::kStatusCount);
  Visual v;
  for (uint8_t s = 0; s < 13; ++s) TEST_ASSERT_TRUE(Aqi::lookupVisual(s, v));
  TEST_ASSERT_FALSE(Aqi::lookupVisual(13, v));
  TEST_ASSERT_FALSE(Aqi::lookupVisual(0x7F, v));
  TEST_ASSERT_FALSE(Aqi::lookupVisual(0xFF, v));
}

void test_contract_patterns_match_specification() {
  const Pattern expected[13] = {
      Pattern::kInner,      // 0x00 Excellent
      Pattern::kStatic,     // 0x01 Good
      Pattern::kBreathing,  // 0x02 Good (degrading)
      Pattern::kStatic,     // 0x03 Moderate
      Pattern::kDualZone,   // 0x04 Moderate (degrading)
      Pattern::kStatic,     // 0x05 Poor
      Pattern::kBreathing,  // 0x06 Poor (degrading)
      Pattern::kStatic,     // 0x07 Unhealthy
      Pattern::kBreathing,  // 0x08 Unhealthy (degrading)
      Pattern::kStatic,     // 0x09 Very Unhealthy
      Pattern::kBreathing,  // 0x0A Very Unhealthy (degrading)
      Pattern::kBlink,      // 0x0B Hazardous
      Pattern::kAlternate,  // 0x0C Extreme
  };
  for (uint8_t s = 0; s < 13; ++s) {
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(expected[s]), static_cast<int>(visualOf(s).pattern),
                              "pattern mismatch");
  }
}

void test_contract_colour_bands() {
  // Green band: 0x00..0x02 share one colour.
  TEST_ASSERT_TRUE(sameRgb(visualOf(0).primary, visualOf(1).primary));
  TEST_ASSERT_TRUE(sameRgb(visualOf(1).primary, visualOf(2).primary));
  // Yellow band: 0x03..0x04.
  TEST_ASSERT_TRUE(sameRgb(visualOf(3).primary, visualOf(4).primary));
  // 0x04's inner zone is the orange of the next band.
  TEST_ASSERT_TRUE(sameRgb(visualOf(4).secondary, visualOf(5).primary));
  // Orange band: 0x05..0x06.
  TEST_ASSERT_TRUE(sameRgb(visualOf(5).primary, visualOf(6).primary));
  // Red band: 0x07..0x08.
  TEST_ASSERT_TRUE(sameRgb(visualOf(7).primary, visualOf(8).primary));
  // Purple band: 0x09..0x0C.
  TEST_ASSERT_TRUE(sameRgb(visualOf(9).primary, visualOf(10).primary));
  TEST_ASSERT_TRUE(sameRgb(visualOf(10).primary, visualOf(11).primary));
  TEST_ASSERT_TRUE(sameRgb(visualOf(11).primary, visualOf(12).primary));
  // Extreme alternates with a red, distinct from purple.
  TEST_ASSERT_FALSE(sameRgb(visualOf(12).secondary, visualOf(12).primary));
  TEST_ASSERT_TRUE(visualOf(12).secondary.r > 0 && visualOf(12).secondary.b == 0);
  // Bands are distinct from each other.
  TEST_ASSERT_FALSE(sameRgb(visualOf(1).primary, visualOf(3).primary));
  TEST_ASSERT_FALSE(sameRgb(visualOf(3).primary, visualOf(5).primary));
  TEST_ASSERT_FALSE(sameRgb(visualOf(5).primary, visualOf(7).primary));
  TEST_ASSERT_FALSE(sameRgb(visualOf(7).primary, visualOf(9).primary));
}

void test_every_status_is_visually_unique() {
  for (uint8_t a = 0; a < 13; ++a) {
    for (uint8_t b = a + 1; b < 13; ++b) {
      const Visual va = visualOf(a);
      const Visual vb = visualOf(b);
      const bool same = va.pattern == vb.pattern && sameRgb(va.primary, vb.primary) &&
                        sameRgb(va.secondary, vb.secondary);
      TEST_ASSERT_FALSE_MESSAGE(same, "two statuses share pattern and colours");
    }
  }
}

// --- display: standby and validation --------------------------------------

void test_boots_into_standby_breathing() {
  Display display;
  display.begin(1000);
  TEST_ASSERT_TRUE(display.inStandby());
  TEST_ASSERT_FALSE(display.inTransition());

  uint8_t frame[kFrameBytes];
  TEST_ASSERT_TRUE(display.render(1000, frame));
  const Rgb px = pixelAt(frame, 0, 0);
  TEST_ASSERT_FALSE(isBlack(px));
  TEST_ASSERT_TRUE(allPixelsAre(frame, px));
  // Standby is white-blue: blue dominant.
  TEST_ASSERT_TRUE(px.b >= px.g && px.g >= px.r);
}

void test_invalid_status_is_rejected_and_ignored() {
  Display display;
  display.begin(0);
  TEST_ASSERT_FALSE(display.setStatus(13, 10));
  TEST_ASSERT_FALSE(display.setStatus(0xFF, 10));
  TEST_ASSERT_TRUE(display.inStandby());
  TEST_ASSERT_FALSE(display.inTransition());
}

void test_render_reports_unchanged_frames() {
  Display display;
  display.begin(0);
  const uint32_t t = settle(display, 0x01, 0);  // Good: static green

  uint8_t frame[kFrameBytes];
  display.render(t + 1, frame);
  TEST_ASSERT_FALSE(display.render(t + 2, frame));
  TEST_ASSERT_FALSE(display.render(t + 5000, frame));
}

// --- display: transition ---------------------------------------------------

void test_status_change_starts_wipe_over_previous_frame() {
  Display display;
  display.begin(0);
  uint8_t standby[kFrameBytes];
  display.render(0, standby);
  const Rgb standbyPx = pixelAt(standby, 0, 0);

  TEST_ASSERT_TRUE(display.setStatus(0x07, 100));  // Unhealthy: red
  TEST_ASSERT_TRUE(display.inTransition());
  TEST_ASSERT_FALSE(display.inStandby());

  uint8_t frame[kFrameBytes];
  display.render(100, frame);
  // First wipe pixel is the top-right corner; everything else is the old frame.
  TEST_ASSERT_TRUE(sameRgb(pixelAt(frame, 3, 0), visualOf(0x07).primary));
  TEST_ASSERT_TRUE(sameRgb(pixelAt(frame, 2, 0), standbyPx));
  TEST_ASSERT_TRUE(sameRgb(pixelAt(frame, 0, 3), standbyPx));

  // Half way through the wipe, 8 pixels (top two rows) are red.
  display.render(100 + AppConfig::kAqiTransitionWipeStepMs * 7, frame);
  for (uint8_t x = 0; x < 4; ++x) {
    TEST_ASSERT_TRUE(sameRgb(pixelAt(frame, x, 0), visualOf(0x07).primary));
    TEST_ASSERT_TRUE(sameRgb(pixelAt(frame, x, 1), visualOf(0x07).primary));
    TEST_ASSERT_TRUE(sameRgb(pixelAt(frame, x, 2), standbyPx));
  }
}

void test_transition_holds_solid_then_settles() {
  Display display;
  display.begin(0);
  TEST_ASSERT_TRUE(display.setStatus(0x00, 0));  // Excellent: inner 2x2 green
  const Rgb green = visualOf(0x00).primary;

  uint8_t frame[kFrameBytes];
  // End of wipe: fully solid in the new colour, even for a pattern whose
  // steady state is not a full fill.
  display.render(kWipe, frame);
  TEST_ASSERT_TRUE(display.inTransition());
  TEST_ASSERT_TRUE(allPixelsAre(frame, green));

  display.render(kTotal - 1, frame);
  TEST_ASSERT_TRUE(display.inTransition());
  TEST_ASSERT_TRUE(allPixelsAre(frame, green));

  // Settled: inner 2x2 only.
  display.render(kTotal, frame);
  TEST_ASSERT_FALSE(display.inTransition());
  for (uint8_t y = 0; y < 4; ++y) {
    for (uint8_t x = 0; x < 4; ++x) {
      const Rgb px = pixelAt(frame, x, y);
      if (isInner(x, y)) {
        TEST_ASSERT_TRUE(sameRgb(px, green));
      } else {
        TEST_ASSERT_TRUE(isBlack(px));
      }
    }
  }
}

void test_same_status_is_heartbeat_without_transition() {
  Display display;
  display.begin(0);
  const uint32_t t = settle(display, 0x03, 0);
  TEST_ASSERT_TRUE(display.setStatus(0x03, t + 30000));
  TEST_ASSERT_FALSE(display.inTransition());
}

void test_status_change_mid_transition_restarts_from_current_frame() {
  Display display;
  display.begin(0);
  display.setStatus(0x07, 0);  // red wipe begins
  uint8_t frame[kFrameBytes];
  display.render(AppConfig::kAqiTransitionWipeStepMs * 3, frame);  // 4 red pixels

  display.setStatus(0x01, AppConfig::kAqiTransitionWipeStepMs * 3);  // now green
  display.render(AppConfig::kAqiTransitionWipeStepMs * 3, frame);
  TEST_ASSERT_TRUE(display.inTransition());
  TEST_ASSERT_TRUE(sameRgb(pixelAt(frame, 3, 0), visualOf(0x01).primary));  // new wipe head
  TEST_ASSERT_TRUE(sameRgb(pixelAt(frame, 2, 0), visualOf(0x07).primary));  // old wipe remains
}

// --- display: steady patterns ---------------------------------------------

void test_static_pattern_is_full_and_constant() {
  Display display;
  display.begin(0);
  const uint32_t t = settle(display, 0x05, 0);  // Poor: orange
  uint8_t frame[kFrameBytes];
  display.render(t + 12345, frame);
  TEST_ASSERT_TRUE(allPixelsAre(frame, visualOf(0x05).primary));
}

void test_breathing_pattern_cycles_and_never_goes_dark() {
  Display display;
  display.begin(0);
  const uint32_t t = settle(display, 0x02, 0);  // Good (degrading): green
  uint8_t frame[kFrameBytes];

  uint8_t minG = 255;
  uint8_t maxG = 0;
  for (uint8_t step = 0; step < 16; ++step) {
    display.render(t + step * AppConfig::kAqiBreathingStepMs, frame);
    const Rgb px = pixelAt(frame, 0, 0);
    TEST_ASSERT_TRUE(allPixelsAre(frame, px));
    TEST_ASSERT_EQUAL_UINT8(0, px.r);
    TEST_ASSERT_EQUAL_UINT8(0, px.b);
    TEST_ASSERT_TRUE(px.g > 0);
    if (px.g < minG) minG = px.g;
    if (px.g > maxG) maxG = px.g;
  }
  TEST_ASSERT_TRUE(maxG > minG);
  TEST_ASSERT_TRUE(maxG < visualOf(0x02).primary.g);

  // Period is 16 steps.
  uint8_t a[kFrameBytes];
  uint8_t b[kFrameBytes];
  display.render(t + 3 * AppConfig::kAqiBreathingStepMs, a);
  display.render(t + 19 * AppConfig::kAqiBreathingStepMs, b);
  TEST_ASSERT_EQUAL_MEMORY(a, b, kFrameBytes);
}

void test_blink_pattern_toggles_at_configured_interval() {
  Display display;
  display.begin(0);
  const uint32_t t = settle(display, 0x0B, 0);  // Hazardous
  const Rgb purple = visualOf(0x0B).primary;
  const uint32_t half = AppConfig::kAqiBlinkIntervalMs;
  uint8_t frame[kFrameBytes];

  display.render(t, frame);
  TEST_ASSERT_TRUE(allPixelsAre(frame, purple));
  display.render(t + half - 1, frame);
  TEST_ASSERT_TRUE(allPixelsAre(frame, purple));
  display.render(t + half, frame);
  TEST_ASSERT_TRUE(allPixelsAre(frame, {0, 0, 0}));
  display.render(t + 2 * half, frame);
  TEST_ASSERT_TRUE(allPixelsAre(frame, purple));
}

void test_alternate_pattern_swaps_colours() {
  Display display;
  display.begin(0);
  const uint32_t t = settle(display, 0x0C, 0);  // Extreme
  const Visual v = visualOf(0x0C);
  const uint32_t half = AppConfig::kAqiAlternateIntervalMs;
  uint8_t frame[kFrameBytes];

  display.render(t, frame);
  TEST_ASSERT_TRUE(allPixelsAre(frame, v.primary));
  display.render(t + half, frame);
  TEST_ASSERT_TRUE(allPixelsAre(frame, v.secondary));
  display.render(t + 2 * half, frame);
  TEST_ASSERT_TRUE(allPixelsAre(frame, v.primary));
}

void test_dual_zone_has_static_perimeter_and_breathing_centre() {
  Display display;
  display.begin(0);
  const uint32_t t = settle(display, 0x04, 0);  // Moderate (degrading)
  const Visual v = visualOf(0x04);
  uint8_t early[kFrameBytes];
  uint8_t later[kFrameBytes];
  display.render(t, early);
  display.render(t + 8 * AppConfig::kAqiBreathingStepMs, later);

  for (uint8_t y = 0; y < 4; ++y) {
    for (uint8_t x = 0; x < 4; ++x) {
      const Rgb e = pixelAt(early, x, y);
      const Rgb l = pixelAt(later, x, y);
      if (isInner(x, y)) {
        // Orange-ish (secondary) and changing over time.
        TEST_ASSERT_TRUE(e.r > 0 && e.b == 0);
        TEST_ASSERT_FALSE(sameRgb(e, l));
      } else {
        TEST_ASSERT_TRUE(sameRgb(e, v.primary));
        TEST_ASSERT_TRUE(sameRgb(l, v.primary));
      }
    }
  }
}

// --- display: standby timeout ----------------------------------------------

void test_stale_data_returns_to_standby_with_transition() {
  Display display;
  display.begin(0);
  settle(display, 0x09, 0);
  uint8_t frame[kFrameBytes];

  // Timeout counts from the status message at t=0, not from settle time.
  display.render(AppConfig::kAqiStandbyTimeoutMs - 1, frame);
  TEST_ASSERT_FALSE(display.inStandby());

  display.render(AppConfig::kAqiStandbyTimeoutMs, frame);
  TEST_ASSERT_TRUE(display.inStandby());
  TEST_ASSERT_TRUE(display.inTransition());

  display.render(AppConfig::kAqiStandbyTimeoutMs + kTotal, frame);
  TEST_ASSERT_FALSE(display.inTransition());
  const Rgb px = pixelAt(frame, 0, 0);
  TEST_ASSERT_TRUE(allPixelsAre(frame, px));
  TEST_ASSERT_TRUE(px.b >= px.g && px.g >= px.r);
}

void test_heartbeat_refreshes_standby_timeout() {
  Display display;
  display.begin(0);
  display.setStatus(0x01, 0);
  uint8_t frame[kFrameBytes];

  display.setStatus(0x01, 50000);
  display.render(70000, frame);
  TEST_ASSERT_FALSE(display.inStandby());

  display.render(50000 + AppConfig::kAqiStandbyTimeoutMs, frame);
  TEST_ASSERT_TRUE(display.inStandby());
}

void test_first_status_after_standby_transitions_again() {
  Display display;
  display.begin(0);
  settle(display, 0x01, 0);
  uint8_t frame[kFrameBytes];
  display.render(AppConfig::kAqiStandbyTimeoutMs + kTotal, frame);
  TEST_ASSERT_TRUE(display.inStandby());

  // Same code as before standby must still animate: standby -> status is a change.
  TEST_ASSERT_TRUE(display.setStatus(0x01, AppConfig::kAqiStandbyTimeoutMs + kTotal + 10));
  TEST_ASSERT_TRUE(display.inTransition());
  TEST_ASSERT_FALSE(display.inStandby());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_layout_is_serpentine);
  RUN_TEST(test_contract_has_exactly_13_statuses);
  RUN_TEST(test_contract_patterns_match_specification);
  RUN_TEST(test_contract_colour_bands);
  RUN_TEST(test_every_status_is_visually_unique);
  RUN_TEST(test_boots_into_standby_breathing);
  RUN_TEST(test_invalid_status_is_rejected_and_ignored);
  RUN_TEST(test_render_reports_unchanged_frames);
  RUN_TEST(test_status_change_starts_wipe_over_previous_frame);
  RUN_TEST(test_transition_holds_solid_then_settles);
  RUN_TEST(test_same_status_is_heartbeat_without_transition);
  RUN_TEST(test_status_change_mid_transition_restarts_from_current_frame);
  RUN_TEST(test_static_pattern_is_full_and_constant);
  RUN_TEST(test_breathing_pattern_cycles_and_never_goes_dark);
  RUN_TEST(test_blink_pattern_toggles_at_configured_interval);
  RUN_TEST(test_alternate_pattern_swaps_colours);
  RUN_TEST(test_dual_zone_has_static_perimeter_and_breathing_centre);
  RUN_TEST(test_stale_data_returns_to_standby_with_transition);
  RUN_TEST(test_heartbeat_refreshes_standby_timeout);
  RUN_TEST(test_first_status_after_standby_transitions_again);
  return UNITY_END();
}
