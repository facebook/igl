/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Common.h>

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
  }
  IGL_UNREACHABLE_RETURN(std::string());
}

} // namespace igl
