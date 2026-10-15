/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>
#include <igl/Timer.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl::vulkan {

class VulkanContext;

/// @brief Measures the GPU time of the command buffers it is attached to
/// (`CommandBufferDesc::timer`), from their first to their last command.
///
/// Each attached command buffer measures with one of `kNumSlots` pairs of timestamp queries, used
/// round-robin, skipping slots whose command buffers are still pending when a free slot exists.
/// The results are those of the most recently submitted command buffer that has completed. They
/// stay readable until a newer command buffer completes.
///
/// When more command buffers are in flight (recorded or pending) than there are slots, slots are
/// reused and the older measurements in them are discarded. `begin()` never waits.
///
/// The start timestamp is written at `VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT`, so with back-to-back
/// submissions it can be taken while earlier work is still running, as with Metal's
/// `GPUStartTime`.
class Timer final : public ITimer {
 public:
  static constexpr uint32_t kNumSlots = 4;

  explicit Timer(VulkanContext& ctx);
  ~Timer() override;

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;
  Timer(Timer&&) = delete;
  Timer& operator=(Timer&&) = delete;

  [[nodiscard]] uint64_t getElapsedTimeNanos() const override;
  [[nodiscard]] bool resultsAvailable() const override;

  [[nodiscard]] bool isValid() const;
  [[nodiscard]] bool belongsTo(const VulkanContext& ctx) const noexcept {
    return &ctx_ == &ctx;
  }

  /// @brief Starts measuring `cmdBuf`, which must have no commands recorded yet, and returns the
  /// slot that measures it. It never waits: it takes the first slot, in turn, that is unused or
  /// whose command buffer has completed, keeping that result. If every slot is pending, it takes
  /// the next one in turn and discards its measurement.
  [[nodiscard]] uint32_t begin(VkCommandBuffer cmdBuf);
  /// @brief Records the end of the measurement of `cmdBuf`, right before it is submitted.
  void end(VkCommandBuffer cmdBuf, uint32_t slot);
  /// @brief Marks the measurement of `slot` as the newest one. `handle` is the submission of the
  /// command buffer that was measured.
  void submitted(uint32_t slot, VulkanImmediateCommands::SubmitHandle handle);

 private:
  struct Slot {
    // Empty until the command buffer that measures with the slot is submitted
    VulkanImmediateCommands::SubmitHandle handle;
    // Order of submission among all the measurements of this timer. 0 until submitted.
    uint64_t sequence = 0;
  };

  void updateResults() const;
  bool tryReadSlot(uint32_t slotIndex) const;

  VulkanContext& ctx_;
  VkQueryPool queryPool_ = VK_NULL_HANDLE;
  float timestampPeriod_ = 0.0f;
  uint64_t timestampMask_ = 0;
  uint32_t nextSlot_ = 0;
  uint64_t numSubmitted_ = 0;
  std::array<Slot, kNumSlots> slots_;

  // The latest completed measurement and its `Slot::sequence`. 0 if none has completed.
  mutable uint64_t resultSequence_ = 0;
  mutable uint64_t elapsedNanos_ = 0;
};

} // namespace igl::vulkan
