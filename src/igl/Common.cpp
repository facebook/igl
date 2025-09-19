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

void optimizedMemcpy(void* dst, const void* src, size_t size) {
    if (size == 0) {
        return;
    }
    
    // Use an assertion for a more robust check in debug builds.
    IGL_DEBUG_ASSERT(dst != nullptr && src != nullptr, "dst and src must not be null");
    
    // Explicitly check for null pointers.
    if (!dst || !src) {
        return;
    }

    // Pointers for byte-by-byte and word-by-word operations.
    char* d_byte = static_cast<char*>(dst);
    const char* s_byte = static_cast<const char*>(src);

    // Copy leading unaligned bytes until destination is 8-byte aligned.
    // The mask & 7 checks for alignment on an 8-byte boundary.
    while (size > 0 && reinterpret_cast<uintptr_t>(d_byte) & 7) {
        *d_byte++ = *s_byte++;
        size--;
    }

    // Now, perform 8-byte copies for the main bulk of the data.
    uint64_t* d_word = reinterpret_cast<uint64_t*>(d_byte);
    const uint64_t* s_word = reinterpret_cast<const uint64_t*>(s_byte);
    
    while (size >= 8) {
        *d_word++ = *s_word++;
        size -= 8;
    }

    // Copy any remaining bytes.
    d_byte = reinterpret_cast<char*>(d_word);
    s_byte = reinterpret_cast<const char*>(s_word);
    
    while (size > 0) {
        *d_byte++ = *s_byte++;
        size--;
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
