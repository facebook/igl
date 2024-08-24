/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/PlatformDevice.h>
#include <igl/Texture.h>
#include <igl/vulkan/Common.h>

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
struct AHardwareBuffer;
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

namespace igl {
class ITexture;

namespace vulkan {

class Device;
class VulkanTexture;

/// @brief Implements the igl::IPlatformDevice interface
class PlatformDevice : public IPlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType Type = igl::PlatformDeviceType::Vulkan;

  explicit PlatformDevice(Device& device);
  ~PlatformDevice() override = default;

  /// Creates a Depth Texture through the underlying VulkanSwapChain
  /// This currently is for development purposes only and will be removed in
  /// the future;
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::shared_ptr<ITexture> createTextureFromNativeDepth(uint32_t width,
                                                         uint32_t height,
                                                         Result* outResult);

  /// Creates a texture from a native drawable surface
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::shared_ptr<ITexture> createTextureFromNativeDrawable(Result* outResult);

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
  std::shared_ptr<ITexture> createTextureWithSharedMemory(const TextureDesc& desc,
                                                          Result* outResult) const;
  std::shared_ptr<ITexture> createTextureWithSharedMemory(AHardwareBuffer* buffer,
                                                          Result* outResult) const;
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

  /// @param handle The handle to the GPU Fence
  /// @return The Vulkan fence associated with the handle
  [[nodiscard]] VkFence getVkFenceFromSubmitHandle(SubmitHandle handle) const;

  /// Waits on the GPU Fence associated with the handle
  /// @param handle The handle to the GPU Fence
  void waitOnSubmitHandle(SubmitHandle handle, uint64_t timeoutNanoseconds = UINT64_MAX) const;

  /// Android only for now - Creates the file descriptor for the underlying VkFence
  /// @param handle The handle to the GPU Fence
  /// @return The fd for the Vulkan Fence associated with the handle
#if defined(IGL_PLATFORM_ANDROID) && defined(VK_KHR_external_fence_fd)
  [[nodiscard]] int getFenceFdFromSubmitHandle(SubmitHandle handle) const;
#endif

  /// Clear the cached textures
  void clear() {
    nativeDrawableTextures_.clear();
    nativeDepthTexture_ = nullptr;
  }

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override {
    return t == Type;
  }

 private:
  Device& device_;
  std::vector<std::shared_ptr<ITexture>> nativeDrawableTextures_;
  std::shared_ptr<ITexture> nativeDepthTexture_;
};

} // namespace vulkan
} // namespace igl
