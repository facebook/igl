/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/TextureBufferBase.h>

#if defined(__ANDROID_API__) && __ANDROID_MIN_SDK_VERSION__ >= 26

struct AHardwareBuffer;

namespace igl::opengl::egl::android {

typedef void AHardwareBufferHelper;

// TextureBuffer encapsulates OpenGL textures
class NativeHWTextureBuffer : public TextureBufferBase {
  using Super = TextureBufferBase;

 public:
  struct RangeDesc : TextureRangeDesc {
    size_t stride = 0;
  };

  NativeHWTextureBuffer(IContext& context, TextureFormat format) : Super(context, format) {}
  ~NativeHWTextureBuffer() override;

  // ITexture overrides
  Result upload(const TextureRangeDesc& range,
                const void* data,
                size_t bytesPerRow = 0) const override;

  // Texture overrides
  Result create(const TextureDesc& desc, bool hasStorageAlready) override;
  void bind() override;
  void bindImage(size_t unit) override;
  Result lockHWBuffer(std::byte* _Nullable* _Nonnull dst, RangeDesc& outRange) const;
  Result unlockHWBuffer() const;
  uint64_t getTextureId() const override;

  static bool isValidFormat(TextureFormat format);

 private:
  AHardwareBuffer* hwBuffer_ = nullptr;
  std::shared_ptr<AHardwareBufferHelper> hwBufferHelper_ = nullptr;
};

} // namespace igl::opengl::egl::android
#endif
