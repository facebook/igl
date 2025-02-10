/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "data/ShaderData.h"
#include "data/TextureData.h"
#include "data/VertexIndexData.h"
#include "util/Common.h"
#include "util/TestDevice.h"

#include <gtest/gtest.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/IContext.h>
#include <igl/opengl/PlatformDevice.h>
#endif // #IGL_BACKEND_OPENGL
#include <string>

namespace igl::tests {

// Use a 1x1 Framebuffer for this test
constexpr size_t OFFSCREEN_RT_WIDTH = 1;
constexpr size_t OFFSCREEN_RT_HEIGHT = 1;

//
// FramebufferTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class FramebufferTest : public ::testing::Test {
 private:
 public:
  FramebufferTest() = default;
  ~FramebufferTest() override = default;

  //
  // SetUp()
  //
  // This function sets up a render pass and a render pipeline descriptor
  // so it is ready to render a simple quad with an input texture to an
  // offscreen texture.
  //
  // The actual creation of the render pipeline state object is left
  // to each test so that tests can replace the default settings with
  // something more appropriate.
  //
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    // Create an offscreen texture to render to
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   OFFSCREEN_RT_WIDTH,
                                                   OFFSCREEN_RT_HEIGHT,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment);

    auto depthFormat = TextureFormat::S8_UInt_Z32_UNorm;

    if (backend_ == util::BACKEND_VUL) {
      depthFormat = TextureFormat::S8_UInt_Z24_UNorm;
    }

    TextureDesc depthTexDesc = TextureDesc::new2D(depthFormat,
                                                  OFFSCREEN_RT_WIDTH,
                                                  OFFSCREEN_RT_HEIGHT,
                                                  TextureDesc::TextureUsageBits::Sampled |
                                                      TextureDesc::TextureUsageBits::Attachment);
    depthTexDesc.storage = ResourceStorage::Private;

    Result ret;
    offscreenTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(offscreenTexture_ != nullptr);

    depthStencilTexture_ = iglDev_->createTexture(depthTexDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(depthStencilTexture_ != nullptr);

    // Create framebuffer using the offscreen texture
    FramebufferDesc framebufferDesc;

    framebufferDesc.debugName = "test";
    framebufferDesc.colorAttachments[0].texture = offscreenTexture_;
    framebufferDesc.depthAttachment.texture = depthStencilTexture_;
    framebufferDesc.stencilAttachment.texture = depthStencilTexture_;

    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(framebuffer_ != nullptr);

    // Initialize render pass descriptor
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

    renderPass_.depthAttachment.loadAction = LoadAction::Clear;
    renderPass_.depthAttachment.storeAction = StoreAction::Store;
    renderPass_.depthAttachment.clearDepth = 0.0;

    renderPass_.stencilAttachment.loadAction = LoadAction::Clear;
    renderPass_.stencilAttachment.storeAction = StoreAction::Store;
    renderPass_.stencilAttachment.clearStencil = 0;

    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    igl::tests::util::createSimpleShaderStages(iglDev_, stages);
    shaderStages_ = std::move(stages);

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

    // Initialize index buffer
    BufferDesc bufDesc;

    bufDesc.type = BufferDesc::BufferTypeBits::Index;
    bufDesc.data = data::vertex_index::QUAD_IND;
    bufDesc.length = sizeof(data::vertex_index::QUAD_IND);

    ib_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(ib_ != nullptr);

    // Initialize vertex and sampler buffers
    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = data::vertex_index::QUAD_VERT;
    bufDesc.length = sizeof(data::vertex_index::QUAD_VERT);

    vb_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(vb_ != nullptr);

    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = data::vertex_index::QUAD_UV;
    bufDesc.length = sizeof(data::vertex_index::QUAD_UV);

    uv_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(uv_ != nullptr);

    // Initialize sampler state
    const SamplerStateDesc samplerDesc;
    samp_ = iglDev_->createSamplerState(samplerDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(samp_ != nullptr);

    // Initialize Render Pipeline Descriptor, but leave the creation
    // to the individual tests in case further customization is required
    renderPipelineDesc_.vertexInputState = vertexInputState_;
    renderPipelineDesc_.shaderStages = shaderStages_;
    renderPipelineDesc_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc_.targetDesc.colorAttachments[0].textureFormat =
        offscreenTexture_->getFormat();
    renderPipelineDesc_.targetDesc.depthAttachmentFormat = depthStencilTexture_->getFormat();
    renderPipelineDesc_.targetDesc.stencilAttachmentFormat = depthStencilTexture_->getFormat();
    renderPipelineDesc_.cullMode = igl::CullMode::Disabled;

    //
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> cmdBuf_;
  CommandBufferDesc cbDesc_ = {};
  const std::string backend_ = IGL_BACKEND_TYPE;

  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<ITexture> depthStencilTexture_;

  std::shared_ptr<IFramebuffer> framebuffer_;

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> vb_, uv_, ib_;

  std::shared_ptr<ISamplerState> samp_;

  RenderPipelineDesc renderPipelineDesc_;
};

//
// Framebuffer Clear Test
//
// This test exercises the FB clearing behavior. The expectation is color
// buffer will be cleared to the color specified, and there will be no
// leaked settings from previous render passes
//
TEST_F(FramebufferTest, Clear) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  std::shared_ptr<igl::IDepthStencilState> depthStencilState;
  DepthStencilStateDesc desc;
  desc.isDepthWriteEnabled = true;

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, OFFSCREEN_RT_WIDTH, OFFSCREEN_RT_HEIGHT);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(pipelineState != nullptr);

  depthStencilState = iglDev_->createDepthStencilState(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(depthStencilState != nullptr);

  //---------------------------------
  // Clear FB to {0.5, 0.5, 0.5, 0.5}
  //---------------------------------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(cmdBuf_ != nullptr);

  renderPass_.colorAttachments[0].clearColor = {0.501f, 0.501f, 0.501f, 0.501f};

  renderPass_.depthAttachment.clearDepth = 0.501f;
  renderPass_.stencilAttachment.clearStencil = 128;

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------
  // Read back framebuffer
  //----------------------
  auto pixels = std::vector<uint32_t>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH);
  auto pixels_depth = std::vector<float>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH);
  auto pixels_stencil = std::vector<uint8_t>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH);

  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);
  ASSERT_EQ(pixels[0], 0x80808080);

