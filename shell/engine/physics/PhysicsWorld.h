/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace engine {

class PhysicsWorld {
 public:
  PhysicsWorld() = default;

  void step(float deltaTime) {
    // TODO: Implement physics simulation
  }

  void setGravity(float gravity) {
    gravity_ = gravity;
  }

  float getGravity() const {
    return gravity_;
  }

 private:
  float gravity_ = -9.81f;
};

} // namespace engine
