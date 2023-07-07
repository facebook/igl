/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/ViewTextureTarget.h>

namespace igl {
namespace opengl {

TextureType ViewTextureTarget::getType() const {
  return TextureType::TwoD;
}

ulong_t ViewTextureTarget::getUsage() const {
  return TextureDesc::TextureUsageBits::Attachment;
}

void ViewTextureTarget::bind() {
  IGL_ASSERT_NOT_REACHED();
}

void ViewTextureTarget::bindImage(size_t /*unit*/) {
  IGL_ASSERT_NOT_REACHED();
}

void ViewTextureTarget::unbind() {
  IGL_ASSERT_NOT_REACHED();
}

void ViewTextureTarget::attachAsColor(uint32_t /* index: not used */,
                                      uint32_t /*face*/,
                                      uint32_t /*mipmapLevel*/) {
  // No-op. This texture is already attached to view's implicit framebuffer
}

void ViewTextureTarget::detachAsColor(uint32_t /* index: not used */,
                                      uint32_t /*face*/,
                                      uint32_t /*mipmapLevel*/) {
  // No-op. This cannot be done for this texture type.
}

void ViewTextureTarget::attachAsDepth() {
  // No-op. This texture is already attached to view's implicit framebuffer
}

void ViewTextureTarget::attachAsStencil() {
  // No-op. This texture is already attached to view's implicit framebuffer
}

bool ViewTextureTarget::isImplicitStorage() const {
  return true;
}

} // namespace opengl
} // namespace igl
