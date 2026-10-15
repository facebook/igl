/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <vector>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Device.h>
#include <igl/Framebuffer.h>
#include <igl/Texture.h>
#include <igl/Timer.h>

namespace iglu::gpu_benchmark::tests {

inline std::vector<std::string>& events() {
  static auto* const kEvents = new std::vector<std::string>();
  return *kEvents;
}

class FakeTexture final : public igl::ITexture {
 public:
  FakeTexture(igl::TextureFormat format,
              uint32_t width,
              uint32_t height,
              uint32_t samples = 1,
              igl::ResourceStorage storage = igl::ResourceStorage::Private,
              uint32_t layers = 1,
              uint32_t mips = 1) :
    ITexture(format),
    width_(width),
    height_(height),
    samples_(samples),
    storage_(storage),
    layers_(layers),
    mips_(mips) {}

  [[nodiscard]] igl::Dimensions getDimensions() const override {
    return igl::Dimensions{width_, height_, 1};
  }
  [[nodiscard]] uint32_t getNumLayers() const override {
    return layers_;
  }
  [[nodiscard]] igl::TextureType getType() const override {
    return igl::TextureType::TwoD;
  }
  [[nodiscard]] igl::TextureDesc::TextureUsage getUsage() const override {
    return igl::TextureDesc::TextureUsageBits::Attachment;
  }
  [[nodiscard]] uint32_t getSamples() const override {
    return samples_;
  }
  void generateMipmap(igl::ICommandQueue& /*cmdQueue*/,
                      const igl::TextureRangeDesc* /*range*/) const override {}
  void generateMipmap(igl::ICommandBuffer& /*cmdBuffer*/,
                      const igl::TextureRangeDesc* /*range*/) const override {}
  [[nodiscard]] uint32_t getNumMipLevels() const override {
    return mips_;
  }
  [[nodiscard]] bool isRequiredGenerateMipmap() const override {
    return false;
  }
  [[nodiscard]] uint64_t getTextureId() const override {
    return 0;
  }
  [[nodiscard]] void* getNativeImage() const override {
    return nullptr;
  }
  [[nodiscard]] void* getNativeImageView() const override {
    return nullptr;
  }
  [[nodiscard]] const igl::base::AttachmentInteropDesc& getDesc() const override {
    return desc_;
  }
  [[nodiscard]] igl::ResourceStorage getStorage() const override {
    return storage_;
  }

 private:
  uint32_t width_;
  uint32_t height_;
  uint32_t samples_;
  igl::ResourceStorage storage_;
  uint32_t layers_;
  uint32_t mips_;
  igl::base::AttachmentInteropDesc desc_{};
};

class FakeFramebuffer final : public igl::IFramebuffer {
 public:
  std::vector<std::shared_ptr<igl::ITexture>> colors;
  std::vector<std::shared_ptr<igl::ITexture>> colorResolves;
  std::shared_ptr<igl::ITexture> depth;
  std::shared_ptr<igl::ITexture> depthResolve;
  std::shared_ptr<igl::ITexture> stencil;
  igl::FramebufferMode mode = igl::FramebufferMode::Mono;

