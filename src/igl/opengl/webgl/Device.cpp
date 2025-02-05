/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/opengl/webgl/Device.h>

#include <igl/Common.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/webgl/Context.h>

namespace igl::opengl::webgl {

Device::Device(std::unique_ptr<IContext> context) :
  opengl::Device(std::move(context)), platformDevice_(*this) {}

const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return platformDevice_;
}

Device::~Device() = default;

} // namespace igl::opengl::webgl
