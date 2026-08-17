/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if __has_include(<gtest/gtest.h>)

#include <gtest/gtest.h>

#include <IGLU/texture_loader/stb_jpeg/Header.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace iglu::textureloader::stb::jpeg::tests {

namespace {
constexpr Tag kValidTag{{0xFF, 0xD8, 0xFF}};
} // namespace

// ============================================================================
// tagIsValid()
// ============================================================================

TEST(HeaderTest, ValidJpegTagIsValid) {
  const Header header{.tag = kValidTag};

  EXPECT_TRUE(header.tagIsValid());
}

TEST(HeaderTest, AllZeroTagIsInvalid) {
  const Header header{.tag = {{0x00, 0x00, 0x00}}};

  EXPECT_FALSE(header.tagIsValid());
}

TEST(HeaderTest, RandomGarbageTagIsInvalid) {
  const Header header{.tag = {{0x12, 0x34, 0x56}}};

  EXPECT_FALSE(header.tagIsValid());
}

TEST(HeaderTest, SingleByteMutationSweepIsInvalid) {
  // Independently corrupt each byte of an otherwise-valid tag; a single
  // mismatched byte anywhere in the 3-byte SOI marker must invalidate it.
  for (size_t i = 0; i < kValidTag.size(); ++i) {
    SCOPED_TRACE(::testing::Message() << "byte index " << i);

    Tag tag = kValidTag;
    tag[i] = static_cast<uint8_t>(tag[i] ^ 0xFF);
    const Header header{.tag = tag};

    EXPECT_FALSE(header.tagIsValid());
  }
}

TEST(HeaderTest, ReorderedBytesIsInvalid) {
  // Same three byte values as the valid tag, but out of order.
  const Header header{.tag = {{0xD8, 0xFF, 0xFF}}};

  EXPECT_FALSE(header.tagIsValid());
}

TEST(HeaderTest, PngMagicPrefixIsInvalid) {
  // First 3 bytes of the PNG signature (0x89 'P' 'N').
  const Header header{.tag = {{0x89, 0x50, 0x4E}}};

  EXPECT_FALSE(header.tagIsValid());
}

TEST(HeaderTest, RadianceHdrMagicPrefixIsInvalid) {
  // First 3 bytes of the Radiance HDR signature ('#' '?' 'R').
  const Header header{.tag = {{0x23, 0x3F, 0x52}}};

  EXPECT_FALSE(header.tagIsValid());
}

TEST(HeaderTest, MemcpyConstructedHeaderRoundTrips) {
  const std::array<uint8_t, kHeaderLength> raw{{0xFF, 0xD8, 0xFF}};

  Header header{};
  std::memcpy(&header, raw.data(), sizeof(Header));

  EXPECT_TRUE(header.tagIsValid());
}

TEST(HeaderTest, MemcpyConstructedInvalidHeaderRoundTrips) {
  const std::array<uint8_t, kHeaderLength> raw{{0x00, 0x01, 0x02}};

  Header header{};
  std::memcpy(&header, raw.data(), sizeof(Header));

  EXPECT_FALSE(header.tagIsValid());
}

// ============================================================================
// Type invariants
// ============================================================================

TEST(HeaderTest, SizeAndHeaderLengthInvariants) {
  EXPECT_EQ(sizeof(Header), 3u);
  EXPECT_EQ(kHeaderLength, 3u);
}

TEST(HeaderTest, HeaderIsTriviallyCopyable) {
  EXPECT_TRUE(std::is_trivially_copyable_v<Header>);
}

} // namespace iglu::textureloader::stb::jpeg::tests

#endif // __has_include(<gtest/gtest.h>)
