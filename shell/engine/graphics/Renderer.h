/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Framebuffer.h>
#include <memory>

namespace igl::shell {
class Platform;
}

namespace engine {

class Scene;
class Camera;

class Renderer {
 public:
  explicit Renderer(std::shared_ptr<igl::shell::Platform> platform);

  void renderScene(Scene& scene);

  void setFramebuffer(std::shared_ptr<igl::IFramebuffer> framebuffer) {
    framebuffer_ = std::move(framebuffer);
  }

  void setCommandQueue(std::shared_ptr<igl::ICommandQueue> commandQueue) {
    commandQueue_ = std::move(commandQueue);
  }

 private:
  std::shared_ptr<igl::shell::Platform> platform_;
  std::shared_ptr<igl::IFramebuffer> framebuffer_;
  std::shared_ptr<igl::ICommandQueue> commandQueue_;
};

} // namespace engine
