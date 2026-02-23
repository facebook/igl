/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/Framebuffer.h>
#include <igl/base/IFramebufferInterop.h>

namespace igl {

/// @brief Wrapper that adapts IFramebuffer to base::IFramebufferInterop interface
/// @note This wrapper holds a shared_ptr to the underlying IFramebuffer
class FramebufferWrapper : public base::IFramebufferInterop {
 public:
  explicit FramebufferWrapper(std::shared_ptr<IFramebuffer> framebuffer);
  ~FramebufferWrapper() override = default;

  // Non-copyable but movable
  FramebufferWrapper(const FramebufferWrapper&) = delete;
  FramebufferWrapper& operator=(const FramebufferWrapper&) = delete;
  FramebufferWrapper(FramebufferWrapper&&) noexcept = default;
  FramebufferWrapper& operator=(FramebufferWrapper&&) noexcept = default;

  /// @brief Get the underlying IFramebuffer
  [[nodiscard]] const std::shared_ptr<IFramebuffer>& getFramebuffer() const {
    return framebuffer_;
  }

  // base::IFramebufferInterop interface
  [[nodiscard]] base::IAttachmentInterop* IGL_NULLABLE
  getColorAttachment(size_t index) const override;
  [[nodiscard]] base::IAttachmentInterop* IGL_NULLABLE getDepthAttachment() const override;
  [[nodiscard]] void* IGL_NULLABLE getNativeFramebuffer() const override;

 private:
  std::shared_ptr<IFramebuffer> framebuffer_;
};

} // namespace igl
