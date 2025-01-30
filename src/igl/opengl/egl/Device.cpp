/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/egl/Device.h>

#include <EGL/eglplatform.h>
#include <cstring>
#include <igl/opengl/Errors.h>
#include <igl/opengl/egl/Context.h>

namespace igl::opengl::egl {

Device::Device(std::unique_ptr<IContext> context) :
  opengl::Device(std::move(context)), platformDevice_(*this) {}

const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return platformDevice_;
}

void Device::updateSurface(void* nativeWindowType) {
  std::static_pointer_cast<opengl::egl::Context>(getSharedContext())
      ->updateSurface((NativeWindowType)nativeWindowType);
}

} // namespace igl::opengl::egl
