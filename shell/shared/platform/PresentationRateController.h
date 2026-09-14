/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <igl/Common.h>

namespace igl::shell {

/// How a presentation-rate request names its target.
enum class PresentationRateMode : uint8_t {
  /// The highest refresh rate the display reports.
  DisplayMaximum,
  /// The rate named by PresentationRateRequest::hz.
  Fixed,
};

/// A request to change the rate at which frames are presented.
///
/// The default names the display's own maximum rather than inventing a rate,
/// so a default-constructed request is inert: with no backend installed the
/// controller refuses it (see below), and a backend can only ever grant a
/// rate the display reports. A failure therefore never produces a rate
/// nobody asked the display for.
struct PresentationRateRequest {
  PresentationRateMode mode = PresentationRateMode::DisplayMaximum;
  /// Target rate in Hz. Only read when `mode` is PresentationRateMode::Fixed, and then
  /// it must be finite and above zero. It must be finite whatever the mode, because a
  /// NaN or infinity here is a caller bug worth reporting rather than ignoring.
  float hz = 0.0f;
};

/// What the display and the windowing backend can actually do.
struct PresentationRateCapabilities {
  /// Highest refresh rate the display reports, in Hz. Zero when the backend cannot
  /// report one; a zero here disables the controller's range check.
  float maxRefreshRateHz = 0.0f;
  /// Lowest rate the display can be driven at, in Hz. Zero when unknown.
  float minRefreshRateHz = 0.0f;
  /// Whether the backend can hold presentation below `maxRefreshRateHz`.
  bool canSetFixedRate = false;
};

/// A presentation rate a platform actually put into effect.
struct PresentationRateGrant {
  /// The mode that was granted. Reported back so a caller can tell a granted
  /// DisplayMaximum from a Fixed request that happened to land on the same rate.
  PresentationRateMode mode = PresentationRateMode::DisplayMaximum;
  /// The rate now in effect, in Hz. Always finite and above zero — a grant that could
  /// not name one is reported as a backend error instead of being recorded.
  float hz = 0.0f;
};

/// The outcome of the most recent request, plus what is presenting right now.
struct PresentationRateState {
  /// What was asked for. Recorded even when the request was refused, and left at its
  /// default until the first request.
  PresentationRateRequest requested;
  /// Ok means the backend applied `requested`. Any other code carries a message naming
  /// what happened instead. Before the first request this is
  /// Result::Code::InvalidOperation, so "nothing has been asked for yet" never reads as
  /// success.
  Result result =
      Result{Result::Code::InvalidOperation, "No presentation rate has been requested yet."};
  /// What is presenting right now, or nothing when there is no rate anyone can vouch
  /// for. A refused request leaves an earlier grant untouched — the backend said it
  /// changed nothing, so the display keeps doing what it was already doing. Two things
  /// do clear it: dropping the backend, since whatever was applying that rate is gone,
  /// and a backend that reports success without naming a usable rate, since it claims
  /// to have replaced the old rate but never said with what.
  std::optional<PresentationRateGrant> grant;
};

/// Platform-agnostic half of the presentation-rate seam: validates requests against
/// what the display reports, remembers what was asked for and what came back, and
/// delegates the one windowing-system-specific step to a Backend.
///
/// With no backend installed every request is refused with Result::Code::Unsupported
/// and an explanation, so a platform that has no leg yet reports that instead of
/// appearing to have changed rate.
class PresentationRateController {
 public:
  /// The windowing-system-specific leg of the seam: one implementation per
  /// display-link or swapchain owner.
  class Backend {
   public:
    Backend() noexcept = default;
    virtual ~Backend();
    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;
    Backend(Backend&&) = delete;
    Backend& operator=(Backend&&) = delete;

    [[nodiscard]] virtual PresentationRateCapabilities getCapabilities() const noexcept = 0;

    /// Applies `request`. On Ok, `outGrantedHz` must be set to the finite, above-zero
    /// rate now in effect; leaving it unset, or setting it to zero, a negative, a NaN or
    /// an infinity, is reported back as a backend error and drops the recorded grant. On
    /// any other code the message must name what the backend did instead, and the rate
    /// already in effect is assumed unchanged.
    virtual Result apply(const PresentationRateRequest& request, float& outGrantedHz) = 0;
  };

  /// Installs, or with nullptr removes, the platform leg. Any existing grant is dropped
  /// first — it belonged to the outgoing backend. A request made before a backend
  /// existed is then re-applied, so a caller that picks a rate during session startup
  /// does not lose it to window-creation ordering.
  void setBackend(std::unique_ptr<Backend> backend);

  /// What the installed backend reports. All-zero and all-false when there is none.
  [[nodiscard]] PresentationRateCapabilities getCapabilities() const noexcept;

  /// Asks for `request` and returns the resulting state.
  const PresentationRateState& requestRate(const PresentationRateRequest& request);

  /// The outcome of the most recent request and the rate in effect, for callers that
  /// report requested-versus-granted without re-issuing anything.
  [[nodiscard]] const PresentationRateState& getState() const noexcept;

 private:
  std::unique_ptr<Backend> backend_;
  PresentationRateState state_;
  bool hasRequest_ = false;
};

} // namespace igl::shell
