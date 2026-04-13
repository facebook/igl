/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "data/ShaderData.h"
#include "util/Color.h"
#include "util/Common.h"
#include "util/TextureValidationHelpers.h"

#include <array>
#include <cstdint>
#include <memory>
#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/ComputePipelineState.h>
#include <igl/NameHandle.h>
#include <igl/Shader.h>
#include <igl/ShaderCreator.h>
#include <igl/Texture.h>

namespace igl::tests {

namespace {

// Orange in sRGB: (255, 128, 0, 255) = (1.0, 0.502, 0.0, 1.0)
constexpr uint8_t kOrangeR = 255;
constexpr uint8_t kOrangeG = 128;
constexpr uint8_t kOrangeB = 0;
constexpr uint8_t kOrangeA = 255;

constexpr uint32_t kTexWidth = 2;
constexpr uint32_t kTexHeight = 2;

constexpr std::string_view kColorBufName = "colorBuf";
constexpr size_t kColorBufIndex = 1;

// clang-format off

//
// Compute shaders that write a color (passed via buffer) to every pixel of a storage image.
// The color is passed as a float4 so we can control exactly what value is written per-backend
// (e.g., linear orange for Metal sRGB, direct orange for Vulkan sRGB).
//

constexpr std::string_view kOglImageStoreShader =
  IGL_TO_STRING(VERSION(310 es)
    precision highp float;
    layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout (rgba8, binding = 0) writeonly uniform highp image2D destImage;
    layout (std430, binding = 1) readonly buffer colorBuf {
      vec4 color;
    };

    void main() {
      ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
      imageStore(destImage, pos, color);
    });

constexpr std::string_view kMtlImageStoreShader =
  IGL_TO_STRING(using namespace metal;

    kernel void imageStoreOrange(
        texture2d<float, access::write> destImage [[texture(0)]],
        device float4* colorBuf [[buffer(1)]],
        uint2 gid [[thread_position_in_grid]]) {
      destImage.write(colorBuf[0], gid);
    });

constexpr std::string_view kVulkanImageStoreShader =
  IGL_TO_STRING(
    layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout (rgba8, binding = 0, set = 2) writeonly uniform image2D destImage;
    layout (std430, binding = 1, set = 1) readonly buffer colorBuf {
      vec4 color;
    };

    void main() {
      ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
      imageStore(destImage, pos, color);
    });

constexpr std::string_view kD3D12ImageStoreShader =
  "RWTexture2D<float4> destImage : register(u0);\n"
  "cbuffer colorBuf : register(b1) { float4 color; };\n"
  "[numthreads(1, 1, 1)]\n"
  "void imageStoreOrange(uint3 gid : SV_DispatchThreadID) {\n"
  "  destImage[gid.xy] = color;\n"
  "}\n";

constexpr std::string_view kImageStoreFunc = "imageStoreOrange";

// clang-format on

} // namespace

class ComputeImageStoreTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  void runImageStoreTest(TextureFormat format);
};

