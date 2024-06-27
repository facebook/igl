/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <string>

namespace igl::tests {

//
// VertexInputStateTest
//
// Cover all paths in igl::opengl::VertexInputState::create()
//
class VertexInputStateOGLTest : public ::testing::Test {
 public:
  VertexInputStateOGLTest() = default;
  ~VertexInputStateOGLTest() override = default;

  // Set up VertexInputStateDesc for the different test cases
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// DefaultCreate
//
// Case 1: create VertexInputState with default values of VertexInputStateDesc
// (desc.numAttributes == 0). Expect this to pass.
//
TEST_F(VertexInputStateOGLTest, DefaultCreate) {
  Result ret;
  std::shared_ptr<IVertexInputState> vertexInputState;

  VertexInputStateDesc inputDesc_;
  inputDesc_.numAttributes = 0;

  vertexInputState = iglDev_->createVertexInputState(inputDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(vertexInputState != nullptr);
}

//
// TwoAttribOneBinding
//
// Case 2: create VertexInputState with two buffer indices and one buffer
// binding (numInputBindings = 1, but inputDesc_.numAttributes = 2). Expect this
// to fail because two buffer indices requires at least two bindings.
//
TEST_F(VertexInputStateOGLTest, TwoAttribOneBinding) {
  Result ret;
  std::shared_ptr<IVertexInputState> vertexInputState;

  const char Unused1[] = "Unused1";
  const size_t Unused1Index = 2;
  const char Unused2[] = "Unused2";
  const size_t Unused2Index = 3;

  VertexInputStateDesc inputDesc_;
  inputDesc_.attributes[0].format = VertexAttributeFormat::Float4;
  inputDesc_.attributes[0].offset = 0;
  inputDesc_.attributes[0].location = 0;
  inputDesc_.attributes[0].bufferIndex = Unused1Index;
  inputDesc_.attributes[0].name = Unused1;
  inputDesc_.inputBindings[0].stride = sizeof(float) * 4;

  inputDesc_.attributes[1].format = VertexAttributeFormat::Float2;
  inputDesc_.attributes[1].offset = 0;
  inputDesc_.attributes[1].location = 1;
  inputDesc_.attributes[1].bufferIndex = Unused2Index;
  inputDesc_.attributes[1].name = Unused2;
  inputDesc_.inputBindings[1].stride = sizeof(float) * 2;

  // numAttributes has to equal to bindings when using more than 1 buffer
  // the following are just for covering codes
  inputDesc_.numAttributes = 2;
  inputDesc_.numInputBindings = 1;

  vertexInputState = iglDev_->createVertexInputState(inputDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::ArgumentInvalid);
  ASSERT_TRUE(vertexInputState == nullptr);
}

//
// TwoAttribTwoBinding
//
// Case 3: create VertexInputState with two buffer indices and two buffer
// binding (numInputBindings = 2, and inputDesc_.numAttributes = 2).
// Expect this to pass.
//
TEST_F(VertexInputStateOGLTest, TwoAttribTwoBinding) {
  Result ret;
  std::shared_ptr<IVertexInputState> vertexInputState;

  const char Unused1[] = "Unused1";
  const size_t Unused1Index = 2;
  const char Unused2[] = "Unused2";
  const size_t Unused2Index = 3;

  VertexInputStateDesc inputDesc_;
  inputDesc_.attributes[0].format = VertexAttributeFormat::Float4;
  inputDesc_.attributes[0].offset = 0;
  inputDesc_.attributes[0].location = 0;
  inputDesc_.attributes[0].bufferIndex = Unused1Index;
  inputDesc_.attributes[0].name = Unused1;
  inputDesc_.inputBindings[0].stride = sizeof(float) * 4;

  inputDesc_.attributes[1].format = VertexAttributeFormat::Float2;
  inputDesc_.attributes[1].offset = 0;
  inputDesc_.attributes[1].location = 1;
  inputDesc_.attributes[1].bufferIndex = Unused2Index;
  inputDesc_.attributes[1].name = Unused2;
  inputDesc_.inputBindings[1].stride = sizeof(float) * 2;

  // numAttributes has to equal to bindings when using more than 1 buffer
  inputDesc_.numAttributes = inputDesc_.numInputBindings = 2;

  vertexInputState = iglDev_->createVertexInputState(inputDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(vertexInputState != nullptr);
}

} // namespace igl::tests
