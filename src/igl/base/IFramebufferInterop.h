/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <igl/base/IAttachmentInterop.h>

namespace igl {
class ITexture;
} // namespace igl

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

  /// @brief Get the owning IGL texture for the color attachment at `index`.
  ///
  /// Returns the same shared_ptr the underlying IFramebuffer owns, so callers
  /// can keep the GPU resource alive independently of this interop pointer.
  /// Default implementation returns nullptr — overrides should provide the
  /// texture when the interop is backed by a real IFramebuffer. Implementations
  /// built from foreign native handles (no IFramebuffer behind them) should
  /// leave the default.
  [[nodiscard]] virtual std::shared_ptr<ITexture> getColorAttachmentTexture(
      size_t /*index*/) const noexcept {
    return nullptr;
  }

  /// @brief Get the owning IGL texture for the depth attachment.
  /// Returns nullptr when there is no depth attachment or when the interop is
  /// not backed by a real IFramebuffer. See getColorAttachmentTexture().
  [[nodiscard]] virtual std::shared_ptr<ITexture> getDepthAttachmentTexture() const noexcept {
    return nullptr;
  }
};

} // namespace igl::base
