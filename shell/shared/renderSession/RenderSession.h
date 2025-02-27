/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <optional>
#include <shell/shared/platform/Platform.h>

namespace igl::shell {
struct AppParams;
struct ShellParams;

class RenderSession {
 public:
  explicit RenderSession(std::shared_ptr<Platform> platform);
  virtual ~RenderSession() noexcept = default;

  virtual void initialize() noexcept {}
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  virtual void update(IGL_MAYBE_UNUSED SurfaceTextures surfaceTextures) noexcept {}
  virtual void teardown() noexcept {}

  void updateDisplayScale(float scale) noexcept;

  [[nodiscard]] float pixelsPerPoint() const noexcept;
  void setPixelsPerPoint(float scale) noexcept;

  virtual void setShellParams(const ShellParams& shellParams) noexcept {
    shellParams_ = &shellParams;
  }

  /// @brief Params provided to the session by the host
  /// @remark Params may vary each frame.
  [[nodiscard]] const ShellParams& shellParams() const noexcept;

  /// @brief Params provided by the session to the host.
  /// @remark Params may vary each frame.
  [[nodiscard]] const AppParams& appParams() const noexcept;

  void setCurrentQuadLayer(size_t layer) noexcept {
    currentQuadLayer_ = layer;
  }

  /// return the number of seconds since the last call
  float getDeltaSeconds() noexcept;

  static double getSeconds() noexcept;

  virtual ICommandQueue* getCommandQueue() noexcept {
    return commandQueue_.get();
  }

  std::shared_ptr<IFramebuffer> getFramebuffer() noexcept {
    return framebuffer_;
  }

  void releaseFramebuffer() {
    framebuffer_ = nullptr;
  }

  void setPreferredClearColor(const igl::Color& color) noexcept;
  Color getPreferredClearColor() noexcept;

 protected:
  Platform& getPlatform() noexcept;
  [[nodiscard]] const Platform& getPlatform() const noexcept;

  [[nodiscard]] const std::shared_ptr<Platform>& platform() const noexcept;

  AppParams& appParamsRef() noexcept;

  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<ICommandQueue> commandQueue_;
  size_t currentQuadLayer_ = 0;
  double lastTime_ = getSeconds();

 private:
  std::shared_ptr<Platform> platform_;
  std::shared_ptr<AppParams> appParams_;
  std::optional<Color> preferredClearColor_;
  const ShellParams* shellParams_ = nullptr;
};

} // namespace igl::shell
