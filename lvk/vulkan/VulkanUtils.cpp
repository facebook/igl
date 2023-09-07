/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanUtils.h"

#include <lvk/vulkan/VulkanClasses.h>

#include <glslang/Include/glslang_c_interface.h>
#include <ldrutils/lutils/ScopeExit.h>

const char* lvk::getVulkanResultString(VkResult result) {
#define RESULT_CASE(res) \
  case res:              \
    return #res
  switch (result) {
    RESULT_CASE(VK_SUCCESS);
    RESULT_CASE(VK_NOT_READY);
    RESULT_CASE(VK_TIMEOUT);
    RESULT_CASE(VK_EVENT_SET);
    RESULT_CASE(VK_EVENT_RESET);
    RESULT_CASE(VK_INCOMPLETE);
    RESULT_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
    RESULT_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    RESULT_CASE(VK_ERROR_INITIALIZATION_FAILED);
    RESULT_CASE(VK_ERROR_DEVICE_LOST);
    RESULT_CASE(VK_ERROR_MEMORY_MAP_FAILED);
    RESULT_CASE(VK_ERROR_LAYER_NOT_PRESENT);
    RESULT_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
    RESULT_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
    RESULT_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
    RESULT_CASE(VK_ERROR_TOO_MANY_OBJECTS);
    RESULT_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
    RESULT_CASE(VK_ERROR_SURFACE_LOST_KHR);
    RESULT_CASE(VK_ERROR_OUT_OF_DATE_KHR);
    RESULT_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    RESULT_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    RESULT_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
    RESULT_CASE(VK_ERROR_FRAGMENTED_POOL);
    RESULT_CASE(VK_ERROR_UNKNOWN);
    // Provided by VK_VERSION_1_1
    RESULT_CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
    // Provided by VK_VERSION_1_1
    RESULT_CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
    // Provided by VK_VERSION_1_2
    RESULT_CASE(VK_ERROR_FRAGMENTATION);
    // Provided by VK_VERSION_1_2
    RESULT_CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
    // Provided by VK_KHR_swapchain
    RESULT_CASE(VK_SUBOPTIMAL_KHR);
    // Provided by VK_NV_glsl_shader
    RESULT_CASE(VK_ERROR_INVALID_SHADER_NV);
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR);
#endif
    // Provided by VK_EXT_image_drm_format_modifier
    RESULT_CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
    // Provided by VK_KHR_global_priority
    RESULT_CASE(VK_ERROR_NOT_PERMITTED_KHR);
    // Provided by VK_EXT_full_screen_exclusive
    RESULT_CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
    // Provided by VK_KHR_deferred_host_operations
    RESULT_CASE(VK_THREAD_IDLE_KHR);
    // Provided by VK_KHR_deferred_host_operations
    RESULT_CASE(VK_THREAD_DONE_KHR);
    // Provided by VK_KHR_deferred_host_operations
    RESULT_CASE(VK_OPERATION_DEFERRED_KHR);
    // Provided by VK_KHR_deferred_host_operations
    RESULT_CASE(VK_OPERATION_NOT_DEFERRED_KHR);
  default:
    return "Unknown VkResult Value";
  }
#undef RESULT_CASE
}

void lvk::setResultFrom(Result* outResult, VkResult result) {
  if (outResult) {
    *outResult = getResultFromVkResult(result);
  }
}

lvk::Result lvk::getResultFromVkResult(VkResult result) {
  if (result == VK_SUCCESS) {
    return Result();
  }

  Result res(Result::Code::RuntimeError, lvk::getVulkanResultString(result));

  switch (result) {
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
  case VK_ERROR_OUT_OF_POOL_MEMORY:
  case VK_ERROR_TOO_MANY_OBJECTS:
    res.code = Result::Code::ArgumentOutOfRange;
    return res;
  default:;
    // skip other Vulkan error codes
  }
  return res;
}

