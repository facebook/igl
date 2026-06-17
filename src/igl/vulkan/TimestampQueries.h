/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
// Do not load Vulkan function prototypes, as the IGL Vulkan backend loads
// functions dynamically using Volk. Including <vulkan/vulkan_core.h> with
// prototypes enabled would conflict with the function-pointer declarations
// pulled in transitively from volk.h.
#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES
#endif // !defined(VK_NO_PROTOTYPES)
#include <vulkan/vulkan_core.h>
#include <igl/TimestampQueries.h>

namespace igl::vulkan {

class VulkanContext;

class TimestampQueries final : public ITimestampQueries {
 public:
  static constexpr uint32_t kTimestampsPerTimingSlot = 2;
  static constexpr uint32_t kInvalidSlot = UINT32_MAX;

  TimestampQueries(VulkanContext& ctx, uint32_t maxSlots);
  ~TimestampQueries() override;

  TimestampQueries(const TimestampQueries&) = delete;
  TimestampQueries& operator=(const TimestampQueries&) = delete;
  TimestampQueries(TimestampQueries&&) = delete;
  TimestampQueries& operator=(TimestampQueries&&) = delete;

  [[nodiscard]] uint32_t capacity() const override;
  [[nodiscard]] uint32_t count() const override;
  void reset() override;
  [[nodiscard]] bool resultsAvailable() const override;
  [[nodiscard]] uint64_t getElapsedNanos(uint32_t slotIndex) const override;
  [[nodiscard]] bool isValid() const override;

  [[nodiscard]] uint32_t beginElapsedQuery(VkCommandBuffer commandBuffer, const char* label);
  void endElapsedQuery(VkCommandBuffer commandBuffer, uint32_t slotIndex);

  [[nodiscard]] const char* getLabel(uint32_t slotIndex) const override;

 private:
  // One raw timestamp query result (value + availability), read back via
  // VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT.
  struct QueryResult {
    uint64_t timestamp = 0;
    uint64_t available = 0;
  };

  bool updateResults() const;

  VulkanContext& ctx_;
  VkQueryPool queryPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  uint32_t maxSlots_ = 0;
  uint32_t currentSlot_ = 0;
  bool resetRecorded_ = false;
  float timestampPeriod_ = 0.0f;
  std::vector<std::string> labels_;

  mutable bool resultsReady_ = false;
  mutable std::vector<uint64_t> elapsedNanos_;
  // Result readback scratch, sized once in the ctor (maxSlots_ * 2) and reused
  // every updateResults() call to avoid per-poll heap allocation in the hot path.
  mutable std::vector<QueryResult> queryResults_;
};

} // namespace igl::vulkan
