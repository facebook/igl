/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl::vulkan {

class VulkanContext;

/// @brief A manager that keeps track of ring buffer properties for buffers, such as the number of
/// sub-buffers to allocate and the current index of the sub-buffer being used.
class SyncManager final {
 public:
  using SubmitHandle = VulkanImmediateCommands::SubmitHandle;

  /// @brief Constructs a SyncManager object with the maximum number of resources to allocate. This
  /// class doesn't allocate the resources. It merely keeps track of the current index and the
  /// maximum number of resources that exist in the system.
  explicit SyncManager(const VulkanContext& ctx, uint32_t maxResourceCount);

  /// @brief Returns the index of the current resource being used. Its range is [0,
  /// maxResourceCount).
  [[nodiscard]] uint32_t currentIndex() const noexcept;

  /// @brief Returns the maximum number of resources that must be allocated by ring buffers.
  [[nodiscard]] uint32_t maxResourceCount() const noexcept;

  /// @brief Increments the current index and waits for newly computed index's SubmitHandle to
  /// become free before continuing.
  void acquireNext() noexcept;

  /// @brief Marks the given handle as submitted.
  void markSubmitted(SubmitHandle handle) noexcept;

 private:
  const VulkanContext& ctx_;
  const uint32_t maxResourceCount_ = 1u;
  uint32_t currentIndex_ = 0u;
  std::vector<SubmitHandle> submitHandles_;
};

} // namespace igl::vulkan
