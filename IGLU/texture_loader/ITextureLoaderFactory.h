/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/DataReader.h>
#include <IGLU/texture_loader/ITextureLoader.h>
#include <igl/DeviceFeatures.h>

namespace iglu::textureloader {

/// Interface for creating ITextureLoader instances for a specific format.
class ITextureLoaderFactory {
 protected:
  ITextureLoaderFactory() = default;

 public:
  virtual ~ITextureLoaderFactory() = default;

  [[nodiscard]] virtual uint32_t headerLength() const noexcept = 0;
  [[nodiscard]] bool isHeaderValid(const uint8_t* IGL_NONNULL data,
                                   uint32_t length,
                                   igl::Result* IGL_NULLABLE outResult) const noexcept;
  [[nodiscard]] bool isHeaderValid(DataReader reader,
                                   igl::Result* IGL_NULLABLE outResult) const noexcept;

  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreate(const uint8_t* IGL_NONNULL data,
                                                          uint32_t length,
                                                          igl::Result* IGL_NULLABLE
                                                              outResult) const noexcept;
  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreate(DataReader reader,
                                                          igl::Result* IGL_NULLABLE
                                                              outResult) const noexcept;

 protected:
  [[nodiscard]] virtual bool isHeaderValidInternal(DataReader reader,
                                                   igl::Result* IGL_NULLABLE
                                                       outResult) const noexcept = 0;
  [[nodiscard]] virtual std::unique_ptr<ITextureLoader> tryCreateInternal(
      DataReader reader,
      igl::Result* IGL_NULLABLE outResult) const noexcept = 0;
};

} // namespace iglu::textureloader
