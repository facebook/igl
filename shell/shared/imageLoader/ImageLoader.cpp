/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/ImageLoader.h>

#include <IGLU/texture_loader/stb_hdr/TextureLoaderFactory.h>
#include <IGLU/texture_loader/stb_jpeg/TextureLoaderFactory.h>
#include <IGLU/texture_loader/stb_png/TextureLoaderFactory.h>
#include <array>
#include <cstdio>
#include <shell/shared/fileLoader/FileLoader.h>

namespace igl::shell {
namespace {
std::vector<std::unique_ptr<iglu::textureloader::ITextureLoaderFactory>> createLoaderFactories() {
  std::vector<std::unique_ptr<iglu::textureloader::ITextureLoaderFactory>> factories;
  factories.reserve(3);
  factories.emplace_back(std::make_unique<iglu::textureloader::stb::hdr::TextureLoaderFactory>());
  factories.emplace_back(std::make_unique<iglu::textureloader::stb::jpeg::TextureLoaderFactory>());
  factories.emplace_back(std::make_unique<iglu::textureloader::stb::png::TextureLoaderFactory>());

  return factories;
}

constexpr uint32_t kWidth = 8, kHeight = 8;
constexpr uint32_t kWhite = 0xFFFFFFFF, kBlack = 0xFF000000;
constexpr std::array<uint32_t, 64> kCheckerboard = {
    kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kWhite,
    kWhite, kBlack, kBlack, kWhite, kWhite, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite,
    kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kBlack,
    kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite,
    kBlack, kBlack, kWhite, kWhite, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack,
    kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack, kBlack,
};
constexpr uint32_t kNumBytes = kWidth * kHeight * 4u;

class CheckerboardData : public iglu::textureloader::IData {
 public:
  [[nodiscard]] const uint8_t* IGL_NONNULL data() const noexcept final;
  [[nodiscard]] uint32_t length() const noexcept final;
};

const uint8_t* IGL_NONNULL CheckerboardData::data() const noexcept {
  return reinterpret_cast<const uint8_t*>(kCheckerboard.data());
}

uint32_t CheckerboardData::length() const noexcept {
  return kNumBytes;
}
} // namespace

ImageLoader::ImageLoader(FileLoader& fileLoader) :
  fileLoader_(fileLoader),
  factory_(std::make_unique<iglu::textureloader::TextureLoaderFactory>(createLoaderFactories())) {}

ImageData ImageLoader::defaultLoadImageData(
    const std::string& imageName,
    std::optional<igl::TextureFormat> preferredFormat) noexcept {
  const std::string fullName = fileLoader().fullPath(imageName);

  return loadImageDataFromFile(fullName, preferredFormat);
}

ImageData ImageLoader::loadImageDataFromFile(
    const std::string& fileName,
    std::optional<igl::TextureFormat> preferredFormat) noexcept {
  auto [data, length] = fileLoader_.loadBinaryData(fileName);
  if (IGL_DEBUG_VERIFY(data && length > 0)) {
    return loadImageDataFromMemory(data.get(), length, preferredFormat);
  }

  return {};
}

ImageData ImageLoader::loadImageDataFromMemory(
    const uint8_t* data,
    uint32_t length,
    std::optional<igl::TextureFormat> preferredFormat) noexcept {
  if (IGL_DEBUG_VERIFY_NOT(data == nullptr)) {
    return {};
  }

  Result result;
  auto loader = preferredFormat ? factory_->tryCreate(data, length, *preferredFormat, &result)
                                : factory_->tryCreate(data, length, &result);
  if (loader == nullptr || !result.isOk()) {
    return {};
  }

  auto texData = loader->load(&result);
  if (texData == nullptr || !result.isOk()) {
    return {};
  }

  ImageData imageData;
  imageData.data = std::move(texData);
  imageData.desc = loader->descriptor();

  return imageData;
}

ImageData ImageLoader::checkerboard() noexcept {
  ImageData imageData;
  imageData.data = std::make_unique<CheckerboardData>();
  imageData.desc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                      kWidth,
                                      kHeight,
                                      TextureDesc::TextureUsageBits::Sampled,
                                      "Checkerboard");
  imageData.desc.numMipLevels = TextureDesc::calcNumMipLevels(kWidth, kHeight);

  return imageData;
}

} // namespace igl::shell
