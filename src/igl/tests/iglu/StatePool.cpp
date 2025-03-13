/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../data/ShaderData.h"
#include "../util/Common.h"

#include <IGLU/state_pool/RenderPipelineStatePool.h>
#include <gtest/gtest.h>
#include <igl/CommandBuffer.h>
#include <igl/NameHandle.h>
#include <igl/VertexInputState.h>
#include <string>

namespace igl::tests {

//
// StatePoolTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class StatePoolTest : public ::testing::Test {
 private:
 public:
  StatePoolTest() = default;
  ~StatePoolTest() override = default;

  //
  // SetUp()
  //
  // This function sets up two identical graphics pipeline descriptors
  // so that they can be modified by individual tests to exercise the
  // state pool caching capabilities
  //
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    Result ret;

    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    igl::tests::util::createSimpleShaderStages(iglDev_, stages);
    shaderStages_ = std::move(stages);

    // Initialize input to vertex shader
    VertexInputStateDesc inputDesc;

    inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
    inputDesc.attributes[0].offset = 0;
    inputDesc.attributes[0].location = 0;
    inputDesc.attributes[0].bufferIndex = data::shader::simplePosIndex;
    inputDesc.attributes[0].name = data::shader::simplePos;
    inputDesc.inputBindings[0].stride = sizeof(float) * 4;

    inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
    inputDesc.attributes[1].offset = 0;
    inputDesc.attributes[1].location = 1;
    inputDesc.attributes[1].bufferIndex = data::shader::simpleUvIndex;
    inputDesc.attributes[1].name = data::shader::simpleUv;
    inputDesc.inputBindings[1].stride = sizeof(float) * 2;

    // numAttributes has to equal to bindings when using more than 1 buffer
    inputDesc.numAttributes = inputDesc.numInputBindings = 2;

    vertexInputState_ = iglDev_->createVertexInputState(inputDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vertexInputState_ != nullptr);

    // Initialize Graphics Pipeline Descriptor, but leave the creation
    // to the individual tests in case further customization is required
    renderPipelineDesc1_.vertexInputState = vertexInputState_;
    renderPipelineDesc1_.shaderStages = shaderStages_;
    renderPipelineDesc1_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc1_.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
    renderPipelineDesc1_.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE(data::shader::simpleSampler);
    renderPipelineDesc1_.cullMode = igl::CullMode::Disabled;

    renderPipelineDesc2_.vertexInputState = vertexInputState_;
    renderPipelineDesc2_.shaderStages = shaderStages_;
    renderPipelineDesc2_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc2_.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
    renderPipelineDesc2_.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE(data::shader::simpleSampler);
    renderPipelineDesc2_.cullMode = igl::CullMode::Disabled;

    renderPipelineDesc3_.vertexInputState = vertexInputState_;
    renderPipelineDesc3_.shaderStages = shaderStages_;
    renderPipelineDesc3_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc3_.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
    renderPipelineDesc3_.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE(data::shader::simpleSampler);
    renderPipelineDesc3_.cullMode = igl::CullMode::Disabled;
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> cmdBuf_;
  CommandBufferDesc cbDesc_ = {};

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;

  RenderPipelineDesc renderPipelineDesc1_, renderPipelineDesc2_, renderPipelineDesc3_;
  iglu::state_pool::RenderPipelineStatePool graphicsPool_;
};

//
// renderPipelineDescCaching1 Test
//
// Tests to see if RenderPipelineDesc caching works
//
TEST_F(StatePoolTest, renderPipelineDescCaching1) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> ps1, ps2;

  //---------------------------------------------------------------------
  // Create two pipelines without cache. Should get two different objects
  //---------------------------------------------------------------------
  ps1 = iglDev_->createRenderPipeline(renderPipelineDesc1_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps1 != nullptr);

  ps2 = iglDev_->createRenderPipeline(renderPipelineDesc2_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps2 != nullptr);

  ASSERT_TRUE(ps1.get() != ps2.get());

  //------------------------------------------------------------
  // Create two pipelines with cache. Should get the same object
  //------------------------------------------------------------
  ps1 = graphicsPool_.getOrCreate(*iglDev_, renderPipelineDesc1_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps1 != nullptr);

  ps2 = graphicsPool_.getOrCreate(*iglDev_, renderPipelineDesc2_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps2 != nullptr);

  ASSERT_TRUE(ps1 == ps2);

  //------------------------------------------------------------
  // Modify one of the descriptors, should get different objects
  //------------------------------------------------------------
  renderPipelineDesc2_.cullMode = igl::CullMode::Front;

  ps2 = graphicsPool_.getOrCreate(*iglDev_, renderPipelineDesc2_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps2 != nullptr);

  ASSERT_TRUE(ps1 != ps2);
  renderPipelineDesc2_.cullMode = renderPipelineDesc1_.cullMode; // restore change
}

//
// renderPipelineDescCachingLRU1 Test
//
// Tests to see if RenderPipelineDesc LRU caching works
//
TEST_F(StatePoolTest, renderPipelineDescCachingLRU1) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> ps1, ps2, ps3;
  //------------------------------------------------------------
  // Ensure cache releases objects after it gets full
  //------------------------------------------------------------
  iglu::state_pool::RenderPipelineStatePool smallCachePool;
  smallCachePool.setCacheSize(2);
  renderPipelineDesc2_.cullMode = igl::CullMode::Front;
  renderPipelineDesc3_.cullMode = igl::CullMode::Back;

  // Fill up the cache
  ps1 = iglDev_->createRenderPipeline(renderPipelineDesc1_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps1 != nullptr);

  ps2 = iglDev_->createRenderPipeline(renderPipelineDesc2_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps2 != nullptr);

  // Add new element (PS1 should no longer be stored in the cache)
  ps3 = iglDev_->createRenderPipeline(renderPipelineDesc3_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps3 != nullptr);

  // Adding renderPipelineDesc1_ again should produce a new state than before
  ps2 = iglDev_->createRenderPipeline(renderPipelineDesc1_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ps2 != nullptr);
  ASSERT_TRUE(ps1 != ps2);

  renderPipelineDesc2_.cullMode = renderPipelineDesc1_.cullMode; // restore change
  renderPipelineDesc3_.cullMode = renderPipelineDesc1_.cullMode; // restore change
}

} // namespace igl::tests
