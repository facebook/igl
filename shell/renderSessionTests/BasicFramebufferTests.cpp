/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/renderSessions/BasicFramebufferSession.h>
#include <shell/shared/testShell/TestShell.h>

class BasicFrameBufferTests : public igl::shell::TestShell {};

TEST_F(BasicFrameBufferTests, BasicFramebufferSession) {
  igl::shell::BasicFramebufferSession test(platform_);
  run(test, 1);
}