  [[nodiscard]] std::vector<size_t> getColorAttachmentIndices() const override {
    std::vector<size_t> indices;
    for (size_t i = 0; i < colors.size(); ++i) {
      if (colors[i] || (colorResolves.size() > i && colorResolves[i])) {
        indices.push_back(i);
      }
    }
    return indices;
  }
  [[nodiscard]] std::shared_ptr<igl::ITexture> getColorAttachment(size_t index) const override {
    return index < colors.size() ? colors[index] : nullptr;
  }
  [[nodiscard]] std::shared_ptr<igl::ITexture> getResolveColorAttachment(
      size_t index) const override {
    return index < colorResolves.size() ? colorResolves[index] : nullptr;
  }
  [[nodiscard]] std::shared_ptr<igl::ITexture> getDepthAttachment() const override {
    return depth;
  }
  [[nodiscard]] std::shared_ptr<igl::ITexture> getResolveDepthAttachment() const override {
    return depthResolve;
  }
  [[nodiscard]] std::shared_ptr<igl::ITexture> getStencilAttachment() const override {
    return stencil;
  }
  [[nodiscard]] igl::FramebufferMode getMode() const override {
    return mode;
  }
  [[nodiscard]] bool isSwapchainBound() const override {
    return false;
  }
  void copyBytesColorAttachment(igl::ICommandQueue& /*cmdQueue*/,
                                size_t /*index*/,
                                void* /*pixelBytes*/,
                                const igl::TextureRangeDesc& /*range*/,
                                size_t /*bytesPerRow*/) const override {}
  void copyBytesDepthAttachment(igl::ICommandQueue& /*cmdQueue*/,
                                void* /*pixelBytes*/,
                                const igl::TextureRangeDesc& /*range*/,
                                size_t /*bytesPerRow*/) const override {}
  void copyBytesStencilAttachment(igl::ICommandQueue& /*cmdQueue*/,
                                  void* /*pixelBytes*/,
                                  const igl::TextureRangeDesc& /*range*/,
                                  size_t /*bytesPerRow*/) const override {}
  void copyTextureColorAttachment(igl::ICommandQueue& /*cmdQueue*/,
                                  size_t /*index*/,
                                  std::shared_ptr<igl::ITexture> /*destTexture*/,
                                  const igl::TextureRangeDesc& /*range*/) const override {}
  void updateDrawable(std::shared_ptr<igl::ITexture> /*texture*/) override {}
  void updateDrawable(igl::SurfaceTextures /*surfaceTextures*/) override {}
  void updateResolveAttachment(std::shared_ptr<igl::ITexture> /*texture*/) override {}
};

class FakeTimer final : public igl::ITimer {
 public:
  uint32_t pollsUntilAvailable = 0;
  uint64_t nanos = 0;
  mutable uint32_t pollCount = 0;

  [[nodiscard]] uint64_t getElapsedTimeNanos() const override {
    return nanos;
  }
  [[nodiscard]] bool resultsAvailable() const override {
    return ++pollCount > pollsUntilAvailable;
  }
};

class FakeCommandBuffer final : public igl::ICommandBuffer {
 public:
  explicit FakeCommandBuffer(igl::CommandBufferDesc desc) : ICommandBuffer(std::move(desc)) {}
  std::unique_ptr<igl::IRenderCommandEncoder> createRenderCommandEncoder(
      const igl::RenderPassDesc& /*renderPass*/,
      const std::shared_ptr<igl::IFramebuffer>& /*framebuffer*/,
      const igl::Dependencies& /*dependencies*/,
      igl::Result* IGL_NULLABLE /*outResult*/) override {
    return nullptr;
  }
  std::unique_ptr<igl::IComputeCommandEncoder> createComputeCommandEncoder() override {
    return nullptr;
  }
  void present(const std::shared_ptr<igl::ITexture>& /*surface*/) const override {}
  void waitUntilScheduled() override {}
  void waitUntilCompleted() override {}
  void pushDebugGroupLabel(const char* IGL_NONNULL /*label*/,
                           const igl::Color& /*color*/) const override {}
  void popDebugGroupLabel() const override {}
  void copyBuffer(igl::IBuffer& /*src*/,
                  igl::IBuffer& /*dst*/,
                  uint64_t /*srcOffset*/,
                  uint64_t /*dstOffset*/,
                  uint64_t /*size*/) override {}
  void copyTextureToBuffer(igl::ITexture& /*src*/,
                           igl::IBuffer& /*dst*/,
                           uint64_t /*dstOffset*/,
                           uint32_t /*level*/,
                           uint32_t /*layer*/) override {}
};

class FakeQueue final : public igl::ICommandQueue {
 public:
  std::vector<igl::CommandBufferDesc> descs;

  std::shared_ptr<igl::ICommandBuffer> createCommandBuffer(const igl::CommandBufferDesc& desc,
                                                           igl::Result* IGL_NULLABLE
                                                               outResult) override {
    events().emplace_back("commandBuffer");
    descs.push_back(desc);
    igl::Result::setOk(outResult);
    return std::make_shared<FakeCommandBuffer>(desc);
  }
  igl::SubmitHandle submit(const igl::ICommandBuffer& /*commandBuffer*/,
                           bool /*endOfFrame*/) override {
    return 0;
  }
};

class FakeDevice final : public igl::IDevice {
 public:
  bool hasTimers = true;
  bool timerCreationFails = false;
  mutable std::vector<std::shared_ptr<FakeTimer>> createdTimers;

  [[nodiscard]] bool hasFeature(igl::DeviceFeatures feature) const final {
    return feature == igl::DeviceFeatures::Timers && hasTimers;
  }
  // NOLINTNEXTLINE(bugprone-exception-escape)
  std::shared_ptr<igl::ITimer> createTimer(
      igl::Result* IGL_NULLABLE outResult) const noexcept final {
    events().emplace_back("timer");
    if (timerCreationFails) {
      igl::Result::setResult(outResult, igl::Result::Code::RuntimeError, "no timer");
      return nullptr;
    }
    igl::Result::setOk(outResult);
    auto timer = std::make_shared<FakeTimer>();
    createdTimers.push_back(timer);
    return timer;
  }

