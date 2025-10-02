/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Engine.h"
#include <shell/shared/platform/Platform.h>
#include "../scene/Scene.h"
#include "../graphics/Renderer.h"
#include "../core/InputManager.h"
#include "../resources/ResourceManager.h"
#include "../physics/PhysicsWorld.h"

namespace engine {

Engine::Engine(std::shared_ptr<igl::shell::Platform> platform) : platform_(std::move(platform)) {}

Engine::~Engine() {
  shutdown();
}

void Engine::initialize() {
  // Initialize subsystems in correct order
  resourceManager_ = std::make_unique<ResourceManager>(platform_);
  renderer_ = std::make_unique<Renderer>(platform_);
  inputManager_ = std::make_unique<InputManager>();
  physicsWorld_ = std::make_unique<PhysicsWorld>();
}

void Engine::shutdown() {
  activeScene_.reset();
  physicsWorld_.reset();
  inputManager_.reset();
  renderer_.reset();
  resourceManager_.reset();
}

void Engine::update(float deltaTime) {
  if (inputManager_) {
    inputManager_->update();
  }

  if (activeScene_) {
    activeScene_->update(deltaTime);
  }
}

void Engine::fixedUpdate(float fixedDeltaTime) {
  if (physicsWorld_) {
    physicsWorld_->step(fixedDeltaTime);
  }

  if (activeScene_) {
    activeScene_->fixedUpdate(fixedDeltaTime);
  }
}

void Engine::render() {
  if (renderer_ && activeScene_) {
    renderer_->renderScene(*activeScene_);
  }
}

igl::IDevice& Engine::getDevice() const {
  return platform_->getDevice();
}

Scene* Engine::getActiveScene() const {
  return activeScene_.get();
}

Renderer* Engine::getRenderer() const {
  return renderer_.get();
}

InputManager* Engine::getInputManager() const {
  return inputManager_.get();
}

ResourceManager* Engine::getResourceManager() const {
  return resourceManager_.get();
}

PhysicsWorld* Engine::getPhysicsWorld() const {
  return physicsWorld_.get();
}

void Engine::loadScene(std::unique_ptr<Scene> scene) {
  activeScene_ = std::move(scene);
  if (activeScene_) {
    activeScene_->initialize();
  }
}

} // namespace engine
