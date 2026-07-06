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

  /// Get the label associated with a timing slot, if the backend records one.
  /// Returns a stable C string owned by the queries object (valid until reset()
  /// or destruction); empty string if the backend records no label.
  [[nodiscard]] virtual const char* getLabel(uint32_t /*slotIndex*/) const {
    return "";
  }

  /// Whether this queries object was successfully initialized and is usable.
  /// Returns true by default; backends override to return false if
  /// hardware/driver initialization failed silently.
  [[nodiscard]] virtual bool isValid() const {
    return true;
  }

  /// Read and clear the GPU disjoint flag. Returns true if a disjoint event
  /// (DVFS, context switch) occurred since the last read. Read-and-clear per spec.
  /// Default returns false; override in backends that support EXT_disjoint_timer_query.
  [[nodiscard]] virtual bool readAndClearDisjoint() {
    return false;
  }

  /// Whether the caller must keep framebuffers alive until this object's timer-query results are
  /// consumed. True only on backends whose driver internally references framebuffers that were
  /// active during a timer query: ARM Mali's OpenGL driver does this, so freeing an FBO before its
  /// query resolves is a use-after-free (SEV S638750). Metal, Vulkan, and other backends resolve
  /// counters without referencing the FBO afterward, so they return false. Default false; the
  /// OpenGL backend overrides to true. This lets timing collectors query the backend directly
  /// rather than have each construction site coordinate retention.
  [[nodiscard]] virtual bool requiresFramebufferRetention() const {
    return false;
  }

 protected:
  ITimestampQueries() = default;
};

} // namespace igl
