/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <igl/ComputePipelineState.h>
#include <igl/webgpu/BindLayouts.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class DeviceFeatureSet;
class RenderPipelineReflection;
class ShaderModule;
class WebGPUContext;

/// @brief Implements the igl::IComputePipelineState interface for WebGPU.
///
/// Like render pipelines, compute pipelines use explicit layouts built from the shader's
/// reflection and are created lazily, one per set of sample classes of the bound textures.
class ComputePipelineState final : public IComputePipelineState, public PipelineLayoutSource {
 public:
  [[nodiscard]] static std::shared_ptr<ComputePipelineState> create(
      WebGPUContext& ctx,
      const DeviceFeatureSet& features,
      BindLayoutCache& layoutCache,
      const ComputePipelineDesc& desc,
      Result* IGL_NULLABLE outResult);
  ~ComputePipelineState() override;

  ComputePipelineState(const ComputePipelineState&) = delete;
  ComputePipelineState& operator=(const ComputePipelineState&) = delete;
  ComputePipelineState(ComputePipelineState&&) = delete;
  ComputePipelineState& operator=(ComputePipelineState&&) = delete;

  [[nodiscard]] std::shared_ptr<IComputePipelineReflection> computePipelineReflection() override;
  [[nodiscard]] int getIndexByName(const NameHandle& name) const override;

  [[nodiscard]] const PipelineBindings& getBindings() const noexcept override {
    return bindings_;
  }
  [[nodiscard]] WGPUBindGroupLayout IGL_NULLABLE
  getBindGroupLayout(uint32_t group,
                     SampleClasses classes,
                     Result* IGL_NULLABLE outResult) override;

  /// The pipeline for textures sampled as `classes`, created on first use.
  [[nodiscard]] WGPUComputePipeline IGL_NULLABLE getPipeline(SampleClasses classes,
                                                             Result* IGL_NULLABLE outResult);
  [[nodiscard]] size_t getPipelineCreationCount() const noexcept {
    return variants_.size();
  }

 private:
  ComputePipelineState(WebGPUContext& ctx,
                       const DeviceFeatureSet& features,
                       BindLayoutCache& layoutCache,
                       ComputePipelineDesc desc);

  [[nodiscard]] Result init();

  WebGPUContext& ctx_;
  const DeviceFeatureSet& features_;
  BindLayoutCache& layoutCache_;
  const ComputePipelineDesc desc_;
  std::shared_ptr<ShaderModule> module_;
  PipelineBindings bindings_;
  uint32_t numBindGroups_ = 0;
  std::shared_ptr<RenderPipelineReflection> reflection_;
  std::unordered_map<SampleClasses, Handle<WGPUComputePipeline>> variants_;
};

} // namespace igl::webgpu
