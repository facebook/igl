/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/RenderPipelineReflection.h>

#include "../data/ShaderData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/Device.h>
#include <igl/IGL.h>
#include <igl/metal/Shader.h>
#include <igl/metal/VertexInputState.h>

namespace igl::tests {

class RenderPipelineReflectionMTLTest : public ::testing::Test {
 public:
  RenderPipelineReflectionMTLTest() = default;
  ~RenderPipelineReflectionMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);
    Result ret;

    auto shaderLibrary = ShaderLibraryCreator::fromStringInput(
        *iglDev_,
        data::shader::MTL_SIMPLE_SHADER,
        {
            {ShaderStage::Vertex, data::shader::simpleVertFunc},
            {ShaderStage::Fragment, data::shader::simpleFragFunc},
        },
        "",
        &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

    // Initialize vertex Shader
    vertShader_ = shaderLibrary->getShaderModule(ShaderStage::Vertex, data::shader::simpleVertFunc);
    ASSERT_TRUE(vertShader_ != nullptr);

    // Initialize Fragment Shader
    fragShader_ =
        shaderLibrary->getShaderModule(ShaderStage::Fragment, data::shader::simpleFragFunc);
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
    auto device = MTLCreateSystemDefaultDevice();

// Suppress warnings about MTLPipelineOptionArgumentInfo being deprecated
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    // Create reflection for use later in binding, etc.
    [device newRenderPipelineStateWithDescriptor:metalDesc
                                         options:MTLPipelineOptionArgumentInfo
                                      reflection:&reflection
                                           error:&error];
#pragma GCC diagnostic pop

    pipeRef_ = std::make_shared<metal::RenderPipelineReflection>(reflection);
    ASSERT_NE(pipeRef_, nullptr);
  }
  void TearDown() override {}

 protected:
  std::shared_ptr<metal::RenderPipelineReflection> pipeRef_;

 private:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  std::shared_ptr<IShaderModule> vertShader_;
  std::shared_ptr<IShaderModule> fragShader_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
};

TEST_F(RenderPipelineReflectionMTLTest, GetIndexByName) {
  auto index = pipeRef_->getIndexByName(data::shader::simpleUv, ShaderStage::Vertex);
  ASSERT_EQ(index, 1);
}

TEST_F(RenderPipelineReflectionMTLTest, GetNonexistentIndexByName) {
  auto index = pipeRef_->getIndexByName("", ShaderStage::Fragment);
  ASSERT_EQ(index, -1);
}

TEST_F(RenderPipelineReflectionMTLTest, VerifyBuffers) {
  auto buffers = pipeRef_->allUniformBuffers();
  ASSERT_EQ(buffers.size(), 2);
  for (const auto& buffer : buffers) {
    ASSERT_EQ(buffer.shaderStage, ShaderStage::Vertex);
    EXPECT_TRUE(buffer.name.toString() == data::shader::simplePos ||
                buffer.name.toString() == data::shader::simpleUv);
  }
}

TEST_F(RenderPipelineReflectionMTLTest, VerifyTextures) {
  auto textures = pipeRef_->allTextures();
  ASSERT_EQ(textures.size(), 1);
  const auto& theOneTexture = textures.front();
  ASSERT_EQ(theOneTexture.name, "diffuseTex");
}

TEST_F(RenderPipelineReflectionMTLTest, VerifySamplers) {
  auto samplers = pipeRef_->allSamplers();
  ASSERT_EQ(samplers.size(), 1);
  const auto& theOneSampler = samplers.front();
  ASSERT_EQ(theOneSampler.name, "linearSampler");
}

} // namespace igl::tests
