/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/ITrackedResource.h>

namespace igl {

class ITimer : public ITrackedResource<ITimer> {
 public:
  ~ITimer() override = default;

  /**
   * If results are available, returns the measured time in nanoseconds. Otherwise, returns 0.
   */
  [[nodiscard]] virtual uint64_t getElapsedTimeNanos() const = 0;

  /**
   * Returns true if the timer results are available. Results should be available immediately after
   * calling ICommandBuffer::waitUntilCompleted(), but if you do not wait for the command buffer,
   * it may take a frame or two for the results to become available.
   */
  [[nodiscard]] virtual bool resultsAvailable() const = 0;

 protected:
  ITimer() = default;
};

} // namespace igl
