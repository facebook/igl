/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/RenderPipelineState.h>

#include "../data/ShaderData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/Common.h>
#include <igl/Device.h>
#include <igl/IGL.h>
#include <igl/metal/Shader.h>
#include <igl/metal/VertexInputState.h>
namespace igl::tests {

class RenderPipelineStateMTLTest : public ::testing::Test {
 public:
  RenderPipelineStateMTLTest() = default;
  ~RenderPipelineStateMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);
    Result ret;

    // Initialize vertex Shader
    auto shaderLibrary = ShaderLibraryCreator::fromStringInput(*iglDev_,
                                                               data::shader::MTL_SIMPLE_SHADER,
                                                               data::shader::simpleVertFunc,
                                                               data::shader::simpleFragFunc,
                                                               "",
                                                               &ret);

    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(shaderLibrary != nullptr);

    vertShader_ = shaderLibrary->getShaderModule(data::shader::simpleVertFunc);
    ASSERT_TRUE(vertShader_ != nullptr);

    fragShader_ = shaderLibrary->getShaderModule(data::shader::simpleFragFunc);
    ASSERT_TRUE(fragShader_ != nullptr);

    // Initialize input to vertex shader
    VertexInputStateDesc inputDesc;

    inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
    inputDesc.attributes[0].offset = 0;
    inputDesc.attributes[0].bufferIndex = data::shader::simplePosIndex;
    inputDesc.attributes[0].name = data::shader::simplePos;
    inputDesc.attributes[0].location = 0;
    inputDesc.inputBindings[0].stride = sizeof(float) * 4;

    inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
    inputDesc.attributes[1].offset = 0;
    inputDesc.attributes[1].bufferIndex = data::shader::simpleUvIndex;
    inputDesc.attributes[1].name = data::shader::simpleUv;
    inputDesc.attributes[1].location = 1;
    inputDesc.inputBindings[1].stride = sizeof(float) * 2;

    // numAttributes has to equal to bindings when using more than 1 buffer
    inputDesc.numAttributes = inputDesc.numInputBindings = 2;

    vertexInputState_ = iglDev_->createVertexInputState(inputDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(vertexInputState_ != nullptr);

    NSError* error = nullptr;
    MTLRenderPipelineDescriptor* metalDesc = [MTLRenderPipelineDescriptor new];

    metalDesc.vertexDescriptor =
        static_cast<igl::metal::VertexInputState*>(vertexInputState_.get())->get();
    metalDesc.vertexFunction = static_cast<igl::metal::ShaderModule*>(vertShader_.get())->get();
    IGL_DEBUG_ASSERT(metalDesc.vertexFunction, "RenderPipeline requires non-null vertex function");
    metalDesc.fragmentFunction = static_cast<igl::metal::ShaderModule*>(fragShader_.get())->get();
    metalDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    metalDesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    MTLRenderPipelineReflection* reflection = nil;
    auto device_ = MTLCreateSystemDefaultDevice();

    RenderPipelineDesc pipelineDesc_{};
    pipelineDesc_.cullMode = CullMode::Back;
    pipelineDesc_.frontFaceWinding = igl::WindingMode::CounterClockwise;
    pipelineDesc_.polygonFillMode = PolygonFillMode::Fill;

    // Create reflection for use later in binding, etc.
    id<MTLRenderPipelineState> metalObject =
        [device_ newRenderPipelineStateWithDescriptor:metalDesc
                                              options:MTLPipelineOptionArgumentInfo
                                           reflection:&reflection
                                                error:&error];

    pipeState_ =
        std::make_shared<metal::RenderPipelineState>(metalObject, reflection, pipelineDesc_);

    id<MTLRenderPipelineState> metalObjectWithoutRefl =
        [device_ newRenderPipelineStateWithDescriptor:metalDesc
                                              options:MTLPipelineOptionArgumentInfo
                                           reflection:nullptr
                                                error:&error];

    pipeStateWithNoRefl_ = std::make_shared<metal::RenderPipelineState>(
        metalObjectWithoutRefl, nullptr, pipelineDesc_);

    ASSERT_NE(pipeState_, nullptr);
  }
  void TearDown() override {}

  std::shared_ptr<metal::RenderPipelineState> pipeState_;

 protected:
  std::shared_ptr<metal::RenderPipelineState> pipeStateWithNoRefl_;

  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  std::shared_ptr<IShaderModule> vertShader_;
  std::shared_ptr<IShaderModule> fragShader_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
};

TEST_F(RenderPipelineStateMTLTest, GetIndexByName) {
  auto index = pipeState_->getIndexByName(data::shader::simpleUv, ShaderStage::Vertex);
  ASSERT_EQ(index, 1);
}

TEST_F(RenderPipelineStateMTLTest, GetNonexistentIndexByName) {
  auto index = pipeState_->getIndexByName("", ShaderStage::Fragment);
  ASSERT_EQ(index, -1);
}

TEST_F(RenderPipelineStateMTLTest, GetIndexByNameWithoutRefl) {
  auto index = pipeStateWithNoRefl_->getIndexByName(data::shader::simpleUv, ShaderStage::Vertex);
  ASSERT_EQ(index, -1);
}

TEST_F(RenderPipelineStateMTLTest, GetIndexByNameHandle) {
  auto index =
      pipeState_->getIndexByName(igl::genNameHandle(data::shader::simpleUv), ShaderStage::Vertex);
  ASSERT_EQ(index, 1);
}

TEST_F(RenderPipelineStateMTLTest, GetNonexistentIndexByNameHandle) {
  auto index = pipeState_->getIndexByName("", ShaderStage::Fragment);
  ASSERT_EQ(index, -1);
}

TEST_F(RenderPipelineStateMTLTest, GetIndexByNameHandleWithoutRefl) {
  auto index = pipeStateWithNoRefl_->getIndexByName(igl::genNameHandle(data::shader::simpleUv),
                                                    ShaderStage::Vertex);
  ASSERT_EQ(index, -1);
}

TEST_F(RenderPipelineStateMTLTest, ConvertColorWriteMaskRed) {
  auto mask = pipeState_->convertColorWriteMask(ColorWriteBitsRed);
  ASSERT_EQ(mask, MTLColorWriteMaskRed);
}

TEST_F(RenderPipelineStateMTLTest, ConvertColorWriteMaskGreen) {
  auto mask = pipeState_->convertColorWriteMask(ColorWriteBitsGreen);
  ASSERT_EQ(mask, MTLColorWriteMaskGreen);
}

TEST_F(RenderPipelineStateMTLTest, ConvertColorWriteMaskBlue) {
  auto mask = pipeState_->convertColorWriteMask(ColorWriteBitsBlue);
  ASSERT_EQ(mask, MTLColorWriteMaskBlue);
}

TEST_F(RenderPipelineStateMTLTest, ConvertColorWriteMaskAlpha) {
  auto mask = pipeState_->convertColorWriteMask(ColorWriteBitsAlpha);
  ASSERT_EQ(mask, MTLColorWriteMaskAlpha);
}

} // namespace igl::tests
