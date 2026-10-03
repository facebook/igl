/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <webgpu/webgpu.h>
#include <igl/Common.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief A region of one mip level and array layer to read back.
struct TextureReadbackDesc {
  WGPUTexture IGL_NULLABLE texture = nullptr;
  WGPUTextureAspect aspect = WGPUTextureAspect_All;
  uint32_t mipLevel = 0;
  uint32_t layer = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t bytesPerTexel = 0;
  /// Row pitch of the destination; 0 means tightly packed.
  size_t dstBytesPerRow = 0;
  /// Stores the bottom row first, matching IGL's bottom-up readback convention.
  bool flipVertically = true;
};

/// Copies a texture region through a 256-byte-aligned staging buffer, waits for it and writes the
/// rows to `dst` without padding. Channels are not swizzled (BGRA stays BGRA).
[[nodiscard]] Result readTexture(const WebGPUContext& ctx,
                                 const TextureReadbackDesc& desc,
                                 void* IGL_NONNULL dst);

/// Copies `size` bytes at `offset` of `buffer` to `dst` and waits for them. `offset` and `size`
/// must be multiples of 4 and `buffer` must have CopySrc usage.
[[nodiscard]] Result readBuffer(const WebGPUContext& ctx,
                                WGPUBuffer IGL_NULLABLE buffer,
                                uint64_t offset,
                                uint64_t size,
                                void* IGL_NONNULL dst);

} // namespace igl::webgpu
