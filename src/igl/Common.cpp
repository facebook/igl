/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <igl/Common.h>
#include <igl/Device.h>

namespace igl {

// Make sure the structure igl::Color is tightly packed so it can be passed into APIs which expect
// float[4] RGBA values
static_assert(sizeof(Color) == 4 * sizeof(float));

std::string BackendTypeToString(BackendType backendType) {
  switch (backendType) {
  case BackendType::Invalid:
    return "Invalid";
  case BackendType::OpenGL:
    return "OpenGL";
  case BackendType::Metal:
    return "Metal";
  case BackendType::Vulkan:
    return "Vulkan";
  // @fb-only
    // @fb-only
  case BackendType::Custom:
    return "Custom";
  }
  IGL_UNREACHABLE_RETURN(std::string())
}

void optimizedMemcpy(void* IGL_NULLABLE dst, const void* IGL_NULLABLE src, size_t size) {
  // Add null check for both dst and src
  IGL_DEBUG_ASSERT(dst != nullptr && src != nullptr, "dst and src must not be null");
  if (!dst || !src) {
    return;
  }

  size_t optimizationCase = size;

  // There are cases where this function is used on an array of bytes, For
  // example, an IGL boolean array. In this case, dst or src only need to be
  // byte aligned. The logic here checks to see if dst and src are
  // multiples of 4. If not, then we will use the memcpy default case below
  if ((ptrdiff_t)dst & 0x3 || (ptrdiff_t)src & 0x3) {
    optimizationCase = 1;
  }

  switch (optimizationCase) {
  case 4:
    *((uint32_t*)dst) = *((const uint32_t*)src);
    break;
  case 8:
    *((uint64_t*)dst) = *((const uint64_t*)src);
    break;
  case 12:
    *((uint64_t*)dst) = *((const uint64_t*)src);
    *((uint32_t*)(dst) + 2) = *((const uint32_t*)(src) + 2);
    break;
  case 16:
    *((uint64_t*)dst) = *((const uint64_t*)src);
    *((uint64_t*)(dst) + 1) = *((const uint64_t*)(src) + 1);
    break;
  default:
    memcpy(dst, src, size);
  }
}

void destroy(IDevice* IGL_NULLABLE device, BindGroupTextureHandle handle) {
  if (device) {
    device->destroy(handle);
  }
}

void destroy(IDevice* IGL_NULLABLE device, BindGroupBufferHandle handle) {
  if (device) {
    device->destroy(handle);
  }
}

void destroy(IDevice* IGL_NULLABLE device, TextureHandle handle) {
  if (device) {
    // do nothing until we transition all textures to handles
    (void)handle;
  }
}

void destroy(IDevice* IGL_NULLABLE device, SamplerHandle handle) {
  if (device) {
    device->destroy(handle);
  }
}

void destroy(IDevice* IGL_NULLABLE device, DepthStencilStateHandle handle) {
  if (device) {
    // do nothing until we transition depth-stencil states to handles
    (void)handle;
  }
}

} // namespace igl
