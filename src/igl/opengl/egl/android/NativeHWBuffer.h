/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/android/NativeHWBuffer.h>
#include <igl/opengl/TextureBufferBase.h>

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

namespace igl::opengl::egl::android {

typedef void AHardwareBufferHelper;

// TextureBuffer encapsulates OpenGL textures
class NativeHWTextureBuffer : public igl::android::INativeHWTextureBuffer,
                              public TextureBufferBase {
  using Super = TextureBufferBase;

 public:
  NativeHWTextureBuffer(IContext& context, TextureFormat format) : Super(context, format) {}
  ~NativeHWTextureBuffer() override;

  // Texture overrides
  Result create(const TextureDesc& desc, bool hasStorageAlready) override;
  void bind() override;
  void bindImage(size_t unit) override;
  uint64_t getTextureId() const override;

  bool supportsUpload() const final;
  static bool isValidFormat(TextureFormat format);

  IGL_INLINE void setTextureUsage(TextureDesc::TextureUsage usage) {
    setUsage(usage);
  }

 protected:
  Result createTextureInternal(const TextureDesc& desc, AHardwareBuffer* buffer) override;

 private:
  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* IGL_NULLABLE data,
                        size_t bytesPerRow) const final;

  std::shared_ptr<AHardwareBufferHelper> hwBufferHelper_ = nullptr;
};

} // namespace igl::opengl::egl::android

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
