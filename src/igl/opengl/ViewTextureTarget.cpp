/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/ViewTextureTarget.h>

namespace igl::opengl {

TextureType ViewTextureTarget::getType() const {
  return TextureType::TwoD;
}

TextureDesc::TextureUsage ViewTextureTarget::getUsage() const {
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

void ViewTextureTarget::attachAsColor(uint32_t /*index*/, const AttachmentParams& /*params*/) {
  // No-op. This texture is already attached to view's implicit framebuffer
}

void ViewTextureTarget::detachAsColor(uint32_t /*index*/, bool /*read*/) {
  // No-op. This cannot be done for this texture type.
}

void ViewTextureTarget::attachAsDepth(const AttachmentParams& /*params*/) {
  // No-op. This texture is already attached to view's implicit framebuffer
}

void ViewTextureTarget::detachAsDepth(bool /*read*/) {
  // No-op. This cannot be done for this texture type.
}

void ViewTextureTarget::attachAsStencil(const AttachmentParams& /*params*/) {
  // No-op. This texture is already attached to view's implicit framebuffer
}

void ViewTextureTarget::detachAsStencil(bool /*read*/) {
  // No-op. This cannot be done for this texture type.
}

bool ViewTextureTarget::isImplicitStorage() const {
  return true;
}

} // namespace igl::opengl
