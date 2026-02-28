/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalBindGroupTest
//
// This test covers bind group creation and destruction on Metal.
//
class MetalBindGroupTest : public ::testing::Test {
 public:
  MetalBindGroupTest() = default;
  ~MetalBindGroupTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// CreateTextureBindGroup
//
// Test creating a bind group with textures.
//
TEST_F(MetalBindGroupTest, CreateTextureBindGroup) {
  Result res;

  // Create a texture to put in the bind group
  TextureDesc texDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  auto texture = device_->createTexture(texDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create a sampler
  SamplerStateDesc samplerDesc = SamplerStateDesc::newLinear();
  auto sampler = device_->createSamplerState(samplerDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  BindGroupTextureDesc bgDesc;
  bgDesc.textures[0] = texture;
  bgDesc.samplers[0] = sampler;
  bgDesc.debugName = "testTextureBindGroup";

  auto handle = device_->createBindGroup(bgDesc, nullptr, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_TRUE(handle.valid());
}

//
// CreateBufferBindGroup
//
// Test creating a bind group with buffers.
//
TEST_F(MetalBindGroupTest, CreateBufferBindGroup) {
  Result res;

  // Create a buffer
  const float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
  BufferDesc bufDesc(
      BufferDesc::BufferTypeBits::Uniform, data, sizeof(data), ResourceStorage::Shared);
  auto buffer = device_->createBuffer(bufDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  BindGroupBufferDesc bgDesc;
  bgDesc.buffers[0] = std::shared_ptr<IBuffer>(std::move(buffer));
  bgDesc.debugName = "testBufferBindGroup";

  auto handle = device_->createBindGroup(bgDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_TRUE(handle.valid());
}

//
// DestroyBindGroup
//
// Test that destroying a bind group does not crash.
//
TEST_F(MetalBindGroupTest, DestroyBindGroup) {
  Result res;

  TextureDesc texDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  auto texture = device_->createTexture(texDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  BindGroupTextureDesc bgDesc;
  bgDesc.textures[0] = texture;
  bgDesc.debugName = "testDestroyBindGroup";

  auto handle = device_->createBindGroup(bgDesc, nullptr, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_TRUE(handle.valid());

  // Release the handle (transfers ownership) and manually destroy it.
  // This should not crash.
  BindGroupTextureHandle rawHandle = handle.release();
  device_->destroy(rawHandle);
}

} // namespace igl::tests
