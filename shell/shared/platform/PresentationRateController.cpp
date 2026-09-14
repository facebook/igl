/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/PresentationRateController.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace igl::shell {

namespace {

std::string formatHz(float hz) {
  // Six significant digits, so 59.94 stays distinguishable from 60 while whole rates
  // still print as "60" rather than "60.000000". %g also degrades to exponent form for
  // an absurd value instead of overflowing the way a fixed-point conversion would.
  std::array<char, 32> buffer{};
  const int written = std::snprintf(buffer.data(), buffer.size(), "%.6g", static_cast<double>(hz));
  if (written <= 0 || static_cast<size_t>(written) >= buffer.size()) {
    return "an unrepresentable rate";
  }
  return std::string(buffer.data(), static_cast<size_t>(written)) + " Hz";
}

} // namespace

PresentationRateController::Backend::~Backend() = default;

void PresentationRateController::setBackend(std::unique_ptr<Backend> backend) {
  backend_ = std::move(backend);
  // Whatever rate was in effect belonged to the outgoing backend, so it is gone with it.
  state_.grant.reset();
  if (hasRequest_) {
    // Re-issue through the normal path so validation runs against the new backend's
    // capabilities rather than the previous one's. Copy first: requestRate() assigns to
    // state_.requested, which is the very object we would otherwise pass by reference.
    const PresentationRateRequest pending = state_.requested;
    requestRate(pending);
  }
}

PresentationRateCapabilities PresentationRateController::getCapabilities() const noexcept {
  if (!backend_) {
    return {};
  }
  return backend_->getCapabilities();
}

const PresentationRateState& PresentationRateController::requestRate(
    const PresentationRateRequest& request) {
  state_.requested = request;
  hasRequest_ = true;
  // state_.grant is deliberately left alone on every refusal path below: a request the
  // backend declined does not undo the rate the display is already running at. The one
  // exception is the malformed-success path at the bottom.

  if (!std::isfinite(request.hz)) {
    state_.result = Result{Result::Code::ArgumentOutOfRange,
                           "A presentation-rate request must carry a finite rate, even in a "
                           "mode that ignores it."};
    return state_;
  }

  if (!backend_) {
    state_.result = Result{Result::Code::Unsupported,
                           "No presentation-rate backend is installed for this platform; the "
                           "rate stays whatever the display gives us."};
    return state_;
  }

  const PresentationRateCapabilities capabilities = backend_->getCapabilities();

  if (request.mode == PresentationRateMode::Fixed) {
    if (request.hz <= 0.0f) {
      state_.result =
          Result{Result::Code::ArgumentOutOfRange, "A fixed presentation rate must be above zero."};
      return state_;
    }
    if (!capabilities.canSetFixedRate) {
      state_.result =
          Result{Result::Code::Unsupported,
                 "This platform cannot hold presentation below the display's refresh rate."};
      return state_;
    }
    if (capabilities.maxRefreshRateHz > 0.0f && request.hz > capabilities.maxRefreshRateHz) {
      state_.result =
          Result{Result::Code::ArgumentOutOfRange,
                 "Requested " + formatHz(request.hz) + ", but the display tops out at " +
                     formatHz(capabilities.maxRefreshRateHz) + "."};
      return state_;
    }
    if (capabilities.minRefreshRateHz > 0.0f && request.hz < capabilities.minRefreshRateHz) {
      state_.result =
          Result{Result::Code::ArgumentOutOfRange,
                 "Requested " + formatHz(request.hz) + ", but the display cannot be driven below " +
                     formatHz(capabilities.minRefreshRateHz) + "."};
      return state_;
    }
  }

  float grantedHz = 0.0f;
  state_.result = backend_->apply(request, grantedHz);
  if (!state_.result.isOk()) {
    return state_;
  }

  // Negated rather than `<= 0.0f` so a NaN granted rate is caught too.
  if (!std::isfinite(grantedHz) || !(grantedHz > 0.0f)) {
    // The backend claims it applied the request, so whatever was presenting before is no
    // longer what is presenting — but it never said what replaced it. An earlier grant is
    // now stale, and reporting it would be worse than reporting nothing at all.
    state_.grant.reset();
    state_.result = Result{Result::Code::RuntimeError,
                           "The presentation-rate backend reported success without naming a "
                           "finite rate above zero."};
    return state_;
  }
  state_.grant = PresentationRateGrant{.mode = request.mode, .hz = grantedHz};
  return state_;
}

const PresentationRateState& PresentationRateController::getState() const noexcept {
  return state_;
}

} // namespace igl::shell
