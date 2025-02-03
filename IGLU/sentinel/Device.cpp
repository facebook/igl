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

bool Device::hasFeature(igl::DeviceFeatures /*feature*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

bool Device::hasRequirement(igl::DeviceRequirement /*requirement*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

igl::ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(
    igl::TextureFormat /*format*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return TextureFormatCapabilityBits::Unsupported;
}

bool Device::getFeatureLimits(igl::DeviceFeatureLimits /*featureLimits*/,
                              size_t& /*result*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

igl::ShaderVersion Device::getShaderVersion() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return {};
}

igl::BackendVersion Device::getBackendVersion() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return {};
}

std::shared_ptr<igl::ICommandQueue> Device::createCommandQueue(
    const igl::CommandQueueDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::unique_ptr<igl::IBuffer> Device::createBuffer(
    const igl::BufferDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IDepthStencilState> Device::createDepthStencilState(
    const igl::DepthStencilStateDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ISamplerState> Device::createSamplerState(
    const igl::SamplerStateDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::ITexture> Device::createTexture(const igl::TextureDesc& /*desc*/,
                                                     igl::Result* IGL_NULLABLE
                                                     /*outResult*/) const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IVertexInputState> Device::createVertexInputState(
    const igl::VertexInputStateDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IComputePipelineState> Device::createComputePipeline(
    const igl::ComputePipelineDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IRenderPipelineState> Device::createRenderPipeline(
    const igl::RenderPipelineDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IShaderModule> Device::createShaderModule(
    const igl::ShaderModuleDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::shared_ptr<igl::IFramebuffer> Device::createFramebuffer(
    const igl::FramebufferDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

const igl::IPlatformDevice& Device::getPlatformDevice() const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return platformDevice_;
}

bool Device::verifyScope() {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

igl::BackendType Device::getBackendType() const {
  return igl::BackendType::Invalid;
}

size_t Device::getCurrentDrawCount() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return 0;
}

std::unique_ptr<igl::IShaderLibrary> Device::createShaderLibrary(
    const igl::ShaderLibraryDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

void Device::updateSurface(void* IGL_NONNULL /*nativeWindowType*/) {}
std::unique_ptr<igl::IShaderStages> Device::createShaderStages(
    const igl::ShaderStagesDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

igl::Holder<igl::BindGroupTextureHandle> Device::createBindGroup(
    const igl::BindGroupTextureDesc& /*desc*/,
    const igl::IRenderPipelineState* IGL_NULLABLE /*compatiblePipeline*/,
    igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);

  return {};
}

igl::Holder<igl::BindGroupBufferHandle> Device::createBindGroup(
    const igl::BindGroupBufferDesc& /*desc*/,
    igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);

  return {};
}

void Device::destroy(igl::BindGroupTextureHandle /*handle*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Device::destroy(igl::BindGroupBufferHandle /*handle*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void Device::destroy(igl::SamplerHandle /*handle*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

} // namespace iglu::sentinel
