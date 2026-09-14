/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <shell/shared/platform/FrameDividerRateBackend.h>

#include <limits>
#include <memory>
#include <vector>
#include <shell/shared/platform/PresentationRateController.h>

namespace igl::shell::tests {
namespace {

/// Stands in for a display with switchable modes plus whatever lever the platform has:
/// reports both rates, records every divisor applied, and can refuse the way a driver does.
struct FakeDisplay {
  float currentHz = 60.0f;
  float maxHz = 120.0f;
  int maxDivisor = 16;
  bool dividerWorks = true;
  std::vector<int> appliedDivisors;
  int rateQueryCount = 0;

  DisplayRatesProvider provider() {
    return [this]() {
      ++rateQueryCount;
      return DisplayRates{.currentHz = currentHz, .maxHz = maxHz};
    };
  }

  FrameDivider divider() {
    return FrameDivider{
        .setDivisor = [this](int refreshesPerFrame) -> Result {
          if (!dividerWorks) {
            return Result{Result::Code::RuntimeError, "The fake divider refused."};
          }
          appliedDivisors.push_back(refreshesPerFrame);
          return Result{};
        },
        .maxDivisor = maxDivisor,
    };
  }

  /// A platform with no lever at all — the Android Vulkan case, where nothing can pace
  /// below the display's own rate.
  FrameDivider noDivider() {
    return FrameDivider{.setDivisor = nullptr, .maxDivisor = maxDivisor};
  }
};

constexpr float kQuietNan = std::numeric_limits<float>::quiet_NaN();

PresentationRateController controllerWith(std::unique_ptr<PresentationRateController::Backend> b) {
  PresentationRateController controller;
  controller.setBackend(std::move(b));
  return controller;
}

// ---------------------------------------------------------------------------
// Capabilities
// ---------------------------------------------------------------------------

TEST(FrameDividerRateBackendTest, ReportsThePanelsBestModeRatherThanTheOneItIsIn) {
  FakeDisplay display;
  const auto backend = createFrameDividerRateBackend(display.provider(), display.divider());

  const PresentationRateCapabilities capabilities = backend->getCapabilities();

  // 120, not the 60 the panel happens to be running: a request for the higher mode has to
  // stay expressible or the seam's own range check refuses it before any backend sees it.
  EXPECT_FLOAT_EQ(capabilities.maxRefreshRateHz, 120.0f);
  EXPECT_FLOAT_EQ(capabilities.minRefreshRateHz, 0.0f);
  EXPECT_TRUE(capabilities.canSetFixedRate);
}

TEST(FrameDividerRateBackendTest, CannotSetAFixedRateWithoutADivider) {
  FakeDisplay display;
  const auto backend = createFrameDividerRateBackend(display.provider(), display.noDivider());

  EXPECT_FALSE(backend->getCapabilities().canSetFixedRate);
}

TEST(FrameDividerRateBackendTest, ReportsNoCapabilitiesWhenTheDisplayWillNotNameARate) {
  FakeDisplay display;
  display.currentHz = 0.0f;
  display.maxHz = 0.0f;
  const auto backend = createFrameDividerRateBackend(display.provider(), display.divider());

  const PresentationRateCapabilities capabilities = backend->getCapabilities();

  EXPECT_FLOAT_EQ(capabilities.maxRefreshRateHz, 0.0f);
  EXPECT_FALSE(capabilities.canSetFixedRate);
}

TEST(FrameDividerRateBackendTest, TreatsAnUnusableReadingAsNoReading) {
  for (const float unusable : {kQuietNan, std::numeric_limits<float>::infinity(), -60.0f}) {
    FakeDisplay display;
    display.currentHz = unusable;
    display.maxHz = unusable;
    const auto backend = createFrameDividerRateBackend(display.provider(), display.divider());

    EXPECT_FLOAT_EQ(backend->getCapabilities().maxRefreshRateHz, 0.0f);
    EXPECT_FALSE(backend->getCapabilities().canSetFixedRate);
  }
}

TEST(FrameDividerRateBackendTest, NeverReportsAMaximumBelowTheModeAlreadyRunning) {
  FakeDisplay display;
  display.currentHz = 120.0f;
  // A display that claims a 60 Hz ceiling while running at 120 describes nothing real;
  // trusting it would cap DisplayMaximum below what is on screen right now.
  display.maxHz = 60.0f;
  const auto backend = createFrameDividerRateBackend(display.provider(), display.divider());

  EXPECT_FLOAT_EQ(backend->getCapabilities().maxRefreshRateHz, 120.0f);
}

// ---------------------------------------------------------------------------
// Granting: every grant is one the divider is actually holding
// ---------------------------------------------------------------------------

TEST(FrameDividerRateBackendTest, HoldsACappedRungByDividingTheCurrentMode) {
  FakeDisplay display;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 30.0f});