#if IGL_BACKEND_OPENGL
  // TODO: copyBytesDepthAttachment is not functioning property under Metal/Vulkan
  // due to unimplemented blitting
  // Refer to igl/metal/Framebuffer.mm
  framebuffer_->copyBytesDepthAttachment(*cmdQueue_, pixels_depth.data(), rangeDesc);
  // ASSERT_EQ(pixels_depth[0], 0x80808080);

  // TODO: copyBytesStencilAttachment is not functioning property under Metal/Vulkan
  // due to unimplemented blitting
  // Refer to igl/metal/Framebuffer.mm
  framebuffer_->copyBytesStencilAttachment(*cmdQueue_, pixels_stencil.data(), rangeDesc);
  // ASSERT_EQ(pixels_stencil[0], 0x80808080);
#endif

  //-------------------------------------------------------------------------
  // Clear FB to {0, 0, 0, 0}, but this time bind a pipelineState, disable
  // the colorWriteMasks, and do a no-op draw
  //-------------------------------------------------------------------------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(cmdBuf_ != nullptr);

  renderPipelineDesc_.targetDesc.colorAttachments[0].colorWriteMask = ColorWriteBitsDisabled;
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(pipelineState != nullptr);

  renderPass_.colorAttachments[0].clearColor = {0, 0, 0, 0};

  renderPass_.depthAttachment.clearDepth = 0.0;
  renderPass_.stencilAttachment.clearStencil = 0;

  cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindRenderPipelineState(pipelineState);
  cmds->bindDepthStencilState(depthStencilState);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);
  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(0); // draw 0 indices
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------
  // Read back framebuffer
  //----------------------
  pixels = std::vector<uint32_t>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH);

  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);
  ASSERT_EQ(pixels[0], 0);

#if IGL_BACKEND_OPENGL
  // TODO: copyBytesDepthAttachment is not functioning property under Metal/Vulkan
  // due to unimplemented blitting
  // Refer to igl/metal/Framebuffer.mm
  framebuffer_->copyBytesDepthAttachment(*cmdQueue_, pixels_depth.data(), rangeDesc);
  // ASSERT_EQ(pixels_depth[0], 0);

  // TODO: copyBytesStencilAttachment is not functioning property under Metal/Vulkan
  // due to unimplemented blitting
  // Refer to igl/metal/Framebuffer.mm
  framebuffer_->copyBytesStencilAttachment(*cmdQueue_, pixels_stencil.data(), rangeDesc);
  // ASSERT_EQ(pixels_stencil[0], 0);
