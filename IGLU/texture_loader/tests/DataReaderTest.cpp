/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if __has_include(<gtest/gtest.h>)

#include <gtest/gtest.h>

#include <IGLU/texture_loader/DataReader.h>

#include <array>
#include <cstdint>

namespace iglu::textureloader::tests {

// ============================================================================
// tryCreate
// ============================================================================

TEST(DataReaderTest, TryCreateWithValidDataReturnsReader) {
  const std::array<uint8_t, 4> buffer = {0x10, 0x20, 0x30, 0x40};
  igl::Result result;

  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);

  ASSERT_TRUE(reader.has_value());
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(reader->data(), buffer.data());
  EXPECT_EQ(reader->size(), buffer.size());
}

// ============================================================================
// tryAt - bounds-checked pointer access at offset
// ============================================================================

TEST(DataReaderTest, TryAtWithValidOffsetReturnsPointer) {
  const std::array<uint8_t, 8> buffer = {0, 1, 2, 3, 4, 5, 6, 7};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  const uint8_t* ptr = reader->tryAt(4, &result);

  ASSERT_NE(ptr, nullptr);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(*ptr, 4);
}

TEST(DataReaderTest, TryAtWithOffsetEqualToSizeSucceeds) {
  const std::array<uint8_t, 4> buffer = {0, 1, 2, 3};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  // offset == size is valid (points to one-past-end, like iterators)
  const uint8_t* ptr = reader->tryAt(buffer.size(), &result);

  ASSERT_NE(ptr, nullptr);
  EXPECT_TRUE(result.isOk());
}

TEST(DataReaderTest, TryAtWithOffsetBeyondSizeReturnsNullAndSetsError) {
  const std::array<uint8_t, 4> buffer = {0, 1, 2, 3};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  const uint8_t* ptr = reader->tryAt(buffer.size() + 1, &result);

  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(result.code, igl::Result::Code::InvalidOperation);
}

// ============================================================================
// tryAdvance - bounds-checked data pointer advancement
// ============================================================================

TEST(DataReaderTest, TryAdvanceWithValidBytesSucceeds) {
  const std::array<uint8_t, 8> buffer = {10, 20, 30, 40, 50, 60, 70, 80};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  bool advanced = reader->tryAdvance(3, &result);

  EXPECT_TRUE(advanced);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(reader->size(), 5u);
  EXPECT_EQ(*reader->data(), 40);
}

TEST(DataReaderTest, TryAdvanceBeyondSizeFailsAndSetsError) {
  const std::array<uint8_t, 4> buffer = {1, 2, 3, 4};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  bool advanced = reader->tryAdvance(5, &result);

  EXPECT_FALSE(advanced);
  EXPECT_EQ(result.code, igl::Result::Code::InvalidOperation);
  // Data and size should remain unchanged after failed advance
  EXPECT_EQ(reader->size(), 4u);
  EXPECT_EQ(reader->data(), buffer.data());
}

TEST(DataReaderTest, TryAdvanceExactSizeSucceedsLeavingEmptyReader) {
  const std::array<uint8_t, 4> buffer = {1, 2, 3, 4};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  bool advanced = reader->tryAdvance(4, &result);

  EXPECT_TRUE(advanced);
  EXPECT_EQ(reader->size(), 0u);
}

// ============================================================================
// advance (unchecked) - modifies data pointer and size
// ============================================================================

TEST(DataReaderTest, AdvanceMovesDataPointerAndReducesSize) {
  const std::array<uint8_t, 6> buffer = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  reader->advance(2);

  EXPECT_EQ(reader->size(), 4u);
  EXPECT_EQ(reader->data(), buffer.data() + 2);
  EXPECT_EQ(*reader->data(), 0xCC);
}

// ============================================================================
// Sequential operations - realistic usage pattern
// ============================================================================

TEST(DataReaderTest, SequentialAdvanceAndReadSimulatesHeaderParsing) {
  // Simulate reading a simple binary format: 4-byte magic + 4-byte payload size
  struct Header {
    uint32_t magic;
    uint32_t payloadSize;
  };
  const Header expectedHeader = {.magic = 0xDEADBEEF, .payloadSize = 42};
  const auto* rawBytes = reinterpret_cast<const uint8_t*>(&expectedHeader);

  igl::Result result;
  auto reader = DataReader::tryCreate(rawBytes, sizeof(Header), &result);
  ASSERT_TRUE(reader.has_value());

  // Read the header as a typed struct
  const Header* header = reader->tryAs<Header>(&result);
  ASSERT_NE(header, nullptr);
  EXPECT_EQ(header->magic, 0xDEADBEEF);
  EXPECT_EQ(header->payloadSize, 42u);

  // Advance past the header
  EXPECT_TRUE(reader->tryAdvance(sizeof(Header), &result));
  EXPECT_EQ(reader->size(), 0u);
}

