/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <memory>
#include <vector>
#include <igl/Framebuffer.h>
#include <igl/RenderPass.h>
#include <igl/Texture.h>

namespace igl::tests {
namespace {

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
class FakeTexture final : public ITexture {
 public:
  FakeTexture(TextureDesc::TextureUsage usage, uint32_t samples = 1) :
    ITexture(TextureFormat::RGBA_UNorm8), usage_(usage), samples_(samples) {}

  [[nodiscard]] Dimensions getDimensions() const override {
    return Dimensions{1, 1, 1};
  }

  [[nodiscard]] uint32_t getNumLayers() const override {
    return 1;
  }

  [[nodiscard]] TextureType getType() const override {
    return TextureType::TwoD;
  }

  [[nodiscard]] TextureDesc::TextureUsage getUsage() const override {
    return usage_;
  }

  [[nodiscard]] uint32_t getSamples() const override {
    return samples_;
  }

  void generateMipmap(ICommandQueue& /*cmdQueue*/,
                      const TextureRangeDesc* /*range*/) const override {}

  void generateMipmap(ICommandBuffer& /*cmdBuffer*/,
                      const TextureRangeDesc* /*range*/) const override {}

  [[nodiscard]] uint32_t getNumMipLevels() const override {
    return 1;
  }

  [[nodiscard]] bool isRequiredGenerateMipmap() const override {
    return false;
  }

  [[nodiscard]] uint64_t getTextureId() const override {
    return 0;
  }

  [[nodiscard]] void* getNativeImage() const override {
    return nullptr;
  }

  [[nodiscard]] void* getNativeImageView() const override {
    return nullptr;
  }

  [[nodiscard]] const base::AttachmentInteropDesc& getDesc() const override {
    return desc_;
  }

 private:
  TextureDesc::TextureUsage usage_;
  uint32_t samples_;
  base::AttachmentInteropDesc desc_{};
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
class FakeFramebuffer final : public IFramebuffer {
 public:
  FakeFramebuffer(const std::vector<std::shared_ptr<ITexture>>& colorAttachments,
                  const std::vector<std::shared_ptr<ITexture>>& resolveColorAttachments,
                  const std::shared_ptr<ITexture>& depthAttachment,
                  const std::shared_ptr<ITexture>& resolveDepthAttachment,
                  FramebufferMode mode = FramebufferMode::Mono) :
    colorAttachments_(colorAttachments),
    resolveColorAttachments_(resolveColorAttachments),
    depthAttachment_(depthAttachment),
    resolveDepthAttachment_(resolveDepthAttachment),
    mode_(mode) {}

  [[nodiscard]] std::vector<size_t> getColorAttachmentIndices() const override {
    std::vector<size_t> indices;
    for (size_t i = 0; i < colorAttachments_.size(); ++i) {
      if (colorAttachments_[i]) {
        indices.push_back(i);
      } else if (resolveColorAttachments_.size() > i && resolveColorAttachments_[i]) {
        indices.push_back(i);
      }
    }
    return indices;
  }

  [[nodiscard]] std::shared_ptr<ITexture> getColorAttachment(size_t index) const override {
    if (index < colorAttachments_.size()) {
      return colorAttachments_[index];
    }
    return nullptr;
  }

  [[nodiscard]] std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override {
    if (index < resolveColorAttachments_.size()) {
      return resolveColorAttachments_[index];
    }
    return nullptr;
  }

  [[nodiscard]] std::shared_ptr<ITexture> getDepthAttachment() const override {
    return depthAttachment_;
  }

  [[nodiscard]] std::shared_ptr<ITexture> getResolveDepthAttachment() const override {
    return resolveDepthAttachment_;
  }

  [[nodiscard]] std::shared_ptr<ITexture> getStencilAttachment() const override {
    return nullptr;
  }

  [[nodiscard]] FramebufferMode getMode() const override {
    return mode_;
  }

  [[nodiscard]] bool isSwapchainBound() const override {
    return false;
  }

  void copyBytesColorAttachment(ICommandQueue& /*cmdQueue*/,
                                size_t /*index*/,
                                void* /*pixelBytes*/,
                                const TextureRangeDesc& /*range*/,
                                size_t /*bytesPerRow*/) const override {}

  void copyBytesDepthAttachment(ICommandQueue& /*cmdQueue*/,
                                void* /*pixelBytes*/,
                                const TextureRangeDesc& /*range*/,
                                size_t /*bytesPerRow*/) const override {}

