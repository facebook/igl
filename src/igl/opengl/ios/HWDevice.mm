/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "HWDevice.h"

#include <igl/opengl/ios/Context.h>

namespace igl::opengl::ios {

///--------------------------------------
/// MARK: - opengl::HWDevice

std::unique_ptr<IContext> HWDevice::createContext(RenderingAPI api,
                                                  EGLNativeWindowType /*nativeWindow*/,
                                                  Result* outResult) const {
  Result::setOk(outResult);
  return std::make_unique<Context>(api);
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

} // namespace igl::opengl::ios
