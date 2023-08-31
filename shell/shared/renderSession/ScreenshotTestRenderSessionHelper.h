/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/platform/Platform.h>

#include <memory>

namespace igl {
class ITexture;
class IFramebuffer;
} // namespace igl

namespace igl::shell {
struct AppParams;
struct ShellParams;

class ScreenshotTestRenderSessionHelper {
 public:
  ScreenshotTestRenderSessionHelper() = default;

  void initialize(AppParams& appParams) noexcept;
  bool update(const AppParams& appParams,
              const ShellParams& shellParams,
              const igl::SurfaceTextures& surfaceTextures,
              Platform& platform);
  void dispose() noexcept {}

 private:
  int frameTicked_ = 0;
};

void SaveFrameBufferToPng(const char* absoluteFilename,
                          const std::shared_ptr<IFramebuffer>& framebuffer,
                          Platform& platform);

} // namespace igl::shell
