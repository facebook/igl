/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/IGL.h>

#include "../data/ShaderData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/EnhancedShaderDebuggingStore.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanFeatures.h>
#endif

#if IGL_DEBUG

// Use a 1x1 Framebuffer for this test
constexpr size_t OFFSCREEN_RT_WIDTH = 1;
constexpr size_t OFFSCREEN_RT_HEIGHT = 1;

namespace igl::tests {
using namespace igl::vulkan;

class EnhancedShaderDebuggingStoreTest : public ::testing::Test {
 public:
  EnhancedShaderDebuggingStoreTest() = default;
  ~EnhancedShaderDebuggingStoreTest() override = default;

  // Set up common resources. This will create a device
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    device_ = static_cast<igl::vulkan::Device*>(iglDev_.get());

    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    // Create an offscreen texture to render to
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   OFFSCREEN_RT_WIDTH,
                                                   OFFSCREEN_RT_HEIGHT,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment);

    auto depthFormat = TextureFormat::S8_UInt_Z24_UNorm;

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
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  vulkan::Device* device_;
  RenderPipelineDesc renderPipelineDesc_;
  CommandBufferDesc cbDesc_ = {};
  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<ITexture> depthStencilTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> vb_, uv_, ib_;
  std::shared_ptr<ISamplerState> samp_;
};

TEST_F(EnhancedShaderDebuggingStoreTest, InitializeBuffer) {
  // Create an instance of the EnhancedShaderDebuggingStore
  EnhancedShaderDebuggingStore store;
  store.initialize(device_);

  if (!device_->getVulkanContext()
           .features()
           .VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress) {
    GTEST_SKIP() << "BufferDeviceAddress not supported";
  }

  // Verify that the buffer has been created
  EXPECT_NE(store.vertexBuffer(), nullptr);
}

TEST_F(EnhancedShaderDebuggingStoreTest, createFramebuffer) {
  // Create an instance of the EnhancedShaderDebuggingStore
  EnhancedShaderDebuggingStore store;
  store.initialize(device_);

  if (!device_->getVulkanContext()
           .features()
           .VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress) {
    GTEST_SKIP() << "BufferDeviceAddress not supported";
  }

  // Verify that the buffer has been created
  EXPECT_NE(store.vertexBuffer(), nullptr);

  auto fb = store.framebuffer(*device_, offscreenTexture_);
  ASSERT_TRUE(fb != nullptr);

  auto renderPass = store.renderPassDesc(framebuffer_);
  ASSERT_TRUE(renderPass.colorAttachments.size() == 1);

  store.pipeline(*device_, framebuffer_);
  // ASSERT_TRUE(pipeline != nullptr);
}

TEST_F(EnhancedShaderDebuggingStoreTest, DepthStencilState) {
  // Create an instance of the EnhancedShaderDebuggingStore
  EnhancedShaderDebuggingStore store;
  store.initialize(device_);

  if (!device_->getVulkanContext()
           .features()
           .VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress) {
    GTEST_SKIP() << "BufferDeviceAddress not supported";
  }

  // Verify that the depth stencil state has been created
  EXPECT_NE(store.depthStencilState(), nullptr);
}

TEST_F(EnhancedShaderDebuggingStoreTest, Pipeline) {
  // Create an instance of the EnhancedShaderDebuggingStore
  EnhancedShaderDebuggingStore store;
  store.initialize(device_);

  if (!device_->getVulkanContext()
           .features()
           .VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress) {
    GTEST_SKIP() << "BufferDeviceAddress not supported";
  }

  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  std::shared_ptr<igl::IDepthStencilState> depthStencilState;
  DepthStencilStateDesc desc;
  desc.isDepthWriteEnabled = true;

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(pipelineState != nullptr);

  depthStencilState = iglDev_->createDepthStencilState(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(depthStencilState != nullptr);

  CommandBufferDesc cbDesc_ = {};
  std::shared_ptr<ICommandBuffer> cmdBuf = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(cmdBuf != nullptr);

  renderPass_.colorAttachments[0].clearColor = {0.501f, 0.501f, 0.501f, 0.501f};

  renderPass_.depthAttachment.clearDepth = 0.501f;
  renderPass_.stencilAttachment.clearStencil = 128;

  auto cmds = cmdBuf->createRenderCommandEncoder(renderPass_, framebuffer_);
  EXPECT_NE(store.pipeline(*device_, framebuffer_), nullptr);

  cmdQueue_->submit(*cmdBuf);

  cmdBuf->waitUntilCompleted();
}

TEST_F(EnhancedShaderDebuggingStoreTest, InstallBufferBarrier) {
  // Create an instance of the EnhancedShaderDebuggingStore
  EnhancedShaderDebuggingStore store;
  store.initialize(device_);

  if (!device_->getVulkanContext()
           .features()
           .VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress) {
    GTEST_SKIP() << "BufferDeviceAddress not supported";
  }

  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  std::shared_ptr<igl::IDepthStencilState> depthStencilState;
  DepthStencilStateDesc desc;
  desc.isDepthWriteEnabled = true;

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(pipelineState != nullptr);

  depthStencilState = iglDev_->createDepthStencilState(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(depthStencilState != nullptr);

  CommandBufferDesc cbDesc_ = {};
  std::shared_ptr<ICommandBuffer> cmdBuf = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(cmdBuf != nullptr);

  renderPass_.colorAttachments[0].clearColor = {0.501f, 0.501f, 0.501f, 0.501f};

  renderPass_.depthAttachment.clearDepth = 0.501f;
  renderPass_.stencilAttachment.clearStencil = 128;

  auto cmds = cmdBuf->createRenderCommandEncoder(renderPass_, framebuffer_);
  // Verify that the buffer barrier has been installed
  store.installBufferBarrier(*cmdBuf);
  EXPECT_NE(store.pipeline(*device_, framebuffer_), nullptr);

  cmdQueue_->submit(*cmdBuf);

  cmdBuf->waitUntilCompleted();
}
} // namespace igl::tests
#endif
