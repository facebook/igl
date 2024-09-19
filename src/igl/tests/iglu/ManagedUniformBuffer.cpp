/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/Common.h"
#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>

namespace igl::tests {

//
// ManagedUniformBufferTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class ManagedUniformBufferTest : public ::testing::Test {
 public:
  ManagedUniformBufferTest() = default;
  ~ManagedUniformBufferTest() override = default;

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

TEST_F(ManagedUniformBufferTest, Construction) {
  iglu::ManagedUniformBuffer buffer(*iglDev_, {0, 10});
  EXPECT_TRUE(buffer.getData() != nullptr);
}

TEST_F(ManagedUniformBufferTest, UpdateData) {
  // Using LUT to update data
  {
    iglu::ManagedUniformBuffer buffer(*iglDev_,
                                      {0, 10, {{"myUniform", 0, UniformType::Float, 1, 0, 0}}});
    float data = 1000.0f;

    buffer.buildUnifromLUT();
    buffer.updateData("myUniform", &data, sizeof(float));
    EXPECT_TRUE(*static_cast<float*>(buffer.getData()) == data);
  }

  // Not using LUT to update data
  {
    iglu::ManagedUniformBuffer buffer(*iglDev_,
                                      {0, 10, {{"myUniform", 0, UniformType::Float, 1, 0, 0}}});
    float data = 1000.0f;

    buffer.updateData("myUniform", &data, sizeof(float));
    EXPECT_TRUE(*static_cast<float*>(buffer.getData()) == data);
  }

  // Capped data size
  {
    iglu::ManagedUniformBuffer buffer(*iglDev_,
                                      {0, 10, {{"myUniform", 0, UniformType::Float, 1, 0, 0}}});
    float data[2] = {1000.0f, 1.0f};

    buffer.updateData("myUniform", &data, 2 * sizeof(float));
    EXPECT_TRUE(*static_cast<float*>(buffer.getData()) == data[0]);
  }

  // Wrong index
  {
    iglu::ManagedUniformBuffer buffer(
        *iglDev_, {0, 10, {{"nonExistingUniform", -1, UniformType::Float, 1, 0, 0}}});
    float data = 1000.0f;

    EXPECT_FALSE(buffer.updateData("myUniform", &data, sizeof(float)));
  }
}

TEST_F(ManagedUniformBufferTest, GetUniformDataSize) {
  iglu::ManagedUniformBuffer buffer(*iglDev_,
                                    {0, 10, {{"myUniform", 0, UniformType::Float, 1, 0, 0}}});
  EXPECT_EQ(buffer.getUniformDataSize("myUniform"), sizeof(float));
  EXPECT_EQ(buffer.getUniformDataSize("nonExistingUniform"), 0);
}

} // namespace igl::tests
