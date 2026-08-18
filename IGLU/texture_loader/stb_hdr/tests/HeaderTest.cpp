/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if !defined(IGL_CMAKE_BUILD)

#include <gtest/gtest.h>

#include <IGLU/texture_loader/stb_hdr/Header.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <type_traits>

namespace iglu::textureloader::stb::hdr::tests {

namespace {

// Modern Radiance identifier: exactly kHeaderLength bytes.
constexpr std::array<uint8_t, 11> kRadianceTag{
    {'#', '?', 'R', 'A', 'D', 'I', 'A', 'N', 'C', 'E', '\n'}};

// Legacy identifier: only the first 7 bytes are significant.
constexpr std::array<uint8_t, 7> kRgbeTag{{'#', '?', 'R', 'G', 'B', 'E', '\n'}};

// Builds a Header from the leading bytes of a file; any remaining tag bytes are zeroed.
template<size_t N>
Header makeHeader(const std::array<uint8_t, N>& bytes) {
  static_assert(N <= kHeaderLength);
  Header header{};
  header.tag.fill(0);
  std::copy(bytes.begin(), bytes.end(), header.tag.begin());
  return header;
}

} // namespace

TEST(StbHdrHeaderTest, HeaderLengthMatchesTagSize) {
  EXPECT_EQ(kHeaderLength, 11u);
  EXPECT_EQ(sizeof(Header), kHeaderLength);
  EXPECT_EQ(std::tuple_size<Tag>::value, kHeaderLength);
}

TEST(StbHdrHeaderTest, HeaderIsTriviallyCopyable) {
  EXPECT_TRUE(std::is_trivially_copyable_v<Header>);
}

TEST(StbHdrHeaderTest, RadianceTagIsValid) {
  const Header header = makeHeader(kRadianceTag);
  EXPECT_TRUE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, LegacyRgbeTagIsValid) {
  const Header header = makeHeader(kRgbeTag);
  EXPECT_TRUE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, LegacyRgbeTagIsValidWithTrailingFileData) {
  // Only the first 7 bytes are compared for the legacy identifier, so whatever
  // real file data follows must not affect validation.
  Header header = makeHeader(kRgbeTag);
  header.tag[7] = 'F';
  header.tag[8] = 'O';
  header.tag[9] = 'R';
  header.tag[10] = 'M';
  EXPECT_TRUE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, ZeroedTagIsInvalid) {
  const Header header{};
  EXPECT_FALSE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, GarbageTagIsInvalid) {
  constexpr std::array<uint8_t, 11> kGarbage{
      {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00}};
  const Header header = makeHeader(kGarbage);
  EXPECT_FALSE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, RadianceTagWithCorruptedLastByteIsInvalid) {
  Header header = makeHeader(kRadianceTag);
  header.tag[kHeaderLength - 1] = '\r';
  EXPECT_FALSE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, RadianceTagWithCorruptedFirstByteIsInvalid) {
  Header header = makeHeader(kRadianceTag);
  header.tag[0] = '!';
  EXPECT_FALSE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, RadianceTagWithEverySingleByteCorruptedIsInvalid) {
  for (size_t i = 0; i < kHeaderLength; ++i) {
    Header header = makeHeader(kRadianceTag);
    header.tag[i] = static_cast<uint8_t>(header.tag[i] ^ 0x01u);
    EXPECT_FALSE(header.tagIsValid()) << "byte " << i << " should be significant";
  }
}

TEST(StbHdrHeaderTest, LegacyRgbeTagWithAnyOfItsBytesCorruptedIsInvalid) {
  for (size_t i = 0; i < kRgbeTag.size(); ++i) {
    Header header = makeHeader(kRgbeTag);
    header.tag[i] = static_cast<uint8_t>(header.tag[i] ^ 0x01u);
    EXPECT_FALSE(header.tagIsValid()) << "byte " << i << " should be significant";
  }
}

TEST(StbHdrHeaderTest, LowercaseRadianceTagIsInvalid) {
  constexpr std::array<uint8_t, 11> kLowercase{
      {'#', '?', 'r', 'a', 'd', 'i', 'a', 'n', 'c', 'e', '\n'}};
  const Header header = makeHeader(kLowercase);
  EXPECT_FALSE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, TruncatedRadianceTagIsInvalid) {
  // "#?RADIANCE" without the terminating newline: the missing byte is zero.
  constexpr std::array<uint8_t, 10> kTruncated{{'#', '?', 'R', 'A', 'D', 'I', 'A', 'N', 'C', 'E'}};
  const Header header = makeHeader(kTruncated);
  EXPECT_FALSE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, TruncatedLegacyRgbeTagIsInvalid) {
  constexpr std::array<uint8_t, 6> kTruncated{{'#', '?', 'R', 'G', 'B', 'E'}};
  const Header header = makeHeader(kTruncated);
  EXPECT_FALSE(header.tagIsValid());
}

TEST(StbHdrHeaderTest, HeaderReinterpretedFromRawBytesIsValid) {
  // Mirrors how TextureLoaderFactory reinterprets the leading file bytes.
  const std::array<uint8_t, 16> fileBytes{
      {'#', '?', 'R', 'A', 'D', 'I', 'A', 'N', 'C', 'E', '\n', 'F', 'O', 'R', 'M', 'A'}};
  Header header{};
  std::memcpy(&header, fileBytes.data(), kHeaderLength);
  EXPECT_TRUE(header.tagIsValid());
}

} // namespace iglu::textureloader::stb::hdr::tests

#endif // !defined(IGL_CMAKE_BUILD)
