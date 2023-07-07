/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#if !defined(IGL_CMAKE_BUILD)
#include <secure_lib/secure_string.h>
#else

#include <stdlib.h>
#include <string.h>

#include <igl/Common.h>

#define ERR_POTENTIAL_BUFFER_OVERFLOW 34 // matches with ERANGE in errno.h

inline int try_checked_memcpy(void* destination,
                              size_t destination_size,
                              const void* source,
                              size_t count) {
  if (destination_size < count) {
    return ERR_POTENTIAL_BUFFER_OVERFLOW;
  }
  memcpy(destination, source, count);
  return 0;
}

inline void* checked_memcpy(void* destination,
                            size_t destination_size,
                            const void* source,
                            size_t count) {
  if (destination_size < count) {
    IGL_REPORT_ERROR_MSG(false, "Aborting due to potential buffer overflow");
    exit(EXIT_FAILURE);
  }
  return memcpy(destination, source, count);
}

inline void* checked_memcpy_robust(void* destination,
                                   size_t destination_size,
                                   const void* source,
                                   size_t source_size,
                                   size_t count) {
  if (destination_size < count || source_size < count) {
    IGL_REPORT_ERROR_MSG(false, "Aborting due to potential buffer overflow");
    exit(EXIT_FAILURE);
  }
  return memcpy(destination, source, count);
}

inline void* checked_memcpy_offset(void* destination,
                                   size_t destination_size,
                                   size_t offset,
                                   const void* source,
                                   size_t count) {
  if (destination_size < count || offset >= destination_size) {
    IGL_REPORT_ERROR_MSG(false, "Aborting due to potential buffer overflow");
    exit(EXIT_FAILURE);
  }

  memcpy((char*)destination + offset, source, count);
  return destination;
}

inline int checked_strncmp(const char* str1,
                           size_t str1_size,
                           const char* str2,
                           size_t str2_size,
                           size_t count) {
  if (str1_size < count || str2_size < count) {
    IGL_REPORT_ERROR_MSG(false, "Aborting due to potential buffer overflow");
    exit(EXIT_FAILURE);
  }

  return strncmp(str1, str2, count);
}

#endif // IGL_CMAKE_BUILD
