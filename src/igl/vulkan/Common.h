/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cassert>

// set to 1 to see very verbose debug console logs with Vulkan commands
#define IGL_VULKAN_PRINT_COMMANDS 0

#include <igl/Macros.h>
#include <igl/vulkan/VulkanFunctionTable.h>
#if IGL_PLATFORM_MACOS
#include <vulkan/vulkan_metal.h>
#endif

#include <igl/Common.h>
#include <igl/DepthStencilState.h>
#include <igl/Texture.h>
#include <igl/vulkan/VulkanHelpers.h>

// libc++'s implementation of std::format has a large binary size impact
// (https://github.com/llvm/llvm-project/issues/64180), so avoid it on Android.
#if defined(__cpp_lib_format) && !defined(__ANDROID__)
#include <format>
#define IGL_FORMAT std::format
#else
#include <fmt/core.h>
#define IGL_FORMAT fmt::format
#endif // __cpp_lib_format

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
      IGL_LOG_ERROR("Vulkan API call failed: %s:%i\n  %s\n  %s\n", \
                    __FILE__,                                      \
                    __LINE__,                                      \
                    #func,                                         \
                    ivkGetVulkanResultString(vk_assert_result));   \
      IGL_ASSERT(false);                                           \
    }                                                              \
  }

// Macro that encapsulates a function call and check its return value against VK_SUCCESS. Prints
// location of failure when the result is not VK_SUCCESS, along with a stringified version of the
// result value. Asserts at the end of the code block. The check remains even in build modes other
// than DEBUG
#define VK_ASSERT_FORCE_LOG(func)                           \
  {                                                         \
    const VkResult vk_assert_result = func;                 \
    if (vk_assert_result != VK_SUCCESS) {                   \
      IGLLog(IGLLogLevel::LOG_ERROR,                        \
             "Vulkan API call failed: %s:%i\n  %s\n  %s\n", \
             __FILE__,                                      \
             __LINE__,                                      \
             #func,                                         \
             ivkGetVulkanResultString(vk_assert_result));   \
      IGL_ASSERT(false);                                    \
    }                                                       \
  }

// Macro that encapsulates a function call and check its return value against VK_SUCCESS. Prints
// location of failure when the result is not VK_SUCCESS, along with a stringified version of the
// result value. Asserts at the end of the code block. The check remains even in build modes other
// than DEBUG. Returns the value passed as a parameter
#define VK_ASSERT_RETURN_VALUE(func, value)                        \
  {                                                                \
    const VkResult vk_assert_result = func;                        \
    if (vk_assert_result != VK_SUCCESS) {                          \
      IGL_LOG_ERROR("Vulkan API call failed: %s:%i\n  %s\n  %s\n", \
                    __FILE__,                                      \
                    __LINE__,                                      \
                    #func,                                         \
                    ivkGetVulkanResultString(vk_assert_result));   \
      IGL_ASSERT(false);                                           \
      return value;                                                \
    }                                                              \
  }

// Calls the function provided as `func`, checks the return value from the function call against
// VK_SUCCESS and converts the return value from VkResult to an igl::Result and returns it
#define VK_ASSERT_RETURN(func) VK_ASSERT_RETURN_VALUE(func, getResultFromVkResult(vk_assert_result))

// Calls the function provided as `func`, checks the return value from the function call against
// VK_SUCCESS and returns VK_NULL_HANDLE
#define VK_ASSERT_RETURN_NULL_HANDLE(func) VK_ASSERT_RETURN_VALUE(func, VK_NULL_HANDLE)

namespace igl::vulkan {

// The color definitions below are used by debugging utility functions, such as the ones provided by
// VK_EXT_debug_utils
#define kColorGenerateMipmaps igl::Color(1.f, 0.75f, 0.f)
#define kColorUploadImage igl::Color(1.f, 0.2f, 0.78f)
#define kColorDebugLines igl::Color(0.f, 1.f, 1.f)

// The functions below are convenience functions used to convert to and from Vulkan values to IGL
// values

Result getResultFromVkResult(VkResult result);
void setResultFrom(Result* outResult, VkResult result);
VkFormat textureFormatToVkFormat(igl::TextureFormat format);
igl::TextureFormat vkFormatToTextureFormat(VkFormat format);
igl::ColorSpace vkColorSpaceToColorSpace(VkColorSpaceKHR colorSpace);
VkFormat invertRedAndBlue(VkFormat format);
bool isTextureFormatRGB(VkFormat format);
bool isTextureFormatBGR(VkFormat format);
uint32_t getNumImagePlanes(VkFormat format);
VkColorSpaceKHR colorSpaceToVkColorSpace(igl::ColorSpace colorSpace);
VkMemoryPropertyFlags resourceStorageToVkMemoryPropertyFlags(igl::ResourceStorage resourceStorage);
VkCompareOp compareFunctionToVkCompareOp(igl::CompareFunction func);
VkSampleCountFlagBits getVulkanSampleCountFlags(size_t numSamples);
VkSurfaceFormatKHR colorSpaceToVkSurfaceFormat(igl::ColorSpace colorSpace, bool isBGR);
uint32_t getVkLayer(igl::TextureType type, uint32_t face, uint32_t layer);
TextureRangeDesc atVkLayer(TextureType type, const TextureRangeDesc& range, uint32_t vkLayer);

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

} // namespace igl::vulkan
