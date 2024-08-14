/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "SyncManager.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>

namespace igl::vulkan {

SyncManager::SyncManager(const VulkanContext& ctx, uint32_t maxResourceCount) :
  ctx_(ctx), maxResourceCount_(maxResourceCount) {
  IGL_ASSERT_MSG(maxResourceCount_ > 0, "Max resource count needs to be greater than zero");

  submitHandles_.resize(maxResourceCount_);
}

uint32_t SyncManager::currentIndex() const noexcept {
  return currentIndex_;
}

uint32_t SyncManager::maxResourceCount() const noexcept {
  return maxResourceCount_;
}

void SyncManager::acquireNext() noexcept {
  IGL_PROFILER_FUNCTION();

  currentIndex_ = (currentIndex_ + 1) % maxResourceCount_;

  // Wait for the current buffer to become available
  ctx_.immediate_->wait(submitHandles_[currentIndex_], ctx_.config_.fenceTimeoutNanoseconds);
}

void SyncManager::markSubmitted(SubmitHandle handle) noexcept {
  IGL_PROFILER_FUNCTION();

  submitHandles_[currentIndex_] = handle;

  acquireNext();
}

} // namespace igl::vulkan