  EXPECT_TRUE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  EXPECT_FLOAT_EQ(state.grant->hz, 30.0f);
  EXPECT_EQ(display.appliedDivisors, std::vector<int>{2});
}

TEST(FrameDividerRateBackendTest, GrantsTheRateTheDivisorActuallyProducesNotTheOneAskedFor) {
  FakeDisplay display;
  display.currentHz = 59.94f;
  display.maxHz = 59.94f;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 30.0f});

  EXPECT_TRUE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  // Half of 59.94, because that is what presenting every second refresh delivers. Reporting
  // the requested 30 would be the seam telling a profiling run something untrue.
  EXPECT_FLOAT_EQ(state.grant->hz, 59.94f / 2.0f);
}

TEST(FrameDividerRateBackendTest, TreatsOneModeReportedTwoWaysAsOneModeRatherThanASwitch) {
  FakeDisplay display;
  // Exactly what Android reports for a single 60 Hz mode: the supported-modes list rounds to
  // a nominal 60.0 while the live query gives the true 59.94. Read literally, the maximum is
  // forever above the mode already running and DisplayMaximum is refused on a panel that is
  // in fact at its best.
  display.currentHz = 59.94f;
  display.maxHz = 60.0f;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_TRUE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  // The rate the panel is really running, not the nominal one it was asked for.
  EXPECT_FLOAT_EQ(state.grant->hz, 59.94f);
  EXPECT_EQ(display.appliedDivisors, std::vector<int>{1});
}

TEST(FrameDividerRateBackendTest, UndoesASmallerRungByApplyingADivisorOfOne) {
  FakeDisplay display;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 30.0f});
  controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});

  // The second rung has to reach the lever too. Skipping the call because the divisor is one
  // would leave the display halving frames under the 60 Hz grant it was just given.
  EXPECT_EQ(display.appliedDivisors, (std::vector<int>{2, 1}));
}

TEST(FrameDividerRateBackendTest, ResolvesDisplayMaximumAgainstTheModeActuallyRunning) {
  FakeDisplay display;
  display.currentHz = 120.0f;
  display.maxHz = 120.0f;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_TRUE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  EXPECT_FLOAT_EQ(state.grant->hz, 120.0f);
  EXPECT_EQ(display.appliedDivisors, std::vector<int>{1});
}

TEST(FrameDividerRateBackendTest, GrantsDisplayMaximumWithNoDividerAtAll) {
  FakeDisplay display;
  display.currentHz = 60.0f;
  display.maxHz = 60.0f;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.noDivider()));

  // The Android Vulkan case: nothing can pace below the panel, but presenting every refresh
  // needs no lever, so the uncapped rung is still honestly grantable.
  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_TRUE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  EXPECT_FLOAT_EQ(state.grant->hz, 60.0f);
}

// ---------------------------------------------------------------------------
// Refusing: what it will not claim to have done
// ---------------------------------------------------------------------------

