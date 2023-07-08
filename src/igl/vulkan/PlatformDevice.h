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

namespace igl {
class ITexture;

namespace vulkan {

class Device;
class VulkanTexture;

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

 protected:
  bool isType(PlatformDeviceType t) const noexcept override {
    return t == Type;
  }

 private:
  Device& device_;
  std::vector<std::shared_ptr<ITexture>> nativeDrawableTextures_;
  std::shared_ptr<ITexture> nativeDepthTexture_;
};

} // namespace vulkan
} // namespace igl
