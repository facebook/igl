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

  /// Maximum number of timestamps this object can hold
  [[nodiscard]] virtual uint32_t capacity() const = 0;

  /// Number of timestamps recorded so far
  [[nodiscard]] virtual uint32_t count() const = 0;

  /// Reset the counter to 0 for reuse (does not deallocate)
  virtual void reset() = 0;

  /// True if GPU has completed and all recorded timestamps are readable
  [[nodiscard]] virtual bool resultsAvailable() const = 0;

  /// Get the timestamp at the given index (nanoseconds, GPU clock)
  /// Precondition: resultsAvailable() == true && index < count()
  [[nodiscard]] virtual uint64_t getTimestampNanos(uint32_t index) const = 0;

 protected:
  ITimestampQueries() = default;
};

} // namespace igl
