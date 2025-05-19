/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "util/Common.h"

#include <igl/DepthStencilState.h>
#include <igl/RenderPipelineState.h>

namespace igl::tests {

//
// ResourceTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class ResourceTest : public ::testing::Test {
 public:
  ResourceTest() = default;
  ~ResourceTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
  }

  void TearDown() override {}

  // Member variables
 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  struct FragmentFormat {
    simd::float3 color;
  };
  FragmentFormat fragmentParameters_{};
};

//
// CreateRenderPipelineReturnNull
//
// Make sure that createRenderPipeline() returns nullptr on error.
// We used to return a partially initialized object on error, and this
// was causing difficult to reproduce crashes in production.
//
TEST_F(ResourceTest, CreateRenderPipelineReturnNull) {
  Result ret;

  const RenderPipelineDesc desc;
  std::shared_ptr<IRenderPipelineState> rps;

  // Sending in the blank desc should give an error since the shader modules
  // are nullptr
  rps = iglDev_->createRenderPipeline(desc, &ret);

  ASSERT_TRUE(!ret.isOk());
  ASSERT_TRUE(rps == nullptr);
}

//
// Depth Stencil
//
// Check creation of depth stencil
//
TEST_F(ResourceTest, DepthStencilCreate) {
  Result ret;

  const DepthStencilStateDesc dsDesc = {};
  std::shared_ptr<IDepthStencilState> ds;

  ds = iglDev_->createDepthStencilState(dsDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(ds != nullptr);
}

//
// Buffer
//
// Check creation of vertex buffer
//
TEST_F(ResourceTest, VertexBuffer) {
  Result ret;
  float vertexData[] = {1.0};
  const BufferDesc bufferDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData));
  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);
}

//
// Buffer
//
// Check creation of uninitialized vertex buffer
//
TEST_F(ResourceTest, UninitializedVertexBuffer) {
  Result ret;

  const int bufferLength = 64;
  const BufferDesc bufferDesc = BufferDesc(
      BufferDesc::BufferTypeBits::Vertex, nullptr, bufferLength, ResourceStorage::Shared);
  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);
  ASSERT_EQ(buffer->getSizeInBytes(), bufferLength);
}

//
// Buffer
//
// Check creation of index buffer
//
TEST_F(ResourceTest, IndexBuffer) {
  Result ret;
  uint16_t indexData[] = {
      0,
      1,
      2,
      1,
      3,
      2,
  };
  const BufferDesc bufferDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);
}

//
// Buffer
//
// Check creation of uniform buffer
//
TEST_F(ResourceTest, UniformBuffer) {
  Result ret;

  fragmentParameters_.color = {1.0f, 1.0f, 1.0f};
  BufferDesc bufferDesc;
  bufferDesc.type = BufferDesc::BufferTypeBits::Uniform;
  bufferDesc.data = &fragmentParameters_;
  bufferDesc.length = sizeof(fragmentParameters_);

  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);
}

} // namespace igl::tests
