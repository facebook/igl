/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/EmptySession.h>

#include <igl/CommandBuffer.h>
#include <igl/RenderPass.h>

namespace igl::shell {

void EmptySession::initialize() noexcept {
  getPlatform().getDevice();
}

void EmptySession::update(SurfaceTextures surfaceTextures) noexcept {
  if (surfaceTextures.color == nullptr) {
    return;
  }

  // Create command buffer
  CommandBufferDesc cbDesc;
  auto commandBuffer = getPlatform().getDevice().createCommandQueue({}, nullptr)
                           ->createCommandBuffer(cbDesc, nullptr);

  // Set up render pass with dark blue clear color (Phase 2 target)
  RenderPassDesc renderPass;
  renderPass.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass.colorAttachments[0].clearColor = {0.1f, 0.1f, 0.15f, 1.0f}; // Dark blue

  // Create framebuffer with surface texture
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;

  auto framebuffer = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);

  // Create render encoder and let it clear the screen
  auto encoder = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer, {}, nullptr);

  // End encoding (no draw calls, just clear)
  encoder->endEncoding();

  // Submit command buffer
  commandBuffer->present(surfaceTextures.color);

  getPlatform().getDevice().createCommandQueue({}, nullptr)->submit(*commandBuffer);
}

} // namespace igl::shell
