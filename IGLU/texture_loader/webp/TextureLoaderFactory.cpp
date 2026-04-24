/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

// @fb-only

#include <IGLU/texture_loader/webp/TextureLoaderFactory.h>

#include <IGLU/texture_loader/webp/Header.h>
// @fb-only

namespace iglu::textureloader::webp {
namespace {

class WebPData final : public IData {
 public:
  WebPData(uint8_t* data, uint64_t size) noexcept : data_(data), size_(size) {}
  ~WebPData() override {
    WebPFree(data_);
  }
  WebPData(const WebPData&) = delete;
  WebPData& operator=(const WebPData&) = delete;
  WebPData(WebPData&&) = delete;
  WebPData& operator=(WebPData&&) = delete;

  [[nodiscard]] const uint8_t* IGL_NONNULL data() const noexcept final {
    return data_;
  }
  [[nodiscard]] uint64_t size() const noexcept final {
    return size_;
  }

 private:
  uint8_t* data_;
  uint64_t size_;
};

class TextureLoader final : public ITextureLoader {
  using Super = ITextureLoader;

 public:
  TextureLoader(DataReader reader,
                int width,
                int height,
                igl::TextureFormat preferredFormat) noexcept :
    Super(reader) {
    auto& desc = mutableDescriptor();
    desc.format = preferredFormat != igl::TextureFormat::Invalid ? preferredFormat
                                                                 : igl::TextureFormat::RGBA_UNorm8;
    desc.numLayers = 1;
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.depth = 1;
    desc.type = igl::TextureType::TwoD;
    desc.numMipLevels = igl::TextureDesc::calcNumMipLevels(desc.width, desc.height);
  }

  [[nodiscard]] bool canUploadSourceData() const noexcept final {
    return false;
  }

  [[nodiscard]] bool shouldGenerateMipmaps() const noexcept final {
    return descriptor().numMipLevels > 1;
  }

 private:
  // NOLINTNEXTLINE(bugprone-exception-escape)
  [[nodiscard]] std::unique_ptr<IData> loadInternal(
      igl::Result* IGL_NULLABLE outResult) const noexcept final {
    const auto r = reader();
    int width = 0;
    int height = 0;

    // Decode to RGBA
    uint8_t* pixels = WebPDecodeRGBA(r.data(), r.size(), &width, &height);
    if (pixels == nullptr) {
      igl::Result::setResult(
          outResult, igl::Result::Code::RuntimeError, "Failed to decode WebP image.");
      return nullptr;
    }

    const uint64_t dataSize = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4u;
    return std::make_unique<WebPData>(pixels, dataSize);
  }
};

} // namespace

uint32_t TextureLoaderFactory::minHeaderLength() const noexcept {
  return kHeaderLength;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                             igl::Result* IGL_NULLABLE outResult) const noexcept {
  const auto* header = headerReader.as<Header>();
  if (!header->tagIsValid()) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Not a WebP file.");
    return false;
  }
  return true;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::TextureFormat preferredFormat,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  int width = 0;
  int height = 0;
  if (WebPGetInfo(reader.data(), reader.size(), &width, &height) == 0) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Failed to get WebP image info.");
    return nullptr;
  }

  if (width <= 0 || height <= 0) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Invalid WebP dimensions.");
    return nullptr;
  }

  return std::make_unique<TextureLoader>(reader, width, height, preferredFormat);
}

} // namespace iglu::textureloader::webp

// @fb-only
