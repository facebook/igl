/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanCommandPool.h>
#include <igl/vulkan/VulkanFence.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanSemaphore.h>

namespace igl::vulkan {

/// @brief This class provides a simplified interface for obtaining and submitting Command Buffers,
/// while providing features to help manage their synchronization.
class VulkanImmediateCommands final {
 public:
  // The maximum number of command buffers which can simultaneously exist in the system; when we run
  // out of buffers, we stall and wait until an existing buffer becomes available
  static constexpr uint32_t kMaxCommandBuffers = 32;

  /** @brief Creates an instance of the class for a specific queue family and whether the fences
   * created for each command buffer are exportable (see VulkanFence for more details about the
   * exportable flag). The optional `debugName` parameter can be used to name the resource to make
   * it easier for debugging
   * The constructor initializes the vector of `CommandBufferWrapper` structures with
   * a total of `kMaxCommandBuffers`
   */
  VulkanImmediateCommands(const VulkanFunctionTable& vf,
                          VkDevice device,
                          uint32_t queueFamilyIndex,
                          bool exportableFences,
                          const char* debugName);
  ~VulkanImmediateCommands();
  VulkanImmediateCommands(const VulkanImmediateCommands&) = delete;
  VulkanImmediateCommands& operator=(const VulkanImmediateCommands&) = delete;

  /** @brief A structure that encapsulates synchronization information about command buffers and
   * that is used by the `VulkanImmediateCommands` class to manage command buffer acquisition and
   * reuse.
   * A `SubmitHandle` is composed of two 32-bit integers, a buffer index (`bufferIndex_`) and a
   * submit id (`submitId_`). The buffer index is associated with the location of the command buffer
   * in the vector in which they are stored in `VulkanImmediateCommands` class. The submit id is a
   * monotonically increasing index that is incremented every time we `submit()` a command buffer
   * for execution (any command buffer). A handle is a combination of those two values into a 64-bit
   * integer: the submit id is shifted and occupies the 32 most significant bits of the handle,
   * while the buffer index occupies the least significant 32 bits
   */
  struct SubmitHandle {
    uint32_t bufferIndex_ = 0;
    uint32_t submitId_ = 0;
    SubmitHandle() = default;

    /// @brief Creates a SubmitHandle object from an existing handle
    explicit SubmitHandle(uint64_t handle) :
      bufferIndex_(uint32_t(handle & 0xffffffff)), submitId_(uint32_t(handle >> 32)) {
      IGL_ASSERT(submitId_);
    }

    /// @brief Checks whether the structure is empty and has not been associates with a command
    /// buffer submission yet
    [[nodiscard]] bool empty() const {
      return submitId_ == 0;
    }

    /// @brief Returns a unique identifiable handle, which is made of the `submitId_` and the
    /// `bufferIndex_` member variables
    [[nodiscard]] uint64_t handle() const {
      return (uint64_t(submitId_) << 32) + bufferIndex_;
    }
  };

  /// Ensures that the `SubmitHandle` structure size is not larger than a `uint64_t`
  static_assert(sizeof(SubmitHandle) == sizeof(uint64_t));

  /// @brief The CommandBufferWrapper structure encapsulates all the information needed to manage
  /// the synchronization of a command buffer along with a command buffer
  struct CommandBufferWrapper {
    CommandBufferWrapper(VulkanFence&& fence, VulkanSemaphore&& semaphore, bool isValid = true) :
      fence_(std::move(fence)), semaphore_(std::move(semaphore)), isValid_(isValid) {}

    /// @brief The command buffer handle. It is initialied to VK_NULL_HANDLE. The command buffer
    /// handle stored in `cmdBufAllocated_` is copied into `cmdBuf_` when the command buffer is
    /// acquired for recording
    VkCommandBuffer cmdBuf_ = VK_NULL_HANDLE;
    /// @brief Stores the command buffer handle allocated during initialization
    VkCommandBuffer cmdBufAllocated_ = VK_NULL_HANDLE;
    /// @brief the SubmitHandle object used to synchronize this command buffer
    SubmitHandle handle_ = {};
    /// @brief A VulkanFence object that is associated with the submission of the command buffer. It
    /// is used to check whether a command buffer is still executing or for waiting the command
    /// buffer to finish execution by the GPU
    VulkanFence fence_;
    /// @brief A VulkanSemaphore object associated with the submission of the command buffer for
    /// execution.
    VulkanSemaphore semaphore_;
    bool isEncoding_ = false;

