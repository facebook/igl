/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "XrLog.h"

extern XrInstance getXrInstance();
namespace igl::shell::openxr {
#if IGL_DEBUG
void checkXRErrors(XrResult result, const char* function) {
  if (XR_FAILED(result)) {
    char errorBuffer[XR_MAX_RESULT_STRING_SIZE + 1] = {};
    xrResultToString(getXrInstance(), result, errorBuffer);
    IGL_LOG_ERROR("OpenXR error: %s %s", function, errorBuffer);
  }
}
#endif
} // namespace igl::shell::openxr
