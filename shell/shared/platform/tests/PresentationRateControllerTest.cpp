/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <shell/shared/platform/PresentationRateController.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace igl::shell::tests {
namespace {

class FakeBackend : public PresentationRateController::Backend {
 public:
  [[nodiscard]] PresentationRateCapabilities getCapabilities() const noexcept override {
    return capabilities;
  }

  Result apply(const PresentationRateRequest& request, float& outGrantedHz) override {
    ++applyCount;
    lastRequest = request;
    outGrantedHz = grantHz;
    return applyResult;
  }

  PresentationRateCapabilities capabilities;
  Result applyResult;
  float grantHz = 0.0f;
  int applyCount = 0;
  PresentationRateRequest lastRequest;
};

/// A display that reports 30-120 Hz and can be capped — the shape of a modern phone
/// panel.
PresentationRateCapabilities pacedDisplay() {
  return {
      .maxRefreshRateHz = 120.0f,
      .minRefreshRateHz = 30.0f,
      .canSetFixedRate = true,
  };
}

FakeBackend& install(PresentationRateController& controller,
                     const PresentationRateCapabilities& capabilities,
                     float grantHz = 120.0f) {
  auto backend = std::make_unique<FakeBackend>();
  backend->capabilities = capabilities;
  backend->grantHz = grantHz;
  auto& installed = *backend;
  controller.setBackend(std::move(backend));
  return installed;
}

/// Grants 60 Hz through a paced display, leaving the backend ready for a second request.
FakeBackend& installAndGrant60(PresentationRateController& controller) {
  FakeBackend& backend = install(controller, pacedDisplay(), 60.0f);
  const PresentationRateState& granted =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});
  EXPECT_TRUE(granted.result.isOk());
  return backend;
}

/// Both fields of the grant, so a test cannot pass by checking only the rate.
void expectGrant(const PresentationRateState& state, PresentationRateMode mode, float hz) {
  ASSERT_TRUE(state.grant.has_value());
  EXPECT_EQ(state.grant->mode, mode);
  EXPECT_FLOAT_EQ(state.grant->hz, hz);
}

// ---------------------------------------------------------------------------
// No backend installed
// ---------------------------------------------------------------------------

TEST(PresentationRateControllerTest, StartsWithNoCapabilitiesNoGrantAndANonOkResult) {
  const PresentationRateController controller;

  const PresentationRateCapabilities capabilities = controller.getCapabilities();
  EXPECT_FLOAT_EQ(capabilities.maxRefreshRateHz, 0.0f);
  EXPECT_FLOAT_EQ(capabilities.minRefreshRateHz, 0.0f);
  EXPECT_FALSE(capabilities.canSetFixedRate);

  // Nothing has been asked for yet, so the result must not read as a success.
  EXPECT_EQ(controller.getState().result.code, Result::Code::InvalidOperation);
  EXPECT_FALSE(controller.getState().result.message.empty());
  EXPECT_FALSE(controller.getState().grant.has_value());
}

TEST(PresentationRateControllerTest, RefusesRequestWithNoBackendAndRecordsWhatWasAsked) {
  PresentationRateController controller;

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});

  EXPECT_EQ(state.result.code, Result::Code::Unsupported);
  EXPECT_FALSE(state.result.message.empty());
  EXPECT_FALSE(state.grant.has_value());
  EXPECT_EQ(state.requested.mode, PresentationRateMode::Fixed);
  EXPECT_FLOAT_EQ(state.requested.hz, 60.0f);
}

// ---------------------------------------------------------------------------
// Requests the backend can honor
// ---------------------------------------------------------------------------

TEST(PresentationRateControllerTest, GrantsDisplayMaximum) {
  PresentationRateController controller;
  FakeBackend& backend = install(controller, pacedDisplay(), 120.0f);

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_TRUE(state.result.isOk());
  EXPECT_EQ(backend.applyCount, 1);
  EXPECT_EQ(backend.lastRequest.mode, PresentationRateMode::DisplayMaximum);
  expectGrant(state, PresentationRateMode::DisplayMaximum, 120.0f);
}

