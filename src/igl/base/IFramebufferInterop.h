/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <igl/base/IAttachmentInterop.h>

namespace igl::base {

/// Maximum number of color attachments supported
constexpr uint32_t kMaxColorAttachments = 4;

/// @brief Framebuffer descriptor for interoperability
struct FramebufferInteropDesc {
  AttachmentInteropDesc* colorAttachments[kMaxColorAttachments] = {};
  AttachmentInteropDesc* depthAttachment = nullptr;
  AttachmentInteropDesc* stencilAttachment = nullptr;
};

/// @brief Framebuffer interface for interoperability
class IFramebufferInterop {
 public:
  virtual ~IFramebufferInterop() = default;

  /// @brief Get color attachment at specified index
  /// @param index The color attachment index (0 to kMaxColorAttachments-1)
  /// @return Non-owning pointer to the color attachmant, or nullptr if not set
  [[nodiscard]] virtual IAttachmentInterop* getColorAttachment(size_t index) const = 0;

  /// @brief Get depth attachment
  /// @return Non-owning pointer to the depth attachment, or nullptr if not set
  [[nodiscard]] virtual IAttachmentInterop* getDepthAttachment() const = 0;

  /// @brief Get native framebuffer handle (backend-specific)
  /// @return Platform-specific framebuffer handle if applicable, otherwise nullptr
  [[nodiscard]] virtual void* getNativeFramebuffer() const = 0;
};

} // namespace igl::base
