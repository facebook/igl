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

#if IGL_PLATFORM_MACOSX
#include <shell/shared/platform/mac/DisplayLinkRatePacer.h>
#endif

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

#if IGL_PLATFORM_MACOSX

// ---------------------------------------------------------------------------
// DisplayLinkRatePacer: the macOS CVDisplayLink leg, which caps by dropping
// callbacks because the link itself has no rate to set
// ---------------------------------------------------------------------------

/// Renders over `ticks` callbacks, as a string of 'R' and '.' so a wrong cadence reads as
/// a wrong pattern rather than a wrong count.
std::string renderPattern(DisplayLinkRatePacer& pacer, int ticks) {
  std::string pattern;
  pattern.reserve(static_cast<size_t>(ticks));
  for (int tick = 0; tick < ticks; ++tick) {
    pattern.push_back(pacer.shouldRender() ? 'R' : '.');
  }
  return pattern;
}

TEST(DisplayLinkRatePacerTest, RendersEveryRefreshUntilItIsToldOtherwise) {
  DisplayLinkRatePacer pacer;
  EXPECT_EQ(pacer.getRefreshesPerFrame(), 1);
  EXPECT_EQ(renderPattern(pacer, 6), "RRRRRR");
}

TEST(DisplayLinkRatePacerTest, RendersTheFirstCallbackOfEveryGroup) {
  DisplayLinkRatePacer pacer;
  pacer.setRefreshesPerFrame(2);
  EXPECT_EQ(renderPattern(pacer, 6), "R.R.R.");

  DisplayLinkRatePacer quarterRate;
  quarterRate.setRefreshesPerFrame(4);
  EXPECT_EQ(renderPattern(quarterRate, 9), "R...R...R");
}

TEST(DisplayLinkRatePacerTest, TreatsADivisorBelowOneAsRenderEveryRefresh) {
  DisplayLinkRatePacer pacer;
  pacer.setRefreshesPerFrame(0);
  EXPECT_EQ(pacer.getRefreshesPerFrame(), 1);
  pacer.setRefreshesPerFrame(-4);
  EXPECT_EQ(pacer.getRefreshesPerFrame(), 1);
  EXPECT_EQ(renderPattern(pacer, 4), "RRRR");
}

TEST(DisplayLinkRatePacerTest, PicksUpASmallerDivisorOnTheNextCallback) {
  DisplayLinkRatePacer pacer;
  pacer.setRefreshesPerFrame(4);
  EXPECT_EQ(renderPattern(pacer, 2), "R.");
  // Mid-group. A count-up-and-reset would stall here until the old count was reached.
  pacer.setRefreshesPerFrame(1);
  EXPECT_EQ(renderPattern(pacer, 3), "RRR");
}

TEST(DisplayLinkRatePacerTest, ResolvesRungsToWholeNumbersOfSkippedRefreshes) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  auto backend = createDisplayLinkPacerBackend(pacer, []() { return 120.0f; });

  float grantedHz = 0.0f;
  EXPECT_TRUE(backend->apply({.mode = PresentationRateMode::DisplayMaximum}, grantedHz).isOk());
  EXPECT_FLOAT_EQ(grantedHz, 120.0f);
  EXPECT_EQ(pacer->getRefreshesPerFrame(), 1);

  EXPECT_TRUE(backend->apply({.mode = PresentationRateMode::Fixed, .hz = 60.0f}, grantedHz).isOk());
  EXPECT_FLOAT_EQ(grantedHz, 60.0f);
  EXPECT_EQ(pacer->getRefreshesPerFrame(), 2);

  EXPECT_TRUE(backend->apply({.mode = PresentationRateMode::Fixed, .hz = 30.0f}, grantedHz).isOk());
  EXPECT_FLOAT_EQ(grantedHz, 30.0f);
  EXPECT_EQ(pacer->getRefreshesPerFrame(), 4);
}

TEST(DisplayLinkRatePacerTest, ReportsTheLinkRateAsTheCapability) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  const auto backend = createDisplayLinkPacerBackend(pacer, []() { return 60.0f; });

  const PresentationRateCapabilities capabilities = backend->getCapabilities();

  EXPECT_FLOAT_EQ(capabilities.maxRefreshRateHz, 60.0f);
  EXPECT_TRUE(capabilities.canSetFixedRate);
}

TEST(DisplayLinkRatePacerTest, RefusesOnceTheViewThatOwnedTheLinkIsGone) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  auto backend = createDisplayLinkPacerBackend(pacer, []() { return 120.0f; });
  // What the view's dealloc does, before it releases the CVDisplayLinkRef.
  pacer.reset();

  float grantedHz = -1.0f;
  const Result result =
      backend->apply({.mode = PresentationRateMode::Fixed, .hz = 60.0f}, grantedHz);

  EXPECT_EQ(result.code, Result::Code::InvalidOperation);
  EXPECT_FLOAT_EQ(grantedHz, -1.0f);
}

