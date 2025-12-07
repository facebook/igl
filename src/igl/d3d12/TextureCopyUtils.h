/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <cstdint>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;

namespace igl::d3d12 {

class D3D12Context;
class Device;
class Texture;
class Buffer;

namespace TextureCopyUtils {

/**
 * Executes a texture-to-buffer copy operation.
 * Handles D3D12 row-pitch alignment, readback staging, and unpacking.
 *
 * @param ctx D3D12 context for device/queue access
 * @param iglDevice IGL device for command allocator pooling
 * @param srcTex Source texture to copy from
 * @param dstBuf Destination buffer to copy to
 * @param destinationOffset Offset in bytes into destination buffer
 * @param mipLevel Mipmap level to copy from source texture
 * @param layer Array layer to copy from source texture
 * @return Result indicating success or failure
 */
[[nodiscard]] Result executeCopyTextureToBuffer(D3D12Context& ctx,
                                                 Device& iglDevice,
                                                 Texture& srcTex,
                                                 Buffer& dstBuf,
                                                 uint64_t destinationOffset,
                                                 uint32_t mipLevel,
                                                 uint32_t layer);

} // namespace TextureCopyUtils
} // namespace igl::d3d12
