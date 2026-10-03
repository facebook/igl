/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// In its own file: the session's header declares igl::shell::VertexFormat, as others do.

#include "RenderSnapshotTests.h"

#include <shell/renderSessions/ShaderLibraryWithDataSession.h>

namespace igl::shell {

TEST_F(RenderSnapshotTests, ShaderLibraryWithDataSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP() << "ShaderLibraryWithDataSession loads shader libraries only on Metal and WebGPU";
  }
  ShaderLibraryWithDataSession session(platform_);
  render(session, "ShaderLibraryWithDataSession");
}

} // namespace igl::shell