    /// @brief Tells a valid non-empty command buffer wrapper from an empty one apart. It is used
    /// as a safe way to handle failures to acquire a command buffer.
    const bool isValid_ = true;
  };

  [[nodiscard]] bool canAcquire();

  /// @brief Returns a `CommandBufferWrapper` object with the current command buffer (creates one if
  /// it does not exist) and its associated synchronization objects
  const CommandBufferWrapper& acquire();

  /** @brief Submits a command buffer (stored in a `CommandBufferWrapper` object) for submission and
   * returns the `SubmitHandle` associated with the command buffer. Caches the semaphore associated
   * with the command buffer bineg submitted as the last submitted semaphore
   * (`lastSubmitSemaphore_`). Caches the SubmitHandle associated with the command buffer being
   * submitted for execution in `lastSubmitHandle_`. Resets the current wait semaphore member
   * variable (`waitSemaphore_`).
   *  Submitting a command buffer also marks the `CommandBufferWrapper::encoding_` variable to
   * `false`
   */
  SubmitHandle submit(const CommandBufferWrapper& wrapper);

  /// @brief Stores the semaphore as the current wait semaphore (`waitSemaphore_`)
  void waitSemaphore(VkSemaphore semaphore);

  /// @brief Returns the last semaphore (`lastSubmitSemaphore_`) and reset the member variable to
  /// `VK_NULL_HANDLE`
  VkSemaphore acquireLastSubmitSemaphore();

  /// @brief Returns the last SubmitHandle, which was submitted when `submit()` was last called
  [[nodiscard]] SubmitHandle getLastSubmitHandle() const;

  /// @brief Checks whether the SubmitHandle is recycled. A recycled SubmitHandle is a handle that
  /// has a submit id greater than the submit id associated with the same command buffer stored
  /// internally in `VulkanImmediateCommands`. A SubmitHandle handle is also recycled if it's empty
  [[nodiscard]] bool isRecycled(SubmitHandle handle) const;

  /** @brief Checks whether a SubmitHandle is ready. A SubmitHandle is ready if it is recycled or
   * empty. If it has not been recycled and is not empty, a SubmitHandle is ready if the fence
   * associated with the command buffer referred by the SubmitHandle structure has been signaled.
   *  Note that this function does not wait for a fence to be signaled if it has not been signaled.
   * It merely checks the fence status
   */
  [[nodiscard]] bool isReady(SubmitHandle handle) const;

  /// @brief If the SubmitHandle is not ready, this function waits for the fence associated with the
  /// command buffer referred by the handle to become signaled. The default wait time is
  /// `UINT64_MAX` nanoseconds. Returns a result code if the wait was successful or not.
  VkResult wait(SubmitHandle handle, uint64_t timeoutNanoseconds = UINT64_MAX);

  /// @brief Wait for _all_ fences for all command buffers stored in `VulkanImmediateCommands` to
  /// become signaled. The maximum wait time is `UINT64_MAX` nanoseconds
  void waitAll();

  /// @brief Returns the fence associated with the handle if the handle has not been recycled.
  /// Returns `VK_NULL_HANDLE` otherwise.
  VkFence getVkFenceFromSubmitHandle(SubmitHandle handle);

 private:
  /// @brief Resets all commands buffers and their associated fences that are valid, are not being
  /// encoded, and have completed execution by the GPU (their fences have been signaled). Resets the
  /// number of available command buffers.
  void purge();

 private:
  const VulkanFunctionTable& vf_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VulkanCommandPool commandPool_;
  std::string debugName_;
  std::vector<CommandBufferWrapper> buffers_;

  /// @brief The null/invalid command buffer wrapper that is returned when a command buffer cannot
  /// be acquired.
  const CommandBufferWrapper emptyCommandBufferWrapper_;

  /// @brief The last submitted handle. Updated on `submit()`
  SubmitHandle lastSubmitHandle_ = SubmitHandle();

  /// @brief The semaphore submitted with the last command buffer. Updated on `submit()`
  VkSemaphore lastSubmitSemaphore_ = VK_NULL_HANDLE;

  /// @brief A semaphore to be associated with the next command buffer to be submitted. Can be used
  /// with command buffers that present swapchain images.
  VkSemaphore waitSemaphore_ = VK_NULL_HANDLE;
  uint32_t numAvailableCommandBuffers_ = kMaxCommandBuffers;

  // @brief The submission counter. Incremented on `submit()`
  uint32_t submitCounter_ = 1;
};

} // namespace igl::vulkan
