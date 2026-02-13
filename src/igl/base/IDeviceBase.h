/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/base/Common.h>
#include <igl/base/IAttachmentInterop.h>
#include <igl/base/IFramebufferInterop.h>
#include <igl/base/IStagingBufferInterop.h>

namespace igl::base {

/// @brief Base device interface for interoperability
class IDeviceBase {
 public:
  virtual ~IDeviceBase() = default;

  /// @brief Returns the backend type
  [[nodiscard]] virtual BackendType getBackendType() const = 0;

  /// @brief Returns raw pointer to native device handle
  /// @return Platform-specific device handle
  [[nodiscard]] virtual void* getNativeDevice() const = 0;

  /// @brief Create a framebuffer for interoperability
  /// @param desc The framebuffer descriptor
  /// @param outResult Optional pointer to receive the result code
  /// @return Raw pointer to created framebuffer. Caller MUST take ownership of the returned
  ///         pointer, otherwise it will be a memory leak. Use delete to destroy.
  ///         Returns nullptr if the framebuffer cannot be created.
  [[nodiscard]] virtual IFramebufferInterop* createFramebufferInterop(
      const FramebufferInteropDesc& desc) = 0;

  /// @brief Get access to the staging buffer
  /// @return Pointer to the staging buffer interface, or nullptr if not available
  ///         Returns nullptr if the staging buffer is not available.
  [[nodiscard]] virtual IStagingBufferInterop* getStagingBufferInterop() = 0;
};

} // namespace igl::base
