/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// The umbrella header rather than Device.h alone: IDevice's interface names descriptor and enum
// types this class has to define members for, and Device.h only forward-declares them.
#include <igl/IGL.h>

namespace igl::tests {

// ManagedUniformBuffer exposes neither the descriptor it builds nor the buffer it creates, so the
// only way to pin what it asks createBuffer() for is to hand it a device that records the request.
// iglu::sentinel::Device is final, so it cannot be decorated for this.
//
// Constructed with a delegate, the request is also forwarded to a real device and the buffer that
// comes back is kept, so a test can inspect what the backend actually built. Feature answers always
// come from this object, so the capability gate under test is driven by the test rather than by
// whatever the host device happens to support.
class RecordingDevice final : public IDevice {
 public:
  RecordingDevice() = default;
  explicit RecordingDevice(IDevice& delegate) : delegate_(&delegate) {}

  bool reportsBufferNoCopy = true;
  bool reportsBufferRing = false;

  // Recorded as scalars rather than as a BufferDesc copy: createBuffer() is noexcept, and copying
  // the descriptor's std::string debugName could throw.
  mutable size_t createBufferCount = 0;
  mutable BufferDesc::BufferType recordedType = 0;
  mutable BufferDesc::BufferAPIHint recordedHint = 0;
  // Non-owning: the object under test owns the buffer. Stays null without a delegate.
  mutable IBuffer* IGL_NULLABLE createdBuffer = nullptr;

  std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                        Result* IGL_NULLABLE outResult) const noexcept final {
    ++createBufferCount;
    recordedType = desc.type;
    recordedHint = desc.hint;
    if (delegate_ == nullptr) {
      Result::setOk(outResult);
      return nullptr;
    }
    auto buffer = delegate_->createBuffer(desc, outResult);
    createdBuffer = buffer.get();
    return buffer;
  }

  [[nodiscard]] bool hasFeature(DeviceFeatures feature) const final {
    // Not a switch: IGL builds with -Wswitch-enum, which would require every DeviceFeatures
    // enumerator to be listed here and relisted whenever one is added.
    if (feature == DeviceFeatures::BufferNoCopy) {
      return reportsBufferNoCopy;
    }
    if (feature == DeviceFeatures::BufferRing) {
      return reportsBufferRing;
    }
    return false;
  }

  // Any backend other than OpenGL reaches createBuffer(), and only Metal takes the Apple-only
  // page-aligned allocation branch, so this keeps the test host-independent.
  [[nodiscard]] BackendType getBackendType() const final {
    return BackendType::Vulkan;
  }

  [[nodiscard]] const IPlatformDevice& getPlatformDevice() const noexcept final {
    return platformDevice_;
  }

  // Remainder of IDevice: unreachable from ManagedUniformBuffer.
  Holder<BindGroupTextureHandle> createBindGroup(
      const BindGroupTextureDesc& /*desc*/,
      const IRenderPipelineState* IGL_NULLABLE /*compatiblePipeline*/,
      Result* IGL_NULLABLE /*outResult*/) final {
    return {};
  }
  Holder<BindGroupBufferHandle> createBindGroup(const BindGroupBufferDesc& /*desc*/,
                                                Result* IGL_NULLABLE /*outResult*/) final {
    return {};
  }
  void destroy(BindGroupTextureHandle /*handle*/) final {}
  void destroy(BindGroupBufferHandle /*handle*/) final {}
  void destroy(SamplerHandle /*handle*/) final {}
  [[nodiscard]] bool hasRequirement(DeviceRequirement /*requirement*/) const final {
    return false;
  }
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilities(
      TextureFormat /*format*/) const final {
    return TextureFormatCapabilityBits::Unsupported;
  }
  [[nodiscard]] bool getFeatureLimits(DeviceFeatureLimits /*featureLimits*/,
                                      size_t& /*result*/) const final {
    return false;
  }
  [[nodiscard]] ShaderVersion getShaderVersion() const final {
    return {};
  }
  [[nodiscard]] BackendVersion getBackendVersion() const final {
    return {};
  }
  std::shared_ptr<ICommandQueue> createCommandQueue(const CommandQueueDesc& /*desc*/,
                                                    Result* IGL_NULLABLE
                                                    /*outResult*/) noexcept final {
    return nullptr;
  }
  std::shared_ptr<IDepthStencilState> createDepthStencilState(
      const DepthStencilStateDesc& /*desc*/,
      Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<ISamplerState> createSamplerState(
      const SamplerStateDesc& /*desc*/,
      Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<ITexture> createTexture(const TextureDesc& /*desc*/,
                                          Result* IGL_NULLABLE /*outResult*/) const noexcept final {
    return nullptr;
  }
  std::shared_ptr<ITexture> createTextureView(std::shared_ptr<ITexture> /*texture*/,
                                              const TextureViewDesc& /*desc*/,
                                              Result* IGL_NULLABLE
                                              /*outResult*/) const noexcept final {
    return nullptr;
  }
  std::shared_ptr<IVertexInputState> createVertexInputState(
      const VertexInputStateDesc& /*desc*/,
      Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<IComputePipelineState> createComputePipeline(
      const ComputePipelineDesc& /*desc*/,
      Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<IRenderPipelineState> createRenderPipeline(
      const RenderPipelineDesc& /*desc*/,
      Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<IShaderModule> createShaderModule(
      const ShaderModuleDesc& /*desc*/,
      Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& /*desc*/,
                                                  Result* IGL_NULLABLE /*outResult*/) final {
    return nullptr;
  }
  std::shared_ptr<ITimer> createTimer(Result* IGL_NULLABLE /*outResult*/) const noexcept final {
    return nullptr;
  }
  std::unique_ptr<IShaderLibrary> createShaderLibrary(
      const ShaderLibraryDesc& /*desc*/,
      Result* IGL_NULLABLE /*outResult*/) const final {
    return nullptr;
  }
  std::unique_ptr<IShaderStages> createShaderStages(
      const ShaderStagesDesc& /*desc*/,
      Result* IGL_NULLABLE /*outResult*/) const final {
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
  class NullPlatformDevice final : public IPlatformDevice {
   protected:
    [[nodiscard]] bool isType(PlatformDeviceType /*t*/) const noexcept final {
      return false;
    }
  };

  NullPlatformDevice platformDevice_;
  IDevice* IGL_NULLABLE delegate_ = nullptr;
};

} // namespace igl::tests
