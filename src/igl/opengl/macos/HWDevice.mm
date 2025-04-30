/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/macos/HWDevice.h>

#include <igl/opengl/macos/Context.h>
#include <igl/opengl/macos/Device.h>

namespace igl::opengl::macos {

///--------------------------------------
/// MARK: - opengl::HWDevice

std::unique_ptr<IContext> HWDevice::createContext(Result* outResult) const {
  return Context::createContext(::igl::opengl::RenderingAPI::GL, outResult);
}

std::unique_ptr<IContext> HWDevice::createContext(::igl::opengl::RenderingAPI api,
                                                  EGLNativeWindowType /*nativeWindow*/,
                                                  Result* outResult) const {
  return Context::createContext(api, outResult);
}

std::unique_ptr<opengl::Device> HWDevice::createWithContext(std::unique_ptr<IContext> context,
                                                            Result* outResult) const {
  Result::setOk(outResult);
  if (!context) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "context is null");
    return nullptr;
  }
  return std::make_unique<Device>(std::move(context));
}

} // namespace igl::opengl::macos
