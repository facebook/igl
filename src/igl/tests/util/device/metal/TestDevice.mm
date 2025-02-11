/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/metal/TestDevice.h>

#include <igl/Common.h>
#include <igl/Macros.h>
#include <igl/metal/HWDevice.h>

namespace igl::tests::util::device::metal {

//
// createTestDevice
//
// Used by clients to get an IGL device. The backend is determined by
// the IGL_BACKEND_TYPE compiler flag in the BUCK file
//
std::shared_ptr<IDevice> createTestDevice() {
  auto mtlDevice = MTLCreateSystemDefaultDevice();
  return ::igl::metal::HWDevice().createWithMTLDevice(mtlDevice, nullptr);
}

} // namespace igl::tests::util::device::metal
