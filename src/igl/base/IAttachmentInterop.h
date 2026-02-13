/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/base/Common.h>

namespace igl::base {

/// @brief Basic attachment descriptor for interoperability
struct AttachmentInteropDesc {
  uint32_t width = 1;
  uint32_t height = 1;
  uint32_t depth = 1;
  uint32_t numLayers = 1;
  uint32_t numSamples = 1;
  uint32_t numMipLevels = 1;
  TextureType type = TextureType::TwoD;
  TextureFormat format = TextureFormat::Invalid;
  bool isSampled = true;
};

/// @brief Attachment interface for interoperability
class IAttachmentInterop {
 public:
  virtual ~IAttachmentInterop() = default;

  /// @brief Get native image handle
  /// @return Platform-specific image handle
  [[nodiscard]] virtual void* getNativeImage() const = 0;

  /// @brief Get native image view handle (if applicable)
  /// @return Platform-specific image view handle
  [[nodiscard]] virtual void* getNativeImageView() const = 0;

  /// @brief Get the attachment descriptor
  [[nodiscard]] virtual const AttachmentInteropDesc& getDesc() const = 0;
};

} // namespace igl::base
