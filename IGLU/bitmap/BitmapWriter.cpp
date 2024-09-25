/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/bitmap/BitmapWriter.h>

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

} // namespace

void writeBitmap(const char* filename, const uint8_t* imageData, uint32_t width, uint32_t height) {
  std::ofstream file;
  file.open(filename, std::ios::out | std::ios::binary);
  if (!IGL_VERIFY(file)) {
    return;
  }

  uint32_t imageSize = width * height * 3;

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
