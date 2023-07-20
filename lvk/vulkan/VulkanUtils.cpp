/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanUtils.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>

VkSemaphore lvk::createSemaphore(VkDevice device, const char* debugName) {
  const VkSemaphoreCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .flags = 0,
  };
  VkSemaphore semaphore = VK_NULL_HANDLE;
  VK_ASSERT(vkCreateSemaphore(device, &ci, nullptr, &semaphore));
  VK_ASSERT(
      ivkSetDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, debugName));
  return semaphore;
}

VkFence lvk::createFence(VkDevice device, const char* debugName) {
  const VkFenceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = 0,
  };
  VkFence fence = VK_NULL_HANDLE;
  VK_ASSERT(vkCreateFence(device, &ci, nullptr, &fence));
  VK_ASSERT(ivkSetDebugObjectName(device, VK_OBJECT_TYPE_FENCE, (uint64_t)fence, debugName));
  return fence;
}

uint32_t lvk::findQueueFamilyIndex(VkPhysicalDevice physDev, VkQueueFlags flags) {
  using lvk::vulkan::DeviceQueues;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> props(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueFamilyCount, props.data());

  auto findDedicatedQueueFamilyIndex = [&props](VkQueueFlags require,
                                                VkQueueFlags avoid) -> uint32_t {
    for (uint32_t i = 0; i != props.size(); i++) {
      const bool isSuitable = props[i].queueFlags & require == require;
      const bool isDedicated = (props[i].queueFlags & avoid) == 0;
      if (props[i].queueCount && isSuitable && isDedicated)
        return i;
    }
    return DeviceQueues::INVALID;
  };

  // dedicated queue for compute
  if (flags & VK_QUEUE_COMPUTE_BIT) {
    const uint32_t q = findDedicatedQueueFamilyIndex(flags, VK_QUEUE_GRAPHICS_BIT);
    if (q != DeviceQueues::INVALID)
      return q;
  }

  // dedicated queue for transfer
  if (flags & VK_QUEUE_TRANSFER_BIT) {
    const uint32_t q = findDedicatedQueueFamilyIndex(flags, VK_QUEUE_GRAPHICS_BIT);
    if (q != DeviceQueues::INVALID)
      return q;
  }

  // any suitable
  return findDedicatedQueueFamilyIndex(flags, 0);
}

VmaAllocator lvk::createVmaAllocator(VkPhysicalDevice physDev,
                                     VkDevice device,
                                     VkInstance instance,
                                     uint32_t apiVersion) {
  const VmaVulkanFunctions funcs = {
      .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
      .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
      .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
      .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
      .vkAllocateMemory = vkAllocateMemory,
      .vkFreeMemory = vkFreeMemory,
      .vkMapMemory = vkMapMemory,
      .vkUnmapMemory = vkUnmapMemory,
      .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
      .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
      .vkBindBufferMemory = vkBindBufferMemory,
      .vkBindImageMemory = vkBindImageMemory,
      .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
      .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
      .vkCreateBuffer = vkCreateBuffer,
      .vkDestroyBuffer = vkDestroyBuffer,
      .vkCreateImage = vkCreateImage,
      .vkDestroyImage = vkDestroyImage,
      .vkCmdCopyBuffer = vkCmdCopyBuffer,
      .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
      .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
      .vkBindBufferMemory2KHR = vkBindBufferMemory2,
      .vkBindImageMemory2KHR = vkBindImageMemory2,
      .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
      .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
      .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
  };

  const VmaAllocatorCreateInfo ci = {
      .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice = physDev,
      .device = device,
      .preferredLargeHeapBlockSize = 0,
      .pAllocationCallbacks = nullptr,
      .pDeviceMemoryCallbacks = nullptr,
      .pHeapSizeLimit = nullptr,
      .pVulkanFunctions = &funcs,
      .instance = instance,
      .vulkanApiVersion = apiVersion,
  };
  VmaAllocator vma = VK_NULL_HANDLE;
  VK_ASSERT(vmaCreateAllocator(&ci, &vma));
  return vma;
}

