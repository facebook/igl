/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../metal/VertexInputState.h"

#include "../util/Common.h"

#include <memory>
#include <string>

namespace igl::tests {

// VertexInputStateMTLTest
//
// This test covers igl::metal::Device::createVertexInputState. Most of the testing
// revolves around validating correction rejection of invalid input.
class VertexInputStateMTLTest : public ::testing::Test {
 public:
  VertexInputStateMTLTest() = default;
  ~VertexInputStateMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(VertexInputStateMTLTest, testDefaultVertexInputDesc) {
  ASSERT_TRUE(iglDev_ != nullptr);

  const VertexInputStateDesc inputDesc;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
}

TEST_F(VertexInputStateMTLTest, testWithNumAttributesTooLarge) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = IGL_VERTEX_ATTRIBUTES_MAX + 1;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
  ASSERT_EQ(ret.message, "numAttributes is too large in VertexInputStateDesc");
  ASSERT_TRUE(vertexInputState == nullptr);
}

TEST_F(VertexInputStateMTLTest, testWithNumBindingsTooLarge) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numInputBindings = IGL_BUFFER_BINDINGS_MAX + 1;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
  ASSERT_EQ(ret.message, "numInputBindings is too large in VertexInputStateDesc");
}

// In this test we have a single attribute and no bindings
// and this should fail because the bufferIndexes are invalid.
TEST_F(VertexInputStateMTLTest, testOneAttribute) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 1;
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].name = "unused";
  inputDesc.attributes[0].location = 0;

  inputDesc.numInputBindings = 0;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST_F(VertexInputStateMTLTest, testNegativeBufferIndex) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 1;
  inputDesc.attributes[0].bufferIndex = -1;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].name = "unused";
  inputDesc.attributes[0].location = 0;

  inputDesc.numInputBindings = 1;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

TEST_F(VertexInputStateMTLTest, testNegativeLocation) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 1;
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].name = "unused";
  inputDesc.attributes[0].location = -1;

  inputDesc.numInputBindings = 1;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

// Attribute locations have to be unique.
// Here we set two locations to the same number (1).
// Test should fail
TEST_F(VertexInputStateMTLTest, testLocationUnique) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].name = "unused";
  inputDesc.attributes[0].location = 1;
  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[1].offset = 0;
  inputDesc.attributes[1].name = "unused";
  inputDesc.attributes[1].location = 1;

  inputDesc.numInputBindings = 1;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

// Attribute locations do not have to start at 0 and they do not have to be sequential.
// Here we set attribute locations to 10 and 21.
// Test should pass.
TEST_F(VertexInputStateMTLTest, testLocationNonSequential) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].name = "unused";
  inputDesc.attributes[0].location = 10;
  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[1].offset = 0;
  inputDesc.attributes[1].name = "unused";
  inputDesc.attributes[1].location = 15;

  inputDesc.numInputBindings = 1;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
}

TEST_F(VertexInputStateMTLTest, testTwoAttributesZeroBinding) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;

  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].name = "unused1";
  inputDesc.attributes[0].location = 0;

  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[1].offset = 0;
  inputDesc.attributes[1].name = "unused2";
  inputDesc.attributes[1].location = 1;

  inputDesc.numInputBindings = 0;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentInvalid);
  ASSERT_TRUE(vertexInputState == nullptr);
}

TEST_F(VertexInputStateMTLTest, testTwoAttributesNotCovering) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;

  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].name = "unused1";
  inputDesc.attributes[0].location = 0;

  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[1].offset = 0;
  inputDesc.attributes[1].name = "unused2";
  inputDesc.attributes[1].location = 0;

  inputDesc.numInputBindings = 1;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentInvalid);
  ASSERT_TRUE(vertexInputState == nullptr);
}

