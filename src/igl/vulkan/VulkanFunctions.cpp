/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanFunctions.h"

#include <igl/Common.h>

// clang-format off
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif
// clang-format on

namespace igl::vulkan::functions {

namespace {
PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddr() {
#if defined(_WIN32)
  HMODULE module = LoadLibraryA("vulkan-1.dll");
  if (!module) {
    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)GetProcAddress(module, "vkGetInstanceProcAddr");
#elif defined(__APPLE__)
  void* module = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
  if (!module) {
    module = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
  }
  if (!module) {
    module = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
  }
  if (!module) {
    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)dlsym(module, "vkGetInstanceProcAddr");
#else
  void* module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!module) {
    module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  }
  if (!module) {
    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)dlsym(module, "vkGetInstanceProcAddr");
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
