/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Texture.h"

namespace igl::tests {

TEST_F(TextureTest, Upload) {
  Result ret;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 kOffscreenTexWidth,
                                                 kOffscreenTexHeight,
                                                 TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

  inputTexture_->upload(rangeDesc, data::texture::TEX_RGBA_2x2);

  //----------------
  // Validate data
  //----------------
  util::validateUploadedTexture(
      *iglDev_, *cmdQueue_, inputTexture_, data::texture::TEX_RGBA_2x2, "Passthrough");
}

//
// Texture Passthrough Test
//
// This test uses a simple shader to copy the input texture to a same
// sized output texture (offscreenTexture_)
//
TEST_F(TextureTest, Passthrough) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOffscreenTexWidth,
                                           kOffscreenTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

  inputTexture_->upload(rangeDesc, data::texture::TEX_RGBA_2x2);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  //-------
  // Render
  //-------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(PrimitiveType::Triangle, 6);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------
  // Validate output
  //----------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *framebuffer_, data::texture::TEX_RGBA_2x2, "Passthrough");
}

//
// This test uses a simple shader to copy the input texture with a
// texture to a same sized output texture (offscreenTexture_)
// The difference between this test and PassthroughTexture is that
// a section of the original input texture is updated. This is meant
// to exercise the sub-texture upload path.
//
TEST_F(TextureTest, PassthroughSubTexture) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //------------------------------------------------------
  // Create input texture and sub-texture, and upload data
  //------------------------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOffscreenTexWidth,
                                           kOffscreenTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

  inputTexture_->upload(rangeDesc, data::texture::TEX_RGBA_2x2);

  // Upload right lower corner as a single-pixel sub-texture.
  auto singlePixelDesc =
      TextureRangeDesc::new2D(kOffscreenTexWidth - 1, kOffscreenTexHeight - 1, 1, 1);
  int32_t singlePixelColor = 0x44332211;

  inputTexture_->upload(singlePixelDesc, &singlePixelColor);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  //-------
  // Render
  //-------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(PrimitiveType::Triangle, 6);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------
  // Validate output
  //----------------
  util::validateFramebufferTexture(*iglDev_,
                                   *cmdQueue_,
                                   *framebuffer_,
                                   data::texture::TEX_RGBA_2x2_MODIFIED,
                                   "PassthroughSubTexture");
}

//
// Framebuffer to Texture Copy Test
//
// This test will exercise the copy functionality via the following steps:
//   1. clear FB to (0.5, 0.5, 0.5, 0.5)
//   2. Copy content to a texture
//   3. clear FB to (0, 0, 0, 0) and verify it is cleared
//   4. Copy texture content to FB
//   5. Verify that the FB is back to (0.5, 0.5, 0.5, 0.5)
//
TEST_F(TextureTest, FBCopy) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;
  std::shared_ptr<ITexture> dstTexture;

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

  //--------------------------------
  // Create copy destination texture
  //--------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOffscreenTexWidth,
                                           kOffscreenTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  texDesc.debugName = "Texture: TextureTest::FBCopy::dstTexture";
  dstTexture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(dstTexture != nullptr);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(pipelineState != nullptr);

  //---------------------------------
  // Clear FB to {0.5, 0.5, 0.5, 0.5}
  //---------------------------------
  renderPass_.colorAttachments[0].clearColor = {0.501f, 0.501f, 0.501f, 0.501f};

  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);
  cmds->bindRenderPipelineState(pipelineState);

  // draw 0 indices here just to clear the FB
  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(PrimitiveType::Triangle, 0);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------------------------------------------------------
  // Validate framebuffer texture
  //----------------------------------------------------------------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *framebuffer_, data::texture::TEX_RGBA_GRAY_2x2, "After Initial Clear");

  //------------------------
  // Copy content to texture
  //------------------------
  framebuffer_->copyTextureColorAttachment(*cmdQueue_, 0, dstTexture, rangeDesc);

  //-------------------------
  // Clear FB to {0, 0, 0, 0}
  //-------------------------
  renderPass_.colorAttachments[0].clearColor = {0, 0, 0, 0};

  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());

  cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);
  cmds->bindRenderPipelineState(pipelineState);
  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(PrimitiveType::Triangle, 0);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //-----------------------------------
  // Validate framebuffer texture again
  //-----------------------------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *framebuffer_, data::texture::TEX_RGBA_CLEAR_2x2, "After Second Clear");

  //---------------------------------------------
  // Copy dstTexture to FB so we can read it back
  //---------------------------------------------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());

  cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  // Using dstTexture as input here
  cmds->bindTexture(textureUnit_, BindTarget::kFragment, dstTexture.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(PrimitiveType::Triangle, 6);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //------------------------------------------------------
  // Read back framebuffer. Should be {0.5, 0.5, 0.5, 0.5}
  //------------------------------------------------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *framebuffer_, data::texture::TEX_RGBA_GRAY_2x2, "After Copy");
}

