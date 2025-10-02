/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Time.h"

namespace engine {

Time::Time() : lastTime_(Clock::now()), startTime_(Clock::now()) {}

void Time::tick() {
  auto currentTime = Clock::now();
  std::chrono::duration<float> elapsed = currentTime - lastTime_;
  deltaTime_ = elapsed.count();
  lastTime_ = currentTime;

  std::chrono::duration<float> total = currentTime - startTime_;
  totalTime_ = total.count();
}

void Time::reset() {
  startTime_ = Clock::now();
  lastTime_ = startTime_;
  deltaTime_ = 0.0f;
  totalTime_ = 0.0f;
}

} // namespace engine
