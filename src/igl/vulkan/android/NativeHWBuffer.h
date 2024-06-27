/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/android/NativeHWBuffer.h>

#if IGL_PLATFORM_ANDROID && __ANDROID_MIN_SDK_VERSION__ >= 26

#include <igl/vulkan/Texture.h>

struct AHardwareBuffer;

namespace igl::vulkan::android {

typedef void AHardwareBufferHelper;

// TextureBuffer encapsulates Vulkan textures
class NativeHWTextureBuffer : public igl::android::INativeHWTextureBuffer, public Texture {
  friend class igl::vulkan::PlatformDevice;
  using Super = Texture;

 public:
  NativeHWTextureBuffer(const igl::vulkan::Device& device, TextureFormat format);
  ~NativeHWTextureBuffer() override;

 protected:
  // Texture overrides
  Result create(const TextureDesc& desc) override;

  // INativeHWTextureBuffer overrides
  Result createTextureInternal(const TextureDesc& desc, AHardwareBuffer* buffer) override;
};

} // namespace igl::vulkan::android

#endif
