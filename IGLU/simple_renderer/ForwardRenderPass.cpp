/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "ForwardRenderPass.h"

#include <utility>

namespace iglu::renderpass {

ForwardRenderPass::ForwardRenderPass(igl::IDevice& device) {
  const igl::CommandQueueDesc desc{};
  _commandQueue = device.createCommandQueue(desc, nullptr);
  _backendType = device.getBackendType();
}

void ForwardRenderPass::begin(std::shared_ptr<igl::IFramebuffer> target,
                              const igl::RenderPassDesc* renderPassDescOverride) {
  IGL_ASSERT_MSG(!isActive(), "Drawing already in progress");

  _framebuffer = std::move(target);

  _renderPipelineDesc.targetDesc.colorAttachments.resize(1);
  _renderPipelineDesc.targetDesc.colorAttachments[0].textureFormat =
      _framebuffer->getColorAttachment(0)->getFormat();
  auto depthAttachment = _framebuffer->getDepthAttachment();
  _renderPipelineDesc.targetDesc.depthAttachmentFormat =
      depthAttachment ? depthAttachment->getFormat() : igl::TextureFormat::Invalid;
  auto stencilAttachment = _framebuffer->getStencilAttachment();
  _renderPipelineDesc.targetDesc.stencilAttachmentFormat =
      stencilAttachment ? stencilAttachment->getFormat() : igl::TextureFormat::Invalid;

  igl::RenderPassDesc defaultRenderPassDesc;
  defaultRenderPassDesc.colorAttachments.resize(1);
  defaultRenderPassDesc.colorAttachments[0].loadAction = igl::LoadAction::Clear;
  defaultRenderPassDesc.colorAttachments[0].storeAction = igl::StoreAction::Store;
  defaultRenderPassDesc.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};
  defaultRenderPassDesc.depthAttachment.clearDepth = 1.0f;
  const igl::RenderPassDesc* finalDesc = renderPassDescOverride ? renderPassDescOverride
                                                                : &defaultRenderPassDesc;

  const igl::CommandBufferDesc cbDesc;
  _commandBuffer = _commandQueue->createCommandBuffer(cbDesc, nullptr);
  _commandEncoder =
      _commandBuffer->createRenderCommandEncoder(*finalDesc, _framebuffer, {}, nullptr);
}

void ForwardRenderPass::draw(drawable::Drawable& drawable, igl::IDevice& device) const {
  IGL_ASSERT_MSG(isActive(), "Drawing not in progress");
  drawable.draw(device, *_commandEncoder, _renderPipelineDesc);
}

void ForwardRenderPass::end(bool shouldPresent) {
  IGL_ASSERT_MSG(isActive(), "Drawing not in progress");

  _commandEncoder->endEncoding();

  if (shouldPresent) {
    _commandBuffer->present(_framebuffer->getColorAttachment(0));
  }

  _commandQueue->submit(*_commandBuffer);

  _commandEncoder = nullptr;
  _commandBuffer = nullptr;
  _framebuffer = nullptr;
}

void ForwardRenderPass::bindViewport(const igl::Viewport& viewport, const igl::Size& surfaceSize) {
  IGL_ASSERT_MSG(isActive(), "Drawing not in progress");
  if (_backendType == igl::BackendType::Metal) {
    // In Metal, framebuffer origin is top left but the argument assumes bottom left
    igl::Viewport flippedViewport = viewport;
    flippedViewport.y = surfaceSize.height - viewport.y - viewport.height;
    _commandEncoder->bindViewport(flippedViewport);
  } else {
    _commandEncoder->bindViewport(viewport);
  }
}

bool ForwardRenderPass::isActive() const {
  return _framebuffer != nullptr;
}

igl::IFramebuffer& ForwardRenderPass::activeTarget() {
  IGL_ASSERT_MSG(isActive(), "No valid target when not active");
  return *_framebuffer;
}

igl::IRenderCommandEncoder& ForwardRenderPass::activeCommandEncoder() {
  IGL_ASSERT_MSG(isActive(), "No valid command encoder when not active");
  return *_commandEncoder;
}

} // namespace iglu::renderpass
