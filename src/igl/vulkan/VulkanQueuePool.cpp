/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanQueuePool.h"

#include <igl/Log.h>
#include <map>

namespace igl::vulkan {
namespace {

std::set<VulkanQueueDescriptor> enumerateQueues(const VulkanFunctionTable& vf,
                                                VkPhysicalDevice physicalDevice) {
  uint32_t queueFamilyCount = 0;
  vf.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
  vf.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, properties.data());

  std::set<VulkanQueueDescriptor> descriptors;
  for (uint32_t i = 0; i != properties.size(); i++) {
    for (uint32_t j = 0; j != properties[i].queueCount; j++) {
      VulkanQueueDescriptor descriptor;
      descriptor.queueIndex = j;
      descriptor.familyIndex = i;
      descriptor.queueFlags = properties[i].queueFlags;
      descriptors.insert(descriptor);
    }
  }
  return descriptors;
}

} // namespace

VulkanQueuePool::VulkanQueuePool(const VulkanFunctionTable& vf, VkPhysicalDevice physicalDevice) :
  VulkanQueuePool(enumerateQueues(vf, physicalDevice)) {}

VulkanQueuePool::VulkanQueuePool(std::set<VulkanQueueDescriptor> availableDescriptors) :
  availableDescriptors_(std::move(availableDescriptors)) {}

VulkanQueueDescriptor VulkanQueuePool::findQueueDescriptor(VkQueueFlags flags) const {
  auto findDedicatedQueue = [&](VkQueueFlags required,
                                VkQueueFlags avoid) -> VulkanQueueDescriptor {
    if (flags & required) {
      for (const auto& queueDescriptor : availableDescriptors_) {
        const bool isSuitable = (queueDescriptor.queueFlags & flags) != 0;
        const bool isDedicated = (queueDescriptor.queueFlags & avoid) == 0;
        if (isSuitable && isDedicated) {
          return queueDescriptor;
        }
      }
    }
    return {};
  };

  VulkanQueueDescriptor queueDescriptor;

  // try to find a dedicated queue for compute
  queueDescriptor = findDedicatedQueue(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT);
  if (queueDescriptor.isValid()) {
    return queueDescriptor;
  }
  // try to find a dedicated queue for transfer operations
  queueDescriptor = findDedicatedQueue(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT);
  if (queueDescriptor.isValid()) {
    return queueDescriptor;
  }
  // any suitable queue
  queueDescriptor = findDedicatedQueue(flags, 0);
  if (queueDescriptor.isValid()) {
    return queueDescriptor;
  }

  // Compute and graphics queues support transfer operations, and it is optional to report
  // VK_QUEUE_TRANSFER_BIT on those. So let's check them if no result is found
  if (flags & VK_QUEUE_TRANSFER_BIT) {
    const VkQueueFlags clearFlags = flags & ~VK_QUEUE_TRANSFER_BIT;
    queueDescriptor = findDedicatedQueue(clearFlags | VK_QUEUE_COMPUTE_BIT, 0);
    if (queueDescriptor.isValid()) {
      return queueDescriptor;
    }

    queueDescriptor = findDedicatedQueue(clearFlags | VK_QUEUE_GRAPHICS_BIT, 0);
    if (queueDescriptor.isValid()) {
      return queueDescriptor;
    }
  }

  IGL_LOG_ERROR("No suitable queue found");

  return {};
}

void VulkanQueuePool::reserveQueue(const VulkanQueueDescriptor& queueDescriptor) {
  if (availableDescriptors_.erase(queueDescriptor) != 0) {
    reservedDescriptors_.insert(queueDescriptor);
  }
}

std::vector<VkDeviceQueueCreateInfo> VulkanQueuePool::getQueueCreationInfos() const {
  std::map<uint32_t, std::vector<VulkanQueueDescriptor>> queues;
  for (const auto& queue : reservedDescriptors_) {
    queues[queue.familyIndex].push_back(queue);
  }

  static const float queuePriority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> qcis;
  for (const auto& [family, descriptors] : queues) {
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = family;
    qci.queueCount = static_cast<uint32_t>(descriptors.size());
    qci.pQueuePriorities = &queuePriority;
    qcis.push_back(qci);
  }
  return qcis;
}

} // namespace igl::vulkan
