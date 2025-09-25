/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Color.h>

// Debug macros that take in an igl CommandBuffer and set some debug labels to group the drawcalls
#if IGL_DEBUG && !defined(IGL_DISABLE_DEBUG_BUFFER_LABEL)
#define IGL_DEBUG_BUFFER_LABEL_START(buffer, x) (buffer)->pushDebugGroupLabel(x);
#define IGL_DEBUG_BUFFER_LABEL_END(buffer) (buffer)->popDebugGroupLabel()
#define IGL_DEBUG_BUFFER_LABEL_START_GUARD(buffer, x) \
  IGL_DEBUG_BUFFER_LABEL_START(buffer, x);            \
  auto popDebugGroupLabelScope =                      \
      folly::makeGuard([cmdBuffer = (buffer)]() { IGL_DEBUG_BUFFER_LABEL_END(cmdBuffer); });
#else
#define IGL_DEBUG_BUFFER_LABEL_START(buffer, x)
#define IGL_DEBUG_BUFFER_LABEL_END(buffer)
#define IGL_DEBUG_BUFFER_LABEL_START_GUARD(buffer, x)
#endif // IGL_DEBUG && !defined(IGL_DISABLE_DEBUG_BUFFER_LABEL)
