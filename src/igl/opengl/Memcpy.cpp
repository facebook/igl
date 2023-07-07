/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Memcpy.h>

#include <cstddef>
#include <cstdint>
#include <igl/Common.h>

namespace igl {
namespace opengl {

void optimizedMemcpy(void* dst, const void* src, size_t size) {
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
} // namespace opengl
} // namespace igl
