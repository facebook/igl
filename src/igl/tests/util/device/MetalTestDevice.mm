/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/MetalTestDevice.h>

#include <igl/tests/util/device/metal/TestDevice.h>

namespace igl::tests::util::device {

std::shared_ptr<IDevice> createMetalTestDevice() {
  return metal::createTestDevice();
}

} // namespace igl::tests::util::device
