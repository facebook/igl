/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/DeviceFeatures.h>
#include <igl/PlatformDevice.h>
#include <igl/Texture.h>
#include <utility>
#include <vector>

namespace igl {

struct BufferDesc;
struct ComputePipelineDesc;
struct DepthStencilStateDesc;
struct FramebufferDesc;
struct RenderPipelineDesc;
struct SamplerStateDesc;
struct ShaderLibraryDesc;
struct ShaderModuleDesc;
struct ShaderStages;
struct TextureDesc;
struct VertexInputStateDesc;
class IBuffer;
class ICommandQueue;
class ICommandBuffer;
class IComputePipelineState;
class IDepthStencilState;
class IDevice;
class IFramebuffer;
class IRenderPipelineState;
class ISamplerState;
class IShaderModule;
class ITexture;
class IVertexInputState;

/**
 * @brief Interface to a GPU that is used to draw graphics or do parallel computation.
 */
class IDevice : public ICapabilities {
 public:
  ~IDevice() override = default;

  /**
   * @brief Creates a command queue.
   * @see igl::CommandQueueDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Shared pointer to the created queue.
   */
  virtual std::shared_ptr<ICommandQueue> createCommandQueue(CommandQueueType type,
                                                            Result* outResult) = 0;

  /**
   * @brief Creates a buffer resource.
   * @see igl::BufferDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Unique pointer to the created buffer.
   */
  virtual std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                                Result* outResult) const noexcept = 0;

  /**
   * @brief Creates a depth stencil state.
   * @see igl::DepthStencilStateDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Shared pointer to the created depth stencil state.
   */
  virtual std::shared_ptr<IDepthStencilState> createDepthStencilState(
      const DepthStencilStateDesc& desc,
      Result* outResult) const = 0;

  /**
   * @brief Creates a sampler state.
   * @see igl::SamplerStateDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Shared pointer to the created sampler state.
   */
  virtual std::shared_ptr<ISamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                            Result* outResult) const = 0;

  /**
   * @brief Creates a texture resource.
   * @see igl::TextureDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Shared pointer to the created texture.
   */
  virtual std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                                  Result* outResult) const noexcept = 0;

  /**
   * @brief Creates a compute pipeline state.
   * @see igl::ComputePipelineDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Shared pointer to the created compute pipeline state.
   */
  virtual std::shared_ptr<IComputePipelineState> createComputePipeline(
      const ComputePipelineDesc& desc,
      Result* outResult) const = 0;

  /**
   * @brief Creates a render pipeline state.
   * @see igl::RenderPipelineDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Shared pointer to the created render pipeline state.
   */
  virtual std::shared_ptr<IRenderPipelineState> createRenderPipeline(const RenderPipelineDesc& desc,
                                                                     Result* outResult) const = 0;

  /**
   * @brief Creates a shader module from either source code or pre-compiled data.
   * @see igl::ShaderModuleDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Shared pointer to the created shader module.
   */
  virtual std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc,
                                                            Result* outResult) const = 0;

  /**
   * @brief Creates a frame buffer object.
   * @see igl::FramebufferDesc
   * @param desc Description for the desired resource.
   * @param outResult Pointer to where the result (success, failure, etc) is written. Can be null if
   * no reporting is desired.
   * @return Shared pointer to the created frame buffer.
   */
  virtual std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                          Result* outResult) = 0;

  /**
   * @brief Returns a platform-specific device. If the requested device type does not match that of
   * the actual underlying device, then null is returned.
   * @return Pointer to the underlying platform-specific device.
   */
  template<typename T, typename = std::enable_if_t<std::is_base_of<IPlatformDevice, T>::value>>
  T* getPlatformDevice() noexcept {
    return const_cast<T*>(static_cast<const IDevice*>(this)->getPlatformDevice<T>());
  }

  /**
   * @brief Returns a platform-specific device. If the requested device type does not match that of
   * the actual underlying device, then null is returned.
   * @return Pointer to the underlying platform-specific device.
   */
  template<typename T, typename = std::enable_if_t<std::is_base_of<IPlatformDevice, T>::value>>
  const T* getPlatformDevice() const noexcept {
    const IPlatformDevice& platformDevice = getPlatformDevice();
    if (platformDevice.isType(T::Type)) {
      return static_cast<const T*>(&platformDevice);
    }
    return nullptr;
  }

  /**
   * @brief Returns a platform-specific device. Returned value should not be held longer than the
   * original `IDevice`.
   * @return Pointer to the underlying platform-specific device.
   */
  IPlatformDevice& getPlatformDevice() noexcept {
    return const_cast<IPlatformDevice&>(static_cast<const IDevice*>(this)->getPlatformDevice());
  }

  /**
   * @brief Returns a platform-specific device. Returned value should not be held longer than the
   * original `IDevice`.
   * @return Pointer to the underlying platform-specific device.
   */
  virtual const IPlatformDevice& getPlatformDevice() const noexcept = 0;

  /**
   * @brief Returns the actual graphics API backing this IGL device (Metal, OpenGL, etc).
   * @return The type of the underlying backend.
   */
  virtual BackendType getBackendType() const = 0;

  virtual std::unique_ptr<ShaderStages> createShaderStages(const char* vs,
                                                           std::string debugNameVS,
                                                           const char* fs,
                                                           std::string debugNameFS,
                                                           Result* outResult = nullptr) const;
  virtual std::unique_ptr<ShaderStages> createShaderStages(const char* cs,
                                                           std::string debugName,
                                                           Result* outResult = nullptr) const;

 protected:
  TextureDesc sanitize(const TextureDesc& desc) const;
  IDevice() = default;
};

} // namespace igl