VkFormat lvk::formatToVkFormat(lvk::Format format) {
  using TextureFormat = ::lvk::Format;
  switch (format) {
  case lvk::Format_Invalid:
    return VK_FORMAT_UNDEFINED;
  case lvk::Format_R_UN8:
    return VK_FORMAT_R8_UNORM;
  case lvk::Format_R_UN16:
    return VK_FORMAT_R16_UNORM;
  case lvk::Format_R_F16:
    return VK_FORMAT_R16_SFLOAT;
  case lvk::Format_R_UI16:
    return VK_FORMAT_R16_UINT;
  case lvk::Format_RG_UN8:
    return VK_FORMAT_R8G8_UNORM;
  case lvk::Format_RG_UN16:
    return VK_FORMAT_R16G16_UNORM;
  case lvk::Format_BGRA_UN8:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case lvk::Format_RGBA_UN8:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case lvk::Format_RGBA_SRGB8:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case lvk::Format_BGRA_SRGB8:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case lvk::Format_RG_F16:
    return VK_FORMAT_R16G16_SFLOAT;
  case lvk::Format_RG_F32:
    return VK_FORMAT_R32G32_SFLOAT;
  case lvk::Format_RG_UI16:
    return VK_FORMAT_R16G16_UINT;
  case lvk::Format_R_F32:
    return VK_FORMAT_R32_SFLOAT;
  case lvk::Format_RGBA_F16:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case lvk::Format_RGBA_UI32:
    return VK_FORMAT_R32G32B32A32_UINT;
  case lvk::Format_RGBA_F32:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case lvk::Format_ETC2_RGB8:
    return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
  case lvk::Format_ETC2_SRGB8:
    return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
  case lvk::Format_BC7_RGBA:
    return VK_FORMAT_BC7_UNORM_BLOCK;
  case lvk::Format_Z_UN16:
    return VK_FORMAT_D16_UNORM;
  case lvk::Format_Z_UN24:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case lvk::Format_Z_F32:
    return VK_FORMAT_D32_SFLOAT;
  case lvk::Format_Z_UN24_S_UI8:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case lvk::Format_Z_F32_S_UI8:
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  }
#if defined(_MSC_VER)
  LVK_ASSERT_MSG(false, "TextureFormat value not handled: %d", (int)format);
  return VK_FORMAT_UNDEFINED;
#endif // _MSC_VER
}

lvk::Format lvk::vkFormatToFormat(VkFormat format) {
  switch (format) {
  case VK_FORMAT_UNDEFINED:
    return Format_Invalid;
  case VK_FORMAT_R8_UNORM:
    return Format_R_UN8;
  case VK_FORMAT_R16_UNORM:
    return Format_R_UN16;
  case VK_FORMAT_R16_SFLOAT:
    return Format_R_F16;
  case VK_FORMAT_R16_UINT:
    return Format_R_UI16;
  case VK_FORMAT_R8G8_UNORM:
    return Format_RG_UN8;
  case VK_FORMAT_B8G8R8A8_UNORM:
    return Format_BGRA_UN8;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return Format_RGBA_UN8;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return Format_RGBA_SRGB8;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return Format_BGRA_SRGB8;
  case VK_FORMAT_R16G16_UNORM:
    return Format_RG_UN16;
  case VK_FORMAT_R16G16_SFLOAT:
    return Format_RG_F16;
  case VK_FORMAT_R32G32_SFLOAT:
    return Format_RG_F32;
  case VK_FORMAT_R16G16_UINT:
    return Format_RG_UI16;
  case VK_FORMAT_R32_SFLOAT:
    return Format_R_F32;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return Format_RGBA_F16;
  case VK_FORMAT_R32G32B32A32_UINT:
    return Format_RGBA_UI32;
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return Format_RGBA_F32;
  case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    return Format_ETC2_RGB8;
  case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    return Format_ETC2_SRGB8;
  case VK_FORMAT_D16_UNORM:
    return Format_Z_UN16;
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return Format_BC7_RGBA;
  case VK_FORMAT_X8_D24_UNORM_PACK32:
    return Format_Z_UN24;
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return Format_Z_UN24_S_UI8;
  case VK_FORMAT_D32_SFLOAT:
    return Format_Z_F32;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return Format_Z_F32_S_UI8;
  default:;
  }
  LVK_ASSERT_MSG(false, "VkFormat value not handled: %d", (int)format);
  return Format_Invalid;
}

VkSemaphore lvk::createSemaphore(VkDevice device, const char* debugName) {
  const VkSemaphoreCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .flags = 0,
  };
  VkSemaphore semaphore = VK_NULL_HANDLE;
  VK_ASSERT(vkCreateSemaphore(device, &ci, nullptr, &semaphore));
  VK_ASSERT(lvk::setDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, debugName));
  return semaphore;
}

VkFence lvk::createFence(VkDevice device, const char* debugName) {
  const VkFenceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = 0,
  };
  VkFence fence = VK_NULL_HANDLE;
  VK_ASSERT(vkCreateFence(device, &ci, nullptr, &fence));
  VK_ASSERT(lvk::setDebugObjectName(device, VK_OBJECT_TYPE_FENCE, (uint64_t)fence, debugName));
  return fence;
}

