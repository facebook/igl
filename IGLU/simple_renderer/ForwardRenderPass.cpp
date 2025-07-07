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
  commandQueue_ = device.createCommandQueue(desc, nullptr);
  backendType_ = device.getBackendType();
}

void ForwardRenderPass::begin(std::shared_ptr<igl::IFramebuffer> target,
                              const igl::RenderPassDesc* renderPassDescOverride) {
  IGL_DEBUG_ASSERT(!isActive(), "Drawing already in progress");

  framebuffer_ = std::move(target);

  renderPipelineDesc_.targetDesc.colorAttachments.resize(1);
  renderPipelineDesc_.targetDesc.colorAttachments[0].textureFormat =
      framebuffer_->getColorAttachment(0)->getFormat();
  auto depthAttachment = framebuffer_->getDepthAttachment();
  renderPipelineDesc_.targetDesc.depthAttachmentFormat =
      depthAttachment ? depthAttachment->getFormat() : igl::TextureFormat::Invalid;
  auto stencilAttachment = framebuffer_->getStencilAttachment();
  renderPipelineDesc_.targetDesc.stencilAttachmentFormat =
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
  commandBuffer_ = commandQueue_->createCommandBuffer(cbDesc, nullptr);
  commandEncoder_ =
      commandBuffer_->createRenderCommandEncoder(*finalDesc, framebuffer_, {}, nullptr);
}

void ForwardRenderPass::draw(drawable::Drawable& drawable, igl::IDevice& device) const {
  IGL_DEBUG_ASSERT(isActive(), "Drawing not in progress");
  drawable.draw(device, *commandEncoder_, renderPipelineDesc_);
}

void ForwardRenderPass::end(bool shouldPresent) {
  IGL_DEBUG_ASSERT(isActive(), "Drawing not in progress");

  commandEncoder_->endEncoding();

  if (shouldPresent) {
    commandBuffer_->present(framebuffer_->getColorAttachment(0));
  }

  commandQueue_->submit(*commandBuffer_);

  commandEncoder_ = nullptr;
  commandBuffer_ = nullptr;
  framebuffer_ = nullptr;
}

void ForwardRenderPass::bindViewport(const igl::Viewport& viewport, const igl::Size& surfaceSize) {
  IGL_DEBUG_ASSERT(isActive(), "Drawing not in progress");
  if (backendType_ == igl::BackendType::Metal) {
    // In Metal, framebuffer origin is top left but the argument assumes bottom left
    igl::Viewport flippedViewport = viewport;
    flippedViewport.y = surfaceSize.height - viewport.y - viewport.height;
    commandEncoder_->bindViewport(flippedViewport);
  } else {
    commandEncoder_->bindViewport(viewport);
  }
}

bool ForwardRenderPass::isActive() const {
  return framebuffer_ != nullptr;
}

igl::IFramebuffer& ForwardRenderPass::activeTarget() {
  IGL_DEBUG_ASSERT(isActive(), "No valid target when not active");
  return *framebuffer_;
}

igl::IRenderCommandEncoder& ForwardRenderPass::activeCommandEncoder() {
  IGL_DEBUG_ASSERT(isActive(), "No valid command encoder when not active");
  return *commandEncoder_;
}

} // namespace iglu::renderpass
