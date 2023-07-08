/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define IGL_COMMON_SKIP_CHECK
#include <assert.h>
#include <cstdio>
#include <igl/Assert.h>

// ----------------------------------------------------------------------------

namespace igl {

// Toggle debug break on/off at runtime
bool isDebugBreakEnabled() {
#if IGL_DEBUG
  return true;
#else
  return false;
#endif // IGL_DEBUG
}

} // namespace igl

void _IGLDebugBreak() {
  if (igl::isDebugBreakEnabled()) {
    assert(false);
  }
}
