/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/Common.h"
#include <IGLU/shaderCross/ShaderCross.h>
#include <IGLU/shaderCross/ShaderCrossUniformBuffer.h>

namespace igl::tests {

class ShaderCrossTest : public ::testing::Test {
 public:
  ShaderCrossTest() = default;
  ~ShaderCrossTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(ShaderCrossTest, Construction) {
  const iglu::ShaderCross shaderCross(*iglDev_);
}

TEST_F(ShaderCrossTest, EntryPointName) {
  const iglu::ShaderCross shaderCross(*iglDev_);
  if (iglDev_->getBackendType() == igl::BackendType::Metal) {
    EXPECT_EQ(shaderCross.entryPointName(igl::ShaderStage::Vertex), "main0");
  } else if (iglDev_->getBackendType() == igl::BackendType::OpenGL) {
    EXPECT_EQ(shaderCross.entryPointName(igl::ShaderStage::Vertex), "main");
  } else {
    EXPECT_EQ(shaderCross.entryPointName(igl::ShaderStage::Vertex), "");
  }
}

} // namespace igl::tests
