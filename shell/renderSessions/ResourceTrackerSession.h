/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl {
namespace shell {

#if !IGL_DEBUG
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif

#if IGL_PLATFORM_ANDROID
// For Android instrumentation tests we load a 256 byte checkerboard image instead
constexpr size_t kiglDimensionSize = 8;
#else
constexpr size_t kiglDimensionSize = 1024;
#endif
constexpr size_t kiglExpectedByteCount = kiglDimensionSize * kiglDimensionSize * 4; // 4194304;

class ResourceTrackerSession : public RenderSession {
 public:
  ResourceTrackerSession(std::shared_ptr<Platform> platform,
                         size_t iglExpectedByteCount = kiglExpectedByteCount) :
    RenderSession(std::move(platform)), iglExpectedByteCount_(iglExpectedByteCount) {
    size_t dimensionSize = kiglDimensionSize;
    iglExpectedByteCountWithMipmaps_ = dimensionSize * dimensionSize;
    while (dimensionSize >= 1) {
      dimensionSize /= 2;
      iglExpectedByteCountWithMipmaps_ += dimensionSize * dimensionSize;
    }
    iglExpectedByteCountWithMipmaps_ *= 4;
  }
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  size_t iglExpectedByteCount_;
  size_t iglExpectedByteCountWithMipmaps_;
};

} // namespace shell
} // namespace igl