#endif

  //-------------------------------------------------------------------------
  // Clear FB to {0.5, 0.5, 0.5, 0.5} again. We should not be impacted by the
  // colorWriteMask setting from the render pass above
  //-------------------------------------------------------------------------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(cmdBuf_ != nullptr);

  renderPass_.colorAttachments[0].clearColor = {0.501f, 0.501f, 0.501f, 0.501f};

  cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------
  // Read back framebuffer
  //----------------------
  pixels = std::vector<uint32_t>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH);

  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);
  ASSERT_EQ(pixels[0], 0x80808080);
}

//
// Framebuffer Blit Test
//
// This test exercises OGL platform device's blitFramebufferColor API.
// We are including this here rather than making it OGL-specific to also test
// the device's getPlatformDevice() function, which should return nullptr
// on Metal
//
#if IGL_BACKEND_OPENGL
TEST_F(FramebufferTest, blitFramebufferColor) {
  auto* platformDevice = iglDev_->getPlatformDevice<opengl::PlatformDevice>();
  if (platformDevice) {
    ASSERT_TRUE(backend_ == util::BACKEND_OGL);

    /* Bootcamper: Your Code here
     * At the high level here are the steps:
     *  1. Create second IFramebuffer, call it framebuffer2 look at set up to
     *     see how to do this:
     *        a. create another texture
     *        b. attach it to a FB descriptor
     *        c. create the framebuffer object
     *  2. clear framebuffer_, and make sure it is cleared. See the Clear test
     *     above to see how to do this
     *  3. clear framebuffer2 to a different color than framebuffer_, and make
     *     sure it is cleared
     *  4. call platforDevice->blitFramebufferColor() from framebuffer2 to
     *     framebuffer_
     *  5. Verify that framebuffer_ now has the same color as framebuffer2
     */

    Result ret;

    //-----------------------------------------
    // Create an offscreen texture to render to
    //-----------------------------------------
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   OFFSCREEN_RT_WIDTH,
                                                   OFFSCREEN_RT_HEIGHT,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment);

    const std::shared_ptr<ITexture> offscreenTexture2 = iglDev_->createTexture(texDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(offscreenTexture2 != nullptr);

    //-------------------------------------------------------------
    // Create second IFramebuffer framebuffer2 by offscreenTexture2
    //-------------------------------------------------------------
    FramebufferDesc framebufferDesc;

    framebufferDesc.colorAttachments[0].texture = offscreenTexture2;
    const std::shared_ptr<IFramebuffer> framebuffer2 =
        iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(framebuffer2 != nullptr);

    //---------------------------------
    // Clear FB to {0.5, 0.5, 0.5, 0.5}
    //---------------------------------
    const auto rangeDesc = TextureRangeDesc::new2D(0, 0, OFFSCREEN_RT_WIDTH, OFFSCREEN_RT_HEIGHT);

    cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(cmdBuf_ != nullptr);

    renderPass_.colorAttachments[0].clearColor = {0.501f, 0.501f, 0.501f, 0.501f};

    auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();

    //----------------------
    // Read back framebuffer
    //----------------------
    auto pixels = std::vector<uint32_t>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH);

    framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);
    ASSERT_EQ(pixels[0], 0x80808080);

    //-------------------------------------------------------------------------
    // Clear FB2 to {0, 0, 0, 0}
    //-------------------------------------------------------------------------
    cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(cmdBuf_ != nullptr);

    renderPass_.colorAttachments[0].clearColor = {0, 0, 0, 0};

    cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer2);
    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();

    //-----------------------
    // Read back framebuffer2
    //-----------------------
    auto pixels2 = std::vector<uint32_t>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH);

    framebuffer2->copyBytesColorAttachment(*cmdQueue_, 0, pixels2.data(), rangeDesc);
    ASSERT_EQ(pixels2[0], 0);

    if (platformDevice->getContext().deviceFeatures().hasInternalFeature(
            opengl::InternalFeatures::FramebufferBlit)) {
      //--------------------------------------------------------------
      // Call blitFramebuffer() from framebuffer_ to framebuffer2
      //--------------------------------------------------------------
      platformDevice->blitFramebuffer(framebuffer_,
                                      0,
                                      0,
                                      OFFSCREEN_RT_WIDTH,
                                      OFFSCREEN_RT_WIDTH,
                                      framebuffer2,
                                      0,
                                      0,
                                      OFFSCREEN_RT_WIDTH,
                                      OFFSCREEN_RT_WIDTH,
                                      GL_COLOR_BUFFER_BIT,
                                      &ret);
      ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

      //-------------------------------------------------------------
      // Read back framebuffer_ & framebuffer2, and examine if equal.
      //-------------------------------------------------------------
      framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);
      framebuffer2->copyBytesColorAttachment(*cmdQueue_, 0, pixels2.data(), rangeDesc);

      ASSERT_EQ(pixels[0], pixels2[0]);
      ASSERT_EQ(pixels2[0], 0x80808080);
    }
  } else {
    ASSERT_TRUE(backend_ != util::BACKEND_OGL);
  }
}
#endif // IGL_BACKEND_OPENGL

