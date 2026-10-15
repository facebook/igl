/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Runs Slang programs compiled by tools/slang/generate_slang.py on the current backend and checks
// that every resource binds where IGL expects it.

#include <gtest/gtest.h>

#include "slang/generated/SlangAutodiffSlang.h"
#include "slang/generated/SlangInstancedTargetsSlang.h"
#include "slang/generated/SlangQuadSlang.h"
#include "slang/generated/SlangReduceSlang.h"
#include "util/Common.h"

#include <IGLU/slang/SlangProgram.h>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>
#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/RenderPass.h>
#include <igl/SamplerState.h>
#include <igl/Texture.h>
#include <igl/VertexInputState.h>

namespace igl::tests {

namespace {

constexpr uint32_t kTargetSize = 64;

using Rgba = std::array<int, 4>;

std::shared_ptr<IBuffer> createStorageBuffer(IDevice& device,
                                             const void* data,
                                             size_t length,
                                             size_t stride) {
  const BufferDesc desc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .data = data,
      .length = length,
      .storage = ResourceStorage::Shared,
      .storageStride = stride,
  };
  Result ret;
  auto buffer = device.createBuffer(desc, &ret);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  return buffer;
}

template<typename T>
std::vector<T> readBuffer(IBuffer& buffer, size_t count) {
  std::vector<T> values(count);
  Result ret;
  const void* data = buffer.map(BufferRange(sizeof(T) * count, 0), &ret);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  if (data != nullptr) {
    std::memcpy(values.data(), data, sizeof(T) * count);
    buffer.unmap();
  }
  return values;
}

float sigmoid(float x) {
  return 1.0f / (1.0f + std::exp(-x));
}

} // namespace

class SlangConformanceTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, queue_);
    ASSERT_NE(device_, nullptr);
    ASSERT_NE(queue_, nullptr);
  }

 protected:
  std::shared_ptr<IFramebuffer> createTargets(size_t count) {
    FramebufferDesc desc;
    for (size_t i = 0; i < count; ++i) {
      Result ret;
      desc.colorAttachments[i].texture =
          device_->createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                    kTargetSize,
                                                    kTargetSize,
                                                    TextureDesc::TextureUsageBits::Sampled |
                                                        TextureDesc::TextureUsageBits::Attachment),
                                 &ret);
      EXPECT_TRUE(ret.isOk()) << ret.message;
    }
    Result ret;
    auto framebuffer = device_->createFramebuffer(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return framebuffer;
  }

  static RenderPassDesc clearPass(size_t count) {
    RenderPassDesc renderPass;
    renderPass.colorAttachments.resize(count);
    for (auto& attachment : renderPass.colorAttachments) {
      attachment.loadAction = LoadAction::Clear;
      attachment.storeAction = StoreAction::Store;
      attachment.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    return renderPass;
  }

  // Reads through a framebuffer holding just this target: OpenGL copies only attachment 0.
  std::vector<uint32_t> readTarget(IFramebuffer& framebuffer, size_t index) {
    std::vector<uint32_t> pixels(static_cast<size_t>(kTargetSize) * kTargetSize);
    Result ret;
    const auto single = device_->createFramebuffer(
        FramebufferDesc{.colorAttachments = {{.texture = framebuffer.getColorAttachment(index)}}},
        &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    if (single != nullptr) {
      single->copyBytesColorAttachment(
          *queue_, 0, pixels.data(), TextureRangeDesc::new2D(0, 0, kTargetSize, kTargetSize));
    }
    return pixels;
  }

  // The test images are symmetric about the horizontal center line, so the vertical flip some
  // backends apply in copyBytesColorAttachment() does not matter.
  static Rgba pixel(const std::vector<uint32_t>& pixels, uint32_t x, uint32_t y) {
    const uint32_t value = pixels[static_cast<size_t>(y) * kTargetSize + x];
    return {static_cast<int>(value & 0xff),
            static_cast<int>((value >> 8) & 0xff),
            static_cast<int>((value >> 16) & 0xff),
            static_cast<int>(value >> 24)};
  }

  static void expectPixel(const std::vector<uint32_t>& pixels,
                          uint32_t x,
                          uint32_t y,
                          const Rgba& expected) {
    const Rgba actual = pixel(pixels, x, y);
    for (size_t c = 0; c < 4; ++c) {
      EXPECT_NEAR(actual[c], expected[c], 2) << "channel " << c << " at (" << x << ", " << y << ")";
    }
  }

  void runCompute(const std::shared_ptr<IComputePipelineState>& pipeline,
                  const std::vector<IBuffer*>& buffers,
                  const Dimensions& groups,
                  const Dimensions& groupSize) {
    auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
    ASSERT_NE(cmdBuffer, nullptr);
    auto encoder = cmdBuffer->createComputeCommandEncoder();
    ASSERT_NE(encoder, nullptr);
    encoder->bindComputePipelineState(pipeline);
    for (size_t i = 0; i < buffers.size(); ++i) {
      encoder->bindBuffer(static_cast<uint32_t>(i), buffers[i]);
    }
    encoder->dispatchThreadGroups(groups, groupSize);
    encoder->endEncoding();
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
  }

  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> queue_;
};

TEST_F(SlangConformanceTest, ReduceUsesStorageBuffersGroupsharedAndAtomics) {
  if (!device_->hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "No compute";
  }
  const iglu::slang::Program& program = slang::kSlangReduceProgram;
  Result ret;
  std::shared_ptr<IShaderStages> stages =
      iglu::slang::createComputeStages(*device_, program, "reduceGroups", &ret);
  if (ret.code == Result::Code::Unsupported) {
    GTEST_SKIP() << ret.message;
  }
  ASSERT_TRUE(ret.isOk()) << ret.message;

  std::array<float, 256> input{};
  for (size_t i = 0; i < input.size(); ++i) {
    input[i] = static_cast<float>(i);
  }
  const std::array<float, 4> sums{};
  const uint32_t counter = 0;
  const std::array<uint32_t, 4> params = {200, 0, 0, 0};
  auto inputBuffer = createStorageBuffer(*device_, input.data(), sizeof(input), sizeof(float));
  auto sumsBuffer = createStorageBuffer(*device_, sums.data(), sizeof(sums), sizeof(float));
  auto counterBuffer = createStorageBuffer(*device_, &counter, sizeof(counter), sizeof(counter));
  auto paramsBuffer = createStorageBuffer(*device_, params.data(), sizeof(params), sizeof(params));
  ASSERT_TRUE(inputBuffer && sumsBuffer && counterBuffer && paramsBuffer);

  ComputePipelineDesc desc{.shaderStages = std::move(stages)};
  iglu::slang::addGlResourceNames(program, "reduceGroups", desc);
  auto pipeline = device_->createComputePipeline(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  runCompute(pipeline,
             {inputBuffer.get(), sumsBuffer.get(), counterBuffer.get(), paramsBuffer.get()},
             Dimensions(4, 1, 1),
             iglu::slang::workgroupSize(*iglu::slang::findEntryPoint(program, "reduceGroups")));

  // Sums of 0..63, 64..127, 128..191 and 192..199 (elements past count read as zero).
  const std::vector<float> groupSums = readBuffer<float>(*sumsBuffer, 4);
  EXPECT_FLOAT_EQ(groupSums[0], 2016.0f);
  EXPECT_FLOAT_EQ(groupSums[1], 6112.0f);
  EXPECT_FLOAT_EQ(groupSums[2], 10208.0f);
  EXPECT_FLOAT_EQ(groupSums[3], 1564.0f);
  EXPECT_EQ(readBuffer<uint32_t>(*counterBuffer, 1)[0], 4u);
}

TEST_F(SlangConformanceTest, AutodiffMatchesAnalyticDerivatives) {
  if (!device_->hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "No compute";
  }
  const iglu::slang::Program& program = slang::kSlangAutodiffProgram;
  Result ret;
  std::shared_ptr<IShaderStages> stages =
      iglu::slang::createComputeStages(*device_, program, "evaluateDerivatives", &ret);
  if (ret.code == Result::Code::Unsupported) {
    GTEST_SKIP() << ret.message;
  }
  ASSERT_TRUE(ret.isOk()) << ret.message;

  constexpr size_t kCount = 64;
  std::array<float, kCount * 2> inputs{};
  for (size_t i = 0; i < kCount; ++i) {
    inputs[2 * i] = -2.0f + 4.0f * static_cast<float>(i) / (kCount - 1);
    inputs[2 * i + 1] = 1.5f - 3.0f * static_cast<float>(i % 7) / 6.0f;
  }
  const std::array<float, kCount * 4> results{};
  auto inputBuffer =
      createStorageBuffer(*device_, inputs.data(), sizeof(inputs), sizeof(float) * 2);
  auto resultBuffer =
      createStorageBuffer(*device_, results.data(), sizeof(results), sizeof(float) * 4);
  ASSERT_TRUE(inputBuffer && resultBuffer);

  ComputePipelineDesc desc{.shaderStages = std::move(stages)};
  iglu::slang::addGlResourceNames(program, "evaluateDerivatives", desc);
  auto pipeline = device_->createComputePipeline(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  runCompute(
      pipeline,
      {inputBuffer.get(), resultBuffer.get()},
      Dimensions(1, 1, 1),
      iglu::slang::workgroupSize(*iglu::slang::findEntryPoint(program, "evaluateDerivatives")));

  const std::vector<float> actual = readBuffer<float>(*resultBuffer, kCount * 4);
  for (size_t i = 0; i < kCount; ++i) {
    const float x = inputs[2 * i];
    const float y = inputs[2 * i + 1];
    const float f = std::sin(x) * y + std::log(1.0f + std::exp(x * y)) + 0.5f * x * y;
    const float dfdx = std::cos(x) * y + sigmoid(x * y) * y;
    const float dfdy = std::sin(x) + sigmoid(x * y) * x + 0.5f * x;
    EXPECT_NEAR(actual[4 * i], f, 1e-4f) << "f at " << i;
    EXPECT_NEAR(actual[4 * i + 1], dfdx, 1e-4f) << "forward df/dx at " << i;
    EXPECT_NEAR(actual[4 * i + 2], dfdx, 1e-4f) << "reverse df/dx at " << i;
    EXPECT_NEAR(actual[4 * i + 3], dfdy, 1e-4f) << "reverse df/dy at " << i;
  }
}

TEST_F(SlangConformanceTest, QuadBindsVertexBufferUniformBufferAndTexture) {
  const iglu::slang::Program& program = slang::kSlangQuadProgram;
  Result ret;
  std::shared_ptr<IShaderStages> stages =
      iglu::slang::createRenderStages(*device_, program, "quadVertex", "quadFragment", &ret);
  if (ret.code == Result::Code::Unsupported) {
    GTEST_SKIP() << ret.message;
  }
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_EQ(program.numVertexAttributes, 2u);

  // A quad over [-0.5, 0.5]^2 with u running left to right.
  const std::array<float, 30> vertices = {
      -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
      -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f,
  };
  // A column-major translation by +0.5 in x moves the quad to x in [0, 1]; a transposed upload
  // would move it out of the picture instead.
  const std::array<float, 20> uniforms = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f,
  };
  const std::array<uint8_t, 8> texels = {255, 0, 0, 255, 0, 255, 0, 255};

  auto vertexBuffer = device_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Vertex,
                                                       .data = vertices.data(),
                                                       .length = sizeof(vertices)},
                                            &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto uniformBuffer =
      device_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                       .data = uniforms.data(),
                                       .length = sizeof(uniforms),
                                       .hint = BufferDesc::BufferAPIHintBits::UniformBlock},
                            &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto texture = device_->createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 2, 1, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  texture->upload(TextureRangeDesc::new2D(0, 0, 2, 1), texels.data());
  auto sampler = device_->createSamplerState(SamplerStateDesc{}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  const VertexInputStateDesc inputDesc{
      .numAttributes = 2,
      .attributes = {{.bufferIndex = 0,
                      .format = VertexAttributeFormat::Float3,
                      .offset = 0,
                      .name = program.vertexAttributes[0].name,
                      .location = program.vertexAttributes[0].location},
                     {.bufferIndex = 0,
                      .format = VertexAttributeFormat::Float2,
                      .offset = sizeof(float) * 3,
                      .name = program.vertexAttributes[1].name,
                      .location = program.vertexAttributes[1].location}},
      .numInputBindings = 1,
      .inputBindings = {{.stride = sizeof(float) * 5}},
  };
  auto vertexInput = device_->createVertexInputState(inputDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto framebuffer = createTargets(1);
  ASSERT_NE(framebuffer, nullptr);
  RenderPipelineDesc desc{
      .vertexInputState = vertexInput,
      .shaderStages = std::move(stages),
      .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}}},
      .cullMode = CullMode::Disabled,
  };
  iglu::slang::addGlResourceNames(program, "quadVertex", "quadFragment", desc);
  auto pipeline = device_->createRenderPipeline(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  ASSERT_NE(cmdBuffer, nullptr);
  auto encoder = cmdBuffer->createRenderCommandEncoder(clearPass(1), framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(pipeline);
  encoder->bindVertexBuffer(0, *vertexBuffer);
  encoder->bindBuffer(1, uniformBuffer.get());
  encoder->bindTexture(0, BindTarget::kFragment, texture.get());
  encoder->bindSamplerState(0, BindTarget::kFragment, sampler.get());
  encoder->draw(6);
  encoder->endEncoding();
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  const std::vector<uint32_t> pixels = readTarget(*framebuffer, 0);
  expectPixel(pixels, 8, 32, {0, 0, 0, 0});
  expectPixel(pixels, 40, 32, {255, 0, 0, 255});
  expectPixel(pixels, 56, 32, {0, 128, 0, 255});
  expectPixel(pixels, 40, 4, {0, 0, 0, 0});
}

TEST_F(SlangConformanceTest, InstancedQuadsBlendIntoTwoTargets) {
  if (!device_->hasFeature(DeviceFeatures::MultipleRenderTargets)) {
    GTEST_SKIP() << "No multiple render targets";
  }
  const iglu::slang::Program& program = slang::kSlangInstancedTargetsProgram;
  Result ret;
  std::shared_ptr<IShaderStages> stages = iglu::slang::createRenderStages(
      *device_, program, "instanceVertex", "instanceFragment", &ret);
  if (ret.code == Result::Code::Unsupported) {
    GTEST_SKIP() << ret.message;
  }
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_EQ(program.numVertexAttributes, 2u);

  // (center.xy, half extent.xy, color); the third instance overlaps the first.
  const std::array<float, 24> instances = {
      -0.5f, 0.0f, 0.25f, 0.25f, 0.5f,  0.0f, 0.0f,  0.0f,  0.5f,  0.0f, 0.25f, 0.25f,
      0.0f,  0.5f, 0.0f,  0.0f,  -0.5f, 0.0f, 0.25f, 0.25f, 0.25f, 0.0f, 0.0f,  0.0f,
  };
  auto instanceBuffer = device_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Vertex,
                                                         .data = instances.data(),
                                                         .length = sizeof(instances)},
                                              &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  const VertexInputStateDesc inputDesc{
      .numAttributes = 2,
      .attributes = {{.bufferIndex = 0,
                      .format = VertexAttributeFormat::Float4,
                      .offset = 0,
                      .name = program.vertexAttributes[0].name,
                      .location = program.vertexAttributes[0].location},
                     {.bufferIndex = 0,
                      .format = VertexAttributeFormat::Float4,
                      .offset = sizeof(float) * 4,
                      .name = program.vertexAttributes[1].name,
                      .location = program.vertexAttributes[1].location}},
      .numInputBindings = 1,
      .inputBindings = {{.stride = sizeof(float) * 8,
                         .sampleFunction = VertexSampleFunction::Instance}},
  };
  auto vertexInput = device_->createVertexInputState(inputDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto framebuffer = createTargets(2);
  ASSERT_NE(framebuffer, nullptr);
  const RenderPipelineDesc::TargetDesc::ColorAttachment additive{
      .textureFormat = TextureFormat::RGBA_UNorm8,
      .blendEnabled = true,
      .srcRGBBlendFactor = BlendFactor::One,
      .srcAlphaBlendFactor = BlendFactor::One,
      .dstRGBBlendFactor = BlendFactor::One,
      .dstAlphaBlendFactor = BlendFactor::One,
  };
  RenderPipelineDesc desc{
      .topology = PrimitiveType::TriangleStrip,
      .vertexInputState = vertexInput,
      .shaderStages = std::move(stages),
      .targetDesc = {.colorAttachments = {additive, additive}},
      .cullMode = CullMode::Disabled,
  };
  iglu::slang::addGlResourceNames(program, "instanceVertex", "instanceFragment", desc);
  auto pipeline = device_->createRenderPipeline(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  ASSERT_NE(cmdBuffer, nullptr);
  auto encoder = cmdBuffer->createRenderCommandEncoder(clearPass(2), framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(pipeline);
  encoder->bindVertexBuffer(0, *instanceBuffer);
  encoder->draw(4, 3);
  encoder->endEncoding();
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  const std::vector<uint32_t> color = readTarget(*framebuffer, 0);
  expectPixel(color, 16, 32, {191, 0, 0, 0});
  expectPixel(color, 48, 32, {0, 128, 0, 0});
  expectPixel(color, 32, 32, {0, 0, 0, 0});
  const std::vector<uint32_t> coverage = readTarget(*framebuffer, 1);
  expectPixel(coverage, 16, 32, {0, 0, 128, 128});
  expectPixel(coverage, 48, 32, {0, 0, 64, 64});
}

} // namespace igl::tests
