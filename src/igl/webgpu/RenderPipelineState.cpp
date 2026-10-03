/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/RenderPipelineState.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>
#include <igl/webgpu/DeviceFeatureSet.h>
#include <igl/webgpu/RenderPipelineReflection.h>
#include <igl/webgpu/ShaderModule.h>
#include <igl/webgpu/StateSanitizer.h>
#include <igl/webgpu/VertexInputState.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

// Dynamic uniform buffers WebGPU guarantees per pipeline layout; group 1 uniforms beyond this use
// static bind groups.
constexpr uint32_t kMaxDynamicUniformBuffers = 8;

static_assert(sizeof(RenderPipelineDynamicState) == 48, "RenderPipelineDynamicState has padding");

template<typename T>
uint8_t toByte(T value) {
  return static_cast<uint8_t>(value);
}

StencilStateDesc toStencilDesc(uint8_t compare,
                               uint8_t failOp,
                               uint8_t depthFailOp,
                               uint8_t passOp,
                               uint32_t readMask,
                               uint32_t writeMask) {
  return {
      .stencilFailureOperation = static_cast<StencilOperation>(failOp),
      .depthFailureOperation = static_cast<StencilOperation>(depthFailOp),
      .depthStencilPassOperation = static_cast<StencilOperation>(passOp),
      .stencilCompareFunction = static_cast<CompareFunction>(compare),
      .readMask = readMask,
      .writeMask = writeMask,
  };
}

} // namespace

void RenderPipelineDynamicState::setDepthStencilState(const DepthStencilStateDesc& desc) {
  depthCompare = toByte(desc.compareFunction);
  depthWriteEnabled = desc.isDepthWriteEnabled ? 1 : 0;
  frontStencilCompare = toByte(desc.frontFaceStencil.stencilCompareFunction);
  frontStencilFailOp = toByte(desc.frontFaceStencil.stencilFailureOperation);
  frontDepthFailOp = toByte(desc.frontFaceStencil.depthFailureOperation);
  frontStencilPassOp = toByte(desc.frontFaceStencil.depthStencilPassOperation);
  backStencilCompare = toByte(desc.backFaceStencil.stencilCompareFunction);
  backStencilFailOp = toByte(desc.backFaceStencil.stencilFailureOperation);
  backDepthFailOp = toByte(desc.backFaceStencil.depthFailureOperation);
  backStencilPassOp = toByte(desc.backFaceStencil.depthStencilPassOperation);
  stencilReadMask = desc.frontFaceStencil.readMask;
  stencilWriteMask = desc.frontFaceStencil.writeMask;
}

DepthStencilStateDesc RenderPipelineDynamicState::getDepthStencilState() const {
  return {
      .compareFunction = static_cast<CompareFunction>(depthCompare),
      .isDepthWriteEnabled = depthWriteEnabled != 0,
      .backFaceStencil = toStencilDesc(backStencilCompare,
                                       backStencilFailOp,
                                       backDepthFailOp,
                                       backStencilPassOp,
                                       stencilReadMask,
                                       stencilWriteMask),
      .frontFaceStencil = toStencilDesc(frontStencilCompare,
                                        frontStencilFailOp,
                                        frontDepthFailOp,
                                        frontStencilPassOp,
                                        stencilReadMask,
                                        stencilWriteMask),
  };
}

bool RenderPipelineDynamicState::operator==(const RenderPipelineDynamicState& other) const {
  // Bitwise, to agree with the hash: the struct has no padding, and floats that differ only in
  // representation select separate (identical) pipelines.
  // NOLINTNEXTLINE(bugprone-suspicious-memory-comparison)
  return std::memcmp(this, &other, sizeof(*this)) == 0;
}

size_t RenderPipelineDynamicStateHash::operator()(const RenderPipelineDynamicState& key) const {
  // FNV-1a over the bytes; the struct has no padding.
  uint64_t hash = 14695981039346656037ull;
  const auto* bytes = reinterpret_cast<const uint8_t*>(&key);
  for (size_t i = 0; i < sizeof(key); ++i) {
    hash = (hash ^ bytes[i]) * 1099511628211ull;
  }
  return static_cast<size_t>(hash);
}

std::shared_ptr<RenderPipelineState> RenderPipelineState::create(WebGPUContext& ctx,
                                                                 const DeviceFeatureSet& features,
                                                                 BindLayoutCache& layoutCache,
                                                                 const RenderPipelineDesc& desc,
                                                                 Result* IGL_NULLABLE outResult) {
  auto pipeline = std::shared_ptr<RenderPipelineState>(
      new RenderPipelineState(ctx, features, layoutCache, desc));
  Result result = pipeline->init();
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setOk(outResult);
  return pipeline;
}

RenderPipelineState::RenderPipelineState(WebGPUContext& ctx,
                                         const DeviceFeatureSet& features,
                                         BindLayoutCache& layoutCache,
                                         const RenderPipelineDesc& desc) :
  IRenderPipelineState(desc), ctx_(ctx), features_(features), layoutCache_(layoutCache) {}

