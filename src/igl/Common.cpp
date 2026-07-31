/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Common.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <igl/Device.h>

namespace igl {

// Make sure the structure igl::Color is tightly packed so it can be passed into APIs which expect
// float[4] RGBA values
static_assert(sizeof(Color) == 4 * sizeof(float));
static_assert(std::is_trivially_copyable_v<Color>);

static_assert(sizeof(ScissorRect) == 16);
static_assert(std::is_trivially_copyable_v<ScissorRect>);

static_assert(sizeof(Size) == 8);
static_assert(std::is_trivially_copyable_v<Size>);

static_assert(sizeof(Dimensions) == 12);
static_assert(std::is_trivially_copyable_v<Dimensions>);

static_assert(sizeof(Viewport) == 24);
static_assert(std::is_trivially_copyable_v<Viewport>);

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
  case BackendType::D3D12:
    return "D3D12";
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
  constexpr ptrdiff_t kAlign4Mask = 0x3;
  const bool unaligned = (reinterpret_cast<ptrdiff_t>(dst) & kAlign4Mask) != 0 ||
                         (reinterpret_cast<ptrdiff_t>(src) & kAlign4Mask) != 0;
  if (unaligned) {
    optimizationCase = 1;
  }

  auto* dst32 = static_cast<uint32_t*>(dst);
  auto* dst64 = static_cast<uint64_t*>(dst);
  const auto* src32 = static_cast<const uint32_t*>(src);
  const auto* src64 = static_cast<const uint64_t*>(src);

  switch (optimizationCase) {
  case 4:
    dst32[0] = src32[0];
    break;
  case 8:
    dst64[0] = src64[0];
    break;
  case 12:
    dst64[0] = src64[0];
    dst32[2] = src32[2];
    break;
  case 16:
    dst64[0] = src64[0];
    dst64[1] = src64[1];
    break;
  default:
    // NOLINTNEXTLINE(facebook-security-vulnerable-memcpy)
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