TEST(FrameDividerRateBackendTest, RefusesARateAboveTheModeCurrentlyRunning) {
  FakeDisplay display;
  display.currentHz = 60.0f;
  display.maxHz = 120.0f;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  // Dividing cannot multiply. Only the compositor can move a 60 Hz panel to 120, and nothing
  // here can confirm it did, so this must not come back as a grant of 120 — nor as a grant
  // of 60 wearing 120's name.
  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_FALSE(state.result.isOk());
  EXPECT_EQ(state.result.code, Result::Code::Unsupported);
  EXPECT_FALSE(state.grant.has_value());
  EXPECT_TRUE(display.appliedDivisors.empty());
}

TEST(FrameDividerRateBackendTest, RefusesEveryRequestWhenTheDisplayWillNotNameARate) {
  FakeDisplay display;
  display.currentHz = 0.0f;
  display.maxHz = 0.0f;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_FALSE(state.result.isOk());
  EXPECT_EQ(state.result.code, Result::Code::Unsupported);
  EXPECT_FALSE(state.grant.has_value());
  EXPECT_TRUE(display.appliedDivisors.empty());
}

TEST(FrameDividerRateBackendTest, RefusesARungNeedingMoreRefreshesThanTheDividerWillWaitFor) {
  FakeDisplay display;
  display.currentHz = 120.0f;
  display.maxHz = 120.0f;
  // What an EGL config reporting EGL_MAX_SWAP_INTERVAL of 1 gives: the panel can only be
  // presented every refresh, so a capped rung is out of reach and says so.
  display.maxDivisor = 1;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 30.0f});

  EXPECT_FALSE(state.result.isOk());
  EXPECT_EQ(state.result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_FALSE(state.grant.has_value());
  EXPECT_TRUE(display.appliedDivisors.empty());
}

TEST(FrameDividerRateBackendTest, ReportsADividerRefusalVerbatimAndRecordsNoGrant) {
  FakeDisplay display;
  display.dividerWorks = false;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 30.0f});

  EXPECT_FALSE(state.result.isOk());
  EXPECT_EQ(state.result.message, "The fake divider refused.");
  EXPECT_FALSE(state.grant.has_value());
}

TEST(FrameDividerRateBackendTest, LeavesAnEarlierGrantAloneWhenTheNextRequestIsRefused) {
  FakeDisplay display;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 30.0f});
  // Above the mode running, so nothing is touched — which is precisely what makes it safe
  // for the controller to keep reporting the 30 Hz that is still on screen.
  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_FALSE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  EXPECT_FLOAT_EQ(state.grant->hz, 30.0f);
  EXPECT_EQ(display.appliedDivisors, std::vector<int>{2});
}

// ---------------------------------------------------------------------------
// Reading the display fresh, because a mode switch moves it underneath us
// ---------------------------------------------------------------------------

TEST(FrameDividerRateBackendTest, DerivesTheDivisorFromTheModeRunningAtRequestTime) {
  FakeDisplay display;
  display.currentHz = 120.0f;
  display.maxHz = 120.0f;
  PresentationRateController controller =
      controllerWith(createFrameDividerRateBackend(display.provider(), display.divider()));

  controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});
  ASSERT_EQ(display.appliedDivisors, std::vector<int>{2});

  // The panel drops to 60 underneath us. A divisor of 2 now presents at 30, so re-asking for
  // 60 has to resolve against the new mode and come back with a divisor of 1 — a cached
  // reading would leave the cadence at 30 while the grant still claimed 60.
  display.currentHz = 60.0f;
  display.maxHz = 60.0f;
  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});

  EXPECT_TRUE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  EXPECT_FLOAT_EQ(state.grant->hz, 60.0f);
  EXPECT_EQ(display.appliedDivisors, (std::vector<int>{2, 1}));
}

TEST(FrameDividerRateBackendTest, AsksTheDisplayAgainRatherThanCachingItsFirstAnswer) {
  FakeDisplay display;
  const auto backend = createFrameDividerRateBackend(display.provider(), display.divider());

  (void)backend->getCapabilities();
  (void)backend->getCapabilities();
  float grantedHz = 0.0f;
  backend->apply({.mode = PresentationRateMode::Fixed, .hz = 30.0f}, grantedHz);

  EXPECT_EQ(display.rateQueryCount, 3);
}

} // namespace
} // namespace igl::shell::tests
