/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Texture.h>

namespace igl::opengl {
class DummyTexture : public ITexture {
 public:
  explicit DummyTexture(Size size, TextureFormat format = TextureFormat::BGRA_UNorm8) :
    ITexture(format), size_(size) {}
  ~DummyTexture() override = default;

  [[nodiscard]] Dimensions getDimensions() const override {
    return Dimensions{static_cast<uint32_t>(size_.width), static_cast<uint32_t>(size_.height), 1u};
  }

  [[nodiscard]] uint32_t getNumLayers() const override {
    return 1;
  }
  [[nodiscard]] TextureType getType() const override {
    IGL_ASSERT_NOT_REACHED();
    return TextureType::TwoDArray;
  }
  [[nodiscard]] TextureDesc::TextureUsage getUsage() const override {
    IGL_ASSERT_NOT_REACHED();
    return 0;
  }
  [[nodiscard]] uint32_t getSamples() const override {
    IGL_ASSERT_NOT_REACHED();
    return 1;
  }
  void generateMipmap(ICommandQueue& /* unused */,
                      const TextureRangeDesc* IGL_NULLABLE /* unused */) const override {
    IGL_ASSERT_NOT_REACHED();
  }
  void generateMipmap(ICommandBuffer& /* unused */,
                      const TextureRangeDesc* IGL_NULLABLE /* unused */) const override {
    IGL_ASSERT_NOT_REACHED();
  }
  [[nodiscard]] uint32_t getNumMipLevels() const override {
    IGL_ASSERT_NOT_REACHED();
    return 1;
  }
  [[nodiscard]] bool isRequiredGenerateMipmap() const override {
    return false;
  }
  [[nodiscard]] uint64_t getTextureId() const override {
    IGL_ASSERT_NOT_REACHED();
    return 0;
  }

 private:
  Size size_;
};
} // namespace igl::opengl
