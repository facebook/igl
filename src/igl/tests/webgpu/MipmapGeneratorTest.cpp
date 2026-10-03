/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/MipmapGenerator.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/webgpu/Texture.h>

namespace igl::tests {

namespace {

constexpr uint32_t kRed = 0xff0000ff;
constexpr uint32_t kBlue = 0xffff0000;

} // namespace

class WebGPUMipmapGeneratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
    Result ret;
    queue_ = device_->createCommandQueue({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
  }

  void TearDown() override {
    EXPECT_EQ(device_->getContext().getUncapturedErrorCount(), 0u);
  }

 protected:
  [[nodiscard]] std::shared_ptr<webgpu::Texture> createTexture(TextureDesc desc,
                                                               uint32_t numMipLevels) {
    desc.numMipLevels = numMipLevels;
    Result ret;
    auto texture = device_->createTexture(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return std::static_pointer_cast<webgpu::Texture>(texture);
  }

  void wait() {
    auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
  }

  [[nodiscard]] static std::vector<uint32_t> read(const webgpu::Texture& texture,
                                                  const TextureRangeDesc& range) {
    std::vector<uint32_t> texels(size_t{range.width} * range.height);
    EXPECT_TRUE(
        texture.getBytes(range, WGPUTextureAspect_All, texels.data(), 0, /*flipVertically=*/false)
            .isOk());
    return texels;
  }

  std::unique_ptr<webgpu::Device> device_;
  std::shared_ptr<ICommandQueue> queue_;
};

TEST_F(WebGPUMipmapGeneratorTest, BoxFiltersEachLevel) {
  auto texture = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled),
      3);
  ASSERT_NE(texture, nullptr);
  EXPECT_TRUE(texture->isRequiredGenerateMipmap());
  // Left half red, right half blue.
  std::vector<uint32_t> base(16);
  for (size_t i = 0; i < base.size(); ++i) {
    base[i] = (i % 4) < 2 ? kRed : kBlue;
  }
  ASSERT_TRUE(texture->upload(texture->getFullRange(0), base.data()).isOk());
  texture->generateMipmap(*queue_);
  EXPECT_FALSE(texture->isRequiredGenerateMipmap());
  wait();

  EXPECT_EQ(read(*texture, texture->getFullRange(1)),
            (std::vector<uint32_t>{kRed, kBlue, kRed, kBlue}));
  const std::vector<uint32_t> top = read(*texture, texture->getFullRange(2));
  ASSERT_EQ(top.size(), 1u);
  // Half red, half blue: r and b are 0x7f or 0x80 depending on rounding.
  EXPECT_NEAR(static_cast<int>(top[0] & 0xff), 0x80, 1);
  EXPECT_NEAR(static_cast<int>((top[0] >> 16) & 0xff), 0x80, 1);
  EXPECT_EQ(top[0] >> 24, 0xffu);
}

TEST_F(WebGPUMipmapGeneratorTest, CubeFaceRangeInACommandBuffer) {
  auto texture =
      createTexture(TextureDesc::newCube(
                        TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled),
                    2);
  ASSERT_NE(texture, nullptr);
  const std::array<uint32_t, 4> red = {kRed, kRed, kRed, kRed};
  const std::array<uint32_t, 1> zero = {0};
  for (uint32_t face = 0; face < 6; ++face) {
    ASSERT_TRUE(texture->upload(texture->getCubeFaceRange(face, 0), red.data()).isOk());
    ASSERT_TRUE(texture->upload(texture->getCubeFaceRange(face, 1), zero.data()).isOk());
  }
  // Only face 3.
  const TextureRangeDesc range = texture->getCubeFaceRange(3, 0).withNumMipLevels(2);
  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  texture->generateMipmap(*cmdBuffer, &range);
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();
  for (uint32_t face = 0; face < 6; ++face) {
    EXPECT_EQ(read(*texture, texture->getCubeFaceRange(face, 1)),
              std::vector<uint32_t>{face == 3 ? kRed : 0u})
        << "face " << face;
  }
}

