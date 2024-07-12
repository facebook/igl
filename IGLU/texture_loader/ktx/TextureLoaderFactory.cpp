/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ktx/TextureLoaderFactory.h>

#include <igl/IGLSafeC.h>
#include <ktx.h>
#include <numeric>
#include <vector>

namespace iglu::textureloader::ktx {
namespace {

struct KtxDeleter {
  void operator()(void* p) const {
    ktxTexture_Destroy(ktxTexture(p));
  }
};

class TextureLoader : public ITextureLoader {
  using Super = ITextureLoader;

 public:
  TextureLoader(DataReader reader,
                const igl::TextureRangeDesc& range,
                igl::TextureFormat format,
                std::unique_ptr<ktxTexture, KtxDeleter> texture) noexcept;

  [[nodiscard]] bool canUploadSourceData() const noexcept final;
  [[nodiscard]] bool shouldGenerateMipmaps() const noexcept final;

 private:
  void uploadInternal(igl::ITexture& texture,
                      igl::Result* IGL_NULLABLE outResult) const noexcept final;
  void loadToExternalMemoryInternal(uint8_t* IGL_NONNULL data,
                                    uint32_t length,
                                    igl::Result* IGL_NULLABLE outResult) const noexcept final;

  std::unique_ptr<ktxTexture, KtxDeleter> texture_;
};

TextureLoader::TextureLoader(DataReader reader,
                             const igl::TextureRangeDesc& range,
                             igl::TextureFormat format,
                             std::unique_ptr<ktxTexture, KtxDeleter> texture) noexcept :
  Super(reader), texture_(std::move(texture)) {
  auto& desc = mutableDescriptor();
  desc.format = format;
  desc.numMipLevels = range.numMipLevels;
  desc.numLayers = range.numLayers;
  desc.width = range.width;
  desc.height = range.height;
  desc.depth = range.depth;

  if (range.numFaces == 6u) {
    desc.type = igl::TextureType::Cube;
  } else if (desc.depth > 1) {
    desc.type = igl::TextureType::ThreeD;
  } else if (desc.numLayers > 1) {
    desc.type = igl::TextureType::TwoDArray;
  } else {
    desc.type = igl::TextureType::TwoD;
  }
}

bool TextureLoader::canUploadSourceData() const noexcept {
  return true;
}

bool TextureLoader::shouldGenerateMipmaps() const noexcept {
  return texture_->generateMipmaps;
}

void TextureLoader::uploadInternal(igl::ITexture& texture,
                                   igl::Result* IGL_NULLABLE outResult) const noexcept {
  const auto& desc = descriptor();

  size_t offset;
  for (uint32_t mipLevel = 0; mipLevel < desc.numMipLevels && mipLevel < texture_->numLevels;
       ++mipLevel) {
    auto error = ktxTexture_GetImageOffset(ktxTexture(texture_.get()), mipLevel, 0, 0, &offset);
    if (error != KTX_SUCCESS) {
      IGL_LOG_ERROR("Error getting KTX texture data: %d %s\n", error, ktxErrorString(error));
      igl::Result::setResult(
          outResult, igl::Result::Code::RuntimeError, "Error getting KTX texture data.");
    }
    texture.upload(texture.getFullRange(mipLevel), texture_->pData + offset);
  }

  igl::Result::setOk(outResult);
}

void TextureLoader::loadToExternalMemoryInternal(uint8_t* IGL_NONNULL data,
                                                 uint32_t length,
                                                 igl::Result* IGL_NULLABLE
                                                     outResult) const noexcept {
  const auto& desc = descriptor();

  size_t offset;
  for (uint32_t mipLevel = 0; mipLevel < desc.numMipLevels && mipLevel < texture_->numLevels;
       ++mipLevel) {
    auto error = ktxTexture_GetImageOffset(ktxTexture(texture_.get()), 0, 0, mipLevel, &offset);
    if (error != KTX_SUCCESS) {
      IGL_LOG_ERROR("Error getting KTX texture data: %d %s\n", error, ktxErrorString(error));
      igl::Result::setResult(
          outResult, igl::Result::Code::RuntimeError, "Error getting KTX texture data.");
    }

    auto mipLevelLength = ktxTexture_GetImageSize(ktxTexture(texture_.get()), mipLevel);
    checked_memcpy_offset(data, length, offset, texture_->pData + offset, mipLevelLength);
    offset += mipLevelLength;
  }
}
} // namespace

std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::TextureFormat /*preferredFormat*/,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  const auto range = textureRange(reader);
  auto result = range.validate();
  if (!result.isOk()) {
    igl::Result::setResult(outResult, std::move(result));
    return nullptr;
  }

  if (!validate(reader, range, outResult)) {
    return nullptr;
  }

  ktxTexture* rawTexture = nullptr;
  auto error = ktxTexture_CreateFromMemory(
      reader.data(), reader.length(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &rawTexture);

  if (error != KTX_SUCCESS || rawTexture == nullptr) {
    IGL_LOG_ERROR("Error loading KTX texture: %d %s\n", error, ktxErrorString(error));
    igl::Result::setResult(
        outResult, igl::Result::Code::RuntimeError, "Error loading KTX texture.");
    if (rawTexture != nullptr) {
      ktxTexture_Destroy(rawTexture);
    }
    return nullptr;
  }

  auto texture = std::unique_ptr<ktxTexture, KtxDeleter>(rawTexture);

  if (ktxTexture_NeedsTranscoding(rawTexture)) {
#if IGL_PLATFORM_ANDROID || IGL_PLATFORM_IOS
    const ktx_transcode_fmt_e transcodeFormat = KTX_TTF_ASTC_4x4_RGBA;
#else
    const ktx_transcode_fmt_e transcodeFormat = KTX_TTF_BC7_RGBA;
#endif
    error =
        ktxTexture2_TranscodeBasis(reinterpret_cast<ktxTexture2*>(rawTexture), transcodeFormat, 0);
    if (error != KTX_SUCCESS) {
      IGL_LOG_ERROR("Error transcoding KTX texture: %d %s\n", error, ktxErrorString(error));
      igl::Result::setResult(
          outResult, igl::Result::Code::RuntimeError, "Error transcoding KTX texture.");
      return nullptr;
    }
  }

  const auto format = textureFormat(rawTexture);
  if (format == igl::TextureFormat::Invalid) {
    igl::Result::setResult(
        outResult, igl::Result::Code::RuntimeError, "Unsupported KTX texture format.");
    return nullptr;
  }

  if (texture->numFaces == 6u && texture->numLayers > 1u) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Texture cube arrays not supported.");
    return nullptr;
  }

  if (texture->numLayers > 1 && texture->baseDepth > 1u) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "3D texture arrays not supported.");
    return nullptr;
  }

  if (texture->numFaces != 1u && texture->numFaces != 6u) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "faces must be 1 or 6.");
    return nullptr;
  }

  if (texture->numFaces == 6u && texture->baseDepth != 1u) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "depth must be 1 for cube textures.");
    return nullptr;
  }

  if (texture->numFaces == 6u && texture->baseWidth != texture->baseHeight) {
    igl::Result::setResult(outResult,
                           igl::Result::Code::InvalidOperation,
                           "pixelWidth must match pixelHeight for cube textures.");
    return nullptr;
  }

  return std::make_unique<TextureLoader>(reader, range, format, std::move(texture));
}
} // namespace iglu::textureloader::ktx
