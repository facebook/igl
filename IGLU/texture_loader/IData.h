/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <memory>

namespace iglu::textureloader {

/// Interface for accessing data.
class IData {
 protected:
  IData() noexcept = default;

 public:
  virtual ~IData() = default;

  static std::unique_ptr<IData> tryCreate(std::unique_ptr<uint8_t[]> data,
                                          uint32_t length,
                                          igl::Result* IGL_NULLABLE outResult);

  [[nodiscard]] virtual const uint8_t* IGL_NONNULL data() const noexcept = 0;
  [[nodiscard]] virtual uint32_t length() const noexcept = 0;
};

} // namespace iglu::textureloader
