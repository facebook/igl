/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/PlatformDevice.h>
#include <igl/Texture.h>

namespace igl::d3d12 {

class Device;

/// @brief Implements the igl::IPlatformDevice interface for D3D12
class PlatformDevice : public IPlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType kType = igl::PlatformDeviceType::D3D12;

  explicit PlatformDevice(Device& device);
  ~PlatformDevice() override = default;

  /// Creates a Depth Texture from the D3D12 swapchain
  /// @param width Width of the depth texture
  /// @param height Height of the depth texture
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::shared_ptr<ITexture> createTextureFromNativeDepth(uint32_t width,
                                                         uint32_t height,
                                                         Result* outResult);

  /// Creates a texture from the D3D12 swapchain back buffer
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::shared_ptr<ITexture> createTextureFromNativeDrawable(Result* outResult);

  /// Clear the cached textures
  void clear() {
    nativeDrawableTextures_.clear();
    nativeDepthTexture_ = nullptr;
  }

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override {
    return t == kType;
  }

 private:
  Device& device_;
  std::vector<std::shared_ptr<ITexture>> nativeDrawableTextures_;
  std::shared_ptr<ITexture> nativeDepthTexture_;
};

} // namespace igl::d3d12
