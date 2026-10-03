/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/Texture.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Framebuffer.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>

namespace igl::tests {

namespace {

// Top row first, as uploaded.
std::vector<uint8_t> makePixels(uint32_t width, uint32_t height, uint32_t bytesPerTexel = 4) {
  std::vector<uint8_t> pixels(size_t{width} * height * bytesPerTexel);
  for (size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = static_cast<uint8_t>(i * 7 + 1);
  }
  return pixels;
}

} // namespace

class WebGPUTextureTest : public ::testing::Test {
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
  [[nodiscard]] std::shared_ptr<webgpu::Texture> createTexture(const TextureDesc& desc,
                                                               Result* outResult = nullptr) {
    Result ret;
    auto texture = device_->createTexture(desc, &ret);
    if (outResult != nullptr) {
      *outResult = ret;
    } else {
      EXPECT_TRUE(ret.isOk()) << ret.message;
    }
    return std::static_pointer_cast<webgpu::Texture>(texture);
  }

  [[nodiscard]] static std::vector<uint8_t> readTopDown(const webgpu::Texture& texture,
                                                        const TextureRangeDesc& range) {
    std::vector<uint8_t> result(texture.getProperties().getBytesPerRange(range));
    const Result ret =
        texture.getBytes(range, WGPUTextureAspect_All, result.data(), 0, /*flipVertically=*/false);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return result;
  }

  std::unique_ptr<webgpu::Device> device_;
  std::shared_ptr<ICommandQueue> queue_;
};

TEST_F(WebGPUTextureTest, UploadAndReadBack) {
  auto texture = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 3, 2, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->getWGPUFormat(), WGPUTextureFormat_RGBA8Unorm);
  EXPECT_NE(texture->getSampledView(), nullptr);
  EXPECT_NE(texture->getNativeImage(), nullptr);

  const auto pixels = makePixels(3, 2);
  const auto range = TextureRangeDesc::new2D(0, 0, 3, 2);
  ASSERT_TRUE(texture->upload(range, pixels.data()).isOk());
  EXPECT_EQ(readTopDown(*texture, range), pixels);
}

TEST_F(WebGPUTextureTest, UploadHonorsRowPitchAndSubRegion) {
  auto texture = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(texture, nullptr);
  const auto pixels = makePixels(2, 2);
  // Rows padded to 12 bytes.
  std::vector<uint8_t> padded(24, 0);
  std::memcpy(padded.data(), pixels.data(), 8);
  std::memcpy(padded.data() + 12, pixels.data() + 8, 8);
  const auto range = TextureRangeDesc::new2D(1, 2, 2, 2);
  ASSERT_TRUE(texture->upload(range, padded.data(), 12).isOk());
  EXPECT_EQ(readTopDown(*texture, range), pixels);
}

