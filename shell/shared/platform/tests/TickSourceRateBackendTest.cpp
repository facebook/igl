/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <shell/shared/platform/TickSourceRateBackend.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <shell/shared/platform/PresentationRateController.h>

namespace igl::shell::tests {
namespace {

/// Stands in for a windowing system: says what the display can do, records what the tick
/// source was asked for, and can play dead the way a closed window does.
struct FakeTickSource {
  float maxRateHz = 120.0f;
  bool alive = true;
  std::vector<float> appliedHz;
  /// The max-rate snapshot each apply() handed over, to prove it is the one the cadence
  /// was derived from rather than a fresh reading.
  std::vector<float> appliedMaxRateHz;
  int maxRateQueryCount = 0;

  TickSourceRateBackend::MaxRateProvider provider() {
    return [this]() {
      ++maxRateQueryCount;
      return maxRateHz;
    };
  }

  TickSourceRateBackend::RateApplier applier() {
    return [this](float hz, float snapshotHz) -> Result {
      if (!alive) {
        return Result{Result::Code::InvalidOperation, "The fake tick source is gone."};
      }
      appliedHz.push_back(hz);
      appliedMaxRateHz.push_back(snapshotHz);
      return Result{};
    };
  }
};

constexpr float kQuietNan = std::numeric_limits<float>::quiet_NaN();
constexpr float kInfinity = std::numeric_limits<float>::infinity();

// ---------------------------------------------------------------------------
// snapToTickCadence: what the tick source can actually deliver
// ---------------------------------------------------------------------------

TEST(TickSourceRateBackendTest, SnapsARateAtOrOverTheRefreshRateToTheRefreshRate) {
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(120.0f, 120.0f), 120.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(240.0f, 120.0f), 120.0f);
}

TEST(TickSourceRateBackendTest, SnapsWholeDivisorsToThemselves) {
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(60.0f, 120.0f), 60.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(30.0f, 120.0f), 30.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(30.0f, 60.0f), 30.0f);
}

TEST(TickSourceRateBackendTest, SnapsARateBetweenDivisorsToTheNearestReachableCadence) {
  // The whole point of snapping: a 60 Hz panel cannot tick at 50, so saying so beats
  // echoing 50 back into a HUD that would then read a rate nothing is running at.
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(50.0f, 60.0f), 60.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(45.0f, 120.0f), 40.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(25.0f, 60.0f), 30.0f);
}

TEST(TickSourceRateBackendTest, SnapsAFractionalRefreshRateWithoutRoundingItToAWholeNumber) {
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(59.94f, 59.94f), 59.94f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(30.0f, 59.94f), 29.97f);
}

TEST(TickSourceRateBackendTest, RefusesToSnapANonFiniteOrNonPositiveRate) {
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(kQuietNan, 120.0f), 0.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(kInfinity, 120.0f), 0.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(0.0f, 120.0f), 0.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(-60.0f, 120.0f), 0.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(60.0f, kQuietNan), 0.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(60.0f, kInfinity), 0.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(60.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(60.0f, -120.0f), 0.0f);
}

TEST(TickSourceRateBackendTest, RefusesToSnapARateNoNumberOfSkippedRefreshesReaches) {
  // 120 / 1e-40 overflows a float, so there is no cadence to round to.
  EXPECT_FLOAT_EQ(TickSourceRateBackend::snapToTickCadence(1e-40f, 120.0f), 0.0f);
}

// ---------------------------------------------------------------------------
// Capabilities
// ---------------------------------------------------------------------------

TEST(TickSourceRateBackendTest, ReportsTheDisplayMaximumAndNoFloor) {
  FakeTickSource tickSource;
  const TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  const PresentationRateCapabilities capabilities = backend.getCapabilities();

  EXPECT_FLOAT_EQ(capabilities.maxRefreshRateHz, 120.0f);
  // Capping happens by skipping refreshes, so there is no lower limit to report.
  EXPECT_FLOAT_EQ(capabilities.minRefreshRateHz, 0.0f);
  EXPECT_TRUE(capabilities.canSetFixedRate);
}

TEST(TickSourceRateBackendTest, ReportsNoCapabilitiesWhenTheDisplayWillNotNameARate) {
  FakeTickSource tickSource;
  tickSource.maxRateHz = 0.0f;
  const TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  const PresentationRateCapabilities capabilities = backend.getCapabilities();

  EXPECT_FLOAT_EQ(capabilities.maxRefreshRateHz, 0.0f);
  EXPECT_FALSE(capabilities.canSetFixedRate);
}

