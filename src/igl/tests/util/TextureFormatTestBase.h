/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <gtest/gtest.h>
#include <igl/CommandQueue.h>
#include <igl/NameHandle.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/Texture.h>

namespace igl::tests::util {

class TextureFormatTestBase : public ::testing::Test {
 private:
 public:
  TextureFormatTestBase() = default;
  ~TextureFormatTestBase() override = default;

  void SetUp() override;
  void TearDown() override {}

  std::vector<std::pair<TextureFormat, bool>> getFormatSupport(TextureDesc::TextureUsage usage);
  std::pair<TextureFormat, bool> checkSupport(TextureFormat format,
                                              TextureDesc::TextureUsage usage);

  std::shared_ptr<IFramebuffer> createFramebuffer(std::shared_ptr<ITexture> attachmentTexture,
                                                  Result& ret);
  void render(std::shared_ptr<ITexture> sampledTexture,
              std::shared_ptr<ITexture> attachmentTexture,
              bool linearSampling,
              TextureFormatProperties testProperties);
  void testSampled(std::shared_ptr<ITexture> texture, bool linearSampling);
  void testAttachment(std::shared_ptr<ITexture> texture);
  void testUpload(std::shared_ptr<ITexture> texture);
  void testUsage(TextureDesc::TextureUsage usage, const char* usageName);
  void testUsage(std::pair<TextureFormat, bool> formatSupport,
                 TextureDesc::TextureUsage usage,
                 const char* usageName);
  void testUsage(std::shared_ptr<ITexture> texture,
                 TextureDesc::TextureUsage usage,
                 const char* usageName); // Member variables
 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ITexture> sampledTexture_;
  std::shared_ptr<ITexture> attachmentTexture_;
  RenderPassDesc renderPass_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> vb_, uv_, ib_;
  std::shared_ptr<ISamplerState> nearestSampler_;
  std::shared_ptr<ISamplerState> linearSampler_;
  RenderPipelineDesc renderPipelineDesc_;
  size_t textureUnit_ = 0;
};

} // namespace igl::tests::util
