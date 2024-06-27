/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/ios/Device.h>

#include <cstdio>
#include <cstring>
#include <igl/Common.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/Texture.h>
#include <igl/opengl/VertexInputState.h>
#include <igl/opengl/ios/Context.h>

namespace igl::opengl::ios {

Device::Device(std::unique_ptr<IContext> context) :
  opengl::Device(std::move(context)), platformDevice_(*this) {}

const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return platformDevice_;
}

} // namespace igl::opengl::ios
