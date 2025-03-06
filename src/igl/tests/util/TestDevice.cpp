/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "TestDevice.h"
#include "Common.h"

#include <igl/tests/util/device/TestDevice.h>

namespace igl::tests::util {

//
// createTestDevice
//
// Used by clients to get an IGL device. The backend is determined by the IGL_BACKEND_TYPE compiler
// flag in the BUCK file. For OpenGL ES, the GLES version is determined by the
// IGL_UNIT_TESTS_GLES_VERSION compiler flag.
//
std::shared_ptr<IDevice> createTestDevice() {
  const std::string backend(IGL_BACKEND_TYPE);

  if (backend == "ogl") {
#ifdef IGL_UNIT_TESTS_GLES_VERSION
    std::string backendApi(IGL_UNIT_TESTS_GLES_VERSION);
#else
    const std::string backendApi("2.0");
#endif
    return device::createTestDevice(::igl::BackendType::OpenGL, backendApi);
  } else if (backend == "metal") {
    return device::createTestDevice(::igl::BackendType::Metal);
  } else if (backend == "vulkan") {
    return device::createTestDevice(::igl::BackendType::Vulkan);
  // @fb-only
    // @fb-only
  } else {
    return nullptr;
  }
}

} // namespace igl::tests::util
