/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "data/ShaderData.h"
#include "util/Common.h"
#include "util/TestDevice.h"
#include <igl/NameHandle.h>

#include <string>

namespace igl::tests {

class TestShaderStages : public IShaderStages {
 public:
  TestShaderStages() : IShaderStages(ShaderStagesDesc{}) {}
};

//
// ResourceTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class HashTest : public ::testing::Test {
 public:
  HashTest() = default;
  ~HashTest() override = default;

  // Set up common resources. This will create a device, a command queue,
  // and a command buffer
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);

    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    igl::tests::util::createSimpleShaderStages(iglDev_, stages);
    shaderStages_ = std::move(stages);
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> cmdBuf_;
  CommandBufferDesc cbDesc_ = {};

  std::shared_ptr<IShaderStages> shaderStages_;
};

//
// RenderPipelineDesc1
//
// Test hashing correctness in the RenderPipelineDesc structure
//
TEST_F(HashTest, GraphicsPipeline1) {
  RenderPipelineDesc descOne, descTwo;

  // Should have the same hash
  ASSERT_EQ(std::hash<RenderPipelineDesc>()(descOne), std::hash<RenderPipelineDesc>()(descTwo));

  // Modify descTwo 1
  descTwo.cullMode = igl::CullMode::Front;

  ASSERT_NE(std::hash<RenderPipelineDesc>()(descOne), std::hash<RenderPipelineDesc>()(descTwo));
  descTwo.cullMode = descOne.cullMode;

  // Modify descTwo 2
  descTwo.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("hello");

  ASSERT_NE(std::hash<RenderPipelineDesc>()(descOne), std::hash<RenderPipelineDesc>()(descTwo));
  descTwo.fragmentUnitSamplerMap[0] = descOne.fragmentUnitSamplerMap[0];

  // Modify shaderStages
  descTwo.shaderStages = std::make_shared<TestShaderStages>();

  ASSERT_NE(std::hash<RenderPipelineDesc>()(descOne), std::hash<RenderPipelineDesc>()(descTwo));
  descTwo.shaderStages = descOne.shaderStages;
}

//
// RenderPipelineDesc2
//
// This test checks to see if the definition of GraphicsPipeline has changed.
// For simplicity, we are only checking this on a 64-bit machine since
// developers use 64-bit machines.
//
// If this test fails, then that means you have changed the definition of
// RenderPipelineDesc, most likely by adding extra fields. If this is the
// case, then double check if the hashing function needs to be updated and
// after that, update the expectedSize here so the test will pass.
//
TEST_F(HashTest, GraphicsPipeline2) {
  // Pass this test on a 32-bit machine
  if (sizeof(size_t) == 4) {
    return;
  }

  // 64 is the size without unitSamplerMaps, colorAttachments, and debugName as those fields may
  // vary between compilers and machines
  const size_t expectedSize = 64 + 2 * sizeof(std::unordered_map<size_t, std::string>) +
                              sizeof(std::unordered_map<size_t, igl::NameHandle>) +
                              sizeof(std::vector<RenderPipelineDesc::TargetDesc::ColorAttachment>) +
                              sizeof(igl::NameHandle) +
                              sizeof(std::shared_ptr<ISamplerState>) * IGL_TEXTURE_SAMPLERS_MAX;

  ASSERT_EQ(expectedSize, sizeof(RenderPipelineDesc));
}

//
// RenderPipelineDesc3
//
// This test checks the "==" operator, which is a necessary complement to
// hashing, since this is how unordered_map uses in case of collision.
//
TEST_F(HashTest, GraphicsPipeline3) {
  RenderPipelineDesc descOne, descTwo;

  ASSERT_TRUE(descOne == descTwo);

  // Change and restore cull mode
  descTwo.cullMode = igl::CullMode::Front;
  ASSERT_TRUE(descOne != descTwo);
  descTwo.cullMode = descOne.cullMode;
  ASSERT_TRUE(descOne == descTwo);

  // Change and restore winding mode
  descTwo.frontFaceWinding = igl::WindingMode::Clockwise;
  ASSERT_TRUE(descOne != descTwo);
  descTwo.frontFaceWinding = descOne.frontFaceWinding;
  ASSERT_TRUE(descOne == descTwo);

  // Change and restore depthAttachmentFormat
  descTwo.targetDesc.depthAttachmentFormat = TextureFormat::A_UNorm8;
  ASSERT_TRUE(descOne != descTwo);
  descTwo.targetDesc.depthAttachmentFormat = descOne.targetDesc.depthAttachmentFormat;
  ASSERT_TRUE(descOne == descTwo);

  // Change and restore stencilAttachmentFormat
  descTwo.targetDesc.stencilAttachmentFormat = TextureFormat::A_UNorm8;
  ASSERT_TRUE(descOne != descTwo);
  descTwo.targetDesc.stencilAttachmentFormat = descOne.targetDesc.stencilAttachmentFormat;
  ASSERT_TRUE(descOne == descTwo);

  // Change and restore shaderStages
  descTwo.shaderStages = shaderStages_;
  ASSERT_TRUE(descOne != descTwo);
  descTwo.shaderStages = descOne.shaderStages;
  ASSERT_TRUE(descOne == descTwo);
}

