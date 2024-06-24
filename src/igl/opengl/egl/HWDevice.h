/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/HWDevice.h>

namespace igl::opengl::egl {

class HWDevice final : public ::igl::opengl::HWDevice {
 public:
  ///--------------------------------------
  /// MARK: - opengl::HWDevice

  std::unique_ptr<IContext> createContext(RenderingAPI api,
                                          EGLNativeWindowType nativeWindow,
                                          Result* outResult) const override;

  /**
   * @brief Creates an offscreen context suitable for unit testing.
   */
  std::unique_ptr<IContext> createOffscreenContext(RenderingAPI api,
                                                   size_t width,
                                                   size_t height,
                                                   Result* outResult) const;
  std::unique_ptr<opengl::Device> createWithContext(std::unique_ptr<IContext> context,
                                                    Result* outResult) const override;
};

} // namespace igl::opengl::egl
