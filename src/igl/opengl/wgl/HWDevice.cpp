/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "HWDevice.h"

#include <igl/opengl/wgl/Context.h>
#include <igl/opengl/wgl/Device.h>

namespace igl::opengl::wgl {

std::unique_ptr<IContext> HWDevice::createContext(Result* outResult) const {
  Result::setOk(outResult);
  return std::make_unique<Context>();
}

std::unique_ptr<IContext> HWDevice::createContext([[maybe_unused]] BackendVersion backendVersion,
                                                  EGLNativeWindowType /*nativeWindow*/,
                                                  Result* outResult) const {
  IGL_DEBUG_ASSERT(backendVersion.flavor == BackendFlavor::OpenGL);
  return createContext(outResult);
}

std::unique_ptr<IContext> HWDevice::createOffscreenContext(size_t width,
                                                           size_t height,
                                                           Result* outResult) const {
  Result::setOk(outResult);
  return std::make_unique<Context>();
}

std::unique_ptr<opengl::Device> HWDevice::createWithContext(std::unique_ptr<IContext> context,
                                                            Result* outResult) const {
  if (context) {
    Result::setOk(outResult);
    return std::make_unique<opengl::wgl::Device>(std::move(context));
  } else {
    Result::setResult(outResult, Result::Code::ArgumentNull);
    return nullptr;
  }
}

} // namespace igl::opengl::wgl
