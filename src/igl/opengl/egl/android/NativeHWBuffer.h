/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/TextureBufferBase.h>

#if IGL_PLATFORM_ANDROID && __ANDROID_MIN_SDK_VERSION__ >= 26

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

  // Texture overrides
  Result create(const TextureDesc& desc, bool hasStorageAlready) override;
  Result createHWBuffer(const TextureDesc& desc, bool hasStorageAlready, bool surfaceComposite);
  void bind() override;
  void bindImage(size_t unit) override;
  Result lockHWBuffer(std::byte* IGL_NULLABLE* IGL_NONNULL dst, RangeDesc& outRange) const;
  Result unlockHWBuffer() const;
  uint64_t getTextureId() const override;

  bool supportsUpload() const final;

  static bool isValidFormat(TextureFormat format);

  // exported to use on Java side via JNI into an Android HardwareBuffer
  // Use with great care!
  AHardwareBuffer* hwBuffer_ = nullptr;

 private:
  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* IGL_NULLABLE data,
                        size_t bytesPerRow) const final;

  std::shared_ptr<AHardwareBufferHelper> hwBufferHelper_ = nullptr;
};

} // namespace igl::opengl::egl::android
#endif
