/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/Common.h>

namespace iglu::textureloader {

/// Interface for accessing data.
class IData {
 protected:
  IData() noexcept = default;

 public:
  using Deleter = void (*)(void*);
  virtual ~IData() = default;

  static std::unique_ptr<IData> tryCreate(std::unique_ptr<uint8_t[]> data,
                                          uint32_t length,
                                          igl::Result* IGL_NULLABLE outResult);

  /// @returns a read-only pointer to the data. May be nullptr.
  [[nodiscard]] virtual const uint8_t* IGL_NONNULL data() const noexcept = 0;
  /// @returns the length of the data in bytes.
  [[nodiscard]] virtual uint32_t length() const noexcept = 0;

  struct ExtractedData {
    /// Pointer to data. May be nullptr.
    const void* data = nullptr;
    /// Length in bytes of the data.
    uint32_t length = 0u;
    /// A deleter that can be use to free data. May be nullptr.
    void (*deleter)(void*) = nullptr;
  };
  /// Extracts data from this IData and returns it in an ExtractedData struct. After this method,
  /// the behavior of data() and length() are undefined.
  [[nodiscard]] virtual ExtractedData extractData() noexcept;
};

[[nodiscard]] inline IData::ExtractedData IData::extractData() noexcept {
  return {
      .data = const_cast<uint8_t*>(data()),
      .length = length(),
  };
}
} // namespace iglu::textureloader
