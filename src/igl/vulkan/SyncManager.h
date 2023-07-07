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

namespace igl {
namespace vulkan {

class VulkanContext;

class SyncManager final {
 public:
  using SubmitHandle = VulkanImmediateCommands::SubmitHandle;

  explicit SyncManager(const VulkanContext& ctx, uint32_t maxResourceCount);

  [[nodiscard]] uint32_t currentIndex() const noexcept;

  [[nodiscard]] uint32_t maxResourceCount() const noexcept;

  void acquireNext() noexcept;

  void markSubmit(SubmitHandle handle) noexcept;

 private:
  const VulkanContext& ctx_;
  const uint32_t maxResourceCount_ = 1u;
  uint32_t currentIndex_ = 0u;
  std::vector<SubmitHandle> submitHandles_;
};

} // namespace vulkan
} // namespace igl