TEST_F(VertexInputStateMTLTest, testTwoAttributesOneBuffer) {
  ASSERT_TRUE(iglDev_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;

  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].name = "unused1";
  inputDesc.attributes[0].location = 1;

  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float1;
  inputDesc.attributes[1].offset = 8;
  inputDesc.attributes[1].name = "unused2";
  inputDesc.attributes[1].location = 0;

  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = 16;
  inputDesc.inputBindings[0].sampleFunction = VertexSampleFunction::PerVertex;
  inputDesc.inputBindings[0].sampleRate = 2;
  Result ret;

  const std::shared_ptr<IVertexInputState> vertexInputState =
      iglDev_->createVertexInputState(inputDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(vertexInputState != nullptr);

  const std::shared_ptr<::igl::metal::VertexInputState> metalVertexInputState =
      std::static_pointer_cast<::igl::metal::VertexInputState>(vertexInputState);
  ASSERT_TRUE(metalVertexInputState != nullptr);

  MTLVertexDescriptor* metalVertexDescriptor = metalVertexInputState->get();
  ASSERT_TRUE(metalVertexDescriptor != nullptr);

  ASSERT_EQ(MTLVertexFormatFloat, metalVertexDescriptor.attributes[0].format);
  ASSERT_EQ(0, metalVertexDescriptor.attributes[0].bufferIndex);
  ASSERT_EQ(8, metalVertexDescriptor.attributes[0].offset);

  ASSERT_EQ(MTLVertexFormatFloat, metalVertexDescriptor.attributes[1].format);
  ASSERT_EQ(0, metalVertexDescriptor.attributes[1].bufferIndex);
  ASSERT_EQ(0, metalVertexDescriptor.attributes[1].offset);

  ASSERT_EQ(16, metalVertexDescriptor.layouts[0].stride);
  ASSERT_EQ(MTLVertexStepFunctionPerVertex, metalVertexDescriptor.layouts[0].stepFunction);
  ASSERT_EQ(2, metalVertexDescriptor.layouts[0].stepRate);
}

TEST(VertexInputStateMTLStaticTest, testConvertAttributeFormat) {
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Float1),
            MTLVertexFormatFloat);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Float2),
            MTLVertexFormatFloat2);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Float3),
            MTLVertexFormatFloat3);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Float4),
            MTLVertexFormatFloat4);

  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Byte2),
            MTLVertexFormatChar2);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Byte3),
            MTLVertexFormatChar3);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Byte4),
            MTLVertexFormatChar4);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UByte2),
            MTLVertexFormatUChar2);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UByte3),
            MTLVertexFormatUChar3);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UByte4),
            MTLVertexFormatUChar4);

  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Short2),
            MTLVertexFormatShort2);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Short3),
            MTLVertexFormatShort3);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Short4),
            MTLVertexFormatShort4);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UShort2),
            MTLVertexFormatUShort2);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UShort3),
            MTLVertexFormatUShort3);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UShort4),
            MTLVertexFormatUShort4);

  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Byte1Norm),
            MTLVertexFormatCharNormalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Byte2Norm),
            MTLVertexFormatChar2Normalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Byte3Norm),
            MTLVertexFormatChar3Normalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Byte4Norm),
            MTLVertexFormatChar4Normalized);

  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UByte1Norm),
            MTLVertexFormatUCharNormalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UByte2Norm),
            MTLVertexFormatUChar2Normalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UByte3Norm),
            MTLVertexFormatUChar3Normalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UByte4Norm),
            MTLVertexFormatUChar4Normalized);

  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Short1Norm),
            MTLVertexFormatShortNormalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Short2Norm),
            MTLVertexFormatShort2Normalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Short3Norm),
            MTLVertexFormatShort3Normalized);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Short4Norm),
            MTLVertexFormatShort4Normalized);

  ASSERT_EQ(
      igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UShort1Norm),
      MTLVertexFormatUShortNormalized);
  ASSERT_EQ(
      igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UShort2Norm),
      MTLVertexFormatUShort2Normalized);
  ASSERT_EQ(
      igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UShort3Norm),
      MTLVertexFormatUShort3Normalized);
  ASSERT_EQ(
      igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UShort4Norm),
      MTLVertexFormatUShort4Normalized);

  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Byte1),
            MTLVertexFormatInvalid);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UByte1),
            MTLVertexFormatInvalid);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::Short1),
            MTLVertexFormatInvalid);
  ASSERT_EQ(igl::metal::VertexInputState::convertAttributeFormat(VertexAttributeFormat::UShort1),
            MTLVertexFormatInvalid);
}

TEST(VertexInputStateMTLStaticTest, testConvertSampleFunction) {
  ASSERT_EQ(igl::metal::VertexInputState::convertSampleFunction(VertexSampleFunction::Constant),
            MTLVertexStepFunctionConstant);
  ASSERT_EQ(igl::metal::VertexInputState::convertSampleFunction(VertexSampleFunction::PerVertex),
            MTLVertexStepFunctionPerVertex);
  ASSERT_EQ(igl::metal::VertexInputState::convertSampleFunction(VertexSampleFunction::Instance),
            MTLVertexStepFunctionPerInstance);
}

} // namespace igl::tests
