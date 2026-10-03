/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/SamplerState.h>
#include <igl/ShaderCreator.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/webgpu/Texture.h>

namespace igl::tests {

namespace {

constexpr uint32_t kSize = 4;

// A full-screen triangle at depth 0.5, colored white.
constexpr const char* kVertex = R"(
@vertex
fn main(@builtin(vertex_index) i : u32) -> @builtin(position) vec4f {
  let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
  return vec4f(uv * 2.0 - 1.0, 0.5, 1.0);
}
)";

constexpr const char* kFragment = R"(
@fragment
fn main() -> @location(0) vec4f {
  return vec4f(1.0);
}
)";

constexpr uint32_t kWhite = 0xffffffff;
constexpr uint32_t kBlack = 0xff000000;

} // namespace

class WebGPUTextureTypesTest : public ::testing::Test {
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
  [[nodiscard]] std::shared_ptr<webgpu::Texture> createTexture(const TextureDesc& desc) {
    Result ret;
    auto texture = device_->createTexture(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return std::static_pointer_cast<webgpu::Texture>(texture);
  }

  [[nodiscard]] std::shared_ptr<IRenderPipelineState> createPipeline(TextureFormat depthFormat,
                                                                     uint32_t sampleCount = 1) {
    Result ret;
    auto stages = ShaderStagesCreator::fromModuleStringInput(
        *device_, kVertex, "main", "", kFragment, "main", "", &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    auto pipeline = device_->createRenderPipeline(
        {.shaderStages = std::move(stages),
         .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}},
                        .depthAttachmentFormat = depthFormat},
         .sampleCount = sampleCount},
        &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return pipeline;
  }

