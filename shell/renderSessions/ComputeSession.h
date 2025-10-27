/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

class ComputeSession : public RenderSession {
 public:
  explicit ComputeSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<IComputePipelineState> computePipeline_;
  std::shared_ptr<IBuffer> outputBuffer_;
  bool initialized_ = false;
};

} // namespace igl::shell
