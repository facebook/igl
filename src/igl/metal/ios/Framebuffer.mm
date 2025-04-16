/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/ios/Framebuffer.h>

#include <vector>
#include <igl/metal/Texture.h>

namespace igl::metal::ios {

Framebuffer::Framebuffer(const FramebufferDesc& value) : metal::Framebuffer(value) {}

bool Framebuffer::canCopy(ICommandQueue& /* unused */,
                          id<MTLTexture> texture,
                          const TextureRangeDesc& /*range*/) const {
  return texture.storageMode == MTLStorageModeShared;
}

} // namespace igl::metal::ios