TEST(DataReaderTest, TryAsWithInsufficientDataReturnsNull) {
  const std::array<uint8_t, 2> buffer = {0x01, 0x02};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  const uint32_t* value = reader->tryAs<uint32_t>(&result);

  EXPECT_EQ(value, nullptr);
  EXPECT_EQ(result.code, igl::Result::Code::InvalidOperation);
}

TEST(DataReaderTest, TryReadCopiesValueOnSuccess) {
  const uint32_t expected = 0x12345678;
  const auto* rawBytes = reinterpret_cast<const uint8_t*>(&expected);

  igl::Result result;
  auto reader = DataReader::tryCreate(rawBytes, sizeof(expected), &result);
  ASSERT_TRUE(reader.has_value());

  uint32_t value = 0;
  bool success = reader->tryRead(value, &result);

  EXPECT_TRUE(success);
  EXPECT_EQ(value, expected);
}

TEST(DataReaderTest, TryReadFailsWhenDataTooSmall) {
  const std::array<uint8_t, 1> buffer = {0xFF};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  uint32_t value = 0;
  bool success = reader->tryRead(value, &result);

  EXPECT_FALSE(success);
  EXPECT_EQ(result.code, igl::Result::Code::InvalidOperation);
}

TEST(DataReaderTest, MultipleAdvancesConsumeDataCorrectly) {
  const std::array<uint8_t, 10> buffer = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  // Advance 3 bytes
  EXPECT_TRUE(reader->tryAdvance(3, &result));
  EXPECT_EQ(reader->size(), 7u);
  EXPECT_EQ(*reader->data(), 3);

  // Advance 4 more bytes
  EXPECT_TRUE(reader->tryAdvance(4, &result));
  EXPECT_EQ(reader->size(), 3u);
  EXPECT_EQ(*reader->data(), 7);

  // Try to advance beyond remaining
  EXPECT_FALSE(reader->tryAdvance(4, &result));
  EXPECT_EQ(result.code, igl::Result::Code::InvalidOperation);
  // State unchanged after failure
  EXPECT_EQ(reader->size(), 3u);
}

// ============================================================================
// tryAsAt - bounds-checked typed access at offset
// ============================================================================

TEST(DataReaderTest, TryAsAtWithValidOffsetReturnsTypedPointer) {
  // [uint16_t A] [uint16_t B] [uint32_t C]
  const std::array<uint8_t, 8> buffer = {0x01, 0x00, 0x02, 0x00, 0x78, 0x56, 0x34, 0x12};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  const uint32_t* value = reader->tryAsAt<uint32_t>(4, &result);

  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(*value, 0x12345678u);
}

TEST(DataReaderTest, TryAsAtWithInsufficientRemainingSpaceReturnsNull) {
  const std::array<uint8_t, 6> buffer = {0, 1, 2, 3, 4, 5};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  // Offset 4 leaves only 2 bytes, not enough for a uint32_t
  const uint32_t* value = reader->tryAsAt<uint32_t>(4, &result);

  EXPECT_EQ(value, nullptr);
  EXPECT_EQ(result.code, igl::Result::Code::InvalidOperation);
}

// ============================================================================
// tryReadAt - bounds-checked typed read with copy at offset
// ============================================================================

TEST(DataReaderTest, TryReadAtCopiesValueAtOffset) {
  const std::array<uint8_t, 8> buffer = {0xFF, 0xFF, 0xFF, 0xFF, 0x78, 0x56, 0x34, 0x12};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  uint32_t value = 0;
  bool success = reader->tryReadAt(4, value, &result);

  EXPECT_TRUE(success);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(value, 0x12345678u);
}

TEST(DataReaderTest, TryReadAtFailsWhenOffsetPlusTypeSizeExceedsBuffer) {
  const std::array<uint8_t, 4> buffer = {0, 1, 2, 3};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  uint32_t value = 0;
  bool success = reader->tryReadAt(2, value, &result);

  EXPECT_FALSE(success);
  EXPECT_EQ(result.code, igl::Result::Code::InvalidOperation);
}

// ============================================================================
// tryAdvance<T> - templated advance by sizeof(T)
// ============================================================================

TEST(DataReaderTest, TryAdvanceTemplatedAdvancesBySizeofType) {
  const std::array<uint8_t, 8> buffer = {10, 20, 30, 40, 50, 60, 70, 80};
  igl::Result result;
  auto reader = DataReader::tryCreate(buffer.data(), buffer.size(), &result);
  ASSERT_TRUE(reader.has_value());

  bool advanced = reader->tryAdvance<uint32_t>(&result);

  EXPECT_TRUE(advanced);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(reader->size(), 4u);
  EXPECT_EQ(*reader->data(), 50);
}

} // namespace iglu::textureloader::tests

#endif // __has_include(<gtest/gtest.h>)