constexpr uint32_t kAlignedPixelsWidth = 3u;
constexpr uint32_t kAlignedPixelsHeight = 2u;

constexpr std::array<uint32_t, 6> kPixelsAligned12 = {1u, 2u, 3u, 4u, 5u, 6u};

// clang-format off
constexpr std::array<uint8_t, 28> kPixelsAligned14 = {
    1, 0, 0, 0,
    2, 0, 0, 0,
    3, 0, 0, 0,
    0, 0, // Expected to be skipped
    4, 0, 0, 0,
    5, 0, 0, 0,
    6, 0, 0, 0,
    0, 0, // Expected to be skipped
};
// clang-format on
constexpr std::array<uint32_t, 8> kPixelsAligned16 = {1u,
                                                      2u,
                                                      3u,
                                                      0x00000000u, // Expected to be skipped
                                                      4u,
                                                      5u,
                                                      6u,
                                                      0x00000000u}; // Expected to be skipped
constexpr std::array<uint32_t, 10> kPixelsAligned20 = {1u,
                                                       2u,
                                                       3u,
                                                       0x00000000u, // Expected to be skipped
                                                       0x00000000u, // Expected to be skipped
                                                       4u,
                                                       5u,
                                                       6u,
                                                       0x00000000u, // Expected to be skipped
                                                       0x00000000u}; // Expected to be skipped

constexpr std::array<std::pair<const void*, uint32_t>, 4> kPixelAlignments = {
    // 12 byte row will triggers 4 byte alignment.
    // No padding required since the width equals number of input pixels per row.
    std::make_pair(kPixelsAligned12.data(), kAlignedPixelsWidth * 4u),
    // 14 byte row will trigger 2 byte alignment since texture width is set to 3.
    // Padding of 0.5 pixels used per row of width 3.
    std::make_pair(kPixelsAligned14.data(), kAlignedPixelsWidth * 4u + 2u),
    // 16 byte row will trigger 8 byte alignment since texture width is set to 3.
    // Padding of 1 pixel used per row of width 3.
    std::make_pair(kPixelsAligned16.data(), (kAlignedPixelsWidth + 1u) * 4u),
    // 20 byte row is neither 8, 4, 2, nor 1 byte aligned.
    // Padding of 2 pixels used per row of width 3.
    std::make_pair(kPixelsAligned20.data(), (kAlignedPixelsWidth + 2u) * 4u),
};

