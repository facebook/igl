/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/Common.h"

#include <igl/VertexInputState.h>

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

// Test creating attributes with every data format
TEST_F(VertexInputStateOGLTest, AllFormats0) {
  const char SomeName[] = "Name";

  constexpr size_t sizes[] = {
      // float1 float2 etc
      sizeof(float) * 1,
      sizeof(float) * 2,
      sizeof(float) * 3,
      sizeof(float) * 4,
      // byte1 byte2 etc
      sizeof(char) * 1,
      sizeof(char) * 2,
      sizeof(char) * 3,
      sizeof(char) * 4,
      // ubyte1 ubyte2 etc
      sizeof(char) * 1,
      sizeof(char) * 2,
      sizeof(char) * 3,
      sizeof(char) * 4,
      // short1 short2 etc
      sizeof(short) * 1,
      sizeof(short) * 2,
      sizeof(short) * 3,
      sizeof(short) * 4,
      // ushort1 ushort2 etc
      sizeof(short) * 1,
      sizeof(short) * 2,
      sizeof(short) * 3,
      sizeof(short) * 4,
      // byte1norm byte2norm etc
      sizeof(char) * 1,
      sizeof(char) * 2,
      sizeof(char) * 3,
      sizeof(char) * 4,
      // uByte1Norm uByte1Norm etc
      sizeof(char) * 1,
      sizeof(char) * 2,
      sizeof(char) * 3,
      sizeof(char) * 4,
      // short1Norm short2Norm etc
      sizeof(short) * 1,
      sizeof(short) * 2,
      sizeof(short) * 3,
      sizeof(short) * 4,
      // ushort1norm ushort2norm etc
      sizeof(short) * 1,
      sizeof(short) * 2,
      sizeof(short) * 3,
      sizeof(short) * 4,
      // int1 int2 etc
      sizeof(int) * 1,
      sizeof(int) * 2,
      sizeof(int) * 3,
      sizeof(int) * 4,
      // uint1 uint2 etc
      sizeof(int) * 1,
      sizeof(int) * 2,
      sizeof(int) * 3,
      sizeof(int) * 4,
      // half1 half2 etc
      sizeof(uint16_t) * 1,
      sizeof(uint16_t) * 2,
      sizeof(uint16_t) * 3,
      sizeof(uint16_t) * 4,
      // Int_2_10_10_10_REV
      sizeof(int),
  };

  // There are 49 formats, but only 24 fit inside one vertex input state so we test the 24 formats,
  // the next 24, and the last format on its own
  {
    // First 24 formats
    VertexInputStateDesc inputDesc_;
    for (int i = 0; i < IGL_VERTEX_ATTRIBUTES_MAX; i++) {
      inputDesc_.attributes[i].format = static_cast<VertexAttributeFormat>(i);
      inputDesc_.attributes[i].offset = 0;
      inputDesc_.attributes[i].location = 0;
      inputDesc_.attributes[i].bufferIndex = i;
      inputDesc_.attributes[i].name = SomeName;
      inputDesc_.inputBindings[i].stride = sizes[i];
    }

    // numAttributes has to equal to bindings when using more than 1 buffer
    inputDesc_.numAttributes = inputDesc_.numInputBindings = IGL_VERTEX_ATTRIBUTES_MAX;

    Result ret;
    std::shared_ptr<IVertexInputState> vertexInputState;
    vertexInputState = iglDev_->createVertexInputState(inputDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vertexInputState != nullptr);
  }
  {
    // Next 24 formats
    VertexInputStateDesc inputDesc_;
    for (int i = 0; i < IGL_VERTEX_ATTRIBUTES_MAX; i++) {
      inputDesc_.attributes[i].format =
          static_cast<VertexAttributeFormat>(i + IGL_VERTEX_ATTRIBUTES_MAX);
      inputDesc_.attributes[i].offset = 0;
      inputDesc_.attributes[i].location = 0;
      inputDesc_.attributes[i].bufferIndex = i;
      inputDesc_.attributes[i].name = SomeName;
      inputDesc_.inputBindings[i].stride = sizes[i + IGL_VERTEX_ATTRIBUTES_MAX];
    }

    // numAttributes has to equal to bindings when using more than 1 buffer
    inputDesc_.numAttributes = inputDesc_.numInputBindings = IGL_VERTEX_ATTRIBUTES_MAX;

    Result ret;
    std::shared_ptr<IVertexInputState> vertexInputState;
    vertexInputState = iglDev_->createVertexInputState(inputDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vertexInputState != nullptr);
  }
  {
    // Last format
    VertexInputStateDesc inputDesc_;
    inputDesc_.attributes[0].format = VertexAttributeFormat::Int_2_10_10_10_REV;
    inputDesc_.attributes[0].offset = 0;
    inputDesc_.attributes[0].location = 0;
    inputDesc_.attributes[0].bufferIndex = 0;
    inputDesc_.attributes[0].name = SomeName;
    inputDesc_.inputBindings[0].stride =
        sizes[static_cast<size_t>(VertexAttributeFormat::Int_2_10_10_10_REV)];

    // numAttributes has to equal to bindings when using more than 1 buffer
    inputDesc_.numAttributes = inputDesc_.numInputBindings = 1;

    Result ret;
    std::shared_ptr<IVertexInputState> vertexInputState;
    vertexInputState = iglDev_->createVertexInputState(inputDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vertexInputState != nullptr);
  }
}

} // namespace igl::tests
