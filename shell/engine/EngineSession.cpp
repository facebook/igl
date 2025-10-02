/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "EngineSession.h"
#include "graphics/Renderer.h"
#include "core/InputManager.h"
#include <shell/shared/platform/Platform.h>
#include <shell/shared/input/InputDispatcher.h>

namespace engine {

EngineSession::EngineSession(std::shared_ptr<igl::shell::Platform> platform)
    : igl::shell::RenderSession(std::move(platform)) {}

void EngineSession::initialize() noexcept {
  // Initialize engine
  engine_ = std::make_unique<Engine>(platform());
  engine_->initialize();

  // Set up input
  auto& inputDispatcher = getPlatform().getInputDispatcher();
  inputDispatcher.addKeyListener(std::shared_ptr<igl::shell::IKeyListener>(
      engine_->getInputManager(), [](igl::shell::IKeyListener*) {}));
  inputDispatcher.addMouseListener(std::shared_ptr<igl::shell::IMouseListener>(
      engine_->getInputManager(), [](igl::shell::IMouseListener*) {}));

  // Initialize time
  time_ = std::make_unique<Time>();

  // Initialize game loop
  gameLoop_ = std::make_unique<GameLoop>(engine_.get());

  // Set up framebuffer and command queue
  engine_->getRenderer()->setCommandQueue(commandQueue_);

  // Create scene
  createScene();
}

void EngineSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  if (!engine_ || !gameLoop_ || !time_) {
    return;
  }

  // Update time
  time_->tick();
  float deltaTime = time_->getDeltaTime();

  // Set framebuffer
  engine_->getRenderer()->setFramebuffer(framebuffer_);

  // Run game loop
  gameLoop_->run(deltaTime);
}

void EngineSession::teardown() noexcept {
  if (engine_) {
    engine_->shutdown();
  }
  gameLoop_.reset();
  time_.reset();
  engine_.reset();
}

} // namespace engine