TEST(TickSourceRateBackendTest, TreatsAnUnusableProviderAnswerAsNoAnswer) {
  for (const float unusable : {kQuietNan, kInfinity, -60.0f}) {
    FakeTickSource tickSource;
    tickSource.maxRateHz = unusable;
    const TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

    const PresentationRateCapabilities capabilities = backend.getCapabilities();

    EXPECT_FLOAT_EQ(capabilities.maxRefreshRateHz, 0.0f);
    EXPECT_FALSE(capabilities.canSetFixedRate);
  }
}

TEST(TickSourceRateBackendTest, AsksTheDisplayAgainRatherThanCachingItsFirstAnswer) {
  FakeTickSource tickSource;
  const TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  EXPECT_FLOAT_EQ(backend.getCapabilities().maxRefreshRateHz, 120.0f);
  // The window moved to a 60 Hz display.
  tickSource.maxRateHz = 60.0f;
  EXPECT_FLOAT_EQ(backend.getCapabilities().maxRefreshRateHz, 60.0f);
  EXPECT_EQ(tickSource.maxRateQueryCount, 2);
}

// ---------------------------------------------------------------------------
// apply()
// ---------------------------------------------------------------------------

TEST(TickSourceRateBackendTest, ResolvesDisplayMaximumAgainstWhatTheDisplayReports) {
  FakeTickSource tickSource;
  TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  float grantedHz = 0.0f;
  const Result result =
      backend.apply({.mode = PresentationRateMode::DisplayMaximum, .hz = 0.0f}, grantedHz);

  EXPECT_TRUE(result.isOk());
  EXPECT_FLOAT_EQ(grantedHz, 120.0f);
  ASSERT_EQ(tickSource.appliedHz.size(), 1u);
  EXPECT_FLOAT_EQ(tickSource.appliedHz.front(), 120.0f);
}

TEST(TickSourceRateBackendTest, AppliesAFixedRateTheTickSourceCanReach) {
  FakeTickSource tickSource;
  TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  float grantedHz = 0.0f;
  const Result result =
      backend.apply({.mode = PresentationRateMode::Fixed, .hz = 30.0f}, grantedHz);

  EXPECT_TRUE(result.isOk());
  EXPECT_FLOAT_EQ(grantedHz, 30.0f);
  ASSERT_EQ(tickSource.appliedHz.size(), 1u);
  EXPECT_FLOAT_EQ(tickSource.appliedHz.front(), 30.0f);
}

TEST(TickSourceRateBackendTest, GrantsTheReachableCadenceRatherThanTheRateThatWasAskedFor) {
  FakeTickSource tickSource;
  tickSource.maxRateHz = 60.0f;
  TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  float grantedHz = 0.0f;
  const Result result =
      backend.apply({.mode = PresentationRateMode::Fixed, .hz = 50.0f}, grantedHz);

  EXPECT_TRUE(result.isOk());
  EXPECT_FLOAT_EQ(grantedHz, 60.0f);
  ASSERT_EQ(tickSource.appliedHz.size(), 1u);
  EXPECT_FLOAT_EQ(tickSource.appliedHz.front(), 60.0f);
}

TEST(TickSourceRateBackendTest, RefusesEveryRequestWhenTheDisplayWillNotNameARate) {
  FakeTickSource tickSource;
  tickSource.maxRateHz = 0.0f;
  TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  float grantedHz = -1.0f;
  const Result result =
      backend.apply({.mode = PresentationRateMode::DisplayMaximum, .hz = 0.0f}, grantedHz);

  EXPECT_EQ(result.code, Result::Code::Unsupported);
  EXPECT_FALSE(result.message.empty());
  EXPECT_TRUE(tickSource.appliedHz.empty());
  // Left alone, so the seam has nothing to mistake for a grant.
  EXPECT_FLOAT_EQ(grantedHz, -1.0f);
}

TEST(TickSourceRateBackendTest, RefusesARateNoCadenceReaches) {
  FakeTickSource tickSource;
  TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  float grantedHz = -1.0f;
  const Result result =
      backend.apply({.mode = PresentationRateMode::Fixed, .hz = 1e-40f}, grantedHz);

  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_FALSE(result.message.empty());
  EXPECT_TRUE(tickSource.appliedHz.empty());
  EXPECT_FLOAT_EQ(grantedHz, -1.0f);
}

