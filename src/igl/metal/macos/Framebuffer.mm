/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/macos/Framebuffer.h>

#include <vector>
#include <igl/metal/CommandQueue.h>
#include <igl/metal/Texture.h>

namespace igl::metal::macos {

Framebuffer::Framebuffer(const FramebufferDesc& value) : metal::Framebuffer(value) {}

/// This function assumes the input texture has the content. For example,
/// if iglTexture is MTLStorageModeManaged, then it assumes the caller
/// has already blitted the content from the GPU. The texture cannot
/// be MTLStorageModePrivate.
bool Framebuffer::canCopy(ICommandQueue& cmdQueue,
                          id<MTLTexture> texture,
                          const TextureRangeDesc& range) const {
  const bool result = texture.storageMode == MTLStorageModeManaged;
  auto iglMtlCmdQueue = static_cast<CommandQueue&>(cmdQueue);

  if (result) {
    id<MTLCommandBuffer> cmdBuf = [iglMtlCmdQueue.get() commandBuffer];
    id<MTLBlitCommandEncoder> blitEncoder = [cmdBuf blitCommandEncoder];
    [blitEncoder synchronizeTexture:texture slice:range.layer level:range.mipLevel];
    [blitEncoder endEncoding];
    [cmdBuf commit];
    [cmdBuf waitUntilCompleted];
  }

  return result;
}

} // namespace igl::metal::macos