  // Draws the white triangle at depth 0.5 with a Less test against the contents of `depth`.
  std::array<uint32_t, kSize * kSize> drawWithDepthTest(const std::shared_ptr<ITexture>& depth) {
    Result ret;
    auto color = createTexture(TextureDesc::new2D(
        TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment));
    auto framebuffer = device_->createFramebuffer(
        {.colorAttachments = {{.texture = color}}, .depthAttachment = {.texture = depth}}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    auto depthState = device_->createDepthStencilState(
        {.compareFunction = CompareFunction::Less, .isDepthWriteEnabled = false}, &ret);
    auto pipeline = createPipeline(depth->getFormat());
    auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
    auto encoder = cmdBuffer->createRenderCommandEncoder(
        {.colorAttachments = {{.loadAction = LoadAction::Clear,
                               .storeAction = StoreAction::Store,
                               .clearColor = {0, 0, 0, 1}}},
         .depthAttachment = {.loadAction = LoadAction::Load, .storeAction = StoreAction::Store}},
        framebuffer);
    encoder->bindRenderPipelineState(pipeline);
    encoder->bindDepthStencilState(depthState);
    encoder->draw(3);
    encoder->endEncoding();
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
    std::array<uint32_t, kSize * kSize> pixels = {};
    EXPECT_TRUE(color
                    ->getBytes(TextureRangeDesc::new2D(0, 0, kSize, kSize),
                               WGPUTextureAspect_All,
                               pixels.data(),
                               0,
                               /*flipVertically=*/false)
                    .isOk());
    return pixels;
  }

  std::unique_ptr<webgpu::Device> device_;
  std::shared_ptr<ICommandQueue> queue_;
};

TEST_F(WebGPUTextureTypesTest, Depth32FloatUploadIsRendered) {
  auto depth = createTexture(TextureDesc::new2D(
      TextureFormat::Z_UNorm32, kSize, kSize, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(depth, nullptr);
  std::vector<float> values(kSize * kSize);
  for (size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<float>(i) / 16.0f;
  }
  ASSERT_TRUE(depth->upload(depth->getFullRange(), values.data()).isOk());
  // A sub-region upload keeps the rest.
  const std::array<float, 4> corner = {0.25f, 0.5f, 0.75f, 1.0f};
  ASSERT_TRUE(depth->upload(TextureRangeDesc::new2D(2, 2, 2, 2), corner.data()).isOk());
  values[10] = 0.25f;
  values[11] = 0.5f;
  values[14] = 0.75f;
  values[15] = 1.0f;

  std::vector<float> out(values.size());
  ASSERT_TRUE(depth
                  ->getBytes(TextureRangeDesc::new2D(0, 0, kSize, kSize),
                             WGPUTextureAspect_All,
                             out.data(),
                             0,
                             /*flipVertically=*/false)
                  .isOk());
  EXPECT_EQ(out, values);
}

TEST_F(WebGPUTextureTypesTest, Depth24PlusUploadIsTested) {
  auto depth = createTexture(TextureDesc::new2D(TextureFormat::Z_UNorm24,
                                                kSize,
                                                kSize,
                                                TextureDesc::TextureUsageBits::Sampled |
                                                    TextureDesc::TextureUsageBits::Attachment));
  ASSERT_NE(depth, nullptr);
  // Left half 0.25 (the triangle at 0.5 fails Less), right half 0.75 (it passes).
  std::vector<uint8_t> values(kSize * kSize * 3);
  for (uint32_t i = 0; i < kSize * kSize; ++i) {
    const uint32_t value = (i % kSize) < kSize / 2 ? 0x3fffff : 0xbfffff;
    values[3 * i] = value & 0xff;
    values[3 * i + 1] = (value >> 8) & 0xff;
    values[3 * i + 2] = (value >> 16) & 0xff;
  }
  const Result upload = depth->upload(depth->getFullRange(), values.data());
  ASSERT_TRUE(upload.isOk()) << upload.message;
  const auto pixels = drawWithDepthTest(depth);
  for (uint32_t i = 0; i < kSize * kSize; ++i) {
    EXPECT_EQ(pixels[i], (i % kSize) < kSize / 2 ? kBlack : kWhite) << "pixel " << i;
  }
}

TEST_F(WebGPUTextureTypesTest, MultisampleResolveIntoCubeFace) {
  TextureDesc msaaDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment);
  msaaDesc.numSamples = 4;
  auto msaa = createTexture(msaaDesc);
  auto cube = createTexture(TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment));
  ASSERT_TRUE(msaa && cube);
  Result ret;
  auto framebuffer = device_->createFramebuffer(
      {.colorAttachments = {{.texture = msaa, .resolveTexture = cube}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto pipeline = createPipeline(TextureFormat::Invalid, 4);
  ASSERT_NE(pipeline, nullptr);
  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  auto encoder = cmdBuffer->createRenderCommandEncoder(
      {.colorAttachments = {{.loadAction = LoadAction::Clear,
                             .storeAction = StoreAction::MsaaResolve,
                             .face = 3,
                             .clearColor = {0, 0, 0, 1}}}},
      framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(pipeline);
  encoder->draw(3);
  encoder->endEncoding();
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  for (const uint32_t face : {0u, 3u}) {
    std::array<uint32_t, kSize * kSize> pixels = {};
    TextureRangeDesc range = TextureRangeDesc::new2D(0, 0, kSize, kSize);
    range.face = face;
    ASSERT_TRUE(cube->getBytes(range, WGPUTextureAspect_All, pixels.data(), 0).isOk());
    EXPECT_EQ(pixels[0], face == 3 ? kWhite : 0u) << "face " << face;
  }
}

TEST_F(WebGPUTextureTypesTest, MultisampleResolve) {
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::MultiSample));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::MultiSampleResolve));
  TextureDesc msaaDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment);
  msaaDesc.numSamples = 4;
  auto msaa = createTexture(msaaDesc);
  auto resolve = createTexture(TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment));
  ASSERT_TRUE(msaa && resolve);
  Result ret;
  auto framebuffer = device_->createFramebuffer(
      {.colorAttachments = {{.texture = msaa, .resolveTexture = resolve}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto pipeline = createPipeline(TextureFormat::Invalid, 4);
  ASSERT_NE(pipeline, nullptr);

  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  auto encoder = cmdBuffer->createRenderCommandEncoder(
      {.colorAttachments = {{.loadAction = LoadAction::Clear,
                             .storeAction = StoreAction::MsaaResolve,
                             .clearColor = {0, 0, 0, 1}}}},
      framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(pipeline);
  encoder->draw(3);
  encoder->endEncoding();
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  std::array<uint32_t, kSize * kSize> pixels = {};
  ASSERT_TRUE(
      resolve
          ->getBytes(
              TextureRangeDesc::new2D(0, 0, kSize, kSize), WGPUTextureAspect_All, pixels.data(), 0)
          .isOk());
  for (const uint32_t pixel : pixels) {
    EXPECT_EQ(pixel, kWhite);
  }
  std::array<uint32_t, kSize * kSize> unused = {};
  EXPECT_EQ(
      msaa->getBytes(
              TextureRangeDesc::new2D(0, 0, kSize, kSize), WGPUTextureAspect_All, unused.data(), 0)
          .code,
      Result::Code::Unsupported);

  // Framebuffer copies of a multisampled color attachment read its resolve texture.
  const auto range = TextureRangeDesc::new2D(0, 0, kSize, kSize);
  std::array<uint32_t, kSize * kSize> readBack = {};
  framebuffer->copyBytesColorAttachment(*queue_, 0, readBack.data(), range);
  for (const uint32_t pixel : readBack) {
    EXPECT_EQ(pixel, kWhite);
  }
  auto copy = createTexture(TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(copy, nullptr);
  framebuffer->copyTextureColorAttachment(*queue_, 0, copy, range);
  std::array<uint32_t, kSize * kSize> copied = {};
  ASSERT_TRUE(copy->getBytes(range, WGPUTextureAspect_All, copied.data(), 0).isOk());
  for (const uint32_t pixel : copied) {
    EXPECT_EQ(pixel, kWhite);
  }

  // A surface without MSAA has no resolve texture.
  auto single = createTexture(TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment));
  ASSERT_NE(single, nullptr);
  framebuffer->updateDrawable({.color = msaa, .colorResolve = resolve});
  framebuffer->updateDrawable({.color = single});
  EXPECT_EQ(framebuffer->getColorAttachment(0), single);
  EXPECT_EQ(framebuffer->getResolveColorAttachment(0), nullptr);
}

TEST_F(WebGPUTextureTypesTest, DisabledMipFilterSamplesBaseLevel) {
  TextureDesc desc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Sampled);
  desc.numMipLevels = 2;
  auto texture = createTexture(desc);
  ASSERT_NE(texture, nullptr);
  const std::vector<uint32_t> level0(kSize * kSize, kWhite);
  const std::vector<uint32_t> level1(kSize * kSize / 4, kBlack);
  ASSERT_TRUE(
      texture->upload(TextureRangeDesc::new2D(0, 0, kSize, kSize, 0), level0.data()).isOk());
  ASSERT_TRUE(texture->upload(TextureRangeDesc::new2D(0, 0, kSize / 2, kSize / 2, 1), level1.data())
                  .isOk());
  Result ret;
  auto sampler = device_->createSamplerState(
      {.mipFilter = SamplerMipFilter::Disabled, .mipLodMin = 1, .mipLodMax = 2}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  constexpr const char* kSampleFragment = R"(
@group(0) @binding(0) var tex : texture_2d<f32>;
@group(0) @binding(1) var samp : sampler;

@fragment
fn main(@builtin(position) position : vec4f) -> @location(0) vec4f {
  return textureSample(tex, samp, position.xy / 4.0);
}
)";
  auto stages = ShaderStagesCreator::fromModuleStringInput(
      *device_, kVertex, "main", "", kSampleFragment, "main", "", &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto pipeline = device_->createRenderPipeline(
      {.shaderStages = std::move(stages),
       .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}}}},
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto color = createTexture(TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment));
  auto framebuffer = device_->createFramebuffer({.colorAttachments = {{.texture = color}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  auto encoder = cmdBuffer->createRenderCommandEncoder(
      {.colorAttachments = {{.loadAction = LoadAction::Clear,
                             .storeAction = StoreAction::Store,
                             .clearColor = {1, 0, 0, 1}}}},
      framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(pipeline);
  encoder->bindTexture(0, texture.get());
  encoder->bindSamplerState(0, BindTarget::kFragment, sampler.get());
  encoder->draw(3);
  encoder->endEncoding();
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  std::array<uint32_t, kSize * kSize> pixels = {};
  ASSERT_TRUE(
      color
          ->getBytes(
              TextureRangeDesc::new2D(0, 0, kSize, kSize), WGPUTextureAspect_All, pixels.data(), 0)
          .isOk());
  for (const uint32_t pixel : pixels) {
    EXPECT_EQ(pixel, kWhite);
  }
}

TEST_F(WebGPUTextureTypesTest, DepthCopyToBufferWithUnalignedRows) {
  constexpr uint32_t kWidth = 3;
  auto depth = createTexture(TextureDesc::new2D(
      TextureFormat::Z_UNorm16, kWidth, kSize, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(depth, nullptr);
  std::vector<uint16_t> texels(kWidth * kSize);
  for (size_t i = 0; i < texels.size(); ++i) {
    texels[i] = static_cast<uint16_t>(i % 2 == 0 ? 0 : 0xffff);
  }
  ASSERT_TRUE(depth->upload(depth->getFullRange(), texels.data()).isOk());

  Result ret;
  auto buffer = device_->createBuffer(
      {.type = BufferDesc::BufferTypeBits::Storage, .length = texels.size() * sizeof(uint16_t)},
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  cmdBuffer->copyTextureToBuffer(*depth, *buffer, 0);
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  const auto* data =
      static_cast<const uint16_t*>(buffer->map({texels.size() * sizeof(uint16_t), 0}, &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(std::vector<uint16_t>(data, data + texels.size()), texels);
  buffer->unmap();
}

TEST_F(WebGPUTextureTypesTest, SrgbViewOfLinearTexture) {
  auto texture = createTexture(TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment));
  ASSERT_NE(texture, nullptr);
  Result ret;
  auto srgb = device_->createTextureView(
      texture, {.format = TextureFormat::RGBA_SRGB, .numLayers = 1, .numMipLevels = 1}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(srgb, nullptr);
  EXPECT_EQ(srgb->getFormat(), TextureFormat::RGBA_SRGB);

  auto framebuffer = device_->createFramebuffer({.colorAttachments = {{.texture = srgb}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  auto encoder = cmdBuffer->createRenderCommandEncoder(
      {.colorAttachments = {{.loadAction = LoadAction::Clear,
                             .storeAction = StoreAction::Store,
                             .clearColor = {0.5f, 0.5f, 0.5f, 1.0f}}}},
      framebuffer);
  encoder->endEncoding();
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  // Linear 0.5 is stored sRGB-encoded (188) in the linear texture.
  std::array<uint8_t, 4> pixel = {};
  ASSERT_TRUE(
      texture->getBytes(TextureRangeDesc::new2D(0, 0, 1, 1), WGPUTextureAspect_All, pixel.data(), 0)
          .isOk());
  EXPECT_EQ(pixel, (std::array<uint8_t, 4>{188, 188, 188, 255}));

  EXPECT_EQ(
      device_->createTextureView(
          texture, {.format = TextureFormat::RGBA_F16, .numLayers = 1, .numMipLevels = 1}, &ret),
      nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST_F(WebGPUTextureTypesTest, ThreeDSlices) {
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::Texture3D));
  auto texture = createTexture(TextureDesc::new3D(
      TextureFormat::RGBA_UNorm8, 2, 2, 3, TextureDesc::TextureUsageBits::Sampled));
  ASSERT_NE(texture, nullptr);
  std::vector<uint32_t> texels(2 * 2 * 3);
  for (size_t i = 0; i < texels.size(); ++i) {
    texels[i] = 0x01010101u * static_cast<uint32_t>(i + 1);
  }
  ASSERT_TRUE(texture->upload(texture->getFullRange(), texels.data()).isOk());
  for (uint32_t z = 0; z < 3; ++z) {
    std::array<uint32_t, 4> slice = {};
    ASSERT_TRUE(texture
                    ->getBytes(TextureRangeDesc::new3D(0, 0, z, 2, 2, 1),
                               WGPUTextureAspect_All,
                               slice.data(),
                               0,
                               /*flipVertically=*/false)
                    .isOk());
    EXPECT_EQ(std::memcmp(slice.data(), texels.data() + 4 * z, sizeof(slice)), 0) << "slice " << z;
  }
}

} // namespace igl::tests