TEST(PresentationRateControllerTest, GrantsFixedRateInsideTheDisplayRange) {
  PresentationRateController controller;
  FakeBackend& backend = install(controller, pacedDisplay(), 60.0f);

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});

  EXPECT_TRUE(state.result.isOk());
  EXPECT_EQ(backend.applyCount, 1);
  EXPECT_EQ(backend.lastRequest.mode, PresentationRateMode::Fixed);
  EXPECT_FLOAT_EQ(backend.lastRequest.hz, 60.0f);
  expectGrant(state, PresentationRateMode::Fixed, 60.0f);
}

TEST(PresentationRateControllerTest, SkipsTheRangeCheckWhenTheDisplayRangeIsUnknown) {
  PresentationRateController controller;
  FakeBackend& backend =
      install(controller,
              {.maxRefreshRateHz = 0.0f, .minRefreshRateHz = 0.0f, .canSetFixedRate = true},
              240.0f);

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 240.0f});

  EXPECT_TRUE(state.result.isOk());
  EXPECT_EQ(backend.applyCount, 1);
  EXPECT_EQ(backend.lastRequest.mode, PresentationRateMode::Fixed);
  EXPECT_FLOAT_EQ(backend.lastRequest.hz, 240.0f);
  expectGrant(state, PresentationRateMode::Fixed, 240.0f);
}

// ---------------------------------------------------------------------------
// Requests refused before they reach the backend
// ---------------------------------------------------------------------------

TEST(PresentationRateControllerTest, RejectsFixedRateAtOrBelowZero) {
  const float badRates[] = {0.0f, -60.0f};

  for (const float hz : badRates) {
    PresentationRateController controller;
    FakeBackend& backend = install(controller, pacedDisplay());

    const PresentationRateState& state =
        controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = hz});

    EXPECT_EQ(state.result.code, Result::Code::ArgumentOutOfRange);
    EXPECT_FALSE(state.grant.has_value());
    EXPECT_EQ(backend.applyCount, 0);
  }
}

TEST(PresentationRateControllerTest, RejectsANonFiniteRequestedRateWhateverTheMode) {
  const float badRates[] = {std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity()};
  const PresentationRateMode modes[] = {PresentationRateMode::DisplayMaximum,
                                        PresentationRateMode::Fixed};

  for (const PresentationRateMode mode : modes) {
    for (const float hz : badRates) {
      PresentationRateController controller;
      FakeBackend& backend = install(controller, pacedDisplay());

      const PresentationRateState& state = controller.requestRate({.mode = mode, .hz = hz});

      EXPECT_EQ(state.result.code, Result::Code::ArgumentOutOfRange);
      EXPECT_FALSE(state.grant.has_value());
      EXPECT_EQ(backend.applyCount, 0);
    }
  }
}

TEST(PresentationRateControllerTest, RejectsFixedRateAboveTheDisplayMaximumAndNamesBothRates) {
  PresentationRateController controller;
  FakeBackend& backend = install(controller, pacedDisplay());

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 240.0f});

  EXPECT_EQ(state.result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_NE(state.result.message.find("240 Hz"), std::string::npos);
  EXPECT_NE(state.result.message.find("120 Hz"), std::string::npos);
  EXPECT_EQ(backend.applyCount, 0);
}

TEST(PresentationRateControllerTest, RejectsFixedRateBelowTheDisplayMinimum) {
  PresentationRateController controller;
  FakeBackend& backend = install(controller, pacedDisplay());

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 10.0f});

  EXPECT_EQ(state.result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_NE(state.result.message.find("30 Hz"), std::string::npos);
  EXPECT_EQ(backend.applyCount, 0);
}

