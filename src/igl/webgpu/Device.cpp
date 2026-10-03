/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Device.h>

#include <new>
#include <string>
#include <utility>
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
#include <igl/VertexInputState.h>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/CommandQueue.h>
#include <igl/webgpu/Framebuffer.h>
#include <igl/webgpu/SamplerState.h>
#include <igl/webgpu/ShaderModule.h>
#include <igl/webgpu/Texture.h>

namespace igl::webgpu {

namespace {

void setUnimplemented(Result* IGL_NULLABLE outResult, const char* what) {
  Result::setResult(outResult, Result::Code::Unimplemented, std::string(what) + " (WebGPU)");
}

} // namespace

Device::Device(std::unique_ptr<WebGPUContext> ctx) :
  ctx_(std::move(ctx)),
  platformDevice_(*this),
  deviceFeatureSet_(ctx_ ? ctx_->getDevice() : nullptr) {
  IGL_DEBUG_ASSERT(ctx_ && ctx_->getDevice() != nullptr);
}

Device::~Device() = default;

Holder<BindGroupTextureHandle> Device::createBindGroup(
    const BindGroupTextureDesc& /*desc*/,
    const IRenderPipelineState* IGL_NULLABLE /*compatiblePipeline*/,
    Result* IGL_NULLABLE outResult) {
  setUnimplemented(outResult, "createBindGroup()");
  return {};
}

Holder<BindGroupBufferHandle> Device::createBindGroup(const BindGroupBufferDesc& /*desc*/,
                                                      Result* IGL_NULLABLE outResult) {
  setUnimplemented(outResult, "createBindGroup()");
  return {};
}

void Device::destroy(BindGroupTextureHandle /*handle*/) {}

void Device::destroy(BindGroupBufferHandle /*handle*/) {}

void Device::destroy(SamplerHandle /*handle*/) {}

std::shared_ptr<ICommandQueue> Device::createCommandQueue(const CommandQueueDesc& /*desc*/,
                                                          Result* IGL_NULLABLE outResult) noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  Result::setOk(outResult);
  return std::make_shared<CommandQueue>(*ctx_);
}

std::unique_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                              Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  auto buffer = Buffer::create(*ctx_, desc, outResult);
  if (buffer && getResourceTracker()) {
    buffer->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  return buffer;
}

std::shared_ptr<IDepthStencilState> Device::createDepthStencilState(
    const DepthStencilStateDesc& /*desc*/,
    Result* IGL_NULLABLE outResult) const {
  setUnimplemented(outResult, "createDepthStencilState()");
  return nullptr;
}

std::shared_ptr<ISamplerState> Device::createSamplerState(const SamplerStateDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  auto sampler = SamplerState::create(*ctx_, desc, outResult);
  if (sampler && getResourceTracker()) {
    sampler->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  return sampler;
}

std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
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
  auto view =
      Texture::createView(std::static_pointer_cast<Texture>(std::move(texture)), desc, outResult);
  if (view && getResourceTracker()) {
    view->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  return view;
}

std::shared_ptr<ITimer> Device::createTimer(Result* IGL_NULLABLE outResult) const noexcept {
  setUnimplemented(outResult, "createTimer()");
  return nullptr;
}

std::shared_ptr<IVertexInputState> Device::createVertexInputState(
    const VertexInputStateDesc& /*desc*/,
    Result* IGL_NULLABLE outResult) const {
  setUnimplemented(outResult, "createVertexInputState()");
  return nullptr;
}

std::shared_ptr<IFramebuffer> Device::createFramebuffer(const FramebufferDesc& desc,
                                                        Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
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
    const ComputePipelineDesc& /*desc*/,
    Result* IGL_NULLABLE outResult) const {
  setUnimplemented(outResult, "createComputePipeline()");
  return nullptr;
}

std::shared_ptr<IRenderPipelineState> Device::createRenderPipeline(
    const RenderPipelineDesc& /*desc*/,
    Result* IGL_NULLABLE outResult) const {
  setUnimplemented(outResult, "createRenderPipeline()");
  return nullptr;
}

std::unique_ptr<IShaderLibrary> Device::createShaderLibrary(const ShaderLibraryDesc& /*desc*/,
                                                            Result* IGL_NULLABLE outResult) const {
  setUnimplemented(outResult, "createShaderLibrary()");
  return nullptr;
}

std::shared_ptr<IShaderModule> Device::createShaderModule(const ShaderModuleDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
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
  return 0;
}

size_t Device::getShaderCompilationCount() const {
  return shaderCompilationCount_;
}

} // namespace igl::webgpu
