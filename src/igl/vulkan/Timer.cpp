/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Timer.h>

#include <algorithm>
#include <future>
#include <numeric>
#include <igl/vulkan/VulkanContext.h>

namespace igl::vulkan {

namespace {

constexpr uint32_t kQueriesPerSlot = 2;

// One timestamp query result (value + availability), read back via
// VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT, which needs 8-byte aligned data
// even where a uint64_t is only 4-byte aligned, as on 32-bit x86.
struct alignas(8) QueryResult {
  uint64_t timestamp = 0;
  uint64_t available = 0;
};
static_assert(sizeof(QueryResult) == 2 * sizeof(uint64_t));

} // namespace

Timer::Timer(VulkanContext& ctx) : ctx_(ctx) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  timestampPeriod_ = ctx_.getVkPhysicalDeviceProperties().limits.timestampPeriod;
  const uint32_t validBits = ctx_.getGraphicsQueueTimestampValidBits();
  timestampMask_ = validBits >= 64 ? ~uint64_t{0} : (uint64_t{1} << validBits) - 1;

  const VkQueryPoolCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = kNumSlots * kQueriesPerSlot,
  };
  if (ctx_.vf_.vkCreateQueryPool(ctx_.getVkDevice(), &createInfo, nullptr, &queryPool_) !=
      VK_SUCCESS) {
    queryPool_ = VK_NULL_HANDLE;
    return;
  }
  const uint64_t poolHandle = (uint64_t)queryPool_;
  VK_ASSERT(ivkSetDebugObjectName(&ctx_.vf_,
                                  ctx_.getVkDevice(),
                                  VK_OBJECT_TYPE_QUERY_POOL,
                                  poolHandle,
                                  IGL_FORMAT("Query Pool: Timer {:#x}", poolHandle).c_str()));
}

Timer::~Timer() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  if (queryPool_ != VK_NULL_HANDLE) {
    ctx_.deferredTask(std::packaged_task<void()>(
        [vf = &ctx_.vf_, device = ctx_.getVkDevice(), queryPool = queryPool_]() {
          vf->vkDestroyQueryPool(device, queryPool, nullptr);
        }));
  }
}

bool Timer::isValid() const {
  return queryPool_ != VK_NULL_HANDLE;
}

uint64_t Timer::getElapsedTimeNanos() const {
  IGL_PROFILER_FUNCTION();
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  updateResults();
  return elapsedNanos_;
}

bool Timer::resultsAvailable() const {
  IGL_PROFILER_FUNCTION();
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  updateResults();
  return resultSequence_ != 0;
}

uint32_t Timer::begin(VkCommandBuffer cmdBuf) {
  IGL_PROFILER_FUNCTION();
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  // Prefer a slot that is unused or has completed over discarding a pending measurement. The GPU
  // orders the reset below after the writes of the command buffers that are still pending.
  uint32_t slotIndex = nextSlot_;
  for (uint32_t i = 0; i < kNumSlots; ++i) {
    const uint32_t candidate = (nextSlot_ + i) % kNumSlots;
    const VulkanImmediateCommands::SubmitHandle handle = slots_[candidate].handle;
    if (handle.empty() || ctx_.immediate_->isReady(handle)) {
      slotIndex = candidate;
      break;
    }
  }
  nextSlot_ = (slotIndex + 1) % kNumSlots;

  // The reset below discards the measurement that the queries hold, so keep it if it has completed.
  tryReadSlot(slotIndex);
  slots_[slotIndex] = {};

  ctx_.vf_.vkCmdResetQueryPool(cmdBuf, queryPool_, slotIndex * kQueriesPerSlot, kQueriesPerSlot);
  ctx_.vf_.vkCmdWriteTimestamp(
      cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_, slotIndex * kQueriesPerSlot);
  return slotIndex;
}

void Timer::end(VkCommandBuffer cmdBuf, uint32_t slot) {
  IGL_PROFILER_FUNCTION();
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  if (!IGL_DEBUG_VERIFY(slot < kNumSlots)) {
    return;
  }
  ctx_.vf_.vkCmdWriteTimestamp(
      cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool_, slot * kQueriesPerSlot + 1);
}

void Timer::submitted(uint32_t slot, VulkanImmediateCommands::SubmitHandle handle) {
  IGL_PROFILER_FUNCTION();
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(&ctx_);

  if (!IGL_DEBUG_VERIFY(slot < kNumSlots)) {
    return;
  }
  slots_[slot].handle = handle;
  slots_[slot].sequence = ++numSubmitted_;
}

void Timer::updateResults() const {
  // Newest submission first: the first slot that has completed holds the latest result.
  std::array<uint32_t, kNumSlots> order = {};
  std::iota(order.begin(), order.end(), 0u);
  std::sort(order.begin(), order.end(), [this](uint32_t lhs, uint32_t rhs) {
    return slots_[lhs].sequence > slots_[rhs].sequence;
  });
  for (const uint32_t slotIndex : order) {
    if (tryReadSlot(slotIndex)) {
      return;
    }
  }
}

bool Timer::tryReadSlot(uint32_t slotIndex) const {
  const Slot& slot = slots_[slotIndex];
  // The queries hold the measurement of a slot only once its command buffer has completed. Before
  // that, they still hold the previous measurement, or have not been reset yet.
  if (slot.handle.empty() || slot.sequence <= resultSequence_ ||
      !ctx_.immediate_->isReady(slot.handle)) {
    return false;
  }

  std::array<QueryResult, kQueriesPerSlot> results = {};
  const VkResult result = ctx_.vf_.vkGetQueryPoolResults(ctx_.getVkDevice(),
                                                         queryPool_,
                                                         slotIndex * kQueriesPerSlot,
                                                         kQueriesPerSlot,
                                                         sizeof(results),
                                                         results.data(),
                                                         sizeof(QueryResult),
                                                         VK_QUERY_RESULT_64_BIT |
                                                             VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  if ((result != VK_SUCCESS && result != VK_NOT_READY) || results[0].available == 0 ||
      results[1].available == 0) {
    return false;
  }

  // Only the valid bits of a timestamp are meaningful, and the counter may wrap around between the
  // two timestamps.
  const uint64_t beginTicks = results[0].timestamp & timestampMask_;
  const uint64_t endTicks = results[1].timestamp & timestampMask_;
  const uint64_t elapsedTicks = (endTicks - beginTicks) & timestampMask_;
  // A timestamp that steps backwards wraps around to nearly the whole range of the counter
  if (elapsedTicks > timestampMask_ / 2) {
    return false;
  }
  elapsedNanos_ = static_cast<uint64_t>(static_cast<double>(elapsedTicks) * timestampPeriod_);
  resultSequence_ = slot.sequence;
  return true;
}

} // namespace igl::vulkan