TEST(DisplayLinkRatePacerTest, RefusesADivisorLargerThanThePacerCanCount) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  // A 120 Hz link and a rung far below what 10000 skipped refreshes reaches.
  auto backend = createDisplayLinkPacerBackend(pacer, []() { return 120.0f; });

  float grantedHz = -1.0f;
  const Result result =
      backend->apply({.mode = PresentationRateMode::Fixed, .hz = 0.001f}, grantedHz);

  // Refused rather than clamped: a clamped divisor would run at a cadence that disagrees
  // with the rate the caller was told it was granted.
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_FALSE(result.message.empty());
  EXPECT_EQ(pacer->getRefreshesPerFrame(), 1);
  EXPECT_FLOAT_EQ(grantedHz, -1.0f);
}

TEST(DisplayLinkRatePacerTest, RefusesARatioTooLargeToConvertToADivisorAtAll) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  auto backend = createDisplayLinkPacerBackend(pacer, []() { return 120.0f; });

  // 120 / 1e-30 is a perfectly finite float and sits far beyond `long`. Rounding it to an
  // integer before range-checking it is undefined, and the unspecified result can land
  // inside the accepted range — an accepted cadence nobody asked for.
  float grantedHz = -1.0f;
  const Result result =
      backend->apply({.mode = PresentationRateMode::Fixed, .hz = 1e-30f}, grantedHz);

  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_FALSE(result.message.empty());
  EXPECT_EQ(pacer->getRefreshesPerFrame(), 1);
  EXPECT_FLOAT_EQ(grantedHz, -1.0f);
}

TEST(DisplayLinkRatePacerTest, AcceptsTheLargestDivisorItCanStillRepresent) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  auto backend = createDisplayLinkPacerBackend(pacer, []() { return 10000.0f; });

  // Exactly kMaxRefreshesPerFrame: the boundary belongs on the accepted side, so the
  // refusal above cannot be hiding an off-by-one that rejects a reachable rung.
  float grantedHz = 0.0f;
  const Result result =
      backend->apply({.mode = PresentationRateMode::Fixed, .hz = 1.0f}, grantedHz);

  EXPECT_TRUE(result.isOk());
  EXPECT_FLOAT_EQ(grantedHz, 1.0f);
  EXPECT_EQ(pacer->getRefreshesPerFrame(), DisplayLinkRatePacer::kMaxRefreshesPerFrame);
}

TEST(DisplayLinkRatePacerTest, DerivesTheDivisorFromTheSnapshotNotAFreshReading) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  int queryCount = 0;
  float linkRateHz = 120.0f;
  auto backend = createDisplayLinkPacerBackend(pacer, [&queryCount, &linkRateHz]() {
    ++queryCount;
    // Every reading after the first reports a different display, the way switching
    // monitors mid-request would.
    const float reported = linkRateHz;
    linkRateHz = 60.0f;
    return reported;
  });

  float grantedHz = 0.0f;
  ASSERT_TRUE(backend->apply({.mode = PresentationRateMode::Fixed, .hz = 30.0f}, grantedHz).isOk());

  EXPECT_EQ(queryCount, 1);
  EXPECT_FLOAT_EQ(grantedHz, 30.0f);
  // 120 / 30, from the snapshot that produced the grant. A second reading would have made
  // it 60 / 30 = 2, and the link would have run at 60 while the caller was told 30.
  EXPECT_EQ(pacer->getRefreshesPerFrame(), 4);
}

TEST(DisplayLinkRatePacerTest, RefusesWhenCoreVideoWillNotNameARate) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  auto backend = createDisplayLinkPacerBackend(pacer, []() { return 0.0f; });

  float grantedHz = -1.0f;
  const Result result = backend->apply({.mode = PresentationRateMode::DisplayMaximum}, grantedHz);

  EXPECT_EQ(result.code, Result::Code::Unsupported);
  EXPECT_EQ(pacer->getRefreshesPerFrame(), 1);
  EXPECT_FLOAT_EQ(grantedHz, -1.0f);
}

TEST(DisplayLinkRatePacerTest, DrivesTheCallbackCadenceEndToEndThroughTheController) {
  auto pacer = std::make_shared<DisplayLinkRatePacer>();
  PresentationRateController controller;
  controller.setBackend(createDisplayLinkPacerBackend(pacer, []() { return 60.0f; }));

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 30.0f});

  ASSERT_TRUE(state.grant.has_value());
  EXPECT_FLOAT_EQ(state.grant->hz, 30.0f);
  EXPECT_EQ(renderPattern(*pacer, 6), "R.R.R.");
}

#endif // IGL_PLATFORM_MACOSX

} // namespace
} // namespace igl::shell::tests
