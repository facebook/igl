/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "BasicFramebufferSession.h"

#if !defined(IGL_PLATFORM_UWP)
#include <igl/Common.h>
#endif
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/ShellParams.h>

// ===============================================================
// Mock gtest symbols
// ===============================================================

#if defined(IGL_PLATFORM_UWP)
#define IGL_ASSERT(x)
#endif
// TODO: Only use these definitions when debugging a test
#define ASSERT_TRUE(A) IGL_ASSERT((A))
#define ASSERT_FALSE(A) IGL_ASSERT(!(A))
#define ASSERT_EQ(A, B) IGL_ASSERT((A) == (B))
#define ASSERT_NE(A, B) IGL_ASSERT((A) != (B))
#define ASSERT_LT(A, B) IGL_ASSERT((A) < (B))
#define ASSERT_GT(A, B) IGL_ASSERT((A) > (B))
#define ASSERT_ANY_THROW(expr) \
  do {                         \
    try {                      \
      expr;                    \
      IGL_ASSERT(false);       \
    } catch (...) {            \
      IGL_ASSERT(true);        \
    }                          \
  } while (0)

namespace igl::shell {

void BasicFramebufferSession::initialize() noexcept {
  // Create commandQueue
  const igl::CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = getPlatform().getDevice().createCommandQueue(desc, nullptr);
  ASSERT_TRUE(commandQueue_ != nullptr);

  // Initialize render pass
  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = igl::LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = igl::StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPlatform().getDevice().backendDebugColor();
}

void BasicFramebufferSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  igl::Result ret;
  // Create/update framebuffer
  if (framebuffer_ == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    ASSERT_TRUE(ret.isOk());
    ASSERT_TRUE(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  // Create/submit command buffer
  const igl::CommandBufferDesc cbDesc;
  auto buffer = commandQueue_->createCommandBuffer(cbDesc, &ret);
  ASSERT_TRUE(buffer != nullptr);
  ASSERT_TRUE(ret.isOk());
  auto commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  ASSERT_TRUE(commands != nullptr);
  commands->endEncoding();
  IGL_ASSERT(commandQueue_ != nullptr);
  if (shellParams().shouldPresent) {
    buffer->present(framebuffer_->getColorAttachment(0));
  }
  commandQueue_->submit(*buffer);
}

} // namespace igl::shell