uint32_t lvk::findQueueFamilyIndex(VkPhysicalDevice physDev, VkQueueFlags flags) {
  using lvk::DeviceQueues;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> props(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueFamilyCount, props.data());

  auto findDedicatedQueueFamilyIndex = [&props](VkQueueFlags require,
                                                VkQueueFlags avoid) -> uint32_t {
    for (uint32_t i = 0; i != props.size(); i++) {
      const bool isSuitable = (props[i].queueFlags & require) == require;
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

namespace {

VkFilter samplerFilterToVkFilter(lvk::SamplerFilter filter) {
  switch (filter) {
  case lvk::SamplerFilter_Nearest:
    return VK_FILTER_NEAREST;
  case lvk::SamplerFilter_Linear:
    return VK_FILTER_LINEAR;
  }
  LVK_ASSERT_MSG(false, "SamplerFilter value not handled: %d", (int)filter);
  return VK_FILTER_LINEAR;
}

VkSamplerMipmapMode samplerMipMapToVkSamplerMipmapMode(lvk::SamplerMip filter) {
  switch (filter) {
  case lvk::SamplerMip_Disabled:
  case lvk::SamplerMip_Nearest:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case lvk::SamplerMip_Linear:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
  LVK_ASSERT_MSG(false, "SamplerMipMap value not handled: %d", (int)filter);
  return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode samplerWrapModeToVkSamplerAddressMode(lvk::SamplerWrap mode) {
  switch (mode) {
  case lvk::SamplerWrap_Repeat:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case lvk::SamplerWrap_Clamp:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case lvk::SamplerWrap_MirrorRepeat:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  LVK_ASSERT_MSG(false, "SamplerWrapMode value not handled: %d", (int)mode);
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

} // namespace 

VkSamplerCreateInfo lvk::samplerStateDescToVkSamplerCreateInfo(const lvk::SamplerStateDesc& desc, const VkPhysicalDeviceLimits& limits) {
  LVK_ASSERT_MSG(desc.mipLodMax >= desc.mipLodMin,
                 "mipLodMax (%d) must be greater than or equal to mipLodMin (%d)",
                 (int)desc.mipLodMax,
                 (int)desc.mipLodMin);

  VkSamplerCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = samplerFilterToVkFilter(desc.magFilter),
      .minFilter = samplerFilterToVkFilter(desc.minFilter),
      .mipmapMode = samplerMipMapToVkSamplerMipmapMode(desc.mipMap),
      .addressModeU = samplerWrapModeToVkSamplerAddressMode(desc.wrapU),
      .addressModeV = samplerWrapModeToVkSamplerAddressMode(desc.wrapV),
      .addressModeW = samplerWrapModeToVkSamplerAddressMode(desc.wrapW),
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0.0f,
      .compareEnable = desc.depthCompareEnabled ? VK_TRUE : VK_FALSE,
      .compareOp = desc.depthCompareEnabled ? lvk::compareOpToVkCompareOp(desc.depthCompareOp) : VK_COMPARE_OP_ALWAYS,
      .minLod = float(desc.mipLodMin),
      .maxLod = desc.mipMap == lvk::SamplerMip_Disabled ? 0.0f : float(desc.mipLodMax),
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };

  if (desc.maxAnisotropic > 1) {
    const bool isAnisotropicFilteringSupported = limits.maxSamplerAnisotropy > 1;
    LVK_ASSERT_MSG(isAnisotropicFilteringSupported, "Anisotropic filtering is not supported by the device.");
    ci.anisotropyEnable = isAnisotropicFilteringSupported ? VK_TRUE : VK_FALSE;

    if (limits.maxSamplerAnisotropy < desc.maxAnisotropic) {
      LLOGL(
          "Supplied sampler anisotropic value greater than max supported by the device, setting to "
          "%.0f",
          static_cast<double>(limits.maxSamplerAnisotropy));
    }
    ci.maxAnisotropy = std::min((float)limits.maxSamplerAnisotropy, (float)desc.maxAnisotropic);
  }

  return ci;
}

static glslang_stage_t getGLSLangShaderStage(VkShaderStageFlagBits stage) {
  switch (stage) {
  case VK_SHADER_STAGE_VERTEX_BIT:
    return GLSLANG_STAGE_VERTEX;
  case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
    return GLSLANG_STAGE_TESSCONTROL;
  case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
    return GLSLANG_STAGE_TESSEVALUATION;
  case VK_SHADER_STAGE_GEOMETRY_BIT:
    return GLSLANG_STAGE_GEOMETRY;
  case VK_SHADER_STAGE_FRAGMENT_BIT:
    return GLSLANG_STAGE_FRAGMENT;
  case VK_SHADER_STAGE_COMPUTE_BIT:
    return GLSLANG_STAGE_COMPUTE;
  default:
    assert(false);
  };
  assert(false);
  return GLSLANG_STAGE_COUNT;
}

lvk::Result lvk::compileShader(VkDevice device,
                               VkShaderStageFlagBits stage,
                               const char* code,
                               VkShaderModule* outShaderModule,
                               const glslang_resource_t* glslLangResource) {
  LVK_PROFILER_FUNCTION();

  if (!outShaderModule) {
    return Result(Result::Code::ArgumentOutOfRange, "outShaderModule is NULL");
  }

  const glslang_input_t input = {
      .language = GLSLANG_SOURCE_GLSL,
      .stage = getGLSLangShaderStage(stage),
      .client = GLSLANG_CLIENT_VULKAN,
      .client_version = GLSLANG_TARGET_VULKAN_1_3,
      .target_language = GLSLANG_TARGET_SPV,
      .target_language_version = GLSLANG_TARGET_SPV_1_6,
      .code = code,
      .default_version = 100,
      .default_profile = GLSLANG_NO_PROFILE,
      .force_default_version_and_profile = false,
      .forward_compatible = false,
      .messages = GLSLANG_MSG_DEFAULT_BIT,
      .resource = glslLangResource,
  };

  glslang_shader_t* shader = glslang_shader_create(&input);
  SCOPE_EXIT {
    glslang_shader_delete(shader);
  };

  if (!glslang_shader_preprocess(shader, &input)) {
    LLOGW("Shader preprocessing failed:\n");
    LLOGW("  %s\n", glslang_shader_get_info_log(shader));
    LLOGW("  %s\n", glslang_shader_get_info_debug_log(shader));
    lvk::logShaderSource(code);
    assert(false);
    return Result(Result::Code::RuntimeError, "glslang_shader_preprocess() failed");
  }

  if (!glslang_shader_parse(shader, &input)) {
    LLOGW("Shader parsing failed:\n");
    LLOGW("  %s\n", glslang_shader_get_info_log(shader));
    LLOGW("  %s\n", glslang_shader_get_info_debug_log(shader));
    lvk::logShaderSource(glslang_shader_get_preprocessed_code(shader));
    assert(false);
    return Result(Result::Code::RuntimeError, "glslang_shader_parse() failed");
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);

  SCOPE_EXIT {
    glslang_program_delete(program);
  };

  if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
    LLOGW("Shader linking failed:\n");
    LLOGW("  %s\n", glslang_program_get_info_log(program));
    LLOGW("  %s\n", glslang_program_get_info_debug_log(program));
    assert(false);
    return Result(Result::Code::RuntimeError, "glslang_program_link() failed");
  }

  glslang_spv_options_t options = {
      .generate_debug_info = true,
      .strip_debug_info = false,
      .disable_optimizer = false,
      .optimize_size = true,
      .disassemble = false,
      .validate = true,
      .emit_nonsemantic_shader_debug_info = false,
      .emit_nonsemantic_shader_debug_source = false,
  };

  glslang_program_SPIRV_generate_with_options(program, input.stage, &options);

  if (glslang_program_SPIRV_get_messages(program)) {
    LLOGW("%s\n", glslang_program_SPIRV_get_messages(program));
  }

  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = glslang_program_SPIRV_get_size(program) * sizeof(uint32_t),
      .pCode = glslang_program_SPIRV_get_ptr(program),
  };
  VK_ASSERT_RETURN(vkCreateShaderModule(device, &ci, nullptr, outShaderModule));

  return Result();
}

VkResult lvk::setDebugObjectName(VkDevice device, VkObjectType type, uint64_t handle, const char* name) {
  if (!name || !*name) {
    return VK_SUCCESS;
  }
  const VkDebugUtilsObjectNameInfoEXT ni = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = type,
      .objectHandle = handle,
      .pObjectName = name,
  };
  return vkSetDebugUtilsObjectNameEXT(device, &ni);
}

VkSpecializationInfo lvk::getPipelineShaderStageSpecializationInfo(lvk::SpecializationConstantDesc desc,
                                                                   VkSpecializationMapEntry* outEntries) {
  const uint32_t numEntries = desc.getNumSpecializationConstants();
  if (outEntries) {
    for (uint32_t i = 0; i != numEntries; i++) {
      outEntries[i] = VkSpecializationMapEntry{
          .constantID = desc.entries[i].constantId,
          .offset = desc.entries[i].offset,
          .size = desc.entries[i].size,
      };
    }
  }
  return VkSpecializationInfo{
      .mapEntryCount = numEntries,
      .pMapEntries = outEntries,
      .dataSize = desc.dataSize,
      .pData = desc.data,
  };
}

VkPipelineShaderStageCreateInfo lvk::getPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                                                      VkShaderModule shaderModule,
                                                                      const char* entryPoint,
                                                                      const VkSpecializationInfo* specializationInfo) {
  return VkPipelineShaderStageCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .flags = 0,
      .stage = stage,
      .module = shaderModule,
      .pName = entryPoint ? entryPoint : "main",
      .pSpecializationInfo = specializationInfo,
  };
}

static uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t memoryTypeBits, VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physDev, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    const bool hasProperties = (memProperties.memoryTypes[i].propertyFlags & flags) == flags;
    if ((memoryTypeBits & (1 << i)) && hasProperties) {
      return i;
    }
  }

  assert(false);

  return 0;
}

