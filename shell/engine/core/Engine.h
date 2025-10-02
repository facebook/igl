/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/Device.h>

namespace igl::shell {
class Platform;
} // namespace igl::shell

namespace engine {

class Scene;
class Renderer;
class InputManager;
class ResourceManager;
class PhysicsWorld;

class Engine {
 public:
  explicit Engine(std::shared_ptr<igl::shell::Platform> platform);
  ~Engine();

  // Initialization
  void initialize();
  void shutdown();

  // Main loop
  void update(float deltaTime);
  void fixedUpdate(float fixedDeltaTime);
  void render();

  // Accessors
  igl::IDevice& getDevice() const;
  Scene* getActiveScene() const;
  Renderer* getRenderer() const;
  InputManager* getInputManager() const;
  ResourceManager* getResourceManager() const;
  PhysicsWorld* getPhysicsWorld() const;

  // Scene management
  void loadScene(std::unique_ptr<Scene> scene);

 private:
  std::shared_ptr<igl::shell::Platform> platform_;
  std::unique_ptr<Scene> activeScene_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<InputManager> inputManager_;
  std::unique_ptr<ResourceManager> resourceManager_;
  std::unique_ptr<PhysicsWorld> physicsWorld_;
};

} // namespace engine
