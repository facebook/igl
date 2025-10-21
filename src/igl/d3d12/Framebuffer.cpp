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

} // namespace igl::d3d12
