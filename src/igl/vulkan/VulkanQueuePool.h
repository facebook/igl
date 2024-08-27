/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <set>
#include <vector>

#include <igl/vulkan/Common.h>

namespace igl::vulkan {

struct VulkanQueueDescriptor {
  constexpr static uint32_t INVALID = 0xFFFFFFFF;

  VkQueueFlags queueFlags = 0;
  uint32_t queueIndex = INVALID;
  uint32_t familyIndex = INVALID;

  [[nodiscard]] bool isValid() const {
    return queueIndex != INVALID && familyIndex != INVALID;
  }

  /* familyIndex and queueIndex are sufficient to uniquely identify a VulkanQueueDescriptor. */
  bool operator==(const VulkanQueueDescriptor& other) const {
    return (familyIndex == other.familyIndex && queueIndex == other.queueIndex);
  }

  bool operator<(const VulkanQueueDescriptor& other) const {
    if (familyIndex == other.familyIndex) {
      return queueIndex < other.queueIndex;
    }
    return familyIndex < other.familyIndex;
  }
};

class VulkanQueuePool final {
 public:
  VulkanQueuePool(const VulkanFunctionTable& vf, VkPhysicalDevice physicalDevice);
  explicit VulkanQueuePool(std::set<VulkanQueueDescriptor> availableDescriptors);

  /* Find a queue descriptor that conforms to give queue flags. */
  [[nodiscard]] VulkanQueueDescriptor findQueueDescriptor(VkQueueFlags flags) const;

  /* Reserve the given queue. Reserved queues will not be visible in future
   * find requests and they will participate in resulting queue creation infos.
   */
  void reserveQueue(const VulkanQueueDescriptor& queueDescriptor);

  /* Create the queue creation infos for reserved queues. */
  [[nodiscard]] std::vector<VkDeviceQueueCreateInfo> getQueueCreationInfos() const;

 private:
  std::set<VulkanQueueDescriptor> availableDescriptors_;
  std::set<VulkanQueueDescriptor> reservedDescriptors_;
};

} // namespace igl::vulkan