TEST(TickSourceRateBackendTest, HandsTheApplierTheSnapshotTheCadenceWasDerivedFrom) {
  FakeTickSource tickSource;
  TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  float grantedHz = 0.0f;
  ASSERT_TRUE(backend.apply({.mode = PresentationRateMode::Fixed, .hz = 30.0f}, grantedHz).isOk());

  // One reading per request, and the applier saw that same reading. A leg that re-queried
  // could resolve the request against a display that changed between the two calls.
  EXPECT_EQ(tickSource.maxRateQueryCount, 1);
  ASSERT_EQ(tickSource.appliedMaxRateHz.size(), 1u);
  EXPECT_FLOAT_EQ(tickSource.appliedMaxRateHz.front(), 120.0f);
  EXPECT_FLOAT_EQ(tickSource.appliedHz.front(), grantedHz);
}

TEST(TickSourceRateBackendTest, ReportsAnApplierRefusalVerbatimAndRecordsNoGrant) {
  FakeTickSource tickSource;
  TickSourceRateBackend backend(
      tickSource.provider(), [](float /*hz*/, float /*maxRateHz*/) -> Result {
        return Result{Result::Code::ArgumentOutOfRange, "This leg cannot represent that."};
      });

  float grantedHz = -1.0f;
  const Result result =
      backend.apply({.mode = PresentationRateMode::Fixed, .hz = 30.0f}, grantedHz);

  // The leg's own words, not a generic "tick source is gone" the caller cannot act on.
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(result.message, "This leg cannot represent that.");
  EXPECT_FLOAT_EQ(grantedHz, -1.0f);
}

TEST(TickSourceRateBackendTest, ReportsADeadTickSourceRatherThanGrantingARate) {
  FakeTickSource tickSource;
  tickSource.alive = false;
  TickSourceRateBackend backend(tickSource.provider(), tickSource.applier());

  float grantedHz = -1.0f;
  const Result result =
      backend.apply({.mode = PresentationRateMode::Fixed, .hz = 60.0f}, grantedHz);

  EXPECT_EQ(result.code, Result::Code::InvalidOperation);
  EXPECT_FALSE(result.message.empty());
  EXPECT_FLOAT_EQ(grantedHz, -1.0f);
}

// ---------------------------------------------------------------------------
// Through the seam, the way a platform installs it
// ---------------------------------------------------------------------------

TEST(TickSourceRateBackendTest, GrantsDisplayMaximumThroughTheController) {
  FakeTickSource tickSource;
  PresentationRateController controller;
  controller.setBackend(
      std::make_unique<TickSourceRateBackend>(tickSource.provider(), tickSource.applier()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_TRUE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  EXPECT_EQ(state.grant->mode, PresentationRateMode::DisplayMaximum);
  EXPECT_FLOAT_EQ(state.grant->hz, 120.0f);
}

TEST(TickSourceRateBackendTest, LetsTheControllerRefuseARateAboveTheDisplayMaximum) {
  FakeTickSource tickSource;
  PresentationRateController controller;
  controller.setBackend(
      std::make_unique<TickSourceRateBackend>(tickSource.provider(), tickSource.applier()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 240.0f});

  EXPECT_EQ(state.result.code, Result::Code::ArgumentOutOfRange);
  // The range check belongs to the seam, so the tick source is never touched.
  EXPECT_TRUE(tickSource.appliedHz.empty());
  EXPECT_FALSE(state.grant.has_value());
}

TEST(TickSourceRateBackendTest, LetsTheControllerCapADisplayThatOnlyReports60) {
  FakeTickSource tickSource;
  tickSource.maxRateHz = 60.0f;
  PresentationRateController controller;
  controller.setBackend(
      std::make_unique<TickSourceRateBackend>(tickSource.provider(), tickSource.applier()));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 30.0f});

  EXPECT_TRUE(state.result.isOk());
  ASSERT_TRUE(state.grant.has_value());
  EXPECT_EQ(state.grant->mode, PresentationRateMode::Fixed);
  EXPECT_FLOAT_EQ(state.grant->hz, 30.0f);
}

} // namespace
} // namespace igl::shell::tests
