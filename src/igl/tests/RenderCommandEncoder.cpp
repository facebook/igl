/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

#include "data/ShaderData.h"
#include "data/TextureData.h"
#include "data/VertexIndexData.h"
#include "util/Common.h"

#include <igl/Buffer.h>
#include <igl/DepthStencilState.h>
#include <igl/IGL.h>
#include <igl/NameHandle.h>
#include <igl/RenderPipelineState.h>

#define OFFSCREEN_RT_WIDTH 4
#define OFFSCREEN_RT_HEIGHT 4

#define OFFSCREEN_TEX_WIDTH 4
#define OFFSCREEN_TEX_HEIGHT 4

namespace igl::tests {

const auto quarterPixel = (float)(0.5 / OFFSCREEN_RT_WIDTH);
const float backgroundColor = 0.501f;
const uint32_t backgroundColorHex = 0x80808080;

/**
 * @brief RenderCommandEncoderTest is a test fixture for all the tests in this file.
 * It takes care of common initialization and allocating of common resources.
 */
class RenderCommandEncoderTest : public ::testing::Test {
 private:
 public:
  RenderCommandEncoderTest() = default;
  ~RenderCommandEncoderTest() override = default;

  /**
   * @brief This function sets up a render pass and a render pipeline descriptor
   * so it is ready to render a simple quad with an input texture to an
   * offscreen texture.
   */
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    // Create an offscreen texture to render to
    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
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
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_TRUE(offscreenTexture_ != nullptr);

