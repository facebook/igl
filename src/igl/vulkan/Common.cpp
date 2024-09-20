/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Common.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>

// clang-format off
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif
// clang-format on

#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanShaderModule.h>
#include <igl/vulkan/VulkanTexture.h>
#include <igl/vulkan/util/SpvReflection.h>
#include <igl/vulkan/util/TextureFormat.h>

namespace igl::vulkan {

Result getResultFromVkResult(VkResult result) {
  if (result == VK_SUCCESS) {
    return Result();
  }

  Result res(Result::Code::RuntimeError, ivkGetVulkanResultString(result));

  switch (result) {
  case VK_ERROR_LAYER_NOT_PRESENT:
  case VK_ERROR_EXTENSION_NOT_PRESENT:
  case VK_ERROR_FEATURE_NOT_PRESENT:
    res.code = Result::Code::Unimplemented;
    return res;
  case VK_ERROR_INCOMPATIBLE_DRIVER:
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    res.code = Result::Code::Unsupported;
    return res;
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

void setResultFrom(Result* outResult, VkResult result) {
  if (!outResult) {
    return;
  }

  *outResult = getResultFromVkResult(result);
}

VkStencilOp stencilOperationToVkStencilOp(igl::StencilOperation op) {
  switch (op) {
  case igl::StencilOperation::Keep:
    return VK_STENCIL_OP_KEEP;
  case igl::StencilOperation::Zero:
    return VK_STENCIL_OP_ZERO;
  case igl::StencilOperation::Replace:
    return VK_STENCIL_OP_REPLACE;
  case igl::StencilOperation::IncrementClamp:
    return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
  case igl::StencilOperation::DecrementClamp:
    return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
  case igl::StencilOperation::Invert:
    return VK_STENCIL_OP_INVERT;
  case igl::StencilOperation::IncrementWrap:
    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
  case igl::StencilOperation::DecrementWrap:
    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }
  IGL_ASSERT_NOT_REACHED();
  return VK_STENCIL_OP_KEEP;
}

VkMemoryPropertyFlags resourceStorageToVkMemoryPropertyFlags(igl::ResourceStorage resourceStorage) {
  VkMemoryPropertyFlags memFlags{0};

  switch (resourceStorage) {
  case ResourceStorage::Invalid:
    IGL_ASSERT_MSG(false, "Invalid storage type");
    break;
  case ResourceStorage::Private:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  case ResourceStorage::Shared:
    memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case ResourceStorage::Managed:
    memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case ResourceStorage::Memoryless:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    break;
  }

  return memFlags;
}

VkCompareOp compareFunctionToVkCompareOp(igl::CompareFunction func) {
  switch (func) {
  case igl::CompareFunction::Never:
    return VK_COMPARE_OP_NEVER;
  case igl::CompareFunction::Less:
    return VK_COMPARE_OP_LESS;
  case igl::CompareFunction::Equal:
    return VK_COMPARE_OP_EQUAL;
  case igl::CompareFunction::LessEqual:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case igl::CompareFunction::Greater:
    return VK_COMPARE_OP_GREATER;
  case igl::CompareFunction::NotEqual:
    return VK_COMPARE_OP_NOT_EQUAL;
  case igl::CompareFunction::GreaterEqual:
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case igl::CompareFunction::AlwaysPass:
    return VK_COMPARE_OP_ALWAYS;
  }
  IGL_ASSERT_MSG(false, "CompareFunction value not handled: %d", (int)func);
  return VK_COMPARE_OP_ALWAYS;
}

VkSampleCountFlagBits getVulkanSampleCountFlags(size_t numSamples) {
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

uint32_t getVkLayer(TextureType type, uint32_t face, uint32_t layer) {
  return type == TextureType::Cube ? face : layer;
}

TextureRangeDesc atVkLayer(TextureType type, const TextureRangeDesc& range, uint32_t vkLayer) {
  return type == TextureType::Cube ? range.atFace(vkLayer) : range.atLayer(vkLayer);
}

void transitionToGeneral(VkCommandBuffer cmdBuf, ITexture* texture) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  if (!texture) {
    return;
  }

  const vulkan::Texture& tex = static_cast<vulkan::Texture&>(*texture);
  const vulkan::VulkanImage& img = tex.getVulkanTexture().getVulkanImage();

  if (!img.isStorageImage()) {
    IGL_ASSERT_MSG(false, "Did you forget to specify TextureUsageBits::Storage on your texture?");
    return;
  }

  // "frame graph" heuristics: if we are already in VK_IMAGE_LAYOUT_GENERAL, wait for the previous
  // compute shader, otherwise wait for previous attachment writes
  const VkPipelineStageFlags srcStage =
      (img.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
      : img.isDepthOrStencilFormat_                 ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                                    : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  img.transitionLayout(
      cmdBuf,
      VK_IMAGE_LAYOUT_GENERAL,
      srcStage,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VkImageSubresourceRange{
          img.getImageAspectFlags(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

void transitionToColorAttachment(VkCommandBuffer cmdBuf, ITexture* colorTex) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  if (!colorTex) {
    return;
  }

  const auto& vkTex = static_cast<Texture&>(*colorTex);
  const auto& img = vkTex.getVulkanTexture().getVulkanImage();
  if (IGL_UNEXPECTED(img.isDepthFormat_ || img.isStencilFormat_)) {
    IGL_ASSERT_MSG(false, "Color attachments cannot have depth/stencil formats");
    IGL_LOG_ERROR("Color attachments cannot have depth/stencil formats");
    return;
  }
  IGL_ASSERT_MSG(img.imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  if (!IGL_VERIFY((img.usageFlags_ & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)) {
    IGL_ASSERT_MSG(false, "Did you forget to specify TextureUsageBit::Attachment usage bit?");
    IGL_LOG_ERROR("Did you forget to specify TextureUsageBit::Attachment usage bit?");
  }
  if (img.usageFlags_ & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    // transition to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    img.transitionLayout(
        cmdBuf,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for all subsequent fragment/compute
                                                  // shaders
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void transitionToDepthStencilAttachment(VkCommandBuffer cmdBuf, ITexture* depthStencilTex) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  if (!depthStencilTex) {
    return;
  }

  const auto& vkTex = static_cast<Texture&>(*depthStencilTex);
  const auto& img = vkTex.getVulkanTexture().getVulkanImage();
  if (IGL_UNEXPECTED(!img.isDepthFormat_ && !img.isStencilFormat_)) {
    IGL_ASSERT_MSG(false, "Only depth/stencil formats are accepted");
    IGL_LOG_ERROR("Only depth/stencil formats are accepted");
    return;
  }
  IGL_ASSERT_MSG(img.imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  if (!IGL_VERIFY((img.usageFlags_ & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)) {
    IGL_ASSERT_MSG(false, "Did you forget to specify TextureUsageBit::Attachment usage bit?");
    IGL_LOG_ERROR("Did you forget to specify TextureUsageBit::Attachment usage bit?");
  }
  if (img.usageFlags_ & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    // transition to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    VkImageAspectFlags aspectFlags = 0;
    if (img.isDepthFormat_) {
      aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (img.isStencilFormat_) {
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    img.transitionLayout(
        cmdBuf,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for all subsequent fragment/compute
                                                  // shaders
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VkImageSubresourceRange{
            aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void transitionToShaderReadOnly(VkCommandBuffer cmdBuf, ITexture* texture) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  if (!texture) {
    return;
  }

  const vulkan::Texture& tex = static_cast<vulkan::Texture&>(*texture);
  const vulkan::VulkanImage& img = tex.getVulkanTexture().getVulkanImage();

  const bool isColor = (img.getImageAspectFlags() & VK_IMAGE_ASPECT_COLOR_BIT) > 0;

  if (img.usageFlags_ & VK_IMAGE_USAGE_SAMPLED_BIT) {
    // transition sampled images to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    img.transitionLayout(
        cmdBuf,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        isColor ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for subsequent fragment/compute shaders
        VkImageSubresourceRange{
            img.getImageAspectFlags(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void overrideImageLayout(ITexture* texture, VkImageLayout layout) {
  if (!texture) {
    return;
  }
  const vulkan::Texture* tex = static_cast<vulkan::Texture*>(texture);
  tex->getVulkanTexture().getVulkanImage().imageLayout_ = layout;
}

void ensureShaderModule(IShaderModule* sm) {
  IGL_ASSERT(sm);

  const igl::vulkan::util::SpvModuleInfo& info =
      static_cast<igl::vulkan::ShaderModule*>(sm)->getVulkanShaderModule().getSpvModuleInfo();

  for (const auto& t : info.textures) {
    if (!IGL_VERIFY(t.descriptorSet == kBindPoint_CombinedImageSamplers)) {
      IGL_LOG_ERROR(
          "Missing descriptor set id for textures: the shader should contain \"layout(set = "
          "%u, ...)\"",
          kBindPoint_CombinedImageSamplers);
      continue;
    }
  }
  for (const auto& b : info.buffers) {
    if (!IGL_VERIFY(b.descriptorSet == kBindPoint_Buffers)) {
      IGL_LOG_ERROR(
          "Missing descriptor set id for buffers: the shader should contain \"layout(set = "
          "%u, ...)\"",
          kBindPoint_Buffers);
      continue;
    }
  }
}

uint32_t getNumImagePlanes(VkFormat format) {
  switch (format) {
  case VK_FORMAT_UNDEFINED:
    return 0;
  case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    return 2;
  case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    return 3;
  default:
    return 1;
  }
}
} // namespace igl::vulkan

namespace igl::vulkan::functions {

namespace {
PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddr() {
#if defined(_WIN32)
  HMODULE lib = LoadLibraryA("vulkan-1.dll");
  if (!lib) {
    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)GetProcAddress(lib, "vkGetInstanceProcAddr");
#elif defined(__APPLE__)
  void* lib = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    lib = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
  }
  if (!lib) {
    lib = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
  }
  if (!lib) {
    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
#else
  void* lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  }
  if (!lib) {
    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
#endif
  return nullptr;
}
} // namespace

void initialize(VulkanFunctionTable& table) {
  table.vkGetInstanceProcAddr = getVkGetInstanceProcAddr();
  IGL_ASSERT(table.vkGetInstanceProcAddr != nullptr);

  loadVulkanLoaderFunctions(&table, table.vkGetInstanceProcAddr);
}

void loadInstanceFunctions(VulkanFunctionTable& table, VkInstance instance) {
  IGL_ASSERT(table.vkGetInstanceProcAddr != nullptr);
  loadVulkanInstanceFunctions(&table, instance, table.vkGetInstanceProcAddr);
}

void loadDeviceFunctions(VulkanFunctionTable& table, VkDevice device) {
  IGL_ASSERT(table.vkGetDeviceProcAddr != nullptr);
  loadVulkanDeviceFunctions(&table, device, table.vkGetDeviceProcAddr);
}

} // namespace igl::vulkan::functions