VkResult lvk::allocateMemory(VkPhysicalDevice physDev,
                             VkDevice device,
                             const VkMemoryRequirements* memRequirements,
                             VkMemoryPropertyFlags props,
                             VkDeviceMemory* outMemory) {
  assert(memRequirements);

  const VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,
  };
  const VkMemoryAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = memRequirements->size,
      .memoryTypeIndex = findMemoryType(physDev, memRequirements->memoryTypeBits, props),
  };
  return vkAllocateMemory(device, &ai, nullptr, outMemory);
}

VkDescriptorSetLayoutBinding lvk::getDSLBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount) {
  return VkDescriptorSetLayoutBinding{
      .binding = binding,
      .descriptorType = descriptorType,
      .descriptorCount = descriptorCount,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
      .pImmutableSamplers = nullptr,
  };
}

void lvk::imageMemoryBarrier(VkCommandBuffer buffer,
                             VkImage image,
                             VkAccessFlags srcAccessMask,
                             VkAccessFlags dstAccessMask,
                             VkImageLayout oldImageLayout,
                             VkImageLayout newImageLayout,
                             VkPipelineStageFlags srcStageMask,
                             VkPipelineStageFlags dstStageMask,
                             VkImageSubresourceRange subresourceRange) {
  const VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = srcAccessMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldImageLayout,
      .newLayout = newImageLayout,
      .image = image,
      .subresourceRange = subresourceRange,
  };
  vkCmdPipelineBarrier(buffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkSampleCountFlagBits lvk::getVulkanSampleCountFlags(uint32_t numSamples) {
  if (numSamples <= 1) {
    return VK_SAMPLE_COUNT_1_BIT;
  }
  if (numSamples <= 2) {
    return VK_SAMPLE_COUNT_2_BIT;
  }
  if (numSamples <= 4) {
    return VK_SAMPLE_COUNT_4_BIT;
  }
  if (numSamples <= 8) {
    return VK_SAMPLE_COUNT_8_BIT;
  }
  if (numSamples <= 16) {
    return VK_SAMPLE_COUNT_16_BIT;
  }
  if (numSamples <= 32) {
    return VK_SAMPLE_COUNT_32_BIT;
  }
  return VK_SAMPLE_COUNT_64_BIT;
}

uint32_t lvk::getBytesPerPixel(VkFormat format) {
  switch (format) {
  case VK_FORMAT_R8_UNORM:
    return 1;
  case VK_FORMAT_R16_SFLOAT:
    return 2;
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_B8G8R8_UNORM:
    return 3;
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R32_SFLOAT:
    return 4;
  case VK_FORMAT_R16G16B16_SFLOAT:
    return 6;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
    return 8;
  case VK_FORMAT_R32G32B32_SFLOAT:
    return 12;
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return 16;
  default:;
  }
  LVK_ASSERT_MSG(false, "VkFormat value not handled: %d", (int)format);
  return 1;
}

VkCompareOp lvk::compareOpToVkCompareOp(lvk::CompareOp func) {
  switch (func) {
  case lvk::CompareOp_Never:
    return VK_COMPARE_OP_NEVER;
  case lvk::CompareOp_Less:
    return VK_COMPARE_OP_LESS;
  case lvk::CompareOp_Equal:
    return VK_COMPARE_OP_EQUAL;
  case lvk::CompareOp_LessEqual:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case lvk::CompareOp_Greater:
    return VK_COMPARE_OP_GREATER;
  case lvk::CompareOp_NotEqual:
    return VK_COMPARE_OP_NOT_EQUAL;
  case lvk::CompareOp_GreaterEqual:
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case lvk::CompareOp_AlwaysPass:
    return VK_COMPARE_OP_ALWAYS;
  }
  LVK_ASSERT_MSG(false, "CompareFunction value not handled: %d", (int)func);
  return VK_COMPARE_OP_ALWAYS;
}