  void copyBytesStencilAttachment(ICommandQueue& /*cmdQueue*/,
                                  void* /*pixelBytes*/,
                                  const TextureRangeDesc& /*range*/,
                                  size_t /*bytesPerRow*/) const override {}

  void copyTextureColorAttachment(ICommandQueue& /*cmdQueue*/,
                                  size_t /*index*/,
                                  std::shared_ptr<ITexture> /*destTexture*/,
                                  const TextureRangeDesc& /*range*/) const override {}

  void updateDrawable(std::shared_ptr<ITexture> /*texture*/) override {}

  void updateDrawable(SurfaceTextures /*surfaceTextures*/) override {}

  void updateResolveAttachment(std::shared_ptr<ITexture> /*texture*/) override {}

 private:
  std::vector<std::shared_ptr<ITexture>> colorAttachments_;
  std::vector<std::shared_ptr<ITexture>> resolveColorAttachments_;
  std::shared_ptr<ITexture> depthAttachment_;
  std::shared_ptr<ITexture> resolveDepthAttachment_;
  FramebufferMode mode_;
};

// Helper to create a render pass with the given number of color attachments and subpasses
[[nodiscard]] RenderPassDesc makeRenderPass(
    size_t numColorAttachments,
    const std::vector<RenderPassDesc::SubpassDesc>& subpasses) {
  RenderPassDesc rp;
  rp.colorAttachments.resize(numColorAttachments);
  for (size_t i = 0; i < numColorAttachments; ++i) {
    rp.colorAttachments[i].loadAction = LoadAction::Clear;
    rp.colorAttachments[i].storeAction = StoreAction::Store;
  }
  rp.subpasses = subpasses;
  return rp;
}

[[nodiscard]] std::vector<std::shared_ptr<ITexture>> textures(
    std::initializer_list<std::shared_ptr<ITexture>> list) {
  return {list};
}

[[nodiscard]] std::shared_ptr<IFramebuffer> makeFramebuffer(
    const std::vector<std::shared_ptr<ITexture>>& colorAttachments,
    FramebufferMode mode = FramebufferMode::Mono) {
  return std::make_shared<FakeFramebuffer>(colorAttachments, textures({}), nullptr, nullptr, mode);
}

} // namespace

TEST(RenderPassValidateTest, ValidEmptySubpasses) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = makeFramebuffer({att0});

  const RenderPassDesc rp = makeRenderPass(1, {});
  const Result ret = rp.validate(*fb);
  EXPECT_TRUE(ret.isOk());
}

TEST(RenderPassValidateTest, ValidDeferredLayout) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment |
                                                  TextureDesc::TextureUsageBits::InputAttachment);
  const auto att2 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment |
                                                  TextureDesc::TextureUsageBits::InputAttachment);
  const auto fb = makeFramebuffer({att0, att1, att2});

  const RenderPassDesc rp =
      makeRenderPass(3,
                     {{.colorAttachmentMask = 0b110},
                      {.colorAttachmentMask = 0b001, .inputAttachmentMask = 0b110}});

  const Result ret = rp.validate(*fb);
  EXPECT_TRUE(ret.isOk());
}

