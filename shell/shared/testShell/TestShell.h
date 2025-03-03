/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <memory>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

struct ScreenSize {
  size_t width;
  size_t height;
};

class TestShellBase {
 public:
  TestShellBase() = default;

  virtual ~TestShellBase() = default;

  void SetUp(ScreenSize screenSize = {1, 1}, bool needsRGBSwapchainSupport = false);

  void TearDown() {};

  std::shared_ptr<Platform> platform_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<ITexture> offscreenDepthTexture_;
};

class TestShell : public ::testing::Test, public TestShellBase {
 public:
  void SetUp() override {
    igl::shell::TestShellBase::SetUp();
  }

  void TearDown() override {
    igl::shell::TestShellBase::TearDown();
  }

  void run(RenderSession& session, size_t numFrames);
};
} // namespace igl::shell
