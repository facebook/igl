/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/RenderCommandEncoder.h>

#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl::tests {
// Use a 1x1 Framebuffer for this test
#define OFFSCREEN_RT_WIDTH 1
#define OFFSCREEN_RT_HEIGHT 1

//
// RenderCommandEncoderMTLTest
//
// This test covers igl::metal::RenderCommandEncoder.
// Most of the testing revolves confirming successful instantiation
// and creation of encoders.
//

class RenderCommandEncoderMTLTest : public ::testing::Test {
 public:
  RenderCommandEncoderMTLTest() = default;
  ~RenderCommandEncoderMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(device_, commandQueue_);

    ASSERT_NE(device_, nullptr);
    ASSERT_NE(commandQueue_, nullptr);
    // Test instantiation and constructor of CommandBuffer.
    Result res;
    const CommandBufferDesc desc;
    commandBuffer_ = commandQueue_->createCommandBuffer(desc, &res);
    ASSERT_TRUE(res.isOk());
  }
  void TearDown() override {}

 public:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> commandQueue_;
  std::shared_ptr<ICommandBuffer> commandBuffer_;

  const std::string label_;
};

//
// RenderCommandEncoderCreation
//
// Test successful creation of MTLRenderCommandEncoder, adding attachment for depth and stencil
//
TEST_F(RenderCommandEncoderMTLTest, CreateRenderCommandEncoderAll) {
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 OFFSCREEN_RT_WIDTH,
                                                 OFFSCREEN_RT_HEIGHT,
                                                 TextureDesc::TextureUsageBits::Sampled |
                                                     TextureDesc::TextureUsageBits::Attachment);
  Result ret;
  const std::shared_ptr<ITexture> offscreenTexture = device_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(offscreenTexture != nullptr);
  // Create framebuffer using the offscreen texture
  FramebufferDesc framebufferDesc;

  framebufferDesc.depthAttachment.texture = offscreenTexture;
  framebufferDesc.stencilAttachment.texture = offscreenTexture;
  auto framebuffer = device_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(framebuffer != nullptr);

  const RenderPassDesc rpDesc;
  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer);
  ASSERT_TRUE(encoder != nullptr);

  encoder->pushDebugGroupLabel(label_.c_str());

  encoder->insertDebugEventLabel(label_.c_str());

  encoder->popDebugGroupLabel();

  // MTLCommandEncoder must always call endEncoding before being released.
  encoder->endEncoding();
}

TEST_F(RenderCommandEncoderMTLTest, ToMTLPrimitiveType) {
  const std::vector<std::pair<PrimitiveType, MTLPrimitiveType>> inputAndExpectedList = {
      std::make_pair(PrimitiveType::Line, MTLPrimitiveTypeLine),
      std::make_pair(PrimitiveType::LineStrip, MTLPrimitiveTypeLineStrip),
      std::make_pair(PrimitiveType::Triangle, MTLPrimitiveTypeTriangle),
      std::make_pair(PrimitiveType::TriangleStrip, MTLPrimitiveTypeTriangleStrip),
      std::make_pair(PrimitiveType::Point, MTLPrimitiveTypePoint)};

  for (auto inputAndExpected : inputAndExpectedList) {
    auto input = inputAndExpected.first;
    auto expected = inputAndExpected.second;
    auto result = metal::RenderCommandEncoder::convertPrimitiveType(input);
    ASSERT_EQ(result, expected);
  }
}

TEST_F(RenderCommandEncoderMTLTest, ToMTLIndexType) {
  const std::vector<std::pair<IndexFormat, MTLIndexType>> inputAndExpectedList = {
      std::make_pair(IndexFormat::UInt16, MTLIndexTypeUInt16),
      std::make_pair(IndexFormat::UInt32, MTLIndexTypeUInt32)};

  for (auto inputAndExpected : inputAndExpectedList) {
    auto input = inputAndExpected.first;
    auto expected = inputAndExpected.second;
    auto result = metal::RenderCommandEncoder::convertIndexType(input);
    ASSERT_EQ(result, expected);
  }
}

TEST_F(RenderCommandEncoderMTLTest, ToMTLLoadAction) {
  const std::vector<std::pair<LoadAction, MTLLoadAction>> inputAndExpectedList = {
      std::make_pair(LoadAction::DontCare, MTLLoadActionDontCare),
      std::make_pair(LoadAction::Load, MTLLoadActionLoad),
      std::make_pair(LoadAction::Clear, MTLLoadActionClear)};

  for (auto inputAndExpected : inputAndExpectedList) {
    auto input = inputAndExpected.first;
    auto expected = inputAndExpected.second;
    auto result = metal::RenderCommandEncoder::convertLoadAction(input);
    ASSERT_EQ(result, expected);
  }
}

TEST_F(RenderCommandEncoderMTLTest, ToMTLStoreAction) {
  const std::vector<std::pair<StoreAction, MTLStoreAction>> inputAndExpectedList = {
      std::make_pair(StoreAction::DontCare, MTLStoreActionDontCare),
      std::make_pair(StoreAction::Store, MTLStoreActionStore),
      std::make_pair(StoreAction::MsaaResolve, MTLStoreActionMultisampleResolve)};

  for (auto inputAndExpected : inputAndExpectedList) {
    auto input = inputAndExpected.first;
    auto expected = inputAndExpected.second;
    auto result = metal::RenderCommandEncoder::convertStoreAction(input);
    ASSERT_EQ(result, expected);
  }
}

} // namespace igl::tests
