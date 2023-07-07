/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/IGL.h>
#include <memory>
#include <string>
#include <vector>

namespace igl::shell {

struct ImageData {
  uint32_t width;
  uint32_t height;
  size_t
      bitsPerComponent; // bits per color channel in a pixel (eg. 16 bit RGBA would be 4, see
                        // https://developer.apple.com/documentation/coregraphics/1454980-cgimagegetbitspercomponent)
  size_t bytesPerRow;
  std::vector<uint8_t> buffer;
};

class ImageLoader {
 public:
  ImageLoader() = default;
  virtual ~ImageLoader() = default;
  virtual ImageData loadImageData(std::string /*imageName*/) noexcept {
    return checkerboard();
  }
  void setHomePath(const std::string& homePath) {
    homePath_ = homePath;
  }
  const std::string& homePath() const noexcept {
    return homePath_;
  }

 private:
  static constexpr size_t kWidth = 8, kHeight = 8;
  using Pixel = uint32_t;
  static constexpr Pixel kWhite = 0xFFFFFFFF, kBlack = 0xFF000000;
  static constexpr Pixel kCheckerboard[kWidth * kHeight] = {
      kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kWhite,
      kWhite, kBlack, kBlack, kWhite, kWhite, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite,
      kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kBlack,
      kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite,
      kBlack, kBlack, kWhite, kWhite, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack,
      kBlack, kWhite, kWhite, kBlack, kBlack, kWhite, kWhite, kBlack, kBlack,
  };
  static constexpr size_t kBytesPerComponent = sizeof(kCheckerboard[0]);
  static constexpr size_t kNumBytes = kWidth * kHeight * kBytesPerComponent;

  static ImageData checkerboard() noexcept {
    ImageData imageData = {
        /*.width = */ kWidth,
        /*.height = */ kHeight,
        /*.bitsPerComponent = */ kBytesPerComponent * 8,
        /*.bytesPerRow = */ kWidth * kBytesPerComponent,
        /*.buffer = */ {},
    };

    auto begin = reinterpret_cast<const uint8_t*>(kCheckerboard);
    auto end = begin + kNumBytes;
    imageData.buffer.reserve(kNumBytes);
    imageData.buffer.assign(begin, end);

    return imageData;
  }

  std::string homePath_;
};

} // namespace igl::shell
