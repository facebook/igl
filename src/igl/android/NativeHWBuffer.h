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
  struct LockGuard {
   public:
    ~LockGuard();

    LockGuard(const LockGuard&) = delete;
    LockGuard(LockGuard&& g);

   private:
    friend class INativeHWTextureBuffer;

    LockGuard(const INativeHWTextureBuffer* hwBufferOwner = nullptr);

    const INativeHWTextureBuffer* hwBufferOwner_;
  };

  struct RangeDesc : TextureRangeDesc {
    size_t stride = 0;
  };

  virtual ~INativeHWTextureBuffer();

  Result attachHWBuffer(AHardwareBuffer* buffer);
  Result createHWBuffer(const TextureDesc& desc, bool hasStorageAlready, bool surfaceComposite);

  LockGuard lockHWBuffer(std::byte* IGL_NULLABLE* IGL_NONNULL dst,
                         RangeDesc& outRange,
                         Result* outResult) const;

  Result lockHWBuffer(std::byte* IGL_NULLABLE* IGL_NONNULL dst, RangeDesc& outRange) const;
  Result unlockHWBuffer() const;

  AHardwareBuffer* getHardwareBuffer();

 protected:
  virtual Result createTextureInternal(const TextureDesc& desc, AHardwareBuffer* buffer) = 0;

  AHardwareBuffer* hwBuffer_ = nullptr;
};

// utils

uint32_t getNativeHWFormat(TextureFormat iglFormat);
uint32_t getNativeHWBufferUsage(TextureDesc::TextureUsage iglUsage);

TextureFormat getIglFormat(uint32_t nativeFormat);
TextureDesc::TextureUsage getIglBufferUsage(uint32_t nativeUsage);

Result allocateNativeHWBuffer(const TextureDesc& desc,
                              bool surfaceComposite,
                              AHardwareBuffer** buffer);

} // namespace igl::android

#endif
