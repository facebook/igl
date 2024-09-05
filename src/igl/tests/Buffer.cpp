/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "util/Common.h"
#include "util/TestDevice.h"

#include <string>

namespace igl::tests {

//
// BufferTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class BufferTest : public ::testing::Test {
 public:
  BufferTest() = default;
  ~BufferTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);

    mapBufferTestsSupported = iglDev_->hasFeature(DeviceFeatures::MapBufferRange);
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  bool mapBufferTestsSupported = false;
};

//
// sizeForUniformElementType
//
// Make sure Buffer::sizeForUniformElementType() returns the expected value
//
TEST_F(BufferTest, sizeForUniformElementType) {
  // Invalid type has element size of 0
  ASSERT_EQ(0, sizeForUniformElementType(UniformType::Invalid));

  // These all have float as the underlying type, so 4 bytes
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Float));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Float2));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Float3));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Float4));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Mat2x2));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Mat3x3));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Mat4x4));

  // IGL defines boolean as 1 byte
  ASSERT_EQ(1, sizeForUniformElementType(UniformType::Boolean));

  // These all have 32-bit integer as the underlying type, so 4 bytes
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Int));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Int2));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Int3));
  ASSERT_EQ(4, sizeForUniformElementType(UniformType::Int4));
}

//
// sizeForUniformType
//
// Verify igl::sizeForUniformType() returns the expected value.
// We include all possible cases to get maximum code coverage.
//
TEST_F(BufferTest, sizeForUniformType) {
  // Invalid type has size of 0
  ASSERT_EQ(0, sizeForUniformType(UniformType::Invalid));
  ASSERT_EQ(0, sizeForUniformType((UniformType)-1));

  // Types with element of Float
  ASSERT_EQ(4, sizeForUniformType(UniformType::Float));
  ASSERT_EQ(8, sizeForUniformType(UniformType::Float2));
  ASSERT_EQ(12, sizeForUniformType(UniformType::Float3));
  ASSERT_EQ(16, sizeForUniformType(UniformType::Float4));

  // Boolean has size of 1
  ASSERT_EQ(1, sizeForUniformType(UniformType::Boolean));

  // Types with element of Int
  ASSERT_EQ(4, sizeForUniformType(UniformType::Int));
  ASSERT_EQ(8, sizeForUniformType(UniformType::Int2));
  ASSERT_EQ(12, sizeForUniformType(UniformType::Int3));
  ASSERT_EQ(16, sizeForUniformType(UniformType::Int4));
  ASSERT_EQ(16, sizeForUniformType(UniformType::Mat2x2));
  ASSERT_EQ(36, sizeForUniformType(UniformType::Mat3x3));
  ASSERT_EQ(64, sizeForUniformType(UniformType::Mat4x4));
}

TEST_F(BufferTest, createWithDebugLabel) {
  Result ret;
  constexpr size_t kIndexDataSize = 6;
  constexpr std::array<uint16_t, kIndexDataSize> kIndexData = {
      0,
      1,
      2,
      1,
      3,
      2,
  };
  BufferDesc bufferDesc = BufferDesc(BufferDesc::BufferTypeBits::Index,
                                     kIndexData.data(),
                                     sizeof(kIndexData),
                                     ResourceStorage::Shared);
  bufferDesc.debugName = "test";

  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);

  ret = buffer->upload(kIndexData.data(), BufferRange{sizeof(kIndexData), 0});
  ASSERT_EQ(ret.code, Result::Code::Ok);
}

TEST_F(BufferTest, mapIndexBuffer) {
  Result ret;
  constexpr size_t indexDataSize = 6;
  uint16_t indexData[indexDataSize] = {
      0,
      1,
      2,
      1,
      3,
      2,
  };
  const BufferDesc bufferDesc = BufferDesc(
      BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData), ResourceStorage::Shared);
  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);

  std::vector<uint16_t> bufferData(indexDataSize);
  auto range = BufferRange(sizeof(indexData), 0);
  auto* data = buffer->map(range, &ret);

  if (!mapBufferTestsSupported) {
    ASSERT_EQ(ret.code, Result::Code::InvalidOperation);
    return;
  }
  ASSERT_EQ(ret.code, Result::Code::Ok);

  memcpy(bufferData.data(), data, sizeof(indexData));

  for (int i = 0; i < indexDataSize; ++i) {
    ASSERT_EQ(bufferData[i], indexData[i]);
  }
  buffer->unmap();
}

TEST_F(BufferTest, mapBufferRangeIndexBuffer) {
  Result ret;
  constexpr size_t indexDataSize = 6;
  uint16_t indexData[indexDataSize] = {
      0,
      1,
      2,
      1,
      3,
      2,
  };
  const BufferDesc bufferDesc = BufferDesc(
      BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData), ResourceStorage::Shared);
  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);

  std::vector<uint16_t> bufferPartialData(3);
  const int numElementsToSkip = 2;
  const int offsetBytes = numElementsToSkip * sizeof(uint16_t);
  const int numElementsToMap = 3;
  const int sizeBytes = numElementsToMap * sizeof(uint16_t);
  auto newRange = BufferRange(sizeBytes, offsetBytes);
  auto* data = buffer->map(newRange, &ret);

  if (!mapBufferTestsSupported) {
    ASSERT_EQ(ret.code, Result::Code::InvalidOperation);
    return;
  }
  ASSERT_EQ(ret.code, Result::Code::Ok);

  memcpy(bufferPartialData.data(), data, sizeBytes);

  for (int i = 0; i < numElementsToMap; ++i) {
    ASSERT_EQ(bufferPartialData[i], indexData[i + numElementsToSkip]);
  }
  buffer->unmap();
}

TEST_F(BufferTest, copyBytesErrorsIndexBuffer) {
  Result ret;
  constexpr size_t indexDataSize = 6;
  uint16_t indexData[indexDataSize] = {
      0,
      1,
      2,
      1,
      3,
      2,
  };
  const BufferDesc bufferDesc = BufferDesc(
      BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData), ResourceStorage::Shared);
  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);

  // Offset starts past end of buffer
  auto range = BufferRange(sizeof(indexData), 1);
  buffer->map(range, &ret);

  ASSERT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
  buffer->unmap();

  // Range extends beyond end of buffer
  range = BufferRange(1, sizeof(indexData));
  buffer->map(range, &ret);
  ASSERT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
  buffer->unmap();
}

TEST_F(BufferTest, mapUniformBuffer) {
  Result ret;

  igl::Color color = {1.0f, 5.0f, 7.0f, 1.0f};
  BufferDesc bufferDesc;
  bufferDesc.type = BufferDesc::BufferTypeBits::Uniform;
  bufferDesc.data = &color;
  bufferDesc.length = sizeof(color);

  const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(buffer != nullptr);

  std::vector<float> bufferData(4);
  auto range = BufferRange(sizeof(color), 0);
  auto* data = buffer->map(range, &ret);

  memcpy(bufferData.data(), data, sizeof(color));

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_EQ(color.r, bufferData[0]);
  ASSERT_EQ(color.g, bufferData[1]);
  ASSERT_EQ(color.b, bufferData[2]);
  ASSERT_EQ(color.a, bufferData[3]);
}

} // namespace igl::tests
