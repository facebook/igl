/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/texture_loader/IData.h>

#include <cstring>
#include <memory>

namespace igl::tests {

class IDataTest : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(IDataTest, TryCreateNullDataFails) {
  Result result;
  auto data = iglu::textureloader::IData::tryCreate(nullptr, 100, &result);
  EXPECT_EQ(data, nullptr);
  EXPECT_FALSE(result.isOk());
  EXPECT_EQ(result.code, Result::Code::ArgumentNull);
}

TEST_F(IDataTest, TryCreateZeroSizeFails) {
  auto buffer = std::make_unique<uint8_t[]>(10);
  Result result;
  auto data = iglu::textureloader::IData::tryCreate(std::move(buffer), 0, &result);
  EXPECT_EQ(data, nullptr);
  EXPECT_FALSE(result.isOk());
  EXPECT_EQ(result.code, Result::Code::ArgumentInvalid);
}

TEST_F(IDataTest, TryCreateValidDataSucceeds) {
  constexpr uint64_t kSize = 100;
  auto buffer = std::make_unique<uint8_t[]>(kSize);
  std::memset(buffer.get(), 0xAB, kSize);

  Result result;
  auto data = iglu::textureloader::IData::tryCreate(std::move(buffer), kSize, &result);
  EXPECT_NE(data, nullptr);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(data->size(), kSize);
  EXPECT_NE(data->data(), nullptr);
  EXPECT_EQ(data->data()[0], 0xAB);
}

TEST_F(IDataTest, TryCreateLargeSizeSucceeds) {
  constexpr uint64_t kSize = 1024 * 1024;
  auto buffer = std::make_unique<uint8_t[]>(kSize);
  buffer[0] = 0xCD;

  Result result;
  auto data = iglu::textureloader::IData::tryCreate(std::move(buffer), kSize, &result);
  EXPECT_NE(data, nullptr);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(data->size(), kSize);
  EXPECT_EQ(data->data()[0], 0xCD);
}

TEST_F(IDataTest, ExtractDataReturnsValidData) {
  constexpr uint64_t kSize = 50;
  auto buffer = std::make_unique<uint8_t[]>(kSize);
  std::memset(buffer.get(), 0xEF, kSize);

  Result result;
  auto data = iglu::textureloader::IData::tryCreate(std::move(buffer), kSize, &result);
  ASSERT_NE(data, nullptr);

  auto extracted = data->extractData();
  EXPECT_NE(extracted.data, nullptr);
  EXPECT_EQ(extracted.size, kSize);
  EXPECT_NE(extracted.deleter, nullptr);

  if (extracted.deleter != nullptr) {
    extracted.deleter(const_cast<void*>(extracted.data));
  }
}

} // namespace igl::tests
