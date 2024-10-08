/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <optional>

namespace igl {
class IDevice;
} // namespace igl

namespace iglu::textureloader {

/// Helper class for reading data
class DataReader {
 public:
  static std::optional<DataReader> tryCreate(const uint8_t* IGL_NONNULL data,
                                             uint32_t length,
                                             igl::Result* IGL_NULLABLE outResult);

  [[nodiscard]] const uint8_t* IGL_NONNULL data() const noexcept;
  [[nodiscard]] uint32_t length() const noexcept;

  [[nodiscard]] const uint8_t* IGL_NULLABLE
  tryAt(uint32_t offset, igl::Result* IGL_NULLABLE outResult) const noexcept;
  [[nodiscard]] const uint8_t* IGL_NONNULL at(uint32_t offset) const noexcept;

  template<typename T>
  [[nodiscard]] const T* IGL_NULLABLE tryAs(igl::Result* IGL_NULLABLE outResult) const noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    if (!ensureLength(static_cast<uint32_t>(sizeof(T)), outResult)) {
      return nullptr;
    }

    return as<T>();
  }

  template<typename T>
  [[nodiscard]] const T* IGL_NONNULL as() const noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    IGL_DEBUG_ASSERT(length_ >= static_cast<uint32_t>(sizeof(T)));
    return reinterpret_cast<const T*>(data_);
  }

  template<typename T>
  [[nodiscard]] const T* IGL_NULLABLE tryAsAt(uint32_t offset,
                                              igl::Result* IGL_NULLABLE outResult) const noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    if (!ensureLength(offset + static_cast<uint32_t>(sizeof(T)), outResult)) {
      return nullptr;
    }

    return asAt<T>(offset);
  }

  template<typename T>
  [[nodiscard]] const T* IGL_NONNULL asAt(uint32_t offset) const noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    IGL_DEBUG_ASSERT(length_ >= offset + static_cast<uint32_t>(sizeof(T)));
    return reinterpret_cast<const T*>(data_ + offset);
  }

  template<typename T>
  [[nodiscard]] bool tryRead(T& outValue, igl::Result* IGL_NULLABLE outResult) const noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    if (!ensureLength(static_cast<uint32_t>(sizeof(T)), outResult)) {
      return false;
    }
    outValue = read<T>();
    return true;
  }

  template<typename T>
  [[nodiscard]] const T& read() const noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    return *as<T>();
  }

  template<typename T>
  [[nodiscard]] bool tryReadAt(uint32_t offset,
                               T& outValue,
                               igl::Result* IGL_NULLABLE outResult) const noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    if (!ensureLength(offset + static_cast<uint32_t>(sizeof(T)), outResult)) {
      return false;
    }
    outValue = readAt<T>(offset);
    return true;
  }

  template<typename T>
  [[nodiscard]] const T& readAt(uint32_t offset) const noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    return *asAt<T>(offset);
  }

  template<typename T>
  [[nodiscard]] bool tryAdvance(igl::Result* IGL_NULLABLE outResult) noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    return tryAdvance(static_cast<uint32_t>(sizeof(T)), outResult);
  }
  [[nodiscard]] bool tryAdvance(uint32_t bytesToAdvance,
                                igl::Result* IGL_NULLABLE outResult) noexcept;

  template<typename T>
  void advance() noexcept {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    advance(static_cast<uint32_t>(sizeof(T)));
  }
  void advance(uint32_t bytesToAdvance) noexcept;

 private:
  DataReader(const uint8_t* IGL_NONNULL data, uint32_t length) noexcept;
  [[nodiscard]] bool ensureLength(uint32_t requestedLength,
                                  igl::Result* IGL_NULLABLE outResult) const noexcept;

  // Prevent heap allocation
  static void* IGL_NONNULL operator new(std::size_t);
  static void* IGL_NONNULL operator new[](std::size_t);

  const uint8_t* FOLLY_NONNULL data_;
  uint32_t length_;
};

} // namespace iglu::textureloader
