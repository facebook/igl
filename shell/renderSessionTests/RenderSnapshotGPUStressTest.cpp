/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// In its own file: the session's header declares igl::shell::VertexFormat, as others do.

#include "RenderSnapshotTests.h"

#include <shell/renderSessions/GPUStressSession.h>

namespace igl::shell {

TEST_F(RenderSnapshotTests, GPUStressSession) {
  if (backendIs({BackendType::Metal, BackendType::OpenGL})) {
    GTEST_SKIP() << "GPUStressSession renders only on Vulkan and WebGPU";
  }
  GPUStressSession session(platform_);
  render(session, "GPUStressSession");
}

} // namespace igl::shell
