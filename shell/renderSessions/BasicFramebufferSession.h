/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/IGL.h>
#include <memory>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

class BasicFramebufferSession : public RenderSession {
 public:
  BasicFramebufferSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<igl::IFramebuffer> framebuffer_;
  std::shared_ptr<igl::ICommandQueue> commandQueue_;
  igl::RenderPassDesc renderPass_;
};

} // namespace igl::shell