//
// Framebuffer Drawable Unbind Test
//
// This test checks that when updateDrawable is called with nullptr, the
// color attachment is no longer bound. It also checks that draw counts
// are properly updated when unbind and rebinding a drawable
//
TEST_F(FramebufferTest, DrawableUnbind) {
  // Currently the drawable is always bound to index 0
  auto colorAttachment = framebuffer_->getColorAttachment(0);
  auto numOfAttachments = framebuffer_->getColorAttachmentIndices().size();
  auto depthAttachment = framebuffer_->getDepthAttachment();
  auto stencilAttachment = framebuffer_->getStencilAttachment();

  ASSERT_TRUE(colorAttachment != nullptr);
  ASSERT_EQ(numOfAttachments, 1);
  ASSERT_TRUE(depthAttachment != nullptr);
  ASSERT_TRUE(stencilAttachment != nullptr);

  framebuffer_->updateDrawable(nullptr);
  numOfAttachments = framebuffer_->getColorAttachmentIndices().size();

  ASSERT_TRUE(framebuffer_->getColorAttachment(0) == nullptr);
  ASSERT_EQ(numOfAttachments, 0);

  // Restore framebuffer_ to its original state
  framebuffer_->updateDrawable(colorAttachment);
  numOfAttachments = framebuffer_->getColorAttachmentIndices().size();

  ASSERT_EQ(numOfAttachments, 1);
}

//
// Framebuffer Drawable Bind Count Test
//
// This test checks that when updateDrawable is called repeatedly with
// the same texture, the number of attachment stays the same.
//
TEST_F(FramebufferTest, DrawableBindCount) {
  // Currently the drawable is always bound to index 0
  auto colorAttachment = framebuffer_->getColorAttachment(0);
  auto numOfAttachments = framebuffer_->getColorAttachmentIndices().size();

  ASSERT_TRUE(colorAttachment != nullptr);
  ASSERT_EQ(numOfAttachments, 1);

  // Rebind the same texture, numAttachment should not change
  framebuffer_->updateDrawable(colorAttachment);
  framebuffer_->updateDrawable(colorAttachment);
  framebuffer_->updateDrawable(colorAttachment);
  numOfAttachments = framebuffer_->getColorAttachmentIndices().size();

  ASSERT_EQ(numOfAttachments, 1);

  // Create another texture
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 OFFSCREEN_RT_WIDTH,
                                                 OFFSCREEN_RT_HEIGHT,
                                                 TextureDesc::TextureUsageBits::Sampled |
                                                     TextureDesc::TextureUsageBits::Attachment);

  Result ret;
  auto newTex = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(newTex != nullptr);

  // Updating one texture for another, numAttachment should not change
  framebuffer_->updateDrawable(newTex);
  numOfAttachments = framebuffer_->getColorAttachmentIndices().size();

  ASSERT_EQ(numOfAttachments, 1);

  // Restore framebuffer_ to its original state
  framebuffer_->updateDrawable(colorAttachment);
  numOfAttachments = framebuffer_->getColorAttachmentIndices().size();

  ASSERT_EQ(numOfAttachments, 1);
}

