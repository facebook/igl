/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "HWDevice.h"

#include <igl/opengl/empty/Context.h>
#include <igl/opengl/empty/Device.h>

namespace igl::opengl::empty {

std::unique_ptr<IContext> HWDevice::createContext(Result* outResult) const {
  Result::setOk(outResult);
  return std::make_unique<Context>();
}

std::unique_ptr<IContext> HWDevice::createContext(BackendVersion /*backendVersion*/,
                                                  EGLNativeWindowType /*nativeWindow*/,
                                                  Result* outResult) const {
  Result::setOk(outResult);
  return std::make_unique<Context>();
}

std::unique_ptr<opengl::Device> HWDevice::createWithContext(std::unique_ptr<IContext> context,
                                                            Result* outResult) const {
  Result::setOk(outResult);
  if (context == nullptr) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "context is null");
    return nullptr;
  }
  return std::make_unique<opengl::empty::Device>(std::move(context));
}

} // namespace igl::opengl::empty
