/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../data/ShaderData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"

namespace igl::tests {

//
// ResourceOGLTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class ResourceOGLTest : public ::testing::Test {
 public:
  ResourceOGLTest() = default;
  ~ResourceOGLTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    Result ret;

    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    igl::tests::util::createSimpleShaderStages(iglDev_, stages);
    shaderStages_ = std::move(stages);

    // Initialize input to vertex shader
    VertexInputStateDesc inputDesc;

    inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
    inputDesc.attributes[0].offset = 0;
    inputDesc.attributes[0].bufferIndex = data::shader::simplePosIndex;
    inputDesc.attributes[0].name = data::shader::simplePos;
    inputDesc.attributes[0].location = 0;
    inputDesc.inputBindings[0].stride = sizeof(float) * 4;

    inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
    inputDesc.attributes[1].offset = 0;
    inputDesc.attributes[1].bufferIndex = data::shader::simpleUvIndex;
    inputDesc.attributes[1].name = data::shader::simpleUv;
    inputDesc.attributes[1].location = 1;
    inputDesc.inputBindings[1].stride = sizeof(float) * 2;

    vertexInputState_ = iglDev_->createVertexInputState(inputDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vertexInputState_ != nullptr);

    // Initialize Graphics Pipeline Descriptor, but leave the creation
    // to the individual tests in case further customization is required
    renderPipelineDesc_.vertexInputState = vertexInputState_;
    renderPipelineDesc_.shaderStages = shaderStages_;
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> cmdBuf_;

  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
  RenderPipelineDesc renderPipelineDesc_;
};

//
// UniformBuffer Initialize Test
//
// Tests the initialize() function of igl::opengl::UniformBuffer
// Tests normal setup.
// Tests a failed setup where the data length is 0.
// Tests a failed setup where the data in null.
//
TEST_F(ResourceOGLTest, UniformBufferInitialize) {
  // Test dynamic draw setup.
  char data[100];
  BufferDesc desc =
      BufferDesc(BufferDesc::BufferTypeBits::Uniform, &data, sizeof(data), ResourceStorage::Shared);
  Result res;
  auto framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);
  ASSERT_EQ(res.code, Result::Code::Ok);
  ASSERT_EQ(framebuffer->getSizeInBytes(), sizeof(data));

  // Test dynamic draw setup.
  desc = BufferDesc(BufferDesc::BufferTypeBits::Uniform, &data, 0, ResourceStorage::Shared);
  framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);
  ASSERT_EQ(res.code, Result::Code::ArgumentOutOfRange);
  ASSERT_EQ(framebuffer->getSizeInBytes(), 0);

  desc = BufferDesc(BufferDesc::BufferTypeBits::Uniform, nullptr, 0, ResourceStorage::Shared);
  framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);
  ASSERT_EQ(res.code, Result::Code::ArgumentNull);
  ASSERT_EQ(framebuffer->getSizeInBytes(), 0);
}

//
// UniformBuffer Upload Test
//
// Tests the upload() function of igl::opengl::UniformBuffer
// Tests a normal upload of 100 bytes
// Tests an upload where the buffer range is too long.
//
TEST_F(ResourceOGLTest, UniformBufferUpload) {
  // Test dynamic draw setup.

  Result res;
  char data[150];
  const BufferDesc desc =
      BufferDesc(BufferDesc::BufferTypeBits::Uniform, &data, sizeof(data), ResourceStorage::Shared);
  auto framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);

  char newData[100];

  // copy newData (100 bytes) into buffer (size 150) starting at offset 30
  uintptr_t offset = 30;
  res = framebuffer->upload(&newData, BufferRange(sizeof(newData), offset));
  ASSERT_EQ(res.code, Result::Code::Ok);

  // try to copy newData (100 bytes) into buffer (size 150) starting at offset 60
  offset = 60;
  res = framebuffer->upload(&newData, BufferRange(sizeof(newData), offset));
  ASSERT_EQ(res.code, Result::Code::ArgumentOutOfRange);
}

//
// ArrayBuffer Initialize Test
//
// Tests the initialize() function of igl::opengl::ArrayBuffer
// Exercise all the success and failure paths
// Test normal dynamic and static draw setups.
// Test failed static draw setup with a nullptr argument.
//
TEST_F(ResourceOGLTest, ArrayBufferInitialize) {
  // Test dynamic draw setup.
  BufferDesc desc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, nullptr, 0, ResourceStorage::Shared);
  Result res;
  auto framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);
  ASSERT_EQ(res.code, Result::Code::Ok);
  ASSERT_EQ(framebuffer->getSizeInBytes(), 0);

  // Test static draw setup.
  char data[100];
  desc = BufferDesc(BufferDesc::BufferTypeBits::Index, &data, 0, ResourceStorage::Managed);
  framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);
  ASSERT_EQ(res.code, Result::Code::Ok);
  ASSERT_EQ(framebuffer->getSizeInBytes(), 0);

  // Test static draw setup with null ptr to the data.
  desc = BufferDesc(BufferDesc::BufferTypeBits::Index, nullptr, 0, ResourceStorage::Managed);
  framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);
  ASSERT_EQ(res.code, Result::Code::ArgumentNull);
  ASSERT_EQ(framebuffer->getSizeInBytes(), 0);
}

//
// ArrayBuffer Upload Test
//
// Tests the upload() function of igl::opengl::ArrayBuffer
// Exercise all the success and failure paths
// Test an upload attempt to a ResourceStorage::Managed buffer for failure.
// Test a normal upload attempt to a ResourceStorage::Shared buffer.
//
TEST_F(ResourceOGLTest, ArrayBufferUpload) {
  // Test dynamic draw upload.
  char data[100];
  BufferDesc desc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, &data, sizeof(data), ResourceStorage::Managed);
  Result res;
  auto framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);
  res = framebuffer->upload(&data, BufferRange(sizeof(data), 1));
  ASSERT_EQ(res.code, Result::Code::InvalidOperation);

  // Test normal static draw upload.
  desc = BufferDesc(BufferDesc::BufferTypeBits::Index, nullptr, 0, ResourceStorage::Shared);
  framebuffer = ResourceOGLTest::iglDev_->createBuffer(desc, &res);
  res = framebuffer->upload(&data, BufferRange(0L, 0L));
  ASSERT_EQ(res.code, Result::Code::Ok);
}

//
// Shader Create Test
//
// Tests the create() function of igl::opengl::ShaderStages
// Exercise the success path
// Test the successful linking of a vertex and fragment shader.
//
TEST_F(ResourceOGLTest, ShaderCreate1) {
  Result res;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  // success path
  pipelineState = ResourceOGLTest::iglDev_->createRenderPipeline(renderPipelineDesc_, &res);
  ASSERT_EQ(res.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);
}

} // namespace igl::tests