TEST_F(WebGPUMipmapGeneratorTest, CubeFaceAcrossCubeArrayLayers) {
  TextureDesc desc = TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled);
  desc.numLayers = 2;
  auto texture = createTexture(desc, 2);
  ASSERT_NE(texture, nullptr);
  const std::array<uint32_t, 4> red = {kRed, kRed, kRed, kRed};
  const std::array<uint32_t, 1> zero = {0};
  for (uint32_t layer = 0; layer < 2; ++layer) {
    for (uint32_t face = 0; face < 6; ++face) {
      const TextureRangeDesc base = texture->getCubeFaceRange(face, 0).atLayer(layer);
      ASSERT_TRUE(texture->upload(base, red.data()).isOk());
      ASSERT_TRUE(texture->upload(base.atMipLevel(1), zero.data()).isOk());
    }
  }
  // Face 3 of both cubes.
  const TextureRangeDesc range =
      texture->getCubeFaceRange(3, 0).withNumLayers(2).withNumMipLevels(2);
  texture->generateMipmap(*queue_, &range);
  wait();
  for (uint32_t layer = 0; layer < 2; ++layer) {
    for (uint32_t face = 0; face < 6; ++face) {
      EXPECT_EQ(read(*texture, texture->getCubeFaceRange(face, 1).atLayer(layer)),
                std::vector<uint32_t>{face == 3 ? kRed : 0u})
          << "layer " << layer << " face " << face;
    }
  }
}

TEST_F(WebGPUMipmapGeneratorTest, CubeFaceRangeMustStayInOneCube) {
  TextureDesc desc = TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled);
  desc.numLayers = 2;
  auto texture = createTexture(desc, 2);
  ASSERT_NE(texture, nullptr);
  const std::array<uint32_t, 4> red = {kRed, kRed, kRed, kRed};
  const std::array<uint32_t, 1> zero = {0};
  for (uint32_t layer = 0; layer < 2; ++layer) {
    for (uint32_t face = 0; face < 6; ++face) {
      const TextureRangeDesc base = texture->getCubeFaceRange(face, 0).atLayer(layer);
      ASSERT_TRUE(texture->upload(base, red.data()).isOk());
      ASSERT_TRUE(texture->upload(base.atMipLevel(1), zero.data()).isOk());
    }
  }
  // Faces 1..6 would run into the next cube.
  TextureRangeDesc range = texture->getCubeFaceRange(1, 0).withNumMipLevels(2);
  range.numFaces = 6;
  texture->generateMipmap(*queue_, &range);
  wait();
  for (uint32_t layer = 0; layer < 2; ++layer) {
    for (uint32_t face = 0; face < 6; ++face) {
      EXPECT_EQ(read(*texture, texture->getCubeFaceRange(face, 1).atLayer(layer)),
                std::vector<uint32_t>{0u})
          << "layer " << layer << " face " << face;
    }
  }
}

TEST_F(WebGPUMipmapGeneratorTest, ViewOfSomeLevelsLeavesTheParentRequired) {
  auto texture = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled),
      3);
  ASSERT_NE(texture, nullptr);
  Result ret;
  auto view = device_->createTextureView(texture, {.mipLevel = 1, .numMipLevels = 2}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  view->generateMipmap(*queue_);
  wait();
  EXPECT_TRUE(texture->isRequiredGenerateMipmap());
  texture->generateMipmap(*queue_);
  wait();
  EXPECT_FALSE(texture->isRequiredGenerateMipmap());
}

TEST_F(WebGPUMipmapGeneratorTest, UploadThroughAViewOfAHigherLevel) {
  auto texture = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled),
      2);
  ASSERT_NE(texture, nullptr);
  Result ret;
  auto view = device_->createTextureView(texture, {.mipLevel = 1, .numMipLevels = 1}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_TRUE(texture->isRequiredGenerateMipmap());
  const uint32_t red = kRed;
  ASSERT_TRUE(view->upload(view->getFullRange(0), &red).isOk());
  EXPECT_FALSE(texture->isRequiredGenerateMipmap());
}

