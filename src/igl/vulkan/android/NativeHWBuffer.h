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

#include <jni.h>

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

  // INativeHWTextureBuffer overrides
  Result createHWBuffer(const TextureDesc& desc,
                        bool hasStorageAlready,
                        bool surfaceComposite) override;

 protected:
  // Texture overrides
  Result create(const TextureDesc& desc) override;
};

} // namespace igl::vulkan::android

#endif
