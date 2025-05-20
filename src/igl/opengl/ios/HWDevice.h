/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/HWDevice.h>
#include <igl/opengl/ios/Device.h>

namespace igl::opengl::ios {

class HWDevice final : public ::igl::opengl::HWDevice {
 public:
  std::unique_ptr<IContext> createContext(Result* outResult) const override;
  std::unique_ptr<IContext> createContext(BackendVersion backendVersion,
                                          EGLNativeWindowType nativeWindow,
                                          Result* outResult) const override;

  std::unique_ptr<opengl::Device> createWithContext(std::unique_ptr<IContext> context,
                                                    Result* outResult) const override;
};

} // namespace igl::opengl::ios
