/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <openxr/openxr.h>

#include <igl/Common.h>

namespace igl::shell::openxr {
#if IGL_DEBUG
void checkXRErrors(XrResult result, const char* function);
#endif
} // namespace igl::shell::openxr

#if IGL_DEBUG
#define XR_CHECK(func) igl::shell::openxr::checkXRErrors(func, #func)
#else
#define XR_CHECK(func) func
#endif
