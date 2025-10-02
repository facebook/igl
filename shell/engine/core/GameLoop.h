/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>

namespace engine {

class Engine;

class GameLoop {
 public:
  explicit GameLoop(Engine* engine);

  void run(float deltaTime);

  void setFixedTimeStep(float timeStep) {
    fixedTimeStep_ = timeStep;
  }

  float getFixedTimeStep() const {
    return fixedTimeStep_;
  }

 private:
  Engine* engine_ = nullptr;
  float fixedTimeStep_ = 1.0f / 60.0f; // 60 FPS physics update
  float accumulator_ = 0.0f;
};

} // namespace engine
