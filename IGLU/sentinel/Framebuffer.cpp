/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include <IGLU/sentinel/Framebuffer.h>

#include <IGLU/sentinel/Assert.h>

namespace iglu::sentinel {

Framebuffer::Framebuffer(bool shouldAssert) : shouldAssert_(shouldAssert) {}

std::vector<size_t> Framebuffer::getColorAttachmentIndices() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return {};
}

std::shared_ptr<igl::ITexture> Framebuffer::getColorAttachment(size_t /*index*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ITexture> Framebuffer::getResolveColorAttachment(size_t /*index*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ITexture> Framebuffer::getDepthAttachment() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ITexture> Framebuffer::getResolveDepthAttachment() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ITexture> Framebuffer::getStencilAttachment() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

igl::FramebufferMode Framebuffer::getMode() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return igl::FramebufferMode::Mono;
}

bool Framebuffer::isSwapchainBound() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

void Framebuffer::copyBytesColorAttachment(igl::ICommandQueue& /*cmdQueue*/,
                                           size_t /*index*/,
                                           void* /*pixelBytes*/,
                                           const igl::TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Framebuffer::copyBytesDepthAttachment(igl::ICommandQueue& /*cmdQueue*/,
                                           void* /*pixelBytes*/,
                                           const igl::TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Framebuffer::copyBytesStencilAttachment(igl::ICommandQueue& /*cmdQueue*/,
                                             void* /*pixelBytes*/,
                                             const igl::TextureRangeDesc& /*range*/,
                                             size_t /*bytesPerRow*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Framebuffer::copyTextureColorAttachment(igl::ICommandQueue& /*cmdQueue*/,
                                             size_t /*index*/,
                                             std::shared_ptr<igl::ITexture> /*destTexture*/,
                                             const igl::TextureRangeDesc& /*range*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Framebuffer::updateDrawable(std::shared_ptr<igl::ITexture> /*texture*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Framebuffer::updateDrawable(igl::SurfaceTextures /*surfaceTextures*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Framebuffer::updateResolveAttachment(std::shared_ptr<igl::ITexture> /*texture*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

} // namespace iglu::sentinel
