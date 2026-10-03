/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <map>
#include <tuple>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief Generates mip levels by rendering: each level is drawn from the level above with a
/// linearly filtered full-screen triangle (nearest for formats WebGPU cannot filter), one render
/// pass per level and layer. Only color formats that are renderable and sampled as floats are
/// supported; compressed textures get their mips from the asset. Owned by WebGPUContext.
class MipmapGenerator final {
 public:
  struct Target {
    WGPUTexture IGL_NULLABLE texture = nullptr;
    WGPUTextureFormat format = WGPUTextureFormat_Undefined;
    bool filterable = true;
    /// WebGPU array layers to process (cube faces are layers).
    uint32_t baseLayer = 0;
    uint32_t numLayers = 1;
    /// Levels baseMipLevel + 1 .. baseMipLevel + numMipLevels - 1 are generated.
    uint32_t baseMipLevel = 0;
    uint32_t numMipLevels = 1;
  };

  explicit MipmapGenerator(const WebGPUContext& ctx) : ctx_(ctx) {}

  /// Encodes the passes into `encoder`, which must have no open pass.
  [[nodiscard]] Result encode(WGPUCommandEncoder IGL_NONNULL encoder, const Target& target);
  /// Creates the pipeline encode() uses for `format`, if it is not cached yet. Its creation may
  /// wait for its error scope, so call it before pushing an error scope around encode().
  [[nodiscard]] Result preparePipeline(WGPUTextureFormat format, bool filterable);

  [[nodiscard]] size_t getPipelineCount() const noexcept {
    return pipelines_.size();
  }

 private:
  [[nodiscard]] WGPURenderPipeline IGL_NULLABLE getPipeline(WGPUTextureFormat format,
                                                            bool filterable,
                                                            Result* IGL_NULLABLE outResult);

  const WebGPUContext& ctx_;
  Handle<WGPUShaderModule> module_;
  // Indexed by whether the source is filterable.
  Handle<WGPUBindGroupLayout> bindGroupLayouts_[2];
  Handle<WGPUPipelineLayout> pipelineLayouts_[2];
  Handle<WGPUSampler> samplers_[2];
  std::map<std::tuple<WGPUTextureFormat, bool>, Handle<WGPURenderPipeline>> pipelines_;
};

} // namespace igl::webgpu
