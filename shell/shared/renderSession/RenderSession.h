/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/AppParams.h>

namespace igl {
class ITexture;
} // namespace igl

namespace igl::shell {
struct ShellParams;

class RenderSession {
 public:
  RenderSession(std::shared_ptr<Platform> platform);
  virtual ~RenderSession() noexcept = default;

  virtual void initialize() noexcept {}
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  virtual void update(IGL_MAYBE_UNUSED igl::SurfaceTextures surfaceTextures) noexcept {}
  virtual void dispose() noexcept {}

  void updateDisplayScale(float scale) noexcept;

  float pixelsPerPoint() const noexcept;
  void setPixelsPerPoint(float scale) noexcept;

  virtual void setShellParams(const ShellParams& shellParams) noexcept;

  /// @brief Params provided to the session by the host
  /// @remark Params may vary each frame.
  const ShellParams& shellParams() const noexcept;

  /// @brief Params provided by the session to the host.
  /// @remark Params may vary each frame.
  const AppParams& appParams() const noexcept;

  void setCurrentQuadLayer(size_t layer) noexcept {
    currentQuadLayer_ = layer;
  }

 protected:
  Platform& getPlatform() noexcept;
  const Platform& getPlatform() const noexcept;

  const std::shared_ptr<Platform>& platform() const noexcept;

  AppParams& appParamsRef() noexcept;

  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<ICommandQueue> commandQueue_;
  size_t currentQuadLayer_ = 0;

 private:
  std::shared_ptr<Platform> platform_;
  std::unique_ptr<AppParams> appParams_;
  const ShellParams* shellParams_ = nullptr;
};

} // namespace igl::shell
