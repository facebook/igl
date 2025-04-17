/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/ImguiSession.h>

#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <igl/CommandBuffer.h>
#include <igl/RenderPass.h>

namespace igl::shell {

void ImguiSession::initialize() noexcept {
  const igl::CommandQueueDesc desc{};
  _commandQueue = getPlatform().getDevice().createCommandQueue(desc, nullptr);

  { // Create the ImGui session
    _imguiSession = std::make_unique<iglu::imgui::Session>(getPlatform().getDevice(),
                                                           getPlatform().getInputDispatcher());
  }
}

void ImguiSession::update(SurfaceTextures surfaceTextures) noexcept {
  const igl::DeviceScope deviceScope(getPlatform().getDevice());

  auto cmdBuffer = _commandQueue->createCommandBuffer(CommandBufferDesc(), nullptr);

  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
  if (_outputFramebuffer) {
    _outputFramebuffer->updateDrawable(surfaceTextures.color);
  } else {
    _outputFramebuffer = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
  }

  RenderPassDesc renderPassDesc;
  renderPassDesc.colorAttachments.resize(1);
  renderPassDesc.colorAttachments[0].loadAction = igl::LoadAction::Clear;
  renderPassDesc.colorAttachments[0].storeAction = igl::StoreAction::Store;
  renderPassDesc.colorAttachments[0].clearColor = getPreferredClearColor();
  auto encoder = cmdBuffer->createRenderCommandEncoder(renderPassDesc, _outputFramebuffer);

  { // Draw using ImGui every frame
    _imguiSession->beginFrame(framebufferDesc, getPlatform().getDisplayContext().pixelsPerPoint);
    ImGui::ShowDemoWindow();
    _imguiSession->endFrame(getPlatform().getDevice(), *encoder);
  }

  encoder->endEncoding();
  if (shellParams().shouldPresent) {
    cmdBuffer->present(surfaceTextures.color);
  }

  _commandQueue->submit(*cmdBuffer);
}

} // namespace igl::shell
