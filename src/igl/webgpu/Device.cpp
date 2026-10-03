/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Device.h>

#include <new>
#include <utility>
#include <vector>
#include <igl/Buffer.h>
#include <igl/CommandQueue.h>
#include <igl/ComputePipelineState.h>
#include <igl/DepthStencilState.h>
#include <igl/Framebuffer.h>
#include <igl/FramebufferWrapper.h>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/Shader.h>
#include <igl/Texture.h>
#include <igl/Timer.h>
#include <igl/TimestampQueries.h>
#include <igl/VertexInputState.h>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/CommandQueue.h>
#include <igl/webgpu/ComputePipelineState.h>
#include <igl/webgpu/DepthStencilState.h>
#include <igl/webgpu/Framebuffer.h>
#include <igl/webgpu/RenderPipelineState.h>
#include <igl/webgpu/SamplerState.h>
#include <igl/webgpu/ShaderModule.h>
#include <igl/webgpu/StateSanitizer.h>
#include <igl/webgpu/Texture.h>
#include <igl/webgpu/Timer.h>
#include <igl/webgpu/TimestampQueries.h>
#include <igl/webgpu/VertexInputState.h>

namespace igl::webgpu {

Device::Device(std::unique_ptr<WebGPUContext> ctx) :
  ctx_(std::move(ctx)),
  platformDevice_(*this),
  deviceFeatureSet_(ctx_ ? ctx_->getDevice() : nullptr),
  bindLayoutCache_(*ctx_) {
  IGL_DEBUG_ASSERT(ctx_ && ctx_->getDevice() != nullptr);
}

Device::~Device() = default;

Holder<BindGroupTextureHandle> Device::createBindGroup(
    const BindGroupTextureDesc& desc,
    const IRenderPipelineState* IGL_NULLABLE /*compatiblePipeline*/,
    Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  const auto handle = bindGroupTexturesPool_.create(BindGroupTextureDesc(desc));
  Result::setResult(outResult,
                    handle.empty() ? Result(Result::Code::RuntimeError, "Cannot create bind group")
                                   : Result());
  return {this, handle};
}

Holder<BindGroupBufferHandle> Device::createBindGroup(const BindGroupBufferDesc& desc,
                                                      Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  const auto handle = bindGroupBuffersPool_.create(BindGroupBufferDesc(desc));
  Result::setResult(outResult,
                    handle.empty() ? Result(Result::Code::RuntimeError, "Cannot create bind group")
                                   : Result());
  return {this, handle};
}

void Device::destroy(BindGroupTextureHandle handle) {
  if (!handle.empty()) {
    bindGroupTexturesPool_.destroy(handle);
  }
}

void Device::destroy(BindGroupBufferHandle handle) {
  if (!handle.empty()) {
    bindGroupBuffersPool_.destroy(handle);
  }
}

void Device::destroy(SamplerHandle /*handle*/) {}

std::shared_ptr<ICommandQueue> Device::createCommandQueue(const CommandQueueDesc& /*desc*/,
                                                          Result* IGL_NULLABLE outResult) noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  Result::setOk(outResult);
  return std::make_shared<CommandQueue>(*this);
}

std::unique_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                              Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  auto buffer = Buffer::create(*ctx_, desc, outResult);
  if (buffer && getResourceTracker()) {
    buffer->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  return buffer;
}

std::shared_ptr<IDepthStencilState> Device::createDepthStencilState(
    const DepthStencilStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  Result::setOk(outResult);
  return std::make_shared<DepthStencilState>(desc);
}

