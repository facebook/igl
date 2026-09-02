/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include <IGLU/sentinel/Device.h>

#include <IGLU/sentinel/Assert.h>
#include <IGLU/sentinel/PlatformDevice.h>
#include <igl/IGL.h>

namespace iglu::sentinel {

Device::Device(bool shouldAssert) : platformDevice_(shouldAssert), shouldAssert_(shouldAssert) {}

bool Device::hasFeatureImpl(igl::DeviceFeatures /*feature*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

bool Device::hasRequirementImpl(igl::DeviceRequirement /*requirement*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

igl::ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilitiesImpl(
    igl::TextureFormat /*format*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return TextureFormatCapabilityBits::Unsupported;
}

bool Device::getFeatureLimitsImpl(igl::DeviceFeatureLimits /*featureLimits*/,
                                  size_t& /*result*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

igl::ShaderVersion Device::getShaderVersionImpl() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return {};
}

igl::BackendVersion Device::getBackendVersionImpl() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return {};
}

std::shared_ptr<igl::ICommandQueue> Device::createCommandQueueImpl(
    const igl::CommandQueueDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::unique_ptr<igl::IBuffer> Device::createBufferImpl(
    const igl::BufferDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IDepthStencilState> Device::createDepthStencilStateImpl(
    const igl::DepthStencilStateDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ISamplerState> Device::createSamplerStateImpl(
    const igl::SamplerStateDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ITexture> Device::createTextureImpl(const igl::TextureDesc& /*desc*/,
                                                         igl::Result* IGL_NULLABLE
                                                         /*outResult*/) const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ITexture> Device::createTextureViewImpl(
    std::shared_ptr<igl::ITexture> texture,
    const igl::TextureViewDesc& desc,
    igl::Result* IGL_NULLABLE /*outResult*/) const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IVertexInputState> Device::createVertexInputStateImpl(
    const igl::VertexInputStateDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IComputePipelineState> Device::createComputePipelineImpl(
    const igl::ComputePipelineDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IRenderPipelineState> Device::createRenderPipelineImpl(
    const igl::RenderPipelineDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IShaderModule> Device::createShaderModuleImpl(
    const igl::ShaderModuleDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IFramebuffer> Device::createFramebufferImpl(
    const igl::FramebufferDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

const igl::IPlatformDevice& Device::getPlatformDeviceImpl() const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return platformDevice_;
}

bool Device::verifyScopeImpl() {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

igl::BackendType Device::getBackendTypeImpl() const {
  return igl::BackendType::Invalid;
}

size_t Device::getCurrentDrawCountImpl() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return 0;
}

size_t Device::getShaderCompilationCountImpl() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return 0;
}

std::unique_ptr<igl::IShaderLibrary> Device::createShaderLibraryImpl(
    const igl::ShaderLibraryDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

void Device::updateSurfaceImpl(void* IGL_NONNULL /*nativeWindowType*/) {}
std::unique_ptr<igl::IShaderStages> Device::createShaderStagesImpl(
    const igl::ShaderStagesDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

igl::Holder<igl::BindGroupTextureHandle> Device::createBindGroupImpl(
    const igl::BindGroupTextureDesc& /*desc*/,
    const igl::IRenderPipelineState* IGL_NULLABLE /*compatiblePipeline*/,
    igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);

  return {};
}

igl::Holder<igl::BindGroupBufferHandle> Device::createBindGroupImpl(
    const igl::BindGroupBufferDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);

  return {};
}

std::shared_ptr<igl::ITimer> Device::createTimerImpl(
    igl::Result* IGL_NULLABLE /*outResult*/) const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);

  return {};
}

void Device::destroyImpl(igl::BindGroupTextureHandle /*handle*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Device::destroyImpl(igl::BindGroupBufferHandle /*handle*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Device::destroyImpl(igl::SamplerHandle /*handle*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

} // namespace iglu::sentinel
