/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "GameLoop.h"
#include "Engine.h"

namespace engine {

GameLoop::GameLoop(Engine* engine) : engine_(engine) {}

void GameLoop::run(float deltaTime) {
  if (!engine_) {
    return;
  }

  // Fixed timestep for physics
  accumulator_ += deltaTime;
  while (accumulator_ >= fixedTimeStep_) {
    engine_->fixedUpdate(fixedTimeStep_);
    accumulator_ -= fixedTimeStep_;
  }

  // Variable timestep for game logic and rendering
  engine_->update(deltaTime);
  engine_->render();
}

} // namespace engine
