/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Framebuffer.h>

namespace igl::d3d12 {

std::vector<size_t> Framebuffer::getColorAttachmentIndices() const {
  std::vector<size_t> indices;
  for (size_t i = 0; i < IGL_COLOR_ATTACHMENTS_MAX; ++i) {
    if (desc_.colorAttachments[i].texture) {
      indices.push_back(i);
    }
  }
  return indices;
}

std::shared_ptr<ITexture> Framebuffer::getColorAttachment(size_t index) const {
  if (index < IGL_COLOR_ATTACHMENTS_MAX) {
    return desc_.colorAttachments[index].texture;
  }
  return nullptr;
}

std::shared_ptr<ITexture> Framebuffer::getResolveColorAttachment(size_t index) const {
  if (index < IGL_COLOR_ATTACHMENTS_MAX) {
    return desc_.colorAttachments[index].resolveTexture;
  }
  return nullptr;
}

std::shared_ptr<ITexture> Framebuffer::getDepthAttachment() const {
  return desc_.depthAttachment.texture;
}

std::shared_ptr<ITexture> Framebuffer::getResolveDepthAttachment() const {
  return desc_.depthAttachment.resolveTexture;
}

std::shared_ptr<ITexture> Framebuffer::getStencilAttachment() const {
  return desc_.stencilAttachment.texture;
}

FramebufferMode Framebuffer::getMode() const {
  return desc_.mode;
}

bool Framebuffer::isSwapchainBound() const {
  return false;
}

void Framebuffer::copyBytesColorAttachment(ICommandQueue& /*cmdQueue*/,
                                           size_t /*index*/,
                                           void* /*pixelBytes*/,
                                           const TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  // TODO: Implement for D3D12
}

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& /*cmdQueue*/,
                                           void* /*pixelBytes*/,
                                           const TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  // TODO: Implement for D3D12
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& /*cmdQueue*/,
                                             void* /*pixelBytes*/,
                                             const TextureRangeDesc& /*range*/,
                                             size_t /*bytesPerRow*/) const {
  // TODO: Implement for D3D12
}

void Framebuffer::copyTextureColorAttachment(ICommandQueue& /*cmdQueue*/,
                                             size_t /*index*/,
                                             std::shared_ptr<ITexture> /*destTexture*/,
                                             const TextureRangeDesc& /*range*/) const {
  // TODO: Implement for D3D12
}

void Framebuffer::updateDrawable(std::shared_ptr<ITexture> texture) {
  desc_.colorAttachments[0].texture = std::move(texture);
}

void Framebuffer::updateDrawable(SurfaceTextures surfaceTextures) {
  desc_.colorAttachments[0].texture = std::move(surfaceTextures.color);
  desc_.depthAttachment.texture = std::move(surfaceTextures.depth);
}

void Framebuffer::updateResolveAttachment(std::shared_ptr<ITexture> texture) {
  desc_.colorAttachments[0].resolveTexture = std::move(texture);
}

} // namespace igl::d3d12