void ComputeImageStoreTest::runImageStoreTest(TextureFormat format) {
  if (!iglDev_->hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "Compute not supported";
  }

  const auto formatCaps = iglDev_->getTextureFormatCapabilities(format);
  if ((formatCaps & ICapabilities::TextureFormatCapabilityBits::Storage) == 0) {
    GTEST_SKIP() << "Storage not supported for this format";
  }

  // Create storage texture
  const TextureDesc texDesc = TextureDesc::new2D(format,
                                                 kTexWidth,
                                                 kTexHeight,
                                                 TextureDesc::TextureUsageBits::Storage |
                                                     TextureDesc::TextureUsageBits::Attachment);

  Result ret;
  auto texture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(texture != nullptr);

  // Determine the color to write.
  // Metal applies sRGB conversion on imageStore, so for sRGB textures we feed the linear
  // equivalent so that after conversion we read back the expected sRGB orange.
  // Vulkan (with the MoltenVK fix) does NOT apply sRGB conversion on imageStore, so we
  // write the sRGB orange directly.
  const bool isSRGB = (format == TextureFormat::RGBA_SRGB);
  const bool isMetal = (iglDev_->getBackendType() == igl::BackendType::Metal);

  glm::vec4 writeColor(kOrangeR / 255.0f, kOrangeG / 255.0f, kOrangeB / 255.0f, kOrangeA / 255.0f);
  if (isSRGB && isMetal) {
    writeColor = util::convertSRGBToLinear(writeColor);
  }

  // Create color buffer
  const float colorData[4] = {writeColor.r, writeColor.g, writeColor.b, writeColor.a};
  const BufferDesc colorBufDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .data = colorData,
      .length = sizeof(colorData),
  };
  auto colorBuffer = iglDev_->createBuffer(colorBufDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(colorBuffer != nullptr);

  // Compile compute shader
  std::string_view source;
  std::string_view entryName;
  if (iglDev_->getBackendType() == igl::BackendType::OpenGL) {
    source = kOglImageStoreShader;
    entryName = kImageStoreFunc;
  } else if (iglDev_->getBackendType() == igl::BackendType::Vulkan) {
    source = kVulkanImageStoreShader;
    entryName = "main";
  } else if (isMetal) {
    source = kMtlImageStoreShader;
    entryName = kImageStoreFunc;
  } else if (iglDev_->getBackendType() == igl::BackendType::D3D12) {
    source = kD3D12ImageStoreShader;
    entryName = kImageStoreFunc;
  } else {
    GTEST_SKIP() << "Unsupported backend";
  }

  auto computeStages = ShaderStagesCreator::fromModuleStringInput(
      *iglDev_, source.data(), std::string(entryName), "", &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(computeStages != nullptr);

  // Create compute pipeline
  ComputePipelineDesc computeDesc;
  computeDesc.shaderStages = std::move(computeStages);
  computeDesc.buffersMap[kColorBufIndex] = IGL_NAMEHANDLE(kColorBufName);
  auto computePipeline = iglDev_->createComputePipeline(computeDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(computePipeline != nullptr);

  // Dispatch compute
  auto cmdBuffer = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(cmdBuffer != nullptr);

  auto computeEncoder = cmdBuffer->createComputeCommandEncoder();
  ASSERT_TRUE(computeEncoder != nullptr);

  computeEncoder->bindComputePipelineState(computePipeline);
  computeEncoder->bindImageTexture(0, texture.get(), format);
  computeEncoder->bindBuffer(kColorBufIndex, colorBuffer.get());

  const Dimensions threadgroupSize(1, 1, 1);
  const Dimensions threadgroupCount(kTexWidth, kTexHeight, 1);
  computeEncoder->dispatchThreadGroups(threadgroupCount, threadgroupSize, {});
  computeEncoder->endEncoding();

  cmdQueue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  // Read back and validate — expect the sRGB orange values
  const uint32_t kOrangePixel =
      (static_cast<uint32_t>(kOrangeA) << 24) | (static_cast<uint32_t>(kOrangeB) << 16) |
      (static_cast<uint32_t>(kOrangeG) << 8) | static_cast<uint32_t>(kOrangeR);

  std::array<uint32_t, kTexWidth * kTexHeight> expectedData{};
  expectedData.fill(kOrangePixel);

  util::validateTextureRange(*iglDev_,
                             *cmdQueue_,
                             texture,
                             false,
                             TextureRangeDesc::new2D(0, 0, kTexWidth, kTexHeight),
                             expectedData.data(),
                             "imageStore should produce correct orange after accounting for sRGB");
}

TEST_F(ComputeImageStoreTest, ImageStoreRGBA) {
  runImageStoreTest(TextureFormat::RGBA_UNorm8);
}

TEST_F(ComputeImageStoreTest, ImageStoreRGBASRGB) {
  runImageStoreTest(TextureFormat::RGBA_SRGB);
}

} // namespace igl::tests