TEST_F(WebGPUMipmapGeneratorTest, AttachmentOnlyTexture) {
  auto texture = createTexture(
      TextureDesc::new2D(
          TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Attachment),
      2);
  ASSERT_NE(texture, nullptr);
  Result ret;
  auto framebuffer = device_->createFramebuffer({.colorAttachments = {{.texture = texture}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  auto encoder = cmdBuffer->createRenderCommandEncoder(
      {.colorAttachments = {{.loadAction = LoadAction::Clear,
                             .storeAction = StoreAction::Store,
                             .clearColor = {1, 0, 0, 1}}}},
      framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();
  queue_->submit(*cmdBuffer);
  texture->generateMipmap(*queue_);
  wait();
  EXPECT_EQ(read(*texture, texture->getFullRange(1)), std::vector<uint32_t>{kRed});
  EXPECT_FALSE(texture->isRequiredGenerateMipmap());
}

TEST_F(WebGPUMipmapGeneratorTest, SingleLevelRangeLeavesMipmapsRequired) {
  auto texture = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled),
      3);
  ASSERT_NE(texture, nullptr);
  const TextureRangeDesc range = texture->getFullRange(0);
  texture->generateMipmap(*queue_, &range);
  EXPECT_TRUE(texture->isRequiredGenerateMipmap());
}

TEST_F(WebGPUMipmapGeneratorTest, AutoGenerateOnUpload) {
  TextureDesc desc =
      TextureDesc::new2D(TextureFormat::RGBA_SRGB, 2, 2, TextureDesc::TextureUsageBits::Sampled);
  desc.mipmapGeneration = TextureDesc::TextureMipmapGeneration::AutoGenerateOnUpload;
  auto texture = createTexture(desc, 2);
  ASSERT_NE(texture, nullptr);
  const std::array<uint32_t, 4> blue = {kBlue, kBlue, kBlue, kBlue};
  ASSERT_TRUE(texture->upload(texture->getFullRange(0), blue.data()).isOk());
  wait();
  EXPECT_EQ(read(*texture, texture->getFullRange(1)), std::vector<uint32_t>{kBlue});

  const uint32_t red = 0xff0000ff;
  EXPECT_EQ(texture->upload(texture->getFullRange(1), &red).code, Result::Code::InvalidOperation);

  // Through a view, the view's level 0 is not the texture's base level.
  TextureDesc threeLevels =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  threeLevels.mipmapGeneration = TextureDesc::TextureMipmapGeneration::AutoGenerateOnUpload;
  auto parent = createTexture(threeLevels, 3);
  ASSERT_NE(parent, nullptr);
  Result ret;
  auto view = device_->createTextureView(parent, {.mipLevel = 1, .numMipLevels = 2}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const std::array<uint32_t, 4> pixels = {red, red, red, red};
  EXPECT_EQ(view->upload(view->getFullRange(0), pixels.data()).code,
            Result::Code::InvalidOperation);
}

TEST_F(WebGPUMipmapGeneratorTest, PreparePipelineCachesThePipeline) {
  auto& generator = device_->getContext().getMipmapGenerator();
  EXPECT_TRUE(generator.preparePipeline(WGPUTextureFormat_RGBA8Unorm, true).isOk());
  EXPECT_TRUE(generator.preparePipeline(WGPUTextureFormat_RGBA8Unorm, true).isOk());
  EXPECT_EQ(generator.getPipelineCount(), 1u);
}

TEST_F(WebGPUMipmapGeneratorTest, UnsupportedTexturesAreLeftAlone) {
  auto integer = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UInt32, 2, 2, TextureDesc::TextureUsageBits::Sampled),
      2);
  ASSERT_NE(integer, nullptr);
  integer->generateMipmap(*queue_);
  auto volume = createTexture(
      TextureDesc::new3D(
          TextureFormat::RGBA_UNorm8, 2, 2, 2, TextureDesc::TextureUsageBits::Sampled),
      2);
  ASSERT_NE(volume, nullptr);
  volume->generateMipmap(*queue_);
  wait();
  EXPECT_TRUE(integer->isRequiredGenerateMipmap());
  EXPECT_TRUE(volume->isRequiredGenerateMipmap());
  EXPECT_EQ(device_->getContext().getMipmapGenerator().getPipelineCount(), 0u);
}

} // namespace igl::tests
