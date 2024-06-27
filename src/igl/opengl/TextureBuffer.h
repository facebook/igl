/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/TextureBufferBase.h>

namespace igl::opengl {

// TextureBuffer encapsulates OpenGL textures
class TextureBuffer : public TextureBufferBase {
  using Super = TextureBufferBase;

 public:
  TextureBuffer(IContext& context, TextureFormat format) : Super(context, format) {}
  ~TextureBuffer() override;

  // Texture overrides
  Result create(const TextureDesc& desc, bool hasStorageAlready) override;
  void bindImage(size_t unit) override;
  uint64_t getTextureId() const override;

 protected:
  Result initialize(const std::string& debugName) const;
  Result initializeWithUpload() const;
  Result initializeWithTexStorage() const;
  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* IGL_NULLABLE data,
                        size_t bytesPerRow) const override;
  Result uploadInternal(GLenum target,
                        const TextureRangeDesc& range,
                        const void* IGL_NULLABLE data,
                        size_t bytesPerRow = 0) const;
  Result upload2D(GLenum target,
                  const TextureRangeDesc& range,
                  bool texImage,
                  const void* IGL_NULLABLE data) const;
  Result upload2DArray(GLenum target,
                       const TextureRangeDesc& range,
                       bool texImage,
                       const void* IGL_NULLABLE data) const;
  Result upload3D(GLenum target,
                  const TextureRangeDesc& range,
                  bool texImage,
                  const void* IGL_NULLABLE data) const;

 protected:
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  FormatDescGL formatDescGL_;

 private:
  [[nodiscard]] bool needsRepacking(const TextureRangeDesc& range, size_t bytesPerRow) const final;
  Result createTexture(const TextureDesc& desc);
  bool canInitialize() const;
  bool supportsTexStorage() const;
  mutable uint64_t textureHandle_ = 0;
};

} // namespace igl::opengl
