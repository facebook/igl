/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <iglu/device/MetalFactory.h>
#include <iglu/device/OpenGLFactory.h>
#include <memory>
#include <shell/shared/renderSession/RenderSession.h>
#define OFFSCREEN_RT_WIDTH 1
#define OFFSCREEN_RT_HEIGHT 1

namespace igl::shell {
class TestShellBase {
 public:
  TestShellBase() = default;

  ~TestShellBase() = default;

  void SetUp();

  void TearDown(){};

  void run(igl::shell::RenderSession& session, size_t numFrames);

  std::shared_ptr<igl::shell::Platform> platform_;
  std::shared_ptr<igl::ITexture> offscreenTexture_;
  std::shared_ptr<igl::ITexture> offscreenDepthTexture_;
};

class TestShell : public ::testing::Test, public igl::shell::TestShellBase {
  void SetUp() override {
    igl::shell::TestShellBase::SetUp();
  }

  void TearDown() override {
    igl::shell::TestShellBase::TearDown();
  };
};
} // namespace igl::shell
