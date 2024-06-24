/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/empty/Device.h>

#include <cstdio>
#include <cstring>
#include <igl/Common.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/empty/Context.h>

namespace igl::opengl::empty {

Device::Device(std::unique_ptr<IContext> context) :
  opengl::Device(std::move(context)), platformDevice_(*this) {}

const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return platformDevice_;
}

} // namespace igl::opengl::empty
