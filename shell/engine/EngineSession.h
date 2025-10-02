/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSession.h>
#include "core/Engine.h"
#include "core/GameLoop.h"
#include "core/Time.h"
#include <memory>

namespace engine {

class EngineSession : public igl::shell::RenderSession {
 public:
  explicit EngineSession(std::shared_ptr<igl::shell::Platform> platform);
  ~EngineSession() override = default;

  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;
  void teardown() noexcept override;

 protected:
  virtual void createScene() = 0;

  Engine* getEngine() const {
    return engine_.get();
  }

 private:
  std::unique_ptr<Engine> engine_;
  std::unique_ptr<GameLoop> gameLoop_;
  std::unique_ptr<Time> time_;
};

} // namespace engine
