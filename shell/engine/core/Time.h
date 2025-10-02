/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>

namespace engine {

class Time {
 public:
  Time();

  void tick();

  float getDeltaTime() const {
    return deltaTime_;
  }

  float getTotalTime() const {
    return totalTime_;
  }

  void reset();

 private:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  TimePoint lastTime_;
  TimePoint startTime_;
  float deltaTime_ = 0.0f;
  float totalTime_ = 0.0f;
};

} // namespace engine
