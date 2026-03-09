/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/ITrackedResource.h>

namespace igl {

class ITimestampQueries : public ITrackedResource<ITimestampQueries> {
 public:
  ~ITimestampQueries() override = default;

  /// Maximum number of timing slots this object can hold
  [[nodiscard]] virtual uint32_t capacity() const = 0;

  /// Number of timing slots recorded so far
  [[nodiscard]] virtual uint32_t count() const = 0;

  /// Reset the counter to 0 for reuse (does not deallocate)
  virtual void reset() = 0;

  /// True if GPU has completed and all recorded results are readable
  [[nodiscard]] virtual bool resultsAvailable() const = 0;

  /// Get the elapsed GPU time in nanoseconds for a timing slot.
  /// A timing slot represents a single render pass measurement.
  /// Metal: computes delta from two sampleBufferAttachments samples (slot*2, slot*2+1).
  /// OpenGL: returns the GL_TIME_ELAPSED query result directly.
  /// Default returns 0; override in backends that support per-slot elapsed queries.
  [[nodiscard]] virtual uint64_t getElapsedNanos(uint32_t /*slotIndex*/) const {
    return 0;
  }

 protected:
  ITimestampQueries() = default;
};

} // namespace igl
