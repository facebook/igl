/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <map>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief Uploads depth values by rendering them: WebGPU cannot write the depth aspect of
/// depth32float or depth24plus textures with wgpuQueueWriteTexture(). The values go into an
/// r32float staging texture that a full-screen draw writes to `@builtin(frag_depth)`. Owned by
/// WebGPUContext.
class DepthUploader final {
 public:
  struct Region {
    uint32_t mipLevel = 0;
    uint32_t layer = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
  };

  explicit DepthUploader(const WebGPUContext& ctx) : ctx_(ctx) {}

  /// Writes `depths` (width * height floats, rows top to bottom) into `region` of `texture`,
  /// which must have RENDER_ATTACHMENT usage. The draw is submitted to the queue at once.
  [[nodiscard]] Result upload(WGPUTexture IGL_NONNULL texture,
                              WGPUTextureFormat format,
                              const Region& region,
                              const float* IGL_NONNULL depths);

 private:
  [[nodiscard]] WGPURenderPipeline IGL_NULLABLE getPipeline(WGPUTextureFormat format,
                                                            Result* IGL_NULLABLE outResult);

  const WebGPUContext& ctx_;
  Handle<WGPUShaderModule> module_;
  Handle<WGPUBindGroupLayout> bindGroupLayout_;
  Handle<WGPUPipelineLayout> pipelineLayout_;
  std::map<WGPUTextureFormat, Handle<WGPURenderPipeline>> pipelines_;
};

} // namespace igl::webgpu
