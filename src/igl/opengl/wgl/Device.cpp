/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/wgl/Device.h>

#include <igl/Common.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/wgl/Context.h>

namespace igl {
namespace opengl {
namespace wgl {

Device::Device(std::unique_ptr<IContext> context) :
  opengl::Device(std::move(context)), platformDevice_(*this) {}

const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return platformDevice_;
}

Device::~Device() = default;

} // namespace wgl
} // namespace opengl
} // namespace igl