//
// Test ITexture::repackData
//
TEST_F(TextureTest, RepackData) {
  const auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
  const auto range = TextureRangeDesc::new2D(0, 0, kAlignedPixelsWidth, kAlignedPixelsHeight);

  for (const auto& [data, bytesPerRow] : kPixelAlignments) {
    const size_t alignedSize =
        static_cast<size_t>(kAlignedPixelsWidth) * static_cast<size_t>(kAlignedPixelsHeight);
    const size_t unalignedSize =
        static_cast<size_t>(kAlignedPixelsHeight) * static_cast<size_t>(bytesPerRow);
    {
      //------------------
      // Test packing data
      //------------------

      std::vector<uint32_t> packedData(alignedSize);
      ITexture::repackData(properties,
                           range,
                           static_cast<const uint8_t*>(data),
                           bytesPerRow,
                           reinterpret_cast<uint8_t*>(packedData.data()),
                           0);

      for (size_t i = 0; i < packedData.size(); ++i) {
        EXPECT_EQ(packedData[i], kPixelsAligned12[i]);
      }
    }

    {
      //-----------------------------
      // Test packing + flipping data
      //-----------------------------

      std::vector<uint32_t> packedFlippedData(alignedSize);
      ITexture::repackData(properties,
                           range,
                           static_cast<const uint8_t*>(data),
                           bytesPerRow,
                           reinterpret_cast<uint8_t*>(packedFlippedData.data()),
                           0,
                           true);

      for (size_t i = 0; i < kAlignedPixelsWidth; ++i) {
        EXPECT_EQ(packedFlippedData[i], kPixelsAligned12[i + kAlignedPixelsWidth]);
        EXPECT_EQ(packedFlippedData[i + kAlignedPixelsWidth], kPixelsAligned12[i]);
      }
    }

    {
      //--------------------
      // Test unpacking data
      //--------------------

      std::vector<uint8_t> unpackedData(unalignedSize);
      ITexture::repackData(properties,
                           range,
                           reinterpret_cast<const uint8_t*>(kPixelsAligned12.data()),
                           0,
                           unpackedData.data(),
                           bytesPerRow);

      for (size_t i = 0; i < unpackedData.size(); ++i) {
        EXPECT_EQ(unpackedData[i], reinterpret_cast<const uint8_t*>(data)[i]);
      }
    }

    {
      //-------------------------------
      // Test unpacking + flipping data
      //-------------------------------

      std::vector<uint8_t> unpackedFlippedData(unalignedSize);
      ITexture::repackData(properties,
                           range,
                           reinterpret_cast<const uint8_t*>(kPixelsAligned12.data()),
                           0,
                           unpackedFlippedData.data(),
                           bytesPerRow,
                           true);

      const auto width = unpackedFlippedData.size() / 2;
      for (size_t i = 0; i < width; ++i) {
        EXPECT_EQ(unpackedFlippedData[i + width], reinterpret_cast<const uint8_t*>(data)[i]);
        EXPECT_EQ(unpackedFlippedData[i], reinterpret_cast<const uint8_t*>(data)[i + width]);
      }
    }
  }
}

//
// Pixel upload alignment test
//
// In openGL, when writing to a gpu texture from cpu memory the cpu memory pixel rows can be
// packed a couple of different ways 1, 2, 4 or 8 byte aligned. This test ensures bytesPerRow gets
// converted to the correct byte alignment in openGL and works as expected in metal
//
// If a row has 3 RGBA pixels but is 8 byte aligned the row will be 16 bytes with the last 4 bytes
// being ignored. If it was instead 1, 2 or 4 byte aligned the row would be 12 bytes as 12 is
// divisible by a single pixels byte size.
//
// Expected output: Pixels read out are correct even when different bytes per pixel are used
// during upload.
//
// Note: This test only covers 4 and 8 byte alignment because copyBytesColorAttachment does not
// support reading non 4 byte formats
//
TEST_F(TextureTest, UploadAlignment) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create new frame buffer with a width and height that can cause different alignments
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kAlignedPixelsWidth,
                                           kAlignedPixelsHeight,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  auto customOffscreenTexture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(customOffscreenTexture != nullptr);

  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = customOffscreenTexture;
  auto customFramebuffer = iglDev_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(customFramebuffer != nullptr);

  for (const auto& [data, bytesPerRow] : kPixelAlignments) {
    //-------------------------------------
    // Create input texture and upload data
    //-------------------------------------
    texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                 kAlignedPixelsWidth,
                                 kAlignedPixelsHeight,
                                 TextureDesc::TextureUsageBits::Sampled);
    inputTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(inputTexture_ != nullptr);

    const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kAlignedPixelsWidth, kAlignedPixelsHeight);

    inputTexture_->upload(rangeDesc, data, static_cast<size_t>(bytesPerRow));

    //----------------
    // Create Pipeline
    //----------------
    pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(pipelineState != nullptr);

    //-------
    // Render
    //-------
    cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf_ != nullptr);

    auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, customFramebuffer);
    cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
    cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

    cmds->bindRenderPipelineState(pipelineState);

    cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
    cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

    cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
    cmds->drawIndexed(PrimitiveType::Triangle, 6);

    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();

    //----------------
    // Validate output
    //----------------
    const std::string alignmentStr = "UploadAlignment: " + std::to_string(bytesPerRow);
    util::validateFramebufferTexture(
        *iglDev_, *cmdQueue_, *customFramebuffer, kPixelsAligned12.data(), alignmentStr.c_str());
  }
}

