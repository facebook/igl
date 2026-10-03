/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief A timestamp query set and its asynchronous readback, shared by ITimer and
/// ITimestampQueries.
///
/// A command buffer that writes queries resolves them before it is finished (encodeResolve()) and
/// maps the readback buffer once it is submitted (mapAfterSubmit()). Results arrive through
/// WebGPU's event processing; poll() processes events and reports whether the last submitted
/// resolve has completed. Every resolve uses its own readback buffer, so a query set can be
/// resolved again while an earlier readback is still pending.
class TimestampQuerySet final {
 public:
  [[nodiscard]] static std::unique_ptr<TimestampQuerySet> create(WebGPUContext& ctx,
                                                                 uint32_t count,
                                                                 Result* IGL_NULLABLE outResult);
  ~TimestampQuerySet();

  TimestampQuerySet(const TimestampQuerySet&) = delete;
  TimestampQuerySet& operator=(const TimestampQuerySet&) = delete;
  TimestampQuerySet(TimestampQuerySet&&) = delete;
  TimestampQuerySet& operator=(TimestampQuerySet&&) = delete;

  [[nodiscard]] WGPUQuerySet IGL_NULLABLE getQuerySet() const noexcept {
    return querySet_.get();
  }
  [[nodiscard]] uint32_t getCount() const noexcept {
    return count_;
  }

  /// Encodes resolving queries [0, count) into a new readback buffer.
  void encodeResolve(WGPUCommandEncoder IGL_NONNULL encoder, uint32_t count);
  /// Maps the readback buffer of the last encodeResolve(); call after its command buffer was
  /// submitted.
  void mapAfterSubmit();
  /// Discards pending and completed results.
  void reset();
  /// Processes WebGPU events; true once the last mapped resolve has completed. A resolve whose
  /// mapping failed completes with no timestamps.
  [[nodiscard]] bool poll() const;
  /// Timestamps in nanoseconds of the last completed resolve.
  [[nodiscard]] std::span<const uint64_t> getTimestamps() const;

 private:
  struct Readback;

  TimestampQuerySet(WebGPUContext& ctx, uint32_t count, Handle<WGPUQuerySet> querySet);

  WebGPUContext& ctx_;
  const uint32_t count_;
  Handle<WGPUQuerySet> querySet_;
  Handle<WGPUBuffer> resolveBuffer_;
  std::shared_ptr<Readback> encoded_;
  std::shared_ptr<Readback> mapped_;
};

} // namespace igl::webgpu
