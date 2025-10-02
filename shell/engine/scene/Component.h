/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace engine {

class GameObject;

class Component {
 public:
  virtual ~Component() = default;

  virtual void update(float deltaTime) {}
  virtual void fixedUpdate(float fixedDeltaTime) {}

  void setOwner(GameObject* owner) {
    owner_ = owner;
  }

  GameObject* getOwner() const {
    return owner_;
  }

 protected:
  GameObject* owner_ = nullptr;
};

} // namespace engine