RenderPipelineState::~RenderPipelineState() = default;

Result RenderPipelineState::init() {
  const auto& stages = desc_.shaderStages;
  if (!stages || !stages->isValid() || stages->getType() != ShaderStagesType::Render ||
      !stages->getVertexModule() || !stages->getFragmentModule()) {
    return Result(Result::Code::ArgumentInvalid,
                  "Render pipelines need vertex and fragment shaders");
  }
  vertexModule_ = std::static_pointer_cast<ShaderModule>(stages->getVertexModule());
  fragmentModule_ = std::static_pointer_cast<ShaderModule>(stages->getFragmentModule());

  Result result = bindings_.add(vertexModule_->getReflection(), WGPUShaderStage_Vertex);
  if (result.isOk()) {
    result = bindings_.add(fragmentModule_->getReflection(), WGPUShaderStage_Fragment);
  }
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

  for (const auto& attachment : desc_.targetDesc.colorAttachments) {
    WGPUColorTargetState target = {};
    WGPUBlendState blend = {};
    result = makeColorTargetState(attachment, features_, target, blend);
    if (!result.isOk()) {
      return result;
    }
  }

  const TextureFormat depthFormat = desc_.targetDesc.depthAttachmentFormat;
  const TextureFormat stencilFormat = desc_.targetDesc.stencilAttachmentFormat;
  if (depthFormat != TextureFormat::Invalid && stencilFormat != TextureFormat::Invalid &&
      depthFormat != stencilFormat) {
    return Result(Result::Code::Unsupported,
                  "WebGPU has one depth/stencil attachment; depth and stencil formats must match");
  }
  const TextureFormat format = depthFormat != TextureFormat::Invalid ? depthFormat : stencilFormat;
  if (format != TextureFormat::Invalid) {
    const std::optional<WGPUTextureFormat> wgpuFormat = textureFormatToWGPUTextureFormat(format);
    if (!wgpuFormat || !TextureFormatProperties::fromTextureFormat(format).isDepthOrStencil()) {
      return Result(Result::Code::Unsupported, "Unsupported depth/stencil attachment format");
    }
    depthStencilFormat_ = *wgpuFormat;
  }

  if (desc_.vertexInputState) {
    const VertexInputStateDesc& vertexInput =
        static_cast<const VertexInputState&>(*desc_.vertexInputState).getDesc();
    for (size_t i = 0; i < vertexInput.numAttributes; ++i) {
      requiredVertexBufferMask_ |= 1u << vertexInput.attributes[i].bufferIndex;
    }
  }

  reflection_ = std::make_shared<RenderPipelineReflection>(
      std::vector<std::pair<ShaderStage, const WgslReflection*>>{
          {ShaderStage::Vertex, &vertexModule_->getReflection()},
          {ShaderStage::Fragment, &fragmentModule_->getReflection()},
      });

  // The default variant reports shader/layout mismatches at creation, like the other backends.
  Result pipelineResult;
  if (getPipeline(getDefaultDynamicState(), &pipelineResult) == nullptr) {
    return pipelineResult;
  }
  return Result();
}

RenderPipelineDynamicState RenderPipelineState::getDefaultDynamicState() const {
  RenderPipelineDynamicState state;
  state.setDepthStencilState({});
  state.cullMode = toByte(desc_.cullMode);
  state.frontFaceWinding = toByte(desc_.frontFaceWinding);
  state.sampleClasses = bindings_.getDefaultSampleClasses();
  return state;
}

WGPURenderPipeline IGL_NULLABLE
RenderPipelineState::getPipeline(const RenderPipelineDynamicState& state,
                                 Result* IGL_NULLABLE outResult) {
  if (const auto it = variants_.find(state); it != variants_.end()) {
    Result::setOk(outResult);
    return it->second.get();
  }
  Handle<WGPURenderPipeline> pipeline;
  Result result = createPipeline(state, pipeline);
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  ++pipelineCreationCount_;
  if (variants_.size() == kVariantWarningThreshold) {
    IGL_LOG_INFO(
        "RenderPipelineState %s: more than %zu variants; animating depth bias, stencil "
        "masks, culling or bound texture formats creates WebGPU pipelines\n",
        desc_.debugName.c_str(),
        kVariantWarningThreshold);
  }
  Result::setOk(outResult);
  return variants_.emplace(state, std::move(pipeline)).first->second.get();
}

WGPUBindGroupLayout IGL_NULLABLE
RenderPipelineState::getBindGroupLayout(uint32_t group,
                                        SampleClasses classes,
                                        Result* IGL_NULLABLE outResult) {
  return layoutCache_.getBindGroupLayout(bindings_, group, classes, outResult);
}

