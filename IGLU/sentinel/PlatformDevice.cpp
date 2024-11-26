/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include <IGLU/sentinel/PlatformDevice.h>

#include <IGLU/sentinel/Assert.h>

namespace iglu::sentinel {

PlatformDevice::PlatformDevice(bool shouldAssert) : shouldAssert_(shouldAssert) {}

bool PlatformDevice::isType(igl::PlatformDeviceType /*t*/) const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return false;
}

} // namespace iglu::sentinel
