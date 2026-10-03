/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <igl/SamplerState.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief Implements the igl::ISamplerState interface for WebGPU.
class SamplerState final : public ISamplerState {
 public:
  [[nodiscard]] static std::shared_ptr<SamplerState> create(const WebGPUContext& ctx,
                                                            const SamplerStateDesc& desc,
                                                            Result* IGL_NULLABLE outResult);

  ~SamplerState() override;

  SamplerState(const SamplerState&) = delete;
  SamplerState& operator=(const SamplerState&) = delete;
  SamplerState(SamplerState&&) = delete;
  SamplerState& operator=(SamplerState&&) = delete;

  [[nodiscard]] bool isYUV() const noexcept override {
    return false;
  }

  [[nodiscard]] WGPUSampler IGL_NULLABLE getWGPUSampler() const noexcept {
    return sampler_.get();
  }
  /// The same sampler with nearest filtering and no anisotropy, for textures WebGPU cannot filter.
  /// Created on first use.
  [[nodiscard]] WGPUSampler IGL_NULLABLE getNonFilteringSampler() const;
  /// Whether any of the min, mag or mip filters is linear.
  [[nodiscard]] bool isFiltering() const noexcept {
    return isFiltering_;
  }
  [[nodiscard]] bool isComparison() const noexcept {
    return isComparison_;
  }
  [[nodiscard]] uint64_t getSamplerId() const noexcept {
    return samplerId_;
  }

 private:
  SamplerState(const WebGPUContext& ctx, const WGPUSamplerDescriptor& desc);

  const WebGPUContext& ctx_;
  WGPUSamplerDescriptor desc_;
  Handle<WGPUSampler> sampler_;
  mutable Handle<WGPUSampler> nonFilteringSampler_;
  const bool isFiltering_;
  const bool isComparison_;
  const uint64_t samplerId_;
};

} // namespace igl::webgpu
