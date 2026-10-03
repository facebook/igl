/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/ComputePipelineState.h>

#include <algorithm>
#include <utility>
#include <vector>
#include <igl/webgpu/DeviceFeatureSet.h>
#include <igl/webgpu/RenderPipelineReflection.h>
#include <igl/webgpu/ShaderModule.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

constexpr uint32_t kMaxDynamicUniformBuffers = 8;

} // namespace

std::shared_ptr<ComputePipelineState> ComputePipelineState::create(WebGPUContext& ctx,
                                                                   const DeviceFeatureSet& features,
                                                                   BindLayoutCache& layoutCache,
                                                                   const ComputePipelineDesc& desc,
                                                                   Result* IGL_NULLABLE outResult) {
  auto pipeline = std::shared_ptr<ComputePipelineState>(
      new ComputePipelineState(ctx, features, layoutCache, desc));
  Result result = pipeline->init();
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setOk(outResult);
  return pipeline;
}

ComputePipelineState::ComputePipelineState(WebGPUContext& ctx,
                                           const DeviceFeatureSet& features,
                                           BindLayoutCache& layoutCache,
                                           ComputePipelineDesc desc) :
  ctx_(ctx), features_(features), layoutCache_(layoutCache), desc_(std::move(desc)) {}

ComputePipelineState::~ComputePipelineState() = default;

Result ComputePipelineState::init() {
  const auto& stages = desc_.shaderStages;
  if (!stages || !stages->isValid() || stages->getType() != ShaderStagesType::Compute ||
      !stages->getComputeModule()) {
    return Result(Result::Code::ArgumentInvalid, "Compute pipelines need a compute shader");
  }
  module_ = std::static_pointer_cast<ShaderModule>(stages->getComputeModule());
  Result result = bindings_.add(module_->getReflection(), WGPUShaderStage_Compute);
  if (!result.isOk()) {
    return result;
  }
  bindings_.assignDynamicOffsets(std::min(
      kMaxDynamicUniformBuffers, features_.getLimits().maxDynamicUniformBuffersPerPipelineLayout));
  for (uint32_t group = 0; group < kNumBindGroups; ++group) {
    if (!bindings_.groups[group].empty()) {
      numBindGroups_ = group + 1;
    }
  }
  reflection_ = std::make_shared<RenderPipelineReflection>(
      std::vector<std::pair<ShaderStage, const WgslReflection*>>{
          {ShaderStage::Compute, &module_->getReflection()},
      });
  Result pipelineResult;
  if (getPipeline(bindings_.getDefaultSampleClasses(), &pipelineResult) == nullptr) {
    return pipelineResult;
  }
  return Result();
}

WGPUBindGroupLayout IGL_NULLABLE
ComputePipelineState::getBindGroupLayout(uint32_t group,
                                         SampleClasses classes,
                                         Result* IGL_NULLABLE outResult) {
  return layoutCache_.getBindGroupLayout(bindings_, group, classes, outResult);
}

WGPUComputePipeline IGL_NULLABLE ComputePipelineState::getPipeline(SampleClasses classes,
                                                                   Result* IGL_NULLABLE outResult) {
  if (const auto it = variants_.find(classes); it != variants_.end()) {
    Result::setOk(outResult);
    return it->second.get();
  }
  std::vector<WGPUBindGroupLayout> groupLayouts;
  for (uint32_t group = 0; group < numBindGroups_; ++group) {
    WGPUBindGroupLayout layout = getBindGroupLayout(group, classes, outResult);
    if (layout == nullptr) {
      return nullptr;
    }
    groupLayouts.push_back(layout);
  }
  WGPUPipelineLayout pipelineLayout = layoutCache_.getPipelineLayout(groupLayouts, outResult);
  if (pipelineLayout == nullptr) {
    return nullptr;
  }
  const PipelineConstants constants = module_->getPipelineConstants();
  WGPUComputePipelineDescriptor pipelineDesc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
  pipelineDesc.label = toWGPUStringView(desc_.debugName);
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.module = module_->getWGPUShaderModule();
  pipelineDesc.compute.entryPoint = toWGPUStringView(module_->info().entryPoint);
  pipelineDesc.compute.constantCount = constants.entries.size();
  pipelineDesc.compute.constants = constants.entries.data();

  ctx_.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPUComputePipeline> pipeline(
      wgpuDeviceCreateComputePipeline(ctx_.getDevice(), &pipelineDesc));
  Result result = ctx_.popErrorScope(ErrorScopeKind::Pipeline);
  if (!result.isOk() || !pipeline) {
    Result::setResult(outResult,
                      !result.isOk() ? std::move(result)
                                     : Result(Result::Code::RuntimeError,
                                              "wgpuDeviceCreateComputePipeline() failed"));
    return nullptr;
  }
  Result::setOk(outResult);
  return variants_.emplace(classes, std::move(pipeline)).first->second.get();
}

std::shared_ptr<IComputePipelineState::IComputePipelineReflection>
ComputePipelineState::computePipelineReflection() {
  return reflection_;
}

int ComputePipelineState::getIndexByName(const NameHandle& name) const {
  return reflection_ ? reflection_->getIndexByName(name.toString(), ShaderStage::Compute) : -1;
}

} // namespace igl::webgpu
