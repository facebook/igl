/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <memory>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/Texture.h>

namespace igl::shell {

struct ScreenSize {
  size_t width;
  size_t height;
};

class TestShellBase {
 public:
  TestShellBase() = default;

  virtual ~TestShellBase() = default;

 protected:
  void setUpInternal(ScreenSize screenSize = {.width = 1, .height = 1}, bool prefersRGB = true);
  void tearDownInternal() {}

  std::shared_ptr<Platform> platform_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<ITexture> offscreenDepthTexture_;
};

class TestShell : public ::testing::Test, public TestShellBase {
 public:
  void SetUp() override {
    setUpInternal();
  }

  void TearDown() override {
    tearDownInternal();
  }

  void run(RenderSession& session, size_t numFrames);
};
} // namespace igl::shell
