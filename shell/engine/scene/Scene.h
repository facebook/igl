/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "GameObject.h"
#include "../graphics/Camera.h"
#include <memory>
#include <vector>

namespace engine {

class Scene {
 public:
  Scene() = default;
  virtual ~Scene() = default;

  virtual void initialize() {}
  virtual void shutdown() {}

  void update(float deltaTime) {
    for (auto& gameObject : gameObjects_) {
      gameObject->update(deltaTime);
    }
  }

  void fixedUpdate(float fixedDeltaTime) {
    for (auto& gameObject : gameObjects_) {
      gameObject->fixedUpdate(fixedDeltaTime);
    }
  }

  GameObject* createGameObject(const std::string& name = "") {
    auto gameObject = std::make_unique<GameObject>(name);
    GameObject* ptr = gameObject.get();
    gameObjects_.push_back(std::move(gameObject));
    return ptr;
  }

  const std::vector<std::unique_ptr<GameObject>>& getGameObjects() const {
    return gameObjects_;
  }

  void setMainCamera(Camera* camera) {
    mainCamera_ = camera;
  }

  Camera* getMainCamera() const {
    return mainCamera_;
  }

 private:
  std::vector<std::unique_ptr<GameObject>> gameObjects_;
  Camera* mainCamera_ = nullptr;
};

} // namespace engine
