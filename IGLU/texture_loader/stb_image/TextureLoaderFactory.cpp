/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/stb_image/TextureLoaderFactory.h>

#ifdef WIN32
#define STBI_MSC_SECURE_CRT
#endif

// Do not use #define STB_IMAGE_IMPLEMENTATION
// Instead rely on //arvr/third-party/stb:stb_image to provide the implementation
#include <stb_image.h>

namespace iglu::textureloader::stb::image {
struct StbImageDeleter {
  void operator()(void* p) const {
    stbi_image_free(p);
  }
};

namespace {
class StbImageData : public IData {
 public:
  StbImageData(uint8_t* data, uint32_t length);

  [[nodiscard]] const uint8_t* IGL_NONNULL data() const noexcept final;
  [[nodiscard]] uint32_t length() const noexcept final;

 private:
  std::unique_ptr<uint8_t, StbImageDeleter> data_;
  uint32_t length_;
};

StbImageData::StbImageData(uint8_t* data, uint32_t length) :
  data_(std::unique_ptr<uint8_t, StbImageDeleter>(data)), length_(length) {}

[[nodiscard]] const uint8_t* IGL_NONNULL StbImageData::data() const noexcept {
  IGL_ASSERT(data_ != nullptr);
  return data_.get();
}

[[nodiscard]] uint32_t StbImageData::length() const noexcept {
  return length_;
}

class TextureLoader : public ITextureLoader {
  using Super = ITextureLoader;

 public:
  explicit TextureLoader(DataReader reader, int width, int height, bool isFloatFormat) noexcept;

  [[nodiscard]] bool canUploadSourceData() const noexcept final;
  [[nodiscard]] bool shouldGenerateMipmaps() const noexcept final;

 private:
  std::unique_ptr<IData> loadInternal(igl::Result* IGL_NULLABLE outResult) const noexcept final;

  bool isFloatFormat_;
};

TextureLoader::TextureLoader(DataReader reader, int width, int height, bool isFloatFormat) noexcept
  :
  Super(reader), isFloatFormat_(isFloatFormat) {
  auto& desc = mutableDescriptor();
  desc.format = isFloatFormat ? igl::TextureFormat::RGBA_F32 : igl::TextureFormat::RGBA_UNorm8;
  desc.numLayers = 1;
  desc.width = static_cast<size_t>(width);
  desc.height = static_cast<size_t>(height);
  desc.depth = 1;
  desc.type = igl::TextureType::TwoD;

  // Floating point mipmaps not always supported
  desc.numMipLevels = isFloatFormat ? 1
                                    : igl::TextureDesc::calcNumMipLevels(desc.width, desc.height);
}

bool TextureLoader::canUploadSourceData() const noexcept {
  return false;
}

bool TextureLoader::shouldGenerateMipmaps() const noexcept {
  return descriptor().numMipLevels > 1;
}

std::unique_ptr<IData> TextureLoader::loadInternal(
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  const auto r = reader();
  const int length = r.length() > std::numeric_limits<int>::max() ? std::numeric_limits<int>::max()
                                                                  : static_cast<int>(r.length());

  int x, y, comp;
  void* data = nullptr;
  // Pass 4 for desired_channels to force RGBA instead of RGB.
  if (isFloatFormat_) {
    data = stbi_loadf_from_memory(r.data(), static_cast<int>(length), &x, &y, &comp, 4);
  } else {
    data = stbi_load_from_memory(r.data(), static_cast<int>(length), &x, &y, &comp, 4);
  }
  if (data == nullptr) {
    igl::Result::setResult(outResult, igl::Result::Code::RuntimeError, "Could not load image daa.");
    return nullptr;
  }

  return std::make_unique<StbImageData>(reinterpret_cast<uint8_t*>(data), memorySizeInBytes());
}
} // namespace

TextureLoaderFactory::TextureLoaderFactory(bool isFloatFormat) noexcept :
  isFloatFormat_(isFloatFormat) {}

bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                             igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (!isIdentifierValid(headerReader)) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Incorrect identifier.");
    return false;
  }

  return true;
}

std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  const int length = reader.length() > std::numeric_limits<int>::max()
                         ? std::numeric_limits<int>::max()
                         : static_cast<int>(reader.length());

  int x, y, comp;
  if (stbi_info_from_memory(reader.data(), length, &x, &y, &comp) == 0) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Could not get HDR metadata.");
    return nullptr;
  }

  if (x < 0 || y < 0 || comp < 0 || comp > 4) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Invalid HDR metadata.");
    return nullptr;
  }

  if (static_cast<uint64_t>(x) * static_cast<uint64_t>(y) > std::numeric_limits<uint32_t>::max()) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Image is too large.");
    return nullptr;
  }

  return std::make_unique<TextureLoader>(reader, x, y, isFloatFormat_);
}

} // namespace iglu::textureloader::stb::image