glslang_resource_t lvk::getGlslangResource(const VkPhysicalDeviceLimits& limits) {
  const glslang_resource_t resource = {
      .max_lights = 32,
      .max_clip_planes = 6,
      .max_texture_units = 32,
      .max_texture_coords = 32,
      .max_vertex_attribs = (int)limits.maxVertexInputAttributes,
      .max_vertex_uniform_components = 4096,
      .max_varying_floats = 64,
      .max_vertex_texture_image_units = 32,
      .max_combined_texture_image_units = 80,
      .max_texture_image_units = 32,
      .max_fragment_uniform_components = 4096,
      .max_draw_buffers = 32,
      .max_vertex_uniform_vectors = 128,
      .max_varying_vectors = 8,
      .max_fragment_uniform_vectors = 16,
      .max_vertex_output_vectors = 16,
      .max_fragment_input_vectors = 15,
      .min_program_texel_offset = -8,
      .max_program_texel_offset = 7,
      .max_clip_distances = (int)limits.maxClipDistances,
      .max_compute_work_group_count_x = (int)limits.maxComputeWorkGroupCount[0],
      .max_compute_work_group_count_y = (int)limits.maxComputeWorkGroupCount[1],
      .max_compute_work_group_count_z = (int)limits.maxComputeWorkGroupCount[2],
      .max_compute_work_group_size_x = (int)limits.maxComputeWorkGroupSize[0],
      .max_compute_work_group_size_y = (int)limits.maxComputeWorkGroupSize[1],
      .max_compute_work_group_size_z = (int)limits.maxComputeWorkGroupSize[2],
      .max_compute_uniform_components = 1024,
      .max_compute_texture_image_units = 16,
      .max_compute_image_uniforms = 8,
      .max_compute_atomic_counters = 8,
      .max_compute_atomic_counter_buffers = 1,
      .max_varying_components = 60,
      .max_vertex_output_components = (int)limits.maxVertexOutputComponents,
      .max_geometry_input_components = (int)limits.maxGeometryInputComponents,
      .max_geometry_output_components = (int)limits.maxGeometryOutputComponents,
      .max_fragment_input_components = (int)limits.maxFragmentInputComponents,
      .max_image_units = 8,
      .max_combined_image_units_and_fragment_outputs = 8,
      .max_combined_shader_output_resources = 8,
      .max_image_samples = 0,
      .max_vertex_image_uniforms = 0,
      .max_tess_control_image_uniforms = 0,
      .max_tess_evaluation_image_uniforms = 0,
      .max_geometry_image_uniforms = 0,
      .max_fragment_image_uniforms = 8,
      .max_combined_image_uniforms = 8,
      .max_geometry_texture_image_units = 16,
      .max_geometry_output_vertices = (int)limits.maxGeometryOutputVertices,
      .max_geometry_total_output_components = (int)limits.maxGeometryTotalOutputComponents,
      .max_geometry_uniform_components = 1024,
      .max_geometry_varying_components = 64,
      .max_tess_control_input_components =
          (int)limits.maxTessellationControlPerVertexInputComponents,
      .max_tess_control_output_components =
          (int)limits.maxTessellationControlPerVertexOutputComponents,
      .max_tess_control_texture_image_units = 16,
      .max_tess_control_uniform_components = 1024,
      .max_tess_control_total_output_components = 4096,
      .max_tess_evaluation_input_components = (int)limits.maxTessellationEvaluationInputComponents,
      .max_tess_evaluation_output_components =
          (int)limits.maxTessellationEvaluationOutputComponents,
      .max_tess_evaluation_texture_image_units = 16,
      .max_tess_evaluation_uniform_components = 1024,
      .max_tess_patch_components = 120,
      .max_patch_vertices = 32,
      .max_tess_gen_level = 64,
      .max_viewports = (int)limits.maxViewports,
      .max_vertex_atomic_counters = 0,
      .max_tess_control_atomic_counters = 0,
      .max_tess_evaluation_atomic_counters = 0,
      .max_geometry_atomic_counters = 0,
      .max_fragment_atomic_counters = 8,
      .max_combined_atomic_counters = 8,
      .max_atomic_counter_bindings = 1,
      .max_vertex_atomic_counter_buffers = 0,
      .max_tess_control_atomic_counter_buffers = 0,
      .max_tess_evaluation_atomic_counter_buffers = 0,
      .max_geometry_atomic_counter_buffers = 0,
      .max_fragment_atomic_counter_buffers = 1,
      .max_combined_atomic_counter_buffers = 1,
      .max_atomic_counter_buffer_size = 16384,
      .max_transform_feedback_buffers = 4,
      .max_transform_feedback_interleaved_components = 64,
      .max_cull_distances = (int)limits.maxCullDistances,
      .max_combined_clip_and_cull_distances = (int)limits.maxCombinedClipAndCullDistances,
      .max_samples = 4,
      .max_mesh_output_vertices_nv = 256,
      .max_mesh_output_primitives_nv = 512,
      .max_mesh_work_group_size_x_nv = 32,
      .max_mesh_work_group_size_y_nv = 1,
      .max_mesh_work_group_size_z_nv = 1,
      .max_task_work_group_size_x_nv = 32,
      .max_task_work_group_size_y_nv = 1,
      .max_task_work_group_size_z_nv = 1,
      .max_mesh_view_count_nv = 4,
      .maxDualSourceDrawBuffersEXT = 1,
      .limits = {
          .non_inductive_for_loops = true,
          .while_loops = true,
          .do_while_loops = true,
          .general_uniform_indexing = true,
          .general_attribute_matrix_vector_indexing = true,
          .general_varying_indexing = true,
          .general_sampler_indexing = true,
          .general_variable_indexing = true,
          .general_constant_matrix_vector_indexing = true,
      }};

  return resource;
}