  [[nodiscard]] igl::BackendType getBackendType() const final {
    return igl::BackendType::Vulkan;
  }
  [[nodiscard]] const igl::IPlatformDevice& getPlatformDevice() const noexcept final {
    return platformDevice_;
  }
  igl::Holder<igl::BindGroupTextureHandle> createBindGroup(
      const igl::BindGroupTextureDesc& /*desc*/,
      const igl::IRenderPipelineState* IGL_NULLABLE /*compatiblePipeline*/,
      igl::Result* IGL_NULLABLE /*outResult*/) final {
    return {};
  }
  igl::Holder<igl::BindGroupBufferHandle> createBindGroup(
      const igl::BindGroupBufferDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) final {
    return {};
  }
  void destroy(igl::BindGroupTextureHandle /*handle*/) final {}
  void destroy(igl::BindGroupBufferHandle /*handle*/) final {}
  void destroy(igl::SamplerHandle /*handle*/) final {}
  [[nodiscard]] bool hasRequirement(igl::DeviceRequirement /*requirement*/) const final {
    return false;
  }
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilities(
      igl::TextureFormat /*format*/) const final {
    return TextureFormatCapabilityBits::Unsupported;
  }
  [[nodiscard]] bool getFeatureLimits(igl::DeviceFeatureLimits /*featureLimits*/,
                                      size_t& /*result*/) const final {
    return false;
  }
  [[nodiscard]] igl::ShaderVersion getShaderVersion() const final {
    return {};
  }
  [[nodiscard]] igl::BackendVersion getBackendVersion() const final {
    return {};
  }
  std::shared_ptr<igl::ICommandQueue> createCommandQueue(
      const igl::CommandQueueDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) noexcept final {
    return nullptr;
  }
  std::unique_ptr<igl::IBuffer> createBuffer(
      const igl::BufferDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const noexcept final {
    return nullptr;
  }
  std::shared_ptr<igl::IDepthStencilState> createDepthStencilState(
      const igl::DepthStencilStateDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<igl::ISamplerState> createSamplerState(
      const igl::SamplerStateDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<igl::ITexture> createTexture(
      const igl::TextureDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const noexcept final {
    return nullptr;
  }
  std::shared_ptr<igl::ITexture> createTextureView(
      std::shared_ptr<igl::ITexture> /*texture*/,
      const igl::TextureViewDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const noexcept final {
    return nullptr;
  }
  std::shared_ptr<igl::IVertexInputState> createVertexInputState(
      const igl::VertexInputStateDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<igl::IComputePipelineState> createComputePipeline(
      const igl::ComputePipelineDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<igl::IRenderPipelineState> createRenderPipeline(
      const igl::RenderPipelineDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<igl::IShaderModule> createShaderModule(
      const igl::ShaderModuleDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<igl::IFramebuffer> createFramebuffer(
      const igl::FramebufferDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) final {
    return nullptr;
  }
  std::unique_ptr<igl::IShaderLibrary> createShaderLibrary(
      const igl::ShaderLibraryDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::unique_ptr<igl::IShaderStages> createShaderStages(
      const igl::ShaderStagesDesc& /*desc*/,
      igl::Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  [[nodiscard]] size_t getCurrentDrawCount() const final {
    return 0;
  }
  [[nodiscard]] size_t getShaderCompilationCount() const final {
    return 0;
  }
  [[nodiscard]] void* IGL_NULLABLE getNativeDevice() const final {
    return nullptr;
  }

 private:
  class NullPlatformDevice final : public igl::IPlatformDevice {
   protected:
    [[nodiscard]] bool isType(igl::PlatformDeviceType /*t*/) const noexcept final {
      return false;
    }
  };
  NullPlatformDevice platformDevice_;
};

inline std::shared_ptr<FakeTexture> makeTexture(
    igl::TextureFormat format,
    uint32_t width,
    uint32_t height,
    uint32_t samples = 1,
    igl::ResourceStorage storage = igl::ResourceStorage::Private,
    uint32_t layers = 1,
    uint32_t mips = 1) {
  return std::make_shared<FakeTexture>(format, width, height, samples, storage, layers, mips);
}

} // namespace iglu::gpu_benchmark::tests