TEST_F(WebGPUTextureTest, BGRAIsNotSwizzled) {
  auto texture = createTexture(
      TextureDesc::new2D(TextureFormat::BGRA_UNorm8, 2, 1, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(texture, nullptr);
  const std::vector<uint8_t> pixels = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
  const auto range = TextureRangeDesc::new2D(0, 0, 2, 1);
  ASSERT_TRUE(texture->upload(range, pixels.data()).isOk());
  EXPECT_EQ(readTopDown(*texture, range), pixels);
}

TEST_F(WebGPUTextureTest, RejectsUnsupportedDescriptors) {
  Result ret;
  auto texture = createTexture(
      {
          .width = 4,
          .height = 4,
          .numSamples = 2,
          .usage = TextureDesc::TextureUsageBits::Attachment,
          .type = TextureType::TwoD,
          .format = TextureFormat::RGBA_UNorm8,
      },
      &ret);
  EXPECT_EQ(texture, nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);

  texture = createTexture(
      TextureDesc::new2D(
          TextureFormat::BGR10_A2_Unorm, 4, 4, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  EXPECT_EQ(texture, nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);

  texture = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_SRGB, 4, 4, TextureDesc::TextureUsageBits::Storage),
      &ret);
  EXPECT_EQ(texture, nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST_F(WebGPUTextureTest, CompressedTextureWithMips) {
  if (!device_->getDeviceFeatureSet().hasWGPUFeature(WGPUFeatureName_TextureCompressionBC)) {
    GTEST_SKIP() << "BC compression is not available";
  }
  Result ret;
  auto attachment = createTexture(
      TextureDesc::new2D(
          TextureFormat::RGBA_BC7_UNORM_4x4, 8, 8, TextureDesc::TextureUsageBits::Attachment),
      &ret);
  EXPECT_EQ(attachment, nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);

  TextureDesc desc = TextureDesc::new2D(
      TextureFormat::RGBA_BC7_UNORM_4x4, 8, 8, TextureDesc::TextureUsageBits::Sampled);
  desc.numMipLevels = 3;
  auto texture = createTexture(desc);
  ASSERT_NE(texture, nullptr);
  // 8x8, 4x4 and 2x2 (one partial block): 4 + 1 + 1 blocks of 16 bytes.
  const std::vector<uint8_t> blocks(6 * 16, 0x5a);
  ASSERT_TRUE(texture->upload(texture->getFullMipRange(), blocks.data()).isOk());
}

TEST_F(WebGPUTextureTest, DepthCopyRestrictions) {
  auto depth24 = createTexture(TextureDesc::new2D(
      TextureFormat::Z_UNorm24, 4, 4, TextureDesc::TextureUsageBits::Attachment));
  ASSERT_NE(depth24, nullptr);
  std::vector<uint8_t> out(64);
  const Result ret =
      depth24->getBytes(TextureRangeDesc::new2D(0, 0, 4, 4), WGPUTextureAspect_All, out.data(), 0);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);

  EXPECT_EQ(webgpu::getCopyBytesPerTexel(WGPUTextureFormat_Depth32Float, WGPUTextureAspect_All),
            4u);
  EXPECT_EQ(webgpu::getCopyBytesPerTexel(WGPUTextureFormat_Depth24PlusStencil8,
                                         WGPUTextureAspect_DepthOnly),
            0u);
  EXPECT_EQ(webgpu::getCopyBytesPerTexel(WGPUTextureFormat_Depth24PlusStencil8,
                                         WGPUTextureAspect_StencilOnly),
            1u);
  EXPECT_EQ(webgpu::getCopyBytesPerTexel(WGPUTextureFormat_RGBA16Float, WGPUTextureAspect_All), 8u);
}

TEST_F(WebGPUTextureTest, MipViewAddressesParentMip) {
  TextureDesc desc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  desc.numMipLevels = 3;
  auto texture = createTexture(desc);
  ASSERT_NE(texture, nullptr);

  Result ret;
  auto view = std::static_pointer_cast<webgpu::Texture>(device_->createTextureView(
      texture, {.mipLevel = 1, .numMipLevels = 2, .debugName = "mip1"}, &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view->getDimensions().width, 2u);
  EXPECT_EQ(view->getDimensions().height, 2u);
  EXPECT_EQ(view->getNumMipLevels(), 2u);
  EXPECT_EQ(view->getBaseMipLevel(), 1u);
  EXPECT_EQ(view->getWGPUTexture(), texture->getWGPUTexture());
  EXPECT_NE(view->getTextureId(), texture->getTextureId());

  const auto pixels = makePixels(2, 2);
  ASSERT_TRUE(view->upload(TextureRangeDesc::new2D(0, 0, 2, 2), pixels.data()).isOk());
  EXPECT_EQ(readTopDown(*texture, TextureRangeDesc::new2D(0, 0, 2, 2, 1)), pixels);

  view = std::static_pointer_cast<webgpu::Texture>(
      device_->createTextureView(texture, {.mipLevel = 2, .numMipLevels = 2}, &ret));
  EXPECT_EQ(view, nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

TEST_F(WebGPUTextureTest, CubeViewLayerCountsCubes) {
  TextureDesc desc = TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled);
  desc.numLayers = 2;
  auto texture = createTexture(desc);
  ASSERT_NE(texture, nullptr);
  const auto pixels = makePixels(2, 2);
  ASSERT_TRUE(
      texture->upload(TextureRangeDesc::new2D(0, 0, 2, 2).atLayer(1).atFace(2), pixels.data())
          .isOk());

  Result ret;
  auto view = std::static_pointer_cast<webgpu::Texture>(device_->createTextureView(
      texture, {.type = TextureType::Cube, .layer = 1, .numLayers = 1}, &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(readTopDown(*view, TextureRangeDesc::new2D(0, 0, 2, 2).atFace(2)), pixels);

  view = std::static_pointer_cast<webgpu::Texture>(device_->createTextureView(
      texture, {.type = TextureType::Cube, .layer = 1, .numLayers = 2}, &ret));
  EXPECT_EQ(view, nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

TEST_F(WebGPUTextureTest, ViewsRejectUnsupportedAspectsAndSwizzles) {
  auto color = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(color, nullptr);
  Result ret;
  EXPECT_EQ(device_->createTextureView(color, {.swizzle = {.r = Swizzle_B, .b = Swizzle_R}}, &ret),
            nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
  EXPECT_EQ(device_->createTextureView(color, {.aspect = ImageAspectBits_Depth}, &ret), nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
  EXPECT_NE(device_->createTextureView(color, {.aspect = ImageAspectBits_Color}, &ret), nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;

  auto depthStencil = createTexture(TextureDesc::new2D(
      TextureFormat::S8_UInt_Z24_UNorm, 2, 2, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(depthStencil, nullptr);
  EXPECT_EQ(device_->createTextureView(depthStencil, {.aspect = ImageAspectBits_Stencil}, &ret),
            nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
  EXPECT_NE(device_->createTextureView(depthStencil, {.aspect = ImageAspectBits_Depth}, &ret),
            nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;

  auto stencil = createTexture(
      TextureDesc::new2D(TextureFormat::S_UInt8, 2, 2, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(stencil, nullptr);
  EXPECT_EQ(device_->createTextureView(stencil, {.aspect = ImageAspectBits_Depth}, &ret), nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
  EXPECT_NE(device_->createTextureView(stencil, {.aspect = ImageAspectBits_Stencil}, &ret),
            nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
}

TEST_F(WebGPUTextureTest, CopyTextureToBuffer) {
  struct Case {
    TextureFormat format;
    uint32_t bytesPerTexel;
    uint32_t width;
  };
  // 256-byte rows are copied directly, 4-byte multiples through a staging buffer, and other rows
  // are packed by a compute pass.
  for (const Case& c : {Case{.format = TextureFormat::RGBA_UNorm8, .bytesPerTexel = 4, .width = 64},
                        Case{.format = TextureFormat::RGBA_UNorm8, .bytesPerTexel = 4, .width = 3},
                        Case{.format = TextureFormat::R_UNorm8, .bytesPerTexel = 1, .width = 3},
                        Case{.format = TextureFormat::R_UNorm8, .bytesPerTexel = 1, .width = 5},
                        Case{.format = TextureFormat::R_UInt16, .bytesPerTexel = 2, .width = 3}}) {
    constexpr uint32_t kHeight = 3;
    constexpr uint64_t kOffset = 8;
    constexpr uint8_t kSentinel = 0xab;
    auto texture = createTexture(
        TextureDesc::new2D(c.format, c.width, kHeight, TextureDesc::TextureUsageBits::Sampled));
    ASSERT_NE(texture, nullptr);
    const auto pixels = makePixels(c.width, kHeight, c.bytesPerTexel);
    ASSERT_TRUE(
        texture->upload(TextureRangeDesc::new2D(0, 0, c.width, kHeight), pixels.data()).isOk());

    Result ret;
    const std::vector<uint8_t> initial(kOffset + pixels.size() + 8, kSentinel);
    auto buffer = device_->createBuffer({.type = BufferDesc::BufferTypeBits::Storage,
                                         .data = initial.data(),
                                         .length = initial.size()},
                                        &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    auto cmdBuffer = queue_->createCommandBuffer({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    cmdBuffer->copyTextureToBuffer(*texture, *buffer, kOffset);
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();

    const auto* data = static_cast<const uint8_t*>(buffer->map({initial.size(), 0}, &ret));
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(data, nullptr);
    const std::vector<uint8_t> result(data, data + initial.size());
    buffer->unmap();
    std::vector<uint8_t> expected = initial;
    std::copy(pixels.begin(), pixels.end(), expected.begin() + kOffset);
    EXPECT_EQ(result, expected) << "format " << static_cast<int>(c.format) << ", width " << c.width;
  }
}

TEST_F(WebGPUTextureTest, CopyTextureToBufferStaysInsideTheBuffer) {
  auto texture = createTexture(
      TextureDesc::new2D(TextureFormat::R_UNorm8, 3, 1, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(texture, nullptr);
  const std::array<uint8_t, 3> pixels = {1, 2, 3};
  ASSERT_TRUE(texture->upload(TextureRangeDesc::new2D(0, 0, 3, 1), pixels.data()).isOk());
  const auto copyInto = [&](size_t length) {
    Result ret;
    const std::vector<uint8_t> initial(length, 0xab);
    auto buffer = device_->createBuffer(
        {.type = BufferDesc::BufferTypeBits::Storage, .data = initial.data(), .length = length},
        &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    auto cmdBuffer = queue_->createCommandBuffer({}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    cmdBuffer->copyTextureToBuffer(*texture, *buffer, 4);
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
    const auto* data = static_cast<const uint8_t*>(buffer->map({length, 0}, &ret));
    EXPECT_TRUE(ret.isOk()) << ret.message;
    std::vector<uint8_t> result(data, data + length);
    buffer->unmap();
    return result;
  };
  // 4 + 3 bytes do not fit in a 5-byte buffer, even though its allocation is 8 bytes.
  EXPECT_EQ(copyInto(5), std::vector<uint8_t>(5, 0xab));
  EXPECT_EQ(copyInto(7), (std::vector<uint8_t>{0xab, 0xab, 0xab, 0xab, 1, 2, 3}));
}

TEST_F(WebGPUTextureTest, DropTextureBeforeSubmit) {
  Result ret;
  auto buffer =
      device_->createBuffer({.type = BufferDesc::BufferTypeBits::Storage, .length = 64}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto cmdBuffer = queue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  {
    auto texture = createTexture(TextureDesc::new2D(
        TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled));
    ASSERT_NE(texture, nullptr);
    cmdBuffer->copyTextureToBuffer(*texture, *buffer, 0);
  }
  EXPECT_EQ(device_->getContext().getResourceTracker().getPendingRetirementCount(), 1u);
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();
  EXPECT_EQ(device_->getContext().getResourceTracker().getPendingRetirementCount(), 0u);
}

TEST_F(WebGPUTextureTest, FramebufferCopiesAreBottomUp) {
  auto texture = createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                  2,
                                                  2,
                                                  TextureDesc::TextureUsageBits::Sampled |
                                                      TextureDesc::TextureUsageBits::Attachment));
  ASSERT_NE(texture, nullptr);
  const auto pixels = makePixels(2, 2);
  const auto range = TextureRangeDesc::new2D(0, 0, 2, 2);
  ASSERT_TRUE(texture->upload(range, pixels.data()).isOk());

  Result ret;
  auto framebuffer = device_->createFramebuffer({.colorAttachments = {{.texture = texture}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(framebuffer, nullptr);
  EXPECT_EQ(framebuffer->getColorAttachmentIndices(), std::vector<size_t>{0});
  EXPECT_FALSE(framebuffer->isSwapchainBound());

  std::vector<uint8_t> result(pixels.size());
  framebuffer->copyBytesColorAttachment(*queue_, 0, result.data(), range);
  EXPECT_EQ(std::vector<uint8_t>(result.begin(), result.begin() + 8),
            std::vector<uint8_t>(pixels.begin() + 8, pixels.end()));
  EXPECT_EQ(std::vector<uint8_t>(result.begin() + 8, result.end()),
            std::vector<uint8_t>(pixels.begin(), pixels.begin() + 8));

  auto copy = createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(copy, nullptr);
  framebuffer->copyTextureColorAttachment(*queue_, 0, copy, range);
  EXPECT_EQ(readTopDown(*copy, range), pixels);
}

TEST_F(WebGPUTextureTest, UpdateDrawable) {
  auto color = createTexture(TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Attachment));
  auto depth = createTexture(TextureDesc::new2D(
      TextureFormat::S8_UInt_Z24_UNorm, 2, 2, TextureDesc::TextureUsageBits::Attachment));
  ASSERT_TRUE(color && depth);
  Result ret;
  auto framebuffer = device_->createFramebuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  framebuffer->updateDrawable({.color = color, .depth = depth});
  EXPECT_EQ(framebuffer->getColorAttachment(0), color);
  EXPECT_EQ(framebuffer->getDepthAttachment(), depth);
  EXPECT_EQ(framebuffer->getStencilAttachment(), depth);
  framebuffer->updateDrawable(std::shared_ptr<ITexture>());
  EXPECT_EQ(framebuffer->getColorAttachment(0), nullptr);

  auto resolve = createTexture(TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Attachment));
  ASSERT_NE(resolve, nullptr);
  framebuffer->updateDrawable({.color = color, .colorResolve = resolve});
  EXPECT_EQ(framebuffer->getResolveColorAttachment(0), resolve);
  // A surface without MSAA has no resolve texture.
  framebuffer->updateDrawable({.color = color});
  EXPECT_EQ(framebuffer->getColorAttachment(0), color);
  EXPECT_EQ(framebuffer->getResolveColorAttachment(0), nullptr);
}

} // namespace igl::tests