TEST(PresentationRateControllerTest, KeepsFractionalRatesDistinctInRefusalMessages) {
  PresentationRateController controller;
  PresentationRateCapabilities capabilities = pacedDisplay();
  // An NTSC-rate panel: rounding this to a whole number would make the refusal read
  // "requested 60 Hz, but the display tops out at 60 Hz".
  capabilities.maxRefreshRateHz = 59.94f;
  install(controller, capabilities);

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});

  EXPECT_EQ(state.result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_NE(state.result.message.find("60 Hz"), std::string::npos);
  EXPECT_NE(state.result.message.find("59.94 Hz"), std::string::npos);
}

TEST(PresentationRateControllerTest, RejectsFixedRateWhenTheBackendCannotCap) {
  PresentationRateController controller;
  PresentationRateCapabilities capabilities = pacedDisplay();
  capabilities.canSetFixedRate = false;
  FakeBackend& backend = install(controller, capabilities);

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});

  EXPECT_EQ(state.result.code, Result::Code::Unsupported);
  EXPECT_FALSE(state.result.message.empty());
  EXPECT_EQ(backend.applyCount, 0);
}

// ---------------------------------------------------------------------------
// Backend-reported failures
// ---------------------------------------------------------------------------

TEST(PresentationRateControllerTest, ReportsABackendFailureVerbatim) {
  PresentationRateController controller;
  FakeBackend& backend = install(controller, pacedDisplay());
  backend.applyResult = Result{Result::Code::RuntimeError, "display link refused the range"};
  backend.grantHz = 60.0f;

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_EQ(state.result.code, Result::Code::RuntimeError);
  EXPECT_EQ(state.result.message, "display link refused the range");
  EXPECT_FALSE(state.grant.has_value());
}

TEST(PresentationRateControllerTest, TreatsSuccessWithoutAGrantedRateAsABackendError) {
  PresentationRateController controller;
  FakeBackend& backend = install(controller, pacedDisplay());
  backend.grantHz = 0.0f;

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_EQ(state.result.code, Result::Code::RuntimeError);
  EXPECT_FALSE(state.result.message.empty());
  EXPECT_FALSE(state.grant.has_value());
}

TEST(PresentationRateControllerTest, TreatsAnUnusableGrantedRateAsABackendError) {
  const float badGrants[] = {
      std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), -30.0f};

  for (const float grantHz : badGrants) {
    PresentationRateController controller;
    FakeBackend& backend = install(controller, pacedDisplay());
    backend.grantHz = grantHz;

    const PresentationRateState& state =
        controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

    EXPECT_EQ(state.result.code, Result::Code::RuntimeError);
    EXPECT_FALSE(state.grant.has_value());
  }
}

// ---------------------------------------------------------------------------
// What a second request does to the rate already in effect
// ---------------------------------------------------------------------------

TEST(PresentationRateControllerTest, KeepsTheGrantWhenALaterRequestIsRefusedBeforeTheBackend) {
  PresentationRateController controller;
  installAndGrant60(controller);

  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 240.0f});

  EXPECT_EQ(state.result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(state.requested.mode, PresentationRateMode::Fixed);
  EXPECT_FLOAT_EQ(state.requested.hz, 240.0f);
  // The display is still running at 60; reporting nothing here would be a lie.
  expectGrant(state, PresentationRateMode::Fixed, 60.0f);
}

TEST(PresentationRateControllerTest, KeepsTheGrantWhenTheBackendRefusesALaterRequest) {
  PresentationRateController controller;
  FakeBackend& backend = installAndGrant60(controller);

  // Both fields of the failed call differ from the standing grant, so writing state from
  // it would be visible in either one.
  backend.applyResult = Result{Result::Code::RuntimeError, "display link refused the range"};
  backend.grantHz = 90.0f;
  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_EQ(state.result.code, Result::Code::RuntimeError);
  EXPECT_EQ(state.requested.mode, PresentationRateMode::DisplayMaximum);
  expectGrant(state, PresentationRateMode::Fixed, 60.0f);
}

