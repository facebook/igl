/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/uniform/Descriptor.h>
#include <IGLU/uniform/Encoder.h>
#include <glm/glm.hpp>
#include <igl/RenderCommandEncoder.h>

namespace iglu::tests {

namespace {

constexpr int kVertexIndex = 3;
constexpr int kFragmentIndex = 5;

struct BindBytesCall {
  size_t count = 0;
  size_t index = 0;
  uint8_t target = 0;
  const void* data = nullptr;
  size_t length = 0;
};

// Records bindBytes(); every other command is unreachable from uniform::Encoder.
class RecordingRenderEncoder final : public igl::IRenderCommandEncoder {
 public:
  RecordingRenderEncoder() : IRenderCommandEncoder(nullptr) {}

  BindBytesCall bindBytesCall;
  size_t bindUniformCount = 0;

  void bindBytes(size_t index, uint8_t target, const void* data, size_t length) final {
    bindBytesCall = {.count = bindBytesCall.count + 1,
                     .index = index,
                     .target = target,
                     .data = data,
                     .length = length};
  }
  void bindUniform(const igl::UniformDesc& /*uniformDesc*/, const void* /*data*/) final {
    ++bindUniformCount;
  }

  void endEncoding() final {}
  void pushDebugGroupLabel(const char* /*label*/, const igl::Color& /*color*/) const final {}
  void insertDebugEventLabel(const char* /*label*/, const igl::Color& /*color*/) const final {}
  void popDebugGroupLabel() const final {}
  void bindViewport(const igl::Viewport& /*viewport*/) final {}
  void bindScissorRect(const igl::ScissorRect& /*rect*/) final {}
  void bindRenderPipelineState(
      const std::shared_ptr<igl::IRenderPipelineState>& /*pipelineState*/) final {}
  void bindDepthStencilState(
      const std::shared_ptr<igl::IDepthStencilState>& /*depthStencilState*/) final {}
  void bindBuffer(uint32_t /*index*/,
                  uint8_t /*target*/,
                  igl::IBuffer* /*buffer*/,
                  size_t /*bufferOffset*/,
                  size_t /*bufferSize*/) final {}
  void bindBuffer(uint32_t /*index*/,
                  igl::IBuffer* /*buffer*/,
                  size_t /*bufferOffset*/,
                  size_t /*bufferSize*/) final {}
  void bindVertexBuffer(uint32_t /*index*/,
                        igl::IBuffer& /*buffer*/,
                        size_t /*bufferOffset*/,
                        size_t /*attributeStride*/) final {}
  void bindIndexBuffer(igl::IBuffer& /*buffer*/,
                       igl::IndexFormat /*format*/,
                       size_t /*bufferOffset*/) final {}
  void bindPushConstants(const void* /*data*/, size_t /*length*/, size_t /*offset*/) final {}
  void bindSamplerState(size_t /*index*/,
                        uint8_t /*target*/,
                        igl::ISamplerState* /*samplerState*/) final {}
  void bindTexture(size_t /*index*/, uint8_t /*target*/, igl::ITexture* /*texture*/) final {}
  void bindTexture(size_t /*index*/, igl::ITexture* /*texture*/) final {}
  void bindBindGroup(igl::BindGroupTextureHandle /*handle*/) final {}
  void bindBindGroup(igl::BindGroupBufferHandle /*handle*/,
                     uint32_t /*numDynamicOffsets*/,
                     const uint32_t* /*dynamicOffsets*/) final {}
  void draw(size_t /*vertexCount*/,
            uint32_t /*instanceCount*/,
            uint32_t /*firstVertex*/,
            uint32_t /*baseInstance*/) final {}
  void drawIndexed(size_t /*indexCount*/,
                   uint32_t /*instanceCount*/,
                   uint32_t /*firstIndex*/,
                   int32_t /*vertexOffset*/,
                   uint32_t /*baseInstance*/) final {}
  void drawMeshTasks(const igl::Dimensions& /*threadgroupsPerGrid*/,
                     const igl::Dimensions& /*threadsPerTaskThreadgroup*/,
                     const igl::Dimensions& /*threadsPerMeshThreadgroup*/) final {}
  void multiDrawIndirect(igl::IBuffer& /*indirectBuffer*/,
                         size_t /*indirectBufferOffset*/,
                         uint32_t /*drawCount*/,
                         uint32_t /*stride*/) final {}
  void multiDrawIndexedIndirect(igl::IBuffer& /*indirectBuffer*/,
                                size_t /*indirectBufferOffset*/,
                                uint32_t /*drawCount*/,
                                uint32_t /*stride*/) final {}
  void setStencilReferenceValue(uint32_t /*value*/) final {}
  void setBlendColor(const igl::Color& /*color*/) final {}
  void setCullMode(igl::CullMode /*cullMode*/) final {}
  void setDepthBias(float /*depthBias*/, float /*slopeScale*/, float /*clamp*/) final {}
  void setFrontFacingWinding(igl::WindingMode /*frontFaceWinding*/) final {}
};

} // namespace

class UniformEncoderTest : public ::testing::Test {
 public:
  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    // mat3 is padded to three vec4 columns when aligned, so a packed upload is distinguishable.
    uniform_.setIndex(igl::ShaderStage::Vertex, kVertexIndex);
    uniform_.setIndex(igl::ShaderStage::Fragment, kFragmentIndex);
  }