    depthStencilTexture_ = iglDev_->createTexture(depthTexDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_TRUE(depthStencilTexture_ != nullptr);

    // Create framebuffer using the offscreen texture
    FramebufferDesc framebufferDesc;

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
    renderPass_.colorAttachments[0].clearColor = {
        backgroundColor, backgroundColor, backgroundColor, backgroundColor};

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

    // Initialize sampler state
    const SamplerStateDesc samplerDesc;
    samp_ = iglDev_->createSamplerState(samplerDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(samp_ != nullptr);

    // Initialize Render Pipeline Descriptor, but leave the creation
    // to the individual tests in case further customization is required
    RenderPipelineDesc renderPipelineDesc_;
    renderPipelineDesc_.vertexInputState = vertexInputState_;
    renderPipelineDesc_.shaderStages = shaderStages_;
    renderPipelineDesc_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc_.targetDesc.colorAttachments[0].textureFormat =
        offscreenTexture_->getFormat();
    renderPipelineDesc_.targetDesc.depthAttachmentFormat = depthStencilTexture_->getFormat();
    renderPipelineDesc_.targetDesc.stencilAttachmentFormat = depthStencilTexture_->getFormat();
    renderPipelineDesc_.fragmentUnitSamplerMap[textureUnit_] =
        IGL_NAMEHANDLE(data::shader::simpleSampler);
    renderPipelineDesc_.cullMode = igl::CullMode::Disabled;

    texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                 OFFSCREEN_TEX_WIDTH,
                                 OFFSCREEN_TEX_HEIGHT,
                                 TextureDesc::TextureUsageBits::Sampled);
    texture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(texture_ != nullptr);
    const auto rangeDesc = TextureRangeDesc::new2D(0, 0, OFFSCREEN_TEX_WIDTH, OFFSCREEN_TEX_HEIGHT);
    texture_->upload(rangeDesc, data::texture::TEX_RGBA_GRAY_4x4);

    auto createPipeline =
        [&renderPipelineDesc_, &ret, this](
            igl::PrimitiveType topology) -> std::shared_ptr<igl::IRenderPipelineState> {
      renderPipelineDesc_.topology = topology;
      return iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
    };
    renderPipelineState_Point_ = createPipeline(PrimitiveType::Point);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(renderPipelineState_Point_ != nullptr);
    renderPipelineState_Line_ = createPipeline(PrimitiveType::Line);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(renderPipelineState_Line_ != nullptr);
    renderPipelineState_LineStrip_ = createPipeline(PrimitiveType::LineStrip);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(renderPipelineState_LineStrip_ != nullptr);
    renderPipelineState_Triangle_ = createPipeline(PrimitiveType::Triangle);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(renderPipelineState_Triangle_ != nullptr);
    renderPipelineState_TriangleStrip_ = createPipeline(PrimitiveType::TriangleStrip);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(renderPipelineState_TriangleStrip_ != nullptr);

    depthStencilState_ = iglDev_->createDepthStencilState({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(depthStencilState_ != nullptr);
  }

  void encodeAndSubmit(
      const std::function<void(const std::unique_ptr<igl::IRenderCommandEncoder>&)>& func) {
    Result ret;

    auto cmdBuffer = cmdQueue_->createCommandBuffer({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(cmdBuffer != nullptr);

    auto encoder = cmdBuffer->createRenderCommandEncoder(renderPass_, framebuffer_);
    encoder->bindTexture(textureUnit_, BindTarget::kFragment, texture_.get());
    encoder->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

    encoder->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
    encoder->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

    encoder->bindDepthStencilState(depthStencilState_);

    const igl::Viewport viewport = {
        0.0f, 0.0f, (float)OFFSCREEN_RT_WIDTH, (float)OFFSCREEN_RT_HEIGHT, 0.0f, +1.0f};
    const igl::ScissorRect scissor = {
        0, 0, (uint32_t)OFFSCREEN_RT_WIDTH, (uint32_t)OFFSCREEN_RT_HEIGHT};
    encoder->bindViewport(viewport);
    encoder->bindScissorRect(scissor);

    func(encoder);

    encoder->endEncoding();

    cmdQueue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
  }

  void verifyFrameBuffer(const std::vector<uint32_t>& expectedPixels) {
    auto pixels =
        std::vector<uint32_t>(static_cast<size_t>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH));
    framebuffer_->copyBytesColorAttachment(
        *cmdQueue_,
        0,
        pixels.data(),
        TextureRangeDesc::new2D(0, 0, OFFSCREEN_RT_WIDTH, OFFSCREEN_RT_HEIGHT));

#if IGL_DEBUG
    debugLog(pixels);
#endif // IGL_DEBUG

    for (int i = 0; i < OFFSCREEN_RT_HEIGHT; i++) {
      for (int j = 0; j < OFFSCREEN_RT_WIDTH; j++) {
        ASSERT_EQ(pixels[(OFFSCREEN_RT_HEIGHT - i - 1) * OFFSCREEN_RT_WIDTH + j],
                  expectedPixels[i * OFFSCREEN_RT_WIDTH + j]);
      }
    }
  }

  void verifyFrameBuffer(const std::function<void(const std::vector<uint32_t>&)>& func) {
    auto pixels =
        std::vector<uint32_t>(static_cast<size_t>(OFFSCREEN_RT_WIDTH * OFFSCREEN_RT_WIDTH));
    framebuffer_->copyBytesColorAttachment(
        *cmdQueue_,
        0,
        pixels.data(),
        TextureRangeDesc::new2D(0, 0, OFFSCREEN_RT_WIDTH, OFFSCREEN_RT_HEIGHT));

#if IGL_DEBUG
    debugLog(pixels);
#endif // IGL_DEBUG

    func(pixels);
  }

  void debugLog(const std::vector<uint32_t>& pixels) {
    IGL_DEBUG_LOG("\nFrameBuffer begins.\n");
    IGL_DEBUG_LOG("%s\n", ::testing::UnitTest::GetInstance()->current_test_info()->name());
    for (int i = OFFSCREEN_RT_HEIGHT - 1; i >= 0; i--) {
      for (int j = 0; j < OFFSCREEN_RT_WIDTH; j++) {
        IGL_DEBUG_LOG("%x, ", pixels[i * OFFSCREEN_RT_WIDTH + j]);
      }
      IGL_DEBUG_LOG("\n");
    }
    IGL_DEBUG_LOG("\nFrameBuffer ends.\n");
  }

  void initializeBuffers(const std::vector<float>& verts, const std::vector<float>& uvs) {
    BufferDesc bufDesc;

    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = verts.data();
    bufDesc.length = sizeof(float) * verts.size();

    Result ret;
    vb_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(vb_ != nullptr);

    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = uvs.data();
    bufDesc.length = sizeof(float) * uvs.size();

    uv_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(uv_ != nullptr);
  }

  void TearDown() override {}

 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<ITexture> depthStencilTexture_;

  std::shared_ptr<IFramebuffer> framebuffer_;

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> vb_, uv_;

  std::shared_ptr<ISamplerState> samp_;

  std::shared_ptr<ITexture> texture_;

  std::shared_ptr<IRenderPipelineState> renderPipelineState_Point_;
  std::shared_ptr<IRenderPipelineState> renderPipelineState_Line_;
  std::shared_ptr<IRenderPipelineState> renderPipelineState_LineStrip_;
  std::shared_ptr<IRenderPipelineState> renderPipelineState_Triangle_;
  std::shared_ptr<IRenderPipelineState> renderPipelineState_TriangleStrip_;
  std::shared_ptr<IDepthStencilState> depthStencilState_;

  const std::string backend_ = IGL_BACKEND_TYPE;

  size_t textureUnit_ = 0;
};

TEST_F(RenderCommandEncoderTest, shouldDrawAPoint) {
  initializeBuffers(
      // clang-format off
      { quarterPixel, quarterPixel, 0.0f, 1.0f },
      { 0.5, 0.5 } // clang-format on
  );

  encodeAndSubmit([this](const std::unique_ptr<igl::IRenderCommandEncoder>& encoder) {
    encoder->bindRenderPipelineState(renderPipelineState_Point_);
    encoder->draw(1);
  });

  auto grayColor = data::texture::TEX_RGBA_GRAY_4x4[0];
  // clang-format off
  std::vector<uint32_t> const expectedPixels {
    backgroundColorHex, backgroundColorHex, backgroundColorHex, backgroundColorHex,
    backgroundColorHex, backgroundColorHex, grayColor,          backgroundColorHex,
    backgroundColorHex, backgroundColorHex, backgroundColorHex, backgroundColorHex,
    backgroundColorHex, backgroundColorHex, backgroundColorHex, backgroundColorHex,
  };
  // clang-format on

  verifyFrameBuffer(expectedPixels);
}

TEST_F(RenderCommandEncoderTest, shouldDrawALine) {
  initializeBuffers(
      // clang-format off
      {
        -1.0f - quarterPixel, -1.0f + quarterPixel, 0.0f, 1.0f,
         1.0f + quarterPixel, -1.0f + quarterPixel, 0.0f, 1.0f,
      },
      {
        0.0f, 0.0f,
        1.0f, 0.0f,
      } // clang-format on
  );

  encodeAndSubmit([this](const std::unique_ptr<igl::IRenderCommandEncoder>& encoder) {
    encoder->bindRenderPipelineState(renderPipelineState_Line_);
    encoder->draw(2);
  });

  auto grayColor = data::texture::TEX_RGBA_GRAY_4x4[0];
  // clang-format off
  std::vector<uint32_t> const expectedPixels {
    backgroundColorHex, backgroundColorHex, backgroundColorHex, backgroundColorHex,
    backgroundColorHex, backgroundColorHex, backgroundColorHex, backgroundColorHex,
    backgroundColorHex, backgroundColorHex, backgroundColorHex, backgroundColorHex,
    grayColor,          grayColor,          grayColor,          grayColor,
  };
  // clang-format on

  verifyFrameBuffer(expectedPixels);
}

TEST_F(RenderCommandEncoderTest, shouldDrawLineStrip) {
  initializeBuffers(
      // clang-format off
      {
        -1.0f - quarterPixel, -1.0f + quarterPixel, 0.0f, 1.0f,
         1.0f + quarterPixel, -1.0f + quarterPixel, 0.0f, 1.0f,
         1.0f - quarterPixel, -1.0f - quarterPixel, 0.0f, 1.0f,
         1.0f - quarterPixel,  1.0f + quarterPixel, 0.0f, 1.0f,
      },
      {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
      } // clang-format on
  );

  encodeAndSubmit([this](const std::unique_ptr<igl::IRenderCommandEncoder>& encoder) {
    encoder->bindRenderPipelineState(renderPipelineState_LineStrip_);
    encoder->draw(4);
  });

  auto grayColor = data::texture::TEX_RGBA_GRAY_4x4[0];
  // clang-format off
  std::vector<uint32_t> const expectedPixels {
    backgroundColorHex, backgroundColorHex, backgroundColorHex, grayColor,
    backgroundColorHex, backgroundColorHex, backgroundColorHex, grayColor,
    backgroundColorHex, backgroundColorHex, backgroundColorHex, grayColor,
    grayColor,          grayColor,          grayColor,          grayColor,
  };
  // clang-format on

  verifyFrameBuffer(expectedPixels);
}

TEST_F(RenderCommandEncoderTest, shouldDrawATriangle) {
  initializeBuffers(
      // clang-format off
      {
        -1.0f - quarterPixel, -1.0f,                0.0f, 1.0f,
         1.0f,                -1.0f,                0.0f, 1.0f,
         1.0f,                 1.0f + quarterPixel, 0.0f, 1.0f,
      },
      {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
      } // clang-format on
  );

  encodeAndSubmit([this](const std::unique_ptr<igl::IRenderCommandEncoder>& encoder) {
    encoder->bindRenderPipelineState(renderPipelineState_Triangle_);
    encoder->draw(3);
  });

  auto grayColor = data::texture::TEX_RGBA_GRAY_4x4[0];
  // clang-format off
  std::vector<uint32_t> const expectedPixels {
    backgroundColorHex, backgroundColorHex, backgroundColorHex, grayColor,
    backgroundColorHex, backgroundColorHex, grayColor,          grayColor,
    backgroundColorHex, grayColor,          grayColor,          grayColor,
    grayColor,          grayColor,          grayColor,          grayColor,
  };

  verifyFrameBuffer(expectedPixels);
}

TEST_F(RenderCommandEncoderTest, shouldDrawTriangleStrip) {
  initializeBuffers(
      // clang-format off
      {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
      },
      {
        0.0, 1.0,
        0.0, 0.0,
        1.0, 1.0,
        1.0, 0.0,
      } // clang-format on
  );

  encodeAndSubmit([this](const std::unique_ptr<igl::IRenderCommandEncoder>& encoder) {
    encoder->bindRenderPipelineState(renderPipelineState_TriangleStrip_);
    encoder->draw(4);
  });

  verifyFrameBuffer([](const std::vector<uint32_t>& pixels) {
    for (const auto& pixel : pixels) {
      ASSERT_EQ(pixel, data::texture::TEX_RGBA_GRAY_4x4[0]);
    }
  });
}

TEST_F(RenderCommandEncoderTest, shouldNotDraw) {
  initializeBuffers(
      // clang-format off
      {
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
      },
      {
        0.0, 1.0,
        1.0, 1.0,
        0.0, 0.0,
        1.0, 1.0,
        1.0, 0.0,
        0.0, 0.0,
      } // clang-format on
  );

  encodeAndSubmit([this](const std::unique_ptr<igl::IRenderCommandEncoder>& encoder) {
    encoder->bindRenderPipelineState(renderPipelineState_Point_);
    encoder->draw(0);
    encoder->bindRenderPipelineState(renderPipelineState_Line_);
    encoder->draw(0);
    encoder->bindRenderPipelineState(renderPipelineState_LineStrip_);
    encoder->draw(0);
    encoder->bindRenderPipelineState(renderPipelineState_Triangle_);
    encoder->draw(0);
    encoder->bindRenderPipelineState(renderPipelineState_TriangleStrip_);
    encoder->draw(0);
  });

  verifyFrameBuffer([](const std::vector<uint32_t>& pixels) {
    for (const auto& pixel : pixels) {
      ASSERT_EQ(pixel, backgroundColorHex);
    }
  });
}

} // namespace igl::tests