//
// Framebuffer Update Drawable Test With Depth And Stencil Attachment
//
// This test checks that updateDrawable can be called to bind and unbind depth and stencil
// attachments.
//
TEST_F(FramebufferTest, UpdateDrawableWithDepthAndStencilTest) {
  // Currently the drawable is always bound to index 0
  auto colorAttachment = framebuffer_->getColorAttachment(0);
  auto depthAttachment = framebuffer_->getDepthAttachment();
  auto stencilAttachment = framebuffer_->getStencilAttachment();

  ASSERT_EQ(framebuffer_->getColorAttachment(0), colorAttachment);
  ASSERT_EQ(framebuffer_->getDepthAttachment(), depthAttachment);
  ASSERT_EQ(framebuffer_->getStencilAttachment(), stencilAttachment);

  framebuffer_->updateDrawable(nullptr);

  ASSERT_EQ(framebuffer_->getColorAttachment(0), nullptr);
  ASSERT_EQ(framebuffer_->getDepthAttachment(), depthAttachment);
  ASSERT_EQ(framebuffer_->getStencilAttachment(), stencilAttachment);

  framebuffer_->updateDrawable(colorAttachment);

  ASSERT_EQ(framebuffer_->getColorAttachment(0), colorAttachment);
  ASSERT_EQ(framebuffer_->getDepthAttachment(), depthAttachment);
  ASSERT_EQ(framebuffer_->getStencilAttachment(), stencilAttachment);

  framebuffer_->updateDrawable(SurfaceTextures{colorAttachment, nullptr});

  ASSERT_EQ(framebuffer_->getColorAttachment(0), colorAttachment);
  ASSERT_EQ(framebuffer_->getDepthAttachment(), nullptr);
  ASSERT_EQ(framebuffer_->getStencilAttachment(), nullptr);

  framebuffer_->updateDrawable(SurfaceTextures{colorAttachment, depthAttachment});

  ASSERT_EQ(framebuffer_->getColorAttachment(0), colorAttachment);
  ASSERT_EQ(framebuffer_->getDepthAttachment(), depthAttachment);
  ASSERT_EQ(framebuffer_->getStencilAttachment(), stencilAttachment);

  framebuffer_->updateDrawable(SurfaceTextures{nullptr, nullptr});

  ASSERT_EQ(framebuffer_->getColorAttachment(0), nullptr);
  ASSERT_EQ(framebuffer_->getDepthAttachment(), nullptr);
  ASSERT_EQ(framebuffer_->getStencilAttachment(), nullptr);

  framebuffer_->updateDrawable(SurfaceTextures{colorAttachment, depthAttachment});

  ASSERT_EQ(framebuffer_->getColorAttachment(0), colorAttachment);
  ASSERT_EQ(framebuffer_->getDepthAttachment(), depthAttachment);
  ASSERT_EQ(framebuffer_->getStencilAttachment(), stencilAttachment);
}

TEST_F(FramebufferTest, GetColorAttachmentTest) {
  const bool backendOpenGL = iglDev_->getBackendType() == igl::BackendType::OpenGL;

  if (backendOpenGL) {
#if IGL_BACKEND_OPENGL
    if (!iglDev_->getPlatformDevice<igl::opengl::PlatformDevice>()
             ->getContext()
             .deviceFeatures()
             .hasInternalFeature(opengl::InternalFeatures::PackRowLength)) {
      GTEST_SKIP() << "Framebuffer PackRowLength is not supported";
    }
#endif
  }

  // Create a texture to be used as color attachment
  const int textureWidth = 3;
  const int textureHeight = 2;
  const int channelCount = 4;
  const int channelSize = sizeof(uint8_t);
  const int textureElementPerRow = textureWidth * channelCount;
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 textureWidth,
                                                 textureHeight,
                                                 TextureDesc::TextureUsageBits::Sampled |
                                                     TextureDesc::TextureUsageBits::Attachment);

  Result ret;
  auto outputTexture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(outputTexture != nullptr);

  // Create framebuffer using the texture
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = outputTexture;
  framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(framebuffer_ != nullptr);

  //----------------
  // Create Pipeline
  //----------------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(cmdBuf_ != nullptr);

  renderPass_.colorAttachments[0].clearColor = {0.501f, 0.501f, 0.501f, 0.501f};

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------
  // Read back framebuffer
  //----------------------
  const int outputImageWidth = textureWidth + 2;
  const int outputImageHeight = textureHeight;
  const int outputElementPerRow = outputImageWidth * channelCount;
  auto pixels = std::vector<uint8_t>(static_cast<size_t>(outputElementPerRow * textureHeight), 0);

  const auto rangeDesc = igl::TextureRangeDesc::new2D(0, 0, textureWidth, textureHeight);
  framebuffer_->copyBytesColorAttachment(*cmdQueue_,
                                         0,
                                         pixels.data(),
                                         rangeDesc,
                                         static_cast<size_t>(outputElementPerRow * channelSize));

  for (int i = 0; i < outputImageHeight; i++) {
    for (int j = 0; j < outputElementPerRow; j++) {
      if (j < textureElementPerRow) {
        EXPECT_EQ(pixels[i * outputElementPerRow + j], 128);
      } else {
        EXPECT_EQ(pixels[i * outputElementPerRow + j], 0);
      }
    }
  }
}

} // namespace igl::tests
