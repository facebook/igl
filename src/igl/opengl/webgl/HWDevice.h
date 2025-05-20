/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#ifndef __EMSCRIPTEN__
#error "Platform not supported"
#endif

#include <igl/opengl/HWDevice.h>

namespace igl::opengl::webgl {

class HWDevice final : public ::igl::opengl::HWDevice {
 public:
  ///--------------------------------------
  /// MARK: - opengl::HWDevice

  std::unique_ptr<IContext> createContext(Result* outResult) const override;

  std::unique_ptr<IContext> createContext(BackendVersion backendVersion,
                                          EGLNativeWindowType nativeWindow,
                                          Result* outResult) const override;

  std::unique_ptr<opengl::Device> createWithContext(std::unique_ptr<IContext> context,
                                                    Result* outResult) const override;
};

} // namespace igl::opengl::webgl
