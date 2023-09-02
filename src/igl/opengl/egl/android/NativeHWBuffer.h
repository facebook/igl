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

  void generateMipmap(ICommandQueue& cmdQueue) const override;
  uint32_t getNumMipLevels() const override;

  static bool isValidFormat(TextureFormat format);

 protected:
  Result initialize() const;
  Result initializeWithUpload() const;
  Result initializeWithTexStorage() const;
  Result upload(GLenum target,
                const TextureRangeDesc& range,
                const void* data,
                size_t bytesPerRow = 0) const;
  Result upload2D(GLenum target, const TextureRangeDesc& range, const void* data) const;

 protected:
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  FormatDescGL formatDescGL_;

 private:
  Result createTexture(const TextureDesc& desc);
  bool canInitialize() const;
  bool supportsTexStorage() const;
  AHardwareBuffer* hwBuffer_ = nullptr;
  std::shared_ptr<AHardwareBufferHelper> hwBufferHelper_ = nullptr;
};

} // namespace igl::opengl::egl::android
#endif