Result RenderPipelineState::createPipeline(const RenderPipelineDynamicState& state,
                                           Handle<WGPURenderPipeline>& outPipeline) {
  std::vector<WGPUBindGroupLayout> groupLayouts;
  for (uint32_t group = 0; group < numBindGroups_; ++group) {
    Result result;
    WGPUBindGroupLayout layout = getBindGroupLayout(group, state.sampleClasses, &result);
    if (layout == nullptr) {
      return result;
    }
    groupLayouts.push_back(layout);
  }
  Result result;
  WGPUPipelineLayout pipelineLayout = layoutCache_.getPipelineLayout(groupLayouts, &result);
  if (pipelineLayout == nullptr) {
    return result;
  }

  std::vector<WGPUVertexBufferLayout> vertexBuffers;
  std::vector<WGPUVertexAttribute> vertexAttributes;
  if (desc_.vertexInputState) {
    makeVertexBufferLayouts(static_cast<const VertexInputState&>(*desc_.vertexInputState).getDesc(),
                            vertexBuffers,
                            vertexAttributes);
  }

  WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
  pipelineDesc.label = toWGPUStringView(desc_.debugName.toString());
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.vertex.module = vertexModule_->getWGPUShaderModule();
  pipelineDesc.vertex.entryPoint = toWGPUStringView(vertexModule_->info().entryPoint);
  const PipelineConstants vertexConstants = vertexModule_->getPipelineConstants();
  pipelineDesc.vertex.constantCount = vertexConstants.entries.size();
  pipelineDesc.vertex.constants = vertexConstants.entries.data();
  pipelineDesc.vertex.bufferCount = vertexBuffers.size();
  pipelineDesc.vertex.buffers = vertexBuffers.data();

  const WGPUIndexFormat stripIndexFormat =
      state.stripIndexFormat == 0
          ? WGPUIndexFormat_Undefined
          : indexFormatToWGPUIndexFormat(static_cast<IndexFormat>(state.stripIndexFormat - 1))
                .value_or(WGPUIndexFormat_Undefined);
  result = makePrimitiveState(desc_,
                              static_cast<CullMode>(state.cullMode),
                              static_cast<WindingMode>(state.frontFaceWinding),
                              stripIndexFormat,
                              pipelineDesc.primitive);
  if (!result.isOk()) {
    return result;
  }

  WGPUDepthStencilState depthStencil = WGPU_DEPTH_STENCIL_STATE_INIT;
  if (hasDepthStencilAttachment()) {
    result = makeDepthStencilState(state.getDepthStencilState(), depthStencilFormat_, depthStencil);
    if (!result.isOk()) {
      return result;
    }
    applyDepthBias(desc_.topology,
                   state.depthBias,
                   state.depthBiasSlopeScale,
                   state.depthBiasClamp,
                   depthStencil);
    pipelineDesc.depthStencil = &depthStencil;
  }

  result = makeMultisampleState(
      desc_.sampleCount, desc_.alphaToCoverageEnabled, pipelineDesc.multisample);
  if (!result.isOk()) {
    return result;
  }

  const size_t numTargets = desc_.targetDesc.colorAttachments.size();
  std::vector<WGPUColorTargetState> targets(numTargets);
  std::vector<WGPUBlendState> blends(numTargets);
  for (size_t i = 0; i < numTargets; ++i) {
    result = makeColorTargetState(
        desc_.targetDesc.colorAttachments[i], features_, targets[i], blends[i]);
    if (!result.isOk()) {
      return result;
    }
  }
  WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
  fragment.module = fragmentModule_->getWGPUShaderModule();
  fragment.entryPoint = toWGPUStringView(fragmentModule_->info().entryPoint);
  const PipelineConstants fragmentConstants = fragmentModule_->getPipelineConstants();
  fragment.constantCount = fragmentConstants.entries.size();
  fragment.constants = fragmentConstants.entries.data();
  fragment.targetCount = targets.size();
  fragment.targets = targets.data();
  pipelineDesc.fragment = &fragment;

  ctx_.pushErrorScope(WGPUErrorFilter_Validation);
  outPipeline.reset(wgpuDeviceCreateRenderPipeline(ctx_.getDevice(), &pipelineDesc));
  result = ctx_.popErrorScope(ErrorScopeKind::Pipeline);
  if (!result.isOk()) {
    outPipeline = nullptr;
    return result;
  }
  return outPipeline
             ? Result()
             : Result(Result::Code::RuntimeError, "wgpuDeviceCreateRenderPipeline() failed");
}

std::shared_ptr<IRenderPipelineReflection> RenderPipelineState::renderPipelineReflection() {
  return reflection_;
}

void RenderPipelineState::setRenderPipelineReflection(
    const IRenderPipelineReflection& /*reflection*/) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

int RenderPipelineState::getIndexByName(const NameHandle& name, ShaderStage stage) const {
  return getIndexByName(name.toString(), stage);
}

int RenderPipelineState::getIndexByName(const std::string& name, ShaderStage stage) const {
  return reflection_ ? reflection_->getIndexByName(name, stage) : -1;
}

} // namespace igl::webgpu
