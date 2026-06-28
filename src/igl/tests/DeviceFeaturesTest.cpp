/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/DeviceFeatures.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// ShaderFamily
// ---------------------------------------------------------------------------

TEST(ShaderFamilyTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::Unknown), 0u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::Glsl), 1u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::GlslEs), 2u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::Metal), 3u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::SpirV), 4u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::Hlsl), 5u);
}

// ---------------------------------------------------------------------------
// ShaderVersion
// ---------------------------------------------------------------------------

TEST(ShaderVersionTest, DefaultConstruction) {
  const ShaderVersion sv;
  EXPECT_EQ(sv.family, ShaderFamily::Unknown);
  EXPECT_EQ(sv.majorVersion, 0u);
  EXPECT_EQ(sv.minorVersion, 0u);
  EXPECT_EQ(sv.extra, 0u);
}

TEST(ShaderVersionTest, DesignatedInitializer) {
  const ShaderVersion sv{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 0,
  };
  EXPECT_EQ(sv.family, ShaderFamily::SpirV);
  EXPECT_EQ(sv.majorVersion, 1u);
  EXPECT_EQ(sv.minorVersion, 5u);
  EXPECT_EQ(sv.extra, 0u);
}

TEST(ShaderVersionTest, GlslEs300) {
  const ShaderVersion sv{
      .family = ShaderFamily::GlslEs,
      .majorVersion = 3,
      .minorVersion = 0,
  };
  EXPECT_EQ(sv.family, ShaderFamily::GlslEs);
  EXPECT_EQ(sv.majorVersion, 3u);
  EXPECT_EQ(sv.minorVersion, 0u);
  EXPECT_EQ(sv.extra, 0u);
}

TEST(ShaderVersionTest, EqualityAllFieldsMatch) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(ShaderVersionTest, InequalityDifferingFamily) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::Glsl,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

TEST(ShaderVersionTest, InequalityDifferingMajorVersion) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::SpirV,
      .majorVersion = 2,
      .minorVersion = 5,
      .extra = 7,
  };
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

TEST(ShaderVersionTest, InequalityDifferingMinorVersion) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 6,
      .extra = 7,
  };
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

TEST(ShaderVersionTest, InequalityDifferingExtra) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 8,
  };
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

// ---------------------------------------------------------------------------
// DeviceRequirement
// ---------------------------------------------------------------------------

TEST(DeviceRequirementTest, EnumValues) {
  EXPECT_EQ(static_cast<int>(DeviceRequirement::ExplicitBindingExtReq), 0);
  EXPECT_EQ(static_cast<int>(DeviceRequirement::ShaderTextureLodExtReq), 1);
  EXPECT_EQ(static_cast<int>(DeviceRequirement::StandardDerivativeExtReq), 2);
  EXPECT_EQ(static_cast<int>(DeviceRequirement::TextureArrayExtReq), 3);
  EXPECT_EQ(static_cast<int>(DeviceRequirement::TextureFormatRGExtReq), 4);
}

// ---------------------------------------------------------------------------
// TextureFormatCapabilityBits
// ---------------------------------------------------------------------------

TEST(TextureFormatCapabilityBitsTest, BitValues) {
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::Unsupported, 0u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::Sampled, 1u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::SampledFiltered, 2u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::Storage, 4u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::Attachment, 8u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::SampledAttachment, 16u);
}

TEST(TextureFormatCapabilityBitsTest, AllIsUnionOfIndividualBits) {
  const uint8_t expected = ICapabilities::TextureFormatCapabilityBits::Sampled |
                           ICapabilities::TextureFormatCapabilityBits::SampledFiltered |
                           ICapabilities::TextureFormatCapabilityBits::Storage |
                           ICapabilities::TextureFormatCapabilityBits::Attachment |
                           ICapabilities::TextureFormatCapabilityBits::SampledAttachment;
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::All, expected);
}

TEST(TextureFormatCapabilityBitsTest, ContainsHelper) {
  const ICapabilities::TextureFormatCapabilities caps =
      ICapabilities::TextureFormatCapabilityBits::Sampled |
      ICapabilities::TextureFormatCapabilityBits::Attachment;
  EXPECT_TRUE(contains(caps, ICapabilities::TextureFormatCapabilityBits::Sampled));
  EXPECT_TRUE(contains(caps, ICapabilities::TextureFormatCapabilityBits::Attachment));
  EXPECT_FALSE(contains(caps, ICapabilities::TextureFormatCapabilityBits::Storage));
}

} // namespace igl::tests