//
// Texture Resize Test
//
// This test uses a simple shader to copy the input texture to a different
// sized output texture (offscreenTexture_)
//
TEST_F(TextureTest, Resize) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  constexpr uint32_t kInputTexWidth = 10u;
  constexpr uint32_t kInputTexHeight = 40u;
  constexpr uint32_t kOutputTexWidth = 5u;
  constexpr uint32_t kOutputTexHeight = 5u;
  constexpr size_t kTextureSize =
      static_cast<size_t>(kInputTexWidth) * static_cast<size_t>(kInputTexHeight);

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kInputTexWidth,
                                           kInputTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  auto rangeDesc = TextureRangeDesc::new2D(0, 0, kInputTexWidth, kInputTexHeight);

  // Allocate input texture and set color to 0x80808080
  std::vector<uint32_t> inputTexData(kTextureSize, 0x80808080);
  inputTexture_->upload(rangeDesc, inputTexData.data());

  //------------------------------------------------------------------------
  // Create a different sized output texture, and attach it to a framebuffer
  //------------------------------------------------------------------------
  texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                               kOutputTexWidth,
                               kOutputTexHeight,
                               TextureDesc::TextureUsageBits::Sampled |
                                   TextureDesc::TextureUsageBits::Attachment);

  auto outputTex = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(outputTex != nullptr);

  // Create framebuffer using the output texture
  FramebufferDesc framebufferDesc;

  framebufferDesc.colorAttachments[0].texture = outputTex;
  auto fb = iglDev_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(fb != nullptr);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  //-------
  // Render
  //-------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, fb);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(PrimitiveType::Triangle, 6);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------
  // Validate output
  //----------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *fb, data::texture::TEX_RGBA_GRAY_5x5, "Resize");
}

//
// Texture Validate Range 2D
//
// This test validates some of the logic in validateRange for 2D textures.
//
TEST_F(TextureTest, ValidateRange2D) {
  Result ret;
  auto texDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 8, 8, TextureDesc::TextureUsageBits::Sampled);
  auto tex = iglDev_->createTexture(texDesc, &ret);

  ret = tex->validateRange(TextureRangeDesc::new2D(0, 0, 8, 8));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2D(4, 4, 4, 4));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2D(0, 0, 4, 4, 1));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2D(0, 0, 12, 12));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2D(0, 0, 0, 0));
  EXPECT_FALSE(ret.isOk());
}

//
// Texture Validate Range Cube
//
// This test validates some of the logic in validateRange for Cube textures.
//
TEST_F(TextureTest, ValidateRangeCube) {
  Result ret;
  auto texDesc = TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, 8, 8, TextureDesc::TextureUsageBits::Sampled);
  auto tex = iglDev_->createTexture(texDesc, &ret);

  ret = tex->validateRange(TextureRangeDesc::newCube(0, 0, 8, 8));
  EXPECT_TRUE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 8, 8, 1));
  EXPECT_TRUE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 8, 8, TextureCubeFace::NegX));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::newCube(4, 4, 4, 4));
  EXPECT_TRUE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(4, 4, 4, 4, 1));
  EXPECT_TRUE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(4, 4, 4, 4, TextureCubeFace::NegX));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::newCube(0, 0, 4, 4, 1));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 4, 4, 1, 1));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 4, 4, TextureCubeFace::NegX, 1));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::newCube(0, 0, 12, 12));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 12, 12, 1));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 12, 12, TextureCubeFace::NegX));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::newCube(0, 0, 0, 0));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 0, 0, 1));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 0, 0, TextureCubeFace::NegX));
  EXPECT_FALSE(ret.isOk());
}

//
// Texture Validate Range 3D
//
// This test validates some of the logic in validateRange for 3D textures.
//
TEST_F(TextureTest, ValidateRange3D) {
  if (!iglDev_->hasFeature(DeviceFeatures::Texture3D)) {
    GTEST_SKIP() << "3D textures not supported. Skipping.";
  }

  Result ret;
  auto texDesc = TextureDesc::new3D(
      TextureFormat::RGBA_UNorm8, 8, 8, 8, TextureDesc::TextureUsageBits::Sampled);
  auto tex = iglDev_->createTexture(texDesc, &ret);

  ret = tex->validateRange(TextureRangeDesc::new3D(0, 0, 0, 8, 8, 8));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new3D(4, 4, 4, 4, 4, 4));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new3D(0, 0, 0, 4, 4, 4, 1));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new3D(0, 0, 0, 12, 12, 12));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new3D(0, 0, 0, 0, 0, 0));
  EXPECT_FALSE(ret.isOk());
}

} // namespace igl::tests
