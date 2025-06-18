/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifndef IGL_VULKAN_COMMON_H
#define IGL_VULKAN_COMMON_H

#include <cassert>
#include <utility>

// set to 1 to see very verbose debug console logs with Vulkan commands
#define IGL_VULKAN_PRINT_COMMANDS 0

#include <igl/Macros.h>
#include <igl/vulkan/VulkanFunctionTable.h>
#if IGL_PLATFORM_MACOSX
#include <vulkan/vulkan_metal.h>
#endif

#include <igl/ColorSpace.h>
#include <igl/Common.h>
#include <igl/DepthStencilState.h>
#include <igl/Format.h>
#include <igl/Texture.h>
#include <igl/VertexInputState.h>
#include <igl/vulkan/VulkanHelpers.h>

// Enable to use VulkanMemoryAllocator (VMA)
#define IGL_VULKAN_USE_VMA 1

// Macro that encapsulates a function call and check its return value against VK_SUCCESS. Prints
// location of failure when the result is not VK_SUCCESS, along with a stringified version of the
// result value. Asserts at the end of the code block. Expands to void function calls when build
// mode is not DEBUG
#define VK_ASSERT(func)                                            \
  {                                                                \
    const VkResult vk_assert_result = func;                        \
    if (vk_assert_result != VK_SUCCESS) {                          \
      IGL_DEBUG_ABORT("Vulkan API call failed: %s\n  %s\n",        \
                      #func,                                       \
                      ivkGetVulkanResultString(vk_assert_result)); \
    }                                                              \
  }

// Macro that encapsulates a function call and check its return value against VK_SUCCESS. Prints
// location of failure when the result is not VK_SUCCESS, along with a stringified version of the
// result value. Asserts at the end of the code block. The check remains even in build modes other
// than DEBUG
#if IGL_VERIFY_ENABLED
// When IGL_VERIFY_ENABLED is 1, VK_ASSERT may assert but will always log so use the existing
// macro.
#define VK_ASSERT_FORCE_LOG(func) VK_ASSERT(func)
#else
// When IGL_VERIFY_ENABLED is 0, VK_ASSERT will neither log nor assert so need a separate definition
// that will explicitly log the result
#define VK_ASSERT_FORCE_LOG(func)                           \
  {                                                         \
    const VkResult vk_assert_result = func;                 \
    if (vk_assert_result != VK_SUCCESS) {                   \
      IGLLog(IGLLogError,                                   \
             "Vulkan API call failed: %s:%i\n  %s\n  %s\n", \
             __FILE__,                                      \
             __LINE__,                                      \
             #func,                                         \
             ivkGetVulkanResultString(vk_assert_result));   \
    }                                                       \
  }
#endif // IGL_VERIFY_ENABLED

// Macro that encapsulates a function call and check its return value against VK_SUCCESS. Prints
// location of failure when the result is not VK_SUCCESS, along with a stringified version of the
// result value. Asserts at the end of the code block. The check remains even in build modes other
// than DEBUG. Returns the value passed as a parameter
#define VK_ASSERT_RETURN_VALUE(func, value)                        \
  {                                                                \
    const VkResult vk_assert_result = func;                        \
    if (vk_assert_result != VK_SUCCESS) {                          \
      IGL_DEBUG_ABORT("Vulkan API call failed: %s\n  %s\n",        \
                      #func,                                       \
                      ivkGetVulkanResultString(vk_assert_result)); \
      return value;                                                \
    }                                                              \
  }

// Calls the function provided as `func`, checks the return value from the function call against
// VK_SUCCESS and converts the return value from VkResult to an igl::Result and returns it
#define VK_ASSERT_RETURN(func) VK_ASSERT_RETURN_VALUE(func, getResultFromVkResult(vk_assert_result))

// Calls the function provided as `func`, checks the return value from the function call against
// VK_SUCCESS and returns VK_NULL_HANDLE
#define VK_ASSERT_RETURN_NULL_HANDLE(func) VK_ASSERT_RETURN_VALUE(func, VK_NULL_HANDLE)

#define IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx) (ctx)->ensureCurrentContextThread()

namespace igl::vulkan {

// The color definitions below are used by debugging utility functions, such as the ones provided by
// VK_EXT_debug_utils
#define kColorGenerateMipmaps igl::Color(1.f, 0.75f, 0.f)
#define kColorUploadImage igl::Color(1.f, 0.2f, 0.78f)
#define kColorDebugLines igl::Color(0.f, 1.f, 1.f)
#define kColorCommandBufferSubmissionWithFence igl::Color(0.878f, 0.69f, 1.0f) // Mauve

// The VulkanContextConfig provides a way to override some of the the default behaviors of the
// VulkanContext
struct VulkanContextConfig {
  bool terminateOnValidationError = false; // invoke std::terminate() on any validation error

  // enable/disable enhanced shader debugging capabilities (line drawing)
  bool enhancedShaderDebugging = false;

  bool enableConcurrentVkDevicesSupport = false;

  bool enableValidation = true;
  bool enableGPUAssistedValidation = true;
  bool enableExtraLogs = true;
  bool enableDescriptorIndexing = false;
  // @fb-only
  bool enableShaderInt16 = true;
  bool enableShaderDrawParameters = true;
  bool enableStorageBuffer16BitAccess = true;
  bool enableDualSrcBlend = true;
  bool enableGfxReconstruct = false;
  bool enableMultiviewPerViewViewports = false;

