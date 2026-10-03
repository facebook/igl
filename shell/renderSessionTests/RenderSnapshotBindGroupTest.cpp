/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// In its own file: the session's header declares igl::shell::VertexFormat, as others do.

#include "RenderSnapshotTests.h"

#include <shell/renderSessions/BindGroupSession.h>

namespace igl::shell {

TEST_F(RenderSnapshotTests, BindGroupSession) {
  BindGroupSession session(platform_);
  render(session, "BindGroupSession");
}

} // namespace igl::shell