 protected:
  uniform::DescriptorValue<glm::mat3> uniform_{glm::mat3(1.0f)};
};

TEST_F(UniformEncoderTest, MetalAndWebGPUBindAlignedBytes) {
  for (const igl::BackendType backend : {igl::BackendType::Metal, igl::BackendType::WebGPU}) {
    const uniform::Encoder encoder(backend);

    RecordingRenderEncoder vertex;
    encoder(vertex, igl::BindTarget::kVertex, uniform_);
    EXPECT_EQ(vertex.bindBytesCall.count, 1u);
    EXPECT_EQ(vertex.bindBytesCall.index, static_cast<size_t>(kVertexIndex));
    EXPECT_EQ(vertex.bindBytesCall.target, igl::BindTarget::kVertex);
    EXPECT_EQ(vertex.bindBytesCall.data, uniform_.data(uniform::Alignment::Aligned));
    EXPECT_EQ(vertex.bindBytesCall.length, uniform_.numBytes(uniform::Alignment::Aligned));
    EXPECT_NE(vertex.bindBytesCall.length, uniform_.numBytes(uniform::Alignment::Packed));

    RecordingRenderEncoder fragment;
    encoder(fragment, igl::BindTarget::kFragment, uniform_);
    EXPECT_EQ(fragment.bindBytesCall.count, 1u);
    EXPECT_EQ(fragment.bindBytesCall.index, static_cast<size_t>(kFragmentIndex));
    EXPECT_EQ(fragment.bindBytesCall.target, igl::BindTarget::kFragment);
    EXPECT_EQ(fragment.bindUniformCount, 0u);
  }
}

TEST_F(UniformEncoderTest, VulkanBindsNothing) {
  const uniform::Encoder encoder(igl::BackendType::Vulkan);
  RecordingRenderEncoder render;
  encoder(render, igl::BindTarget::kVertex, uniform_);
  EXPECT_EQ(render.bindBytesCall.count, 0u);
  EXPECT_EQ(render.bindUniformCount, 0u);
}

TEST_F(UniformEncoderTest, UnassignedIndexBindsNothing) {
  uniform::DescriptorValue<glm::mat3> unassigned(glm::mat3(1.0f));
  const uniform::Encoder encoder(igl::BackendType::WebGPU);
  RecordingRenderEncoder render;
  encoder(render, igl::BindTarget::kVertex, unassigned);
  EXPECT_EQ(render.bindBytesCall.count, 0u);
}

} // namespace iglu::tests