TEST(PresentationRateControllerTest, ClearsTheGrantWhenTheBackendClaimsSuccessWithoutAUsableRate) {
  PresentationRateController controller;
  FakeBackend& backend = installAndGrant60(controller);

  // Ok means the backend replaced the standing rate, but it never said with what, so the
  // old 60 Hz is stale and must not be reported as current.
  backend.grantHz = 0.0f;
  const PresentationRateState& state =
      controller.requestRate({.mode = PresentationRateMode::DisplayMaximum});

  EXPECT_EQ(state.result.code, Result::Code::RuntimeError);
  EXPECT_FALSE(state.grant.has_value());
}

// ---------------------------------------------------------------------------
// Backend installation and removal
// ---------------------------------------------------------------------------

TEST(PresentationRateControllerTest, ReappliesAPendingRequestWhenABackendArrives) {
  PresentationRateController controller;
  const PresentationRateState& refused =
      controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 60.0f});
  ASSERT_EQ(refused.result.code, Result::Code::Unsupported);

  FakeBackend& backend = install(controller, pacedDisplay(), 60.0f);

  EXPECT_EQ(backend.applyCount, 1);
  EXPECT_EQ(backend.lastRequest.mode, PresentationRateMode::Fixed);
  EXPECT_FLOAT_EQ(backend.lastRequest.hz, 60.0f);
  EXPECT_TRUE(controller.getState().result.isOk());
  expectGrant(controller.getState(), PresentationRateMode::Fixed, 60.0f);
}

TEST(PresentationRateControllerTest, DoesNotApplyAnythingWhenNoRequestWasMade) {
  PresentationRateController controller;
  FakeBackend& backend = install(controller, pacedDisplay());

  EXPECT_EQ(backend.applyCount, 0);
  EXPECT_EQ(controller.getState().result.code, Result::Code::InvalidOperation);
  EXPECT_FALSE(controller.getState().grant.has_value());
}

TEST(PresentationRateControllerTest, DropsTheGrantAndRefusesAgainAfterTheBackendIsRemoved) {
  PresentationRateController controller;
  installAndGrant60(controller);

  controller.setBackend(nullptr);

  EXPECT_FLOAT_EQ(controller.getCapabilities().maxRefreshRateHz, 0.0f);
  EXPECT_EQ(controller.getState().result.code, Result::Code::Unsupported);
  // The backend that was holding 60 Hz is gone, so there is no grant to report.
  EXPECT_FALSE(controller.getState().grant.has_value());
}

TEST(PresentationRateControllerTest, GetStateMirrorsTheMostRecentRequest) {
  PresentationRateController controller;
  installAndGrant60(controller);

  controller.requestRate({.mode = PresentationRateMode::Fixed, .hz = 240.0f});

  EXPECT_EQ(controller.getState().requested.mode, PresentationRateMode::Fixed);
  EXPECT_FLOAT_EQ(controller.getState().requested.hz, 240.0f);
  EXPECT_EQ(controller.getState().result.code, Result::Code::ArgumentOutOfRange);
  // Refused, so the earlier 60 Hz grant is still what is presenting.
  expectGrant(controller.getState(), PresentationRateMode::Fixed, 60.0f);
}

TEST(PresentationRateControllerTest, GetCapabilitiesReportsWhatTheBackendReports) {
  PresentationRateController controller;
  install(controller, pacedDisplay());

  const PresentationRateCapabilities capabilities = controller.getCapabilities();
  EXPECT_FLOAT_EQ(capabilities.maxRefreshRateHz, 120.0f);
  EXPECT_FLOAT_EQ(capabilities.minRefreshRateHz, 30.0f);
  EXPECT_TRUE(capabilities.canSetFixedRate);
}

} // namespace
} // namespace igl::shell::tests
