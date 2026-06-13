/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/TimestampQueries.h>

#include <algorithm>
#include <future>
#include <igl/vulkan/VulkanContext.h>

namespace igl::vulkan {

TimestampQueries::TimestampQueries(VulkanContext& ctx, uint32_t maxSlots) :
  ctx_(ctx), maxSlots_(maxSlots) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  const auto& limits = ctx_.getVkPhysicalDeviceProperties().limits;
  timestampPeriod_ = limits.timestampPeriod;
  if (maxSlots_ == 0 || timestampPeriod_ <= 0.0f ||
      limits.timestampComputeAndGraphics == VK_FALSE) {
    return;
  }

  const VkQueryPoolCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = maxSlots_ * kTimestampsPerTimingSlot,
  };

  if (ctx_.vf_.vkCreateQueryPool(ctx_.getVkDevice(), &createInfo, nullptr, &queryPool_) !=
      VK_SUCCESS) {
    queryPool_ = VK_NULL_HANDLE;
    return;
  }
  const std::string debugName = "TimestampQueries[" + std::to_string(maxSlots_) + "]";
  VK_ASSERT(ivkSetDebugObjectName(&ctx_.vf_,
                                  ctx_.getVkDevice(),
                                  VK_OBJECT_TYPE_QUERY_POOL,
                                  reinterpret_cast<uint64_t>(queryPool_),
                                  debugName.c_str()));

  labels_.resize(maxSlots_);
  elapsedNanos_.resize(maxSlots_, 0);
  queryResults_.resize(static_cast<size_t>(maxSlots_) * kTimestampsPerTimingSlot);
}

TimestampQueries::~TimestampQueries() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  if (queryPool_ != VK_NULL_HANDLE) {
    ctx_.deferredTask(std::packaged_task<void()>(
        [vf = &ctx_.vf_, device = ctx_.getVkDevice(), queryPool = queryPool_]() {
          vf->vkDestroyQueryPool(device, queryPool, nullptr);
        }));
  }
}

uint32_t TimestampQueries::capacity() const {
  return maxSlots_;
}

uint32_t TimestampQueries::count() const {
  return currentSlot_;
}

void TimestampQueries::reset() {
  currentSlot_ = 0;
  commandBuffer_ = VK_NULL_HANDLE;
  resetRecorded_ = false;
  resultsReady_ = false;
  std::fill(elapsedNanos_.begin(), elapsedNanos_.end(), 0);
  std::fill(labels_.begin(), labels_.end(), std::string());
}

bool TimestampQueries::resultsAvailable() const {
  if (currentSlot_ == 0) {
    return true;
  }
  return updateResults();
}

uint64_t TimestampQueries::getElapsedNanos(uint32_t slotIndex) const {
  if (slotIndex >= currentSlot_ || !updateResults()) {
    return 0;
  }
  return elapsedNanos_[slotIndex];
}

bool TimestampQueries::isValid() const {
  return queryPool_ != VK_NULL_HANDLE;
}

uint32_t TimestampQueries::beginElapsedQuery(VkCommandBuffer commandBuffer, const char* label) {
  IGL_PROFILER_FUNCTION();
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  if (!isValid() || commandBuffer == VK_NULL_HANDLE || currentSlot_ >= maxSlots_ ||
      (commandBuffer_ != VK_NULL_HANDLE && commandBuffer_ != commandBuffer)) {
    return kInvalidSlot;
  }
  commandBuffer_ = commandBuffer;

  // The pool reset is recorded lazily on the first query of a command buffer.
  // vkCmdResetQueryPool must NOT be recorded inside an active render pass, so a
  // caller timing a render pass must issue the first beginElapsedQuery() before
  // vkCmdBeginRenderPass(). Compute callers (the current consumers) record this
  // outside any encoder, so the reset is always emitted at a legal point.
  if (!resetRecorded_) {
    ctx_.vf_.vkCmdResetQueryPool(
        commandBuffer, queryPool_, 0, maxSlots_ * kTimestampsPerTimingSlot);
    resetRecorded_ = true;
  }

  const uint32_t slot = currentSlot_++;
  labels_[slot] = label != nullptr ? label : "";
  resultsReady_ = false;

  ctx_.vf_.vkCmdWriteTimestamp(commandBuffer,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               queryPool_,
                               slot * kTimestampsPerTimingSlot);
  return slot;
}

void TimestampQueries::endElapsedQuery(VkCommandBuffer commandBuffer, uint32_t slotIndex) {
  IGL_PROFILER_FUNCTION();
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  if (!isValid() || commandBuffer == VK_NULL_HANDLE || commandBuffer != commandBuffer_ ||
      slotIndex >= currentSlot_) {
    return;
  }

  ctx_.vf_.vkCmdWriteTimestamp(commandBuffer,
                               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                               queryPool_,
                               slotIndex * kTimestampsPerTimingSlot + 1);
}

const char* TimestampQueries::getLabel(uint32_t slotIndex) const {
  if (slotIndex >= labels_.size()) {
    return "";
  }
  return labels_[slotIndex].c_str();
}

bool TimestampQueries::updateResults() const {
  IGL_PROFILER_FUNCTION();
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  if (resultsReady_) {
    return true;
  }
  if (!isValid() || currentSlot_ == 0) {
    return false;
  }

  // Reuse the pre-sized queryResults_ scratch (sized maxSlots_ * 2 in the ctor)
  // instead of allocating a fresh vector on every poll.
  const uint32_t queryCount = currentSlot_ * kTimestampsPerTimingSlot;
  const VkResult result = ctx_.vf_.vkGetQueryPoolResults(
      ctx_.getVkDevice(),
      queryPool_,
      0,
      queryCount,
      static_cast<size_t>(queryCount) * sizeof(QueryResult),
      queryResults_.data(),
      sizeof(QueryResult),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  if (result != VK_SUCCESS && result != VK_NOT_READY) {
    return false;
  }

  for (uint32_t i = 0; i < queryCount; ++i) {
    if (queryResults_[i].available == 0) {
      return false;
    }
  }

  for (uint32_t slot = 0; slot < currentSlot_; ++slot) {
    const uint64_t begin = queryResults_[slot * kTimestampsPerTimingSlot].timestamp;
    const uint64_t end = queryResults_[slot * kTimestampsPerTimingSlot + 1].timestamp;
    const uint64_t delta = end > begin ? end - begin : 0;
    elapsedNanos_[slot] = static_cast<uint64_t>(static_cast<double>(delta) * timestampPeriod_);
  }

  resultsReady_ = true;
  return true;
}

} // namespace igl::vulkan