std::shared_ptr<ISamplerState> Device::createSamplerState(const SamplerStateDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  auto sampler = SamplerState::create(*ctx_, desc, outResult);
  if (sampler && getResourceTracker()) {
    sampler->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  return sampler;
}

std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  const TextureDesc sanitized = sanitize(desc);
  auto texture = Texture::create(*ctx_, deviceFeatureSet_, sanitized, outResult);
  if (texture && getResourceTracker()) {
    texture->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  return texture;
}

std::shared_ptr<ITexture> Device::createTextureView(std::shared_ptr<ITexture> texture,
                                                    const TextureViewDesc& desc,
                                                    Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  auto view =
      Texture::createView(std::static_pointer_cast<Texture>(std::move(texture)), desc, outResult);
  if (view && getResourceTracker()) {
    view->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  return view;
}

std::shared_ptr<ITimer> Device::createTimer(Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  auto timer = Timer::create(*ctx_, outResult);
  if (timer && getResourceTracker()) {
    timer->initResourceTracker(getResourceTracker());
  }
  return timer;
}

std::shared_ptr<ITimestampQueries> Device::createTimestampQueries(uint32_t maxTimestamps,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  auto queries = TimestampQueries::create(*ctx_, maxTimestamps, outResult);
  if (queries && getResourceTracker()) {
    queries->initResourceTracker(getResourceTracker());
  }
  return queries;
}

std::shared_ptr<IVertexInputState> Device::createVertexInputState(const VertexInputStateDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  Result result = validateVertexInputState(desc, deviceFeatureSet_.getLimits());
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setOk(outResult);
  return std::make_shared<VertexInputState>(desc);
}

std::shared_ptr<IFramebuffer> Device::createFramebuffer(const FramebufferDesc& desc,
                                                        Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  auto framebuffer = std::make_shared<Framebuffer>(*ctx_, desc);
  if (getResourceTracker()) {
    framebuffer->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  Result::setOk(outResult);
  return framebuffer;
}

base::IFramebufferInterop* IGL_NULLABLE
Device::createFramebufferInterop(const base::FramebufferInteropDesc& desc) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  auto framebuffer = createFramebufferFromBaseDesc(desc);
  if (!framebuffer) {
    return nullptr;
  }
  return new (std::nothrow) FramebufferWrapper(std::move(framebuffer));
}

std::shared_ptr<IComputePipelineState> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  return ComputePipelineState::create(*ctx_, deviceFeatureSet_, bindLayoutCache_, desc, outResult);
}

std::shared_ptr<IRenderPipelineState> Device::createRenderPipeline(const RenderPipelineDesc& desc,
                                                                   Result* IGL_NULLABLE
                                                                       outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  return RenderPipelineState::create(*ctx_, deviceFeatureSet_, bindLayoutCache_, desc, outResult);
}

std::unique_ptr<IShaderLibrary> Device::createShaderLibrary(const ShaderLibraryDesc& desc,
                                                            Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  if (desc.moduleInfo.empty() || !desc.input.isValid()) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Invalid shader library");
    return nullptr;
  }
  if (desc.input.type != ShaderInputType::String) {
    Result::setResult(outResult, Result::Code::Unsupported, "WebGPU shaders must be WGSL source");
    return nullptr;
  }
  WgslModule module;
  Result result = compileWgsl(*ctx_, desc.input.source, desc.debugName, module);
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  ++shaderCompilationCount_;
  std::vector<std::shared_ptr<IShaderModule>> modules;
  modules.reserve(desc.moduleInfo.size());
  for (const ShaderModuleInfo& info : desc.moduleInfo) {
    auto shaderModule = ShaderModule::create(module, info, outResult);
    if (!shaderModule) {
      return nullptr;
    }
    if (getResourceTracker()) {
      shaderModule->initResourceTracker(getResourceTracker(), desc.debugName);
    }
    modules.push_back(std::move(shaderModule));
  }
  auto library = std::make_unique<ShaderLibrary>(std::move(modules));
  if (getResourceTracker()) {
    library->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  Result::setOk(outResult);
  return library;
}

std::shared_ptr<IShaderModule> Device::createShaderModule(const ShaderModuleDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (failIfLost(outResult)) {
    return nullptr;
  }
  auto module = ShaderModule::create(*ctx_, desc, outResult);
  if (module) {
    ++shaderCompilationCount_;
    if (getResourceTracker()) {
      module->initResourceTracker(getResourceTracker(), desc.debugName);
    }
  }
  return module;
}

std::unique_ptr<IShaderStages> Device::createShaderStages(const ShaderStagesDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  auto stages = std::make_unique<ShaderStages>(desc);
  if (!stages->isValid()) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Invalid shader stages");
    return nullptr;
  }
  if (getResourceTracker()) {
    stages->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  Result::setOk(outResult);
  return stages;
}

const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return platformDevice_;
}

void* IGL_NULLABLE Device::getNativeDevice() const {
  return ctx_->getDevice();
}

bool Device::isDeviceLost() const noexcept {
  return ctx_->isDeviceLost();
}

bool Device::failIfLost(Result* IGL_NULLABLE outResult) const noexcept {
  if (!ctx_->isDeviceLost()) {
    return false;
  }
  Result::setResult(outResult, Result::Code::DeviceLost, "The WebGPU device was lost");
  return true;
}

bool Device::hasFeature(DeviceFeatures feature) const {
  return deviceFeatureSet_.hasFeature(feature);
}

bool Device::hasRequirement(DeviceRequirement requirement) const {
  return deviceFeatureSet_.hasRequirement(requirement);
}

bool Device::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  return deviceFeatureSet_.getFeatureLimits(featureLimits, result);
}

ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(
    TextureFormat format) const {
  return deviceFeatureSet_.getTextureFormatCapabilities(format);
}

ShaderVersion Device::getShaderVersion() const {
  return {.family = ShaderFamily::Wgsl, .majorVersion = 1, .minorVersion = 0};
}

BackendVersion Device::getBackendVersion() const {
  return {.flavor = BackendFlavor::WebGPU, .majorVersion = 1, .minorVersion = 0};
}

size_t Device::getCurrentDrawCount() const {
  return drawCount_.load(std::memory_order_relaxed);
}

size_t Device::getShaderCompilationCount() const {
  return shaderCompilationCount_;
}

} // namespace igl::webgpu
