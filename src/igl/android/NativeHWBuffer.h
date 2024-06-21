/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Core.h>

#if IGL_PLATFORM_ANDROID && __ANDROID_MIN_SDK_VERSION__ >= 26

#include <igl/Texture.h>
#include <igl/TextureFormat.h>

struct AHardwareBuffer;

namespace igl::android {

class INativeHWTextureBuffer {
 public:
  struct RangeDesc : TextureRangeDesc {
    size_t stride = 0;
  };

  virtual ~INativeHWTextureBuffer();

  virtual Result createHWBuffer(const TextureDesc& desc,
                                bool hasStorageAlready,
                                bool surfaceComposite) = 0;

  virtual Result lockHWBuffer(std::byte* IGL_NULLABLE* IGL_NONNULL dst, RangeDesc& outRange) const;
  virtual Result unlockHWBuffer() const;

 protected:
  AHardwareBuffer* hwBuffer_ = nullptr;
  bool isHwBufferExternal_ = false;
};

// utils

uint32_t getNativeHWFormat(TextureFormat iglFormat);
uint32_t getNativeHWBufferUsage(TextureDesc::TextureUsage usage);
Result allocateNativeHWBuffer(const TextureDesc& desc,
                              bool surfaceComposite,
                              AHardwareBuffer** buffer);

} // namespace igl::android

#endif