//
// VertexInputStateDesc1
//
// Test hashing correctness in the VertexInputStateDesc structure
//
TEST_F(HashTest, VertexInputState1) {
  VertexInputStateDesc descOne, descTwo;

  // Should have the same hash
  ASSERT_EQ(std::hash<VertexInputStateDesc>()(descOne), std::hash<VertexInputStateDesc>()(descTwo));

  // Modify descTwo to have an attribute
  descTwo.numAttributes = 1;
  descTwo.numInputBindings = 1;
  descTwo.attributes[0].format = VertexAttributeFormat::Float4;
  descTwo.attributes[0].offset = 0;
  descTwo.attributes[0].bufferIndex = data::shader::simplePosIndex;
  descTwo.attributes[0].name = data::shader::simplePos;
  descTwo.attributes[0].location = 0;
  descTwo.inputBindings[0].stride = sizeof(float) * 4;

  ASSERT_NE(std::hash<VertexInputStateDesc>()(descOne), std::hash<VertexInputStateDesc>()(descTwo));

  // Modify descOne to have the same attribute
  descOne.numAttributes = 1;
  descOne.numInputBindings = 1;
  descOne.attributes[0].format = VertexAttributeFormat::Float4;
  descOne.attributes[0].offset = 0;
  descOne.attributes[0].bufferIndex = data::shader::simplePosIndex;
  descOne.attributes[0].name = data::shader::simplePos;
  descOne.attributes[0].location = 0;
  descOne.inputBindings[0].stride = sizeof(float) * 4;

  ASSERT_EQ(std::hash<VertexInputStateDesc>()(descOne), std::hash<VertexInputStateDesc>()(descTwo));

  // Modify a property of the attribute
  descOne.attributes[0].format = VertexAttributeFormat::Float3;

  ASSERT_NE(std::hash<VertexInputStateDesc>()(descOne), std::hash<VertexInputStateDesc>()(descTwo));
}

//
// DepthStencilStateDesc1
//
// Test hashing correctness in the DepthStencilStateDesc structure
//
TEST_F(HashTest, DepthStencilState1) {
  DepthStencilStateDesc descOne, descTwo;

  // Should have the same hash
  ASSERT_EQ(std::hash<DepthStencilStateDesc>()(descOne),
            std::hash<DepthStencilStateDesc>()(descTwo));

  // Modify descTwo
  descTwo.isDepthWriteEnabled = true;
  ASSERT_NE(std::hash<DepthStencilStateDesc>()(descOne),
            std::hash<DepthStencilStateDesc>()(descTwo));

  // Modify descOne to match
  descOne.isDepthWriteEnabled = true;
  ASSERT_EQ(std::hash<DepthStencilStateDesc>()(descOne),
            std::hash<DepthStencilStateDesc>()(descTwo));

  // Modify descTwo's backFaceStencil
  descTwo.backFaceStencil.stencilCompareFunction = CompareFunction::Never;
  ASSERT_NE(std::hash<DepthStencilStateDesc>()(descOne),
            std::hash<DepthStencilStateDesc>()(descTwo));

  // Modify descOne's backFaceStencil to be similar but not the same
  descOne.backFaceStencil.stencilCompareFunction = CompareFunction::Never;
  descOne.backFaceStencil.depthStencilPassOperation = StencilOperation::Replace;
  ASSERT_NE(std::hash<DepthStencilStateDesc>()(descOne),
            std::hash<DepthStencilStateDesc>()(descTwo));

  // Match descTwo to descOne
  descTwo.backFaceStencil.depthStencilPassOperation = StencilOperation::Replace;
  ASSERT_EQ(std::hash<DepthStencilStateDesc>()(descOne),
            std::hash<DepthStencilStateDesc>()(descTwo));
}

} // namespace igl::tests
