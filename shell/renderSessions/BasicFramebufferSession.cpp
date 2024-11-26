/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "BasicFramebufferSession.h"

#if !defined(IGL_PLATFORM_UWP)
#include <igl/Common.h>
#endif
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

void BasicFramebufferSession::initialize() noexcept {
  // Create commandQueue
  const igl::CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = getPlatform().getDevice().createCommandQueue(desc, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  // Initialize render pass
  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = igl::LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = igl::StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPreferredClearColor();
}

void BasicFramebufferSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  igl::Result ret;
  // Create/update framebuffer
  if (framebuffer_ == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  // Create/submit command buffer
  const igl::CommandBufferDesc cbDesc;
  auto buffer = commandQueue_->createCommandBuffer(cbDesc, &ret);
  IGL_DEBUG_ASSERT(buffer != nullptr);
  IGL_DEBUG_ASSERT(ret.isOk());
  auto commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(commands != nullptr);
  commands->endEncoding();
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);
  if (shellParams().shouldPresent) {
    buffer->present(framebuffer_->getColorAttachment(0));
  }
  commandQueue_->submit(*buffer);
}

} // namespace igl::shell
