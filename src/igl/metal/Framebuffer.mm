/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/Framebuffer.h>

#include <igl/metal/CommandQueue.h>
#include <igl/metal/Device.h>
#include <igl/metal/Texture.h>
#include <utility>
#include <vector>

namespace igl::metal {

Framebuffer::Framebuffer(FramebufferDesc value) : value_(std::move(value)) {}

std::vector<size_t> Framebuffer::getColorAttachmentIndices() const {
  std::vector<size_t> ret;

  for (const auto& attachment : value_.colorAttachments) {
    ret.push_back(attachment.first);
  }

  return ret;
}

std::shared_ptr<ITexture> Framebuffer::getColorAttachment(size_t index) const {
  auto colorAttachment = value_.colorAttachments.find(index);

  if (colorAttachment != value_.colorAttachments.end()) {
    return colorAttachment->second.texture;
  }

  return nullptr;
}

std::shared_ptr<ITexture> Framebuffer::getResolveColorAttachment(size_t index) const {
  auto colorAttachment = value_.colorAttachments.find(index);

  if (colorAttachment != value_.colorAttachments.end()) {
    return colorAttachment->second.resolveTexture;
  }

  return nullptr;
}

std::shared_ptr<ITexture> Framebuffer::getDepthAttachment() const {
  return value_.depthAttachment.texture;
}

std::shared_ptr<ITexture> Framebuffer::getResolveDepthAttachment() const {
  return value_.depthAttachment.resolveTexture;
}

std::shared_ptr<ITexture> Framebuffer::getStencilAttachment() const {
  return value_.stencilAttachment.texture;
}

void Framebuffer::copyBytesColorAttachment(ICommandQueue& cmdQueue,
                                           size_t index,
                                           void* pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  IGL_ASSERT_MSG(range.numFaces == 1, "range.numFaces MUST be 1");
  IGL_ASSERT_MSG(range.numLayers == 1, "range.numLayers MUST be 1");
  IGL_ASSERT_MSG(range.numMipLevels == 1, "range.numMipLevels MUST be 1");
  auto colorAttachment = value_.colorAttachments.find(index);

  if (IGL_VERIFY(colorAttachment != value_.colorAttachments.end())) {
    auto texture = colorAttachment->second.texture;
    copyBytes(cmdQueue, texture, pixelBytes, range, bytesPerRow);
  }
}

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& cmdQueue,
                                           void* pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  auto texture = value_.depthAttachment.texture;
  copyBytes(cmdQueue, texture, pixelBytes, range, bytesPerRow);
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& cmdQueue,
                                             void* pixelBytes,
                                             const TextureRangeDesc& range,
                                             size_t bytesPerRow) const {
  auto texture = value_.stencilAttachment.texture;
  copyBytes(cmdQueue, texture, pixelBytes, range, bytesPerRow);
}

/// copyTextureColorAttachment
///
/// This function does not do any cross-queue synchronization. At the time
/// of writing, we are assuming IGL operates on one queue per context
void Framebuffer::copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                             size_t index,
                                             std::shared_ptr<ITexture> destTexture,
                                             const TextureRangeDesc& range) const {
  auto colorAttachment = value_.colorAttachments.find(index);

  if (IGL_VERIFY(colorAttachment != value_.colorAttachments.end())) {
    auto srcTexture = colorAttachment->second.texture;
    id<MTLTexture> srcMtlTexture = static_cast<Texture&>(*srcTexture).get();
    id<MTLTexture> dstMtlTexture = static_cast<Texture&>(*destTexture).get();
    if (IGL_VERIFY(srcMtlTexture && dstMtlTexture)) {
      auto iglMtlCmdQueue = static_cast<CommandQueue&>(cmdQueue);

      id<MTLCommandBuffer> cmdBuf = [iglMtlCmdQueue.get() commandBuffer];
      id<MTLBlitCommandEncoder> blitEncoder = [cmdBuf blitCommandEncoder];

      // NOLINTNEXTLINE(clang-analyzer-nullability.NullabilityBase)
      [blitEncoder copyFromTexture:srcMtlTexture
                       sourceSlice:range.layer
                       sourceLevel:range.mipLevel
                      sourceOrigin:MTLOriginMake(range.x, range.y, 0)
                        sourceSize:MTLSizeMake(range.width, range.height, 1)
                         toTexture:dstMtlTexture
                  destinationSlice:range.layer
                  destinationLevel:range.mipLevel
                 destinationOrigin:MTLOriginMake(range.x, range.y, 0)];
      [blitEncoder endEncoding];
      [cmdBuf commit];
    }
  }
}

void Framebuffer::copyBytes(ICommandQueue& cmdQueue,
                            const std::shared_ptr<ITexture>& iglTexture,
                            void* pixelBytes,
                            const TextureRangeDesc& range,
                            size_t bytesPerRow) const {
  auto mtlTexture = std::static_pointer_cast<Texture>(iglTexture);
  if (bytesPerRow == 0) {
    bytesPerRow = iglTexture->getProperties().getBytesPerRow(range);
  }
  if (IGL_VERIFY(canCopy(cmdQueue, mtlTexture->get(), range))) {
    mtlTexture->getBytes(range, pixelBytes, bytesPerRow);
  } else {
    // Use MTLBlitCommandEncoder to copy into a non-private storage texture that can be read from
    IGL_ASSERT_NOT_IMPLEMENTED();
  }
}

void Framebuffer::updateDrawable(std::shared_ptr<ITexture> texture) {
  if (getColorAttachment(0) != texture) {
    if (!texture) {
      value_.colorAttachments.erase(0);
    } else {
      value_.colorAttachments[0].texture = std::move(texture);
    }
  }
}

void Framebuffer::updateDrawable(SurfaceTextures surfaceTextures) {
  updateDrawable(std::move(surfaceTextures.color));
  if (surfaceTextures.depth && surfaceTextures.depth->getProperties().hasStencil()) {
    if (getStencilAttachment() != surfaceTextures.depth) {
      value_.stencilAttachment.texture = surfaceTextures.depth;
    }
  } else {
    value_.stencilAttachment.texture = nullptr;
  }
  if (getDepthAttachment() != surfaceTextures.depth) {
    value_.depthAttachment.texture = std::move(surfaceTextures.depth);
  }
}

FramebufferMode Framebuffer::getMode() const {
  return value_.mode;
}

} // namespace metal
