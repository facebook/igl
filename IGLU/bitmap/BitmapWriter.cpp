/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/bitmap/BitmapWriter.h>

#include <IGLU/texture_accessor/TextureAccessorFactory.h>
#include <fstream>
#include <igl/Common.h>

namespace igl::iglu {
namespace {
struct BMPHeader {
  // Bitmap file header
  uint16_t signature = 0x4D42; // Signature ("BM" in ASCII), default value 0x4D42 for BMP format
  uint32_t fileSize = 0;
  uint16_t reserved1 = 0;
  uint16_t reserved2 = 0;
  uint32_t dataOffset = sizeof(BMPHeader); // Offset to the start of image data

  // DIB header (Bitmap information header)
  uint32_t headerSize = 40; // Size of the DIB header
  int32_t imageWidth = 0;
  int32_t imageHeight = 0;
  uint16_t colorPlanes = 1; // Number of color planes
  uint16_t bitsPerPixel = 24;
  uint32_t compression = 0; // Compression method (initialized to 0 for no compression)
  uint32_t imageSizeBytes = 0;
  int32_t horizontalResolution = 0; // Horizontal resolution in pixels per meter
  int32_t verticalResolution = 0; // Vertical resolution in pixels per meter
  uint32_t numColors = 0; // Number of colors in the color palette
  uint32_t importantColors = 0; // Number of important colors used
} __attribute__((packed));

bool shouldFlipY(const igl::IDevice& device) {
  return device.getBackendType() != igl::BackendType::OpenGL;
}

struct BufferOffsets {
  size_t r;
  size_t g;
  size_t b;
};

BufferOffsets getBufferOffsets(igl::TextureFormat format) {
  switch (format) {
  case igl::TextureFormat::RGBA_UNorm8:
  case igl::TextureFormat::RGBX_UNorm8:
  case igl::TextureFormat::RGBA_SRGB: {
    return {.r = 0, .g = 1, .b = 2};
  }

  case igl::TextureFormat::BGRA_UNorm8:
  case igl::TextureFormat::BGRA_SRGB: {
    return {.r = 2, .g = 1, .b = 0};
  }

  default:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return {.r = 0, .g = 1, .b = 2};
  }
}

} // namespace

bool isSupportedBitmapTextureFormat(igl::TextureFormat format) {
  switch (format) {
  case igl::TextureFormat::RGBA_UNorm8:
  case igl::TextureFormat::RGBX_UNorm8:
  case igl::TextureFormat::RGBA_SRGB:
  case igl::TextureFormat::BGRA_UNorm8:
  case igl::TextureFormat::BGRA_SRGB:
    return true;

  default:
    return false;
  }
}

void writeBitmap(const char* filename,
                 std::shared_ptr<igl::ITexture> texture,
                 igl::IDevice& device) {
  IGL_DEBUG_ASSERT(texture);
  IGL_DEBUG_ASSERT(texture->getType() == igl::TextureType::TwoD);
  IGL_DEBUG_ASSERT(isSupportedBitmapTextureFormat(texture->getFormat()));

  const auto textureAccessor =
      ::iglu::textureaccessor::TextureAccessorFactory::createTextureAccessor(
          device.getBackendType(), texture, device);

  const igl::CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  igl::Result result;
  const auto commandQueue = device.createCommandQueue(desc, &result);
  if (!IGL_DEBUG_VERIFY(result.isOk()) || !IGL_DEBUG_VERIFY(commandQueue)) {
    return;
  }

  textureAccessor->requestBytes(*commandQueue, nullptr);

  const auto& buffer = textureAccessor->getBytes();
  const auto size = texture->getSize();

  const TextureRangeDesc textureRange = texture->getFullRange();
  const auto& properties = texture->getProperties();
  const uint32_t bytesPerRow = properties.getBytesPerRow(textureRange);

  std::vector<uint8_t> imageData;
  imageData.reserve(size.width * size.height * 3);

  IGL_DEBUG_ASSERT(buffer.size() == size.height * bytesPerRow);

  const auto bufferOffsets = getBufferOffsets(texture->getFormat());
  const bool flipY = shouldFlipY(device);

  for (size_t y = 0; y < size.height; ++y) {
    const size_t row = flipY ? size.height - y - 1 : y;
    for (size_t byte = 0; byte < bytesPerRow; byte += 4) {
      const size_t index = row * bytesPerRow + byte;
      const uint8_t r = buffer[index + bufferOffsets.r];
      const uint8_t g = buffer[index + bufferOffsets.g];
      const uint8_t b = buffer[index + bufferOffsets.b];

      imageData.push_back(b);
      imageData.push_back(g);
      imageData.push_back(r);
    }
  }

  writeBitmap(filename, static_cast<const uint8_t*>(imageData.data()), size.width, size.height);
}

void writeBitmap(const char* filename, const uint8_t* imageData, uint32_t width, uint32_t height) {
  std::ofstream file;
  file.open(filename, std::ios::out | std::ios::binary);
  if (!IGL_DEBUG_VERIFY(file)) {
    return;
  }

  const uint32_t imageSize = width * height * 3;

  BMPHeader header{
      .fileSize = static_cast<uint32_t>(sizeof(BMPHeader) + imageSize),
      .imageWidth = static_cast<int32_t>(width),
      .imageHeight = static_cast<int32_t>(height),
      .imageSizeBytes = static_cast<uint32_t>(imageSize),
  };

  file.write(reinterpret_cast<const char*>(&header), sizeof(header));
  file.write(reinterpret_cast<const char*>(imageData), imageSize);
  file.close();
}

} // namespace igl::iglu