  ColorSpace swapChainColorSpace = igl::ColorSpace::SRGB_NONLINEAR;
  TextureFormat requestedSwapChainTextureFormat = igl::TextureFormat::RGBA_UNorm8;

  // the number of resources to support BufferAPIHintBits::Ring
  uint32_t maxResourceCount = 3u;

  // owned by the application - should be alive until initContext() returns
  const void* pipelineCacheData = nullptr;
  size_t pipelineCacheDataSize = 0;

  // This enables fences generated at the end of submission to be exported to the client.
  // The client can then use the SubmitHandle to wait for the completion of the GPU work.
  bool exportableFences = false;

  // Use VK_EXT_headless_surface to create a headless swapchain
  bool headless = false;

  // Size for VulkanMemoryAllocator's default pool block size parameter.
  // Only relevant if VMA is used for memory allocation.
  // Passing 0 will prompt VMA to a large default value (currently 256 MB).
  // Using a smaller heap size would increase the chance of memory deallocation and result in less
  // memory wastage.
  size_t vmaPreferredLargeHeapBlockSize = 0;

  // Specifies a default fence timeout value.
  uint64_t fenceTimeoutNanoseconds = UINT64_MAX;
};

/**
 * @brief Encapsulates a handle to a VkSampler. The struct also stores the sampler id, which is used
 * for bindless rendering (see the ResourcesBinder and VulkanContext classes for more information)
 */
struct VulkanSampler final {
  VkSampler vkSampler = VK_NULL_HANDLE;
  /**
   * @brief The index into VulkanContext::samplers_. This index is intended to be used with bindless
   * rendering. Its value is set by the context when the resource is created and added to the vector
   * of samplers maintained by the VulkanContext.
   */
  uint32_t samplerId = 0;
};

// The functions below are convenience functions used to convert to and from Vulkan values to IGL
// values

Result getResultFromVkResult(VkResult result);
void setResultFrom(Result* outResult, VkResult result);
VkFormat textureFormatToVkFormat(TextureFormat format);
TextureFormat vkFormatToTextureFormat(VkFormat format);
VkFormat invertRedAndBlue(VkFormat format);
bool isTextureFormatRGB(VkFormat format);
bool isTextureFormatBGR(VkFormat format);
uint32_t getNumImagePlanes(VkFormat format);
VkMemoryPropertyFlags resourceStorageToVkMemoryPropertyFlags(ResourceStorage resourceStorage);
VkCompareOp compareFunctionToVkCompareOp(CompareFunction func);
VkStencilOp stencilOperationToVkStencilOp(StencilOperation op);
VkSampleCountFlagBits getVulkanSampleCountFlags(size_t numSamples);
VkSurfaceFormatKHR colorSpaceToVkSurfaceFormat(ColorSpace colorSpace, bool isBGR);
uint32_t getVkLayer(TextureType type, uint32_t face, uint32_t layer);
TextureRangeDesc atVkLayer(TextureType type, const TextureRangeDesc& range, uint32_t vkLayer);
VkColorSpaceKHR colorSpaceToVkColorSpace(ColorSpace colorSpace);
ColorSpace vkColorSpaceToColorSpace(VkColorSpaceKHR colorSpace);

/// @brief Transition from the current layout to VK_IMAGE_LAYOUT_GENERAL
void transitionToGeneral(VkCommandBuffer cmdBuf, ITexture* texture);

/// @brief Transition from the current layout to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
void transitionToColorAttachment(VkCommandBuffer cmdBuf, ITexture* colorTex);

/// @brief Transition from the current layout to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
void transitionToDepthStencilAttachment(VkCommandBuffer cmdBuf, ITexture* depthStencilTex);

/// @brief Transition from the current layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
void transitionToShaderReadOnly(VkCommandBuffer cmdBuf, ITexture* texture);

/// @brief Overrides the layout stored in the `texture` with the one in `layout`. This function does
/// not perform a transition, it only updates the texture's member variable that stores its current
/// layout
void overrideImageLayout(ITexture* texture, VkImageLayout layout);

/// @brief Ensures that all shader bindings are bound by checking the SPIR-V reflection. If the
/// function doesn't assert at some point, the shader bindings are correct. Only for debugging.
void ensureShaderModule(IShaderModule* sm);

/// @brief Implements the igl::IDepthStencilState interface
struct DepthStencilState final : public IDepthStencilState {
  explicit DepthStencilState(DepthStencilStateDesc desc) : desc_(std::move(desc)) {}
  const DepthStencilStateDesc desc_;
};

/// @brief Implements the igl::IVertexInputState interface
struct VertexInputState final : public IVertexInputState {
 public:
  explicit VertexInputState(VertexInputStateDesc desc) : desc_(std::move(desc)) {}
  const VertexInputStateDesc desc_;
};

} // namespace igl::vulkan

namespace igl::vulkan::functions {

void initialize(VulkanFunctionTable& table);
void loadInstanceFunctions(VulkanFunctionTable& table,
                           VkInstance instance,
                           bool enableExtDebugUtils);
void loadDeviceFunctions(VulkanFunctionTable& table, VkDevice device);

} // namespace igl::vulkan::functions

#endif // IGL_VULKAN_COMMON_H