TEST(RenderPassValidateTest, RejectsOneSubpass) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = makeFramebuffer({att0});

  const RenderPassDesc rp = makeRenderPass(1, {{.colorAttachmentMask = 0b001}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

TEST(RenderPassValidateTest, RejectsTooManySubpasses) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = makeFramebuffer({att0});

  std::vector<RenderPassDesc::SubpassDesc> subpasses(IGL_SUBPASSES_MAX + 1);
  for (auto& sp : subpasses) {
    sp.colorAttachmentMask = 0b001;
  }

  const RenderPassDesc rp = makeRenderPass(1, subpasses);
  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

TEST(RenderPassValidateTest, RejectsMaskBitOutOfRange) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto att2 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = makeFramebuffer({att0, att1, att2});

  // Bit 3 is out of range for 3 attachments (0-2)
  const RenderPassDesc rp =
      makeRenderPass(3, {{.colorAttachmentMask = 0b1000}, {.colorAttachmentMask = 0b0111}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

TEST(RenderPassValidateTest, RejectsStereo) {
  std::shared_ptr<ITexture> att0 =
      std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = std::make_shared<FakeFramebuffer>(
      textures({att0}), textures({}), nullptr, nullptr, FramebufferMode::Stereo);

  const RenderPassDesc rp =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST(RenderPassValidateTest, RejectsMultisampledColor) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment, 4);
  const auto fb = makeFramebuffer({att0});

  const RenderPassDesc rp =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST(RenderPassValidateTest, RejectsMultisampledDepth) {
  std::shared_ptr<ITexture> att0 =
      std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  std::shared_ptr<ITexture> depth =
      std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment, 4);
  auto fb = std::make_shared<FakeFramebuffer>(
      textures({att0}), textures({}), depth, nullptr, FramebufferMode::Mono);

  const RenderPassDesc rp =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST(RenderPassValidateTest, RejectsColorResolve) {
  std::shared_ptr<ITexture> att0 =
      std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  std::shared_ptr<ITexture> att1 =
      std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  auto fb = std::make_shared<FakeFramebuffer>(
      textures({att0}), textures({att1}), nullptr, nullptr, FramebufferMode::Mono);

  const RenderPassDesc rp =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST(RenderPassValidateTest, RejectsDepthResolve) {
  std::shared_ptr<ITexture> att0 =
      std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  std::shared_ptr<ITexture> depth =
      std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  std::shared_ptr<ITexture> depthResolve =
      std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  auto fb = std::make_shared<FakeFramebuffer>(
      textures({att0}), textures({}), depth, depthResolve, FramebufferMode::Mono);

  const RenderPassDesc rp =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST(RenderPassValidateTest, RejectsNonContiguousAttachments) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto att2 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);

  // Create framebuffer with gap (attachment 0 and 2, but not 1)
  std::vector<std::shared_ptr<ITexture>> colors(3);
  colors[0] = att0;
  colors[2] = att2;
  auto fb = std::make_shared<FakeFramebuffer>(
      colors, textures({}), nullptr, nullptr, FramebufferMode::Mono);

  const RenderPassDesc rp =
      makeRenderPass(3, {{.colorAttachmentMask = 0b101}, {.colorAttachmentMask = 0b101}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST(RenderPassValidateTest, RejectsZeroColorAttachments) {
  std::vector<std::shared_ptr<ITexture>> empty;
  auto fb = std::make_shared<FakeFramebuffer>(
      empty, textures({}), nullptr, nullptr, FramebufferMode::Mono);

  const RenderPassDesc rp =
      makeRenderPass(0, {{.colorAttachmentMask = 0b000}, {.colorAttachmentMask = 0b000}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST(RenderPassValidateTest, RejectsShortColorAttachmentDescs) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = makeFramebuffer({att0, att1});

  // RenderPassDesc has only 1 color attachment but framebuffer has 2
  const RenderPassDesc rp =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST(RenderPassValidateTest, RejectsColorInputOverlap) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment |
                                                  TextureDesc::TextureUsageBits::InputAttachment);
  const auto fb = makeFramebuffer({att0});

  const RenderPassDesc rp =
      makeRenderPass(1,
                     {{.colorAttachmentMask = 0b001, .inputAttachmentMask = 0b001},
                      {.colorAttachmentMask = 0b001}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST(RenderPassValidateTest, RejectsInputNotWrittenEarlier) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment |
                                                  TextureDesc::TextureUsageBits::InputAttachment);
  const auto att2 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment |
                                                  TextureDesc::TextureUsageBits::InputAttachment);
  const auto fb = makeFramebuffer({att0, att1, att2});

  // First subpass: write att0, read att1 (which is not written yet)
  // Second subpass: write att1 and att2
  // This should fail because att1 is read before it's written
  const RenderPassDesc rp =
      makeRenderPass(3,
                     {{.colorAttachmentMask = 0b001, .inputAttachmentMask = 0b010},
                      {.colorAttachmentMask = 0b110}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST(RenderPassValidateTest, RejectsUnwrittenAttachment) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = makeFramebuffer({att0, att1});

  // Only write att0, never write att1
  const RenderPassDesc rp =
      makeRenderPass(2, {{.colorAttachmentMask = 0b01}, {.colorAttachmentMask = 0b01}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST(RenderPassValidateTest, RejectsInputWithoutUsageBit) {
  const auto att0 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  // att1 does NOT have InputAttachment bit
  const auto att1 = std::make_shared<FakeTexture>(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = makeFramebuffer({att0, att1});

  const RenderPassDesc rp = makeRenderPass(
      2,
      {{.colorAttachmentMask = 0b01}, {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0b01}});

  const Result ret = rp.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

} // namespace igl::tests
