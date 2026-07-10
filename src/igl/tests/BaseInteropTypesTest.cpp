/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/base/Common.h>
#include <igl/base/IAttachmentInterop.h>
#include <igl/base/IFramebufferInterop.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// base::TextureType
// ---------------------------------------------------------------------------

TEST(BaseTextureTypeTest, InvalidIsZero) {
  EXPECT_EQ(static_cast<uint8_t>(base::TextureType::Invalid), 0u);
}

TEST(BaseTextureTypeTest, AllValuesDistinct) {
  EXPECT_NE(base::TextureType::Invalid, base::TextureType::TwoD);
  EXPECT_NE(base::TextureType::Invalid, base::TextureType::TwoDArray);
  EXPECT_NE(base::TextureType::Invalid, base::TextureType::ThreeD);
  EXPECT_NE(base::TextureType::Invalid, base::TextureType::Cube);
  EXPECT_NE(base::TextureType::Invalid, base::TextureType::ExternalImage);
  EXPECT_NE(base::TextureType::TwoD, base::TextureType::TwoDArray);
  EXPECT_NE(base::TextureType::TwoD, base::TextureType::ThreeD);
  EXPECT_NE(base::TextureType::TwoD, base::TextureType::Cube);
  EXPECT_NE(base::TextureType::TwoD, base::TextureType::ExternalImage);
  EXPECT_NE(base::TextureType::TwoDArray, base::TextureType::ThreeD);
  EXPECT_NE(base::TextureType::TwoDArray, base::TextureType::Cube);
  EXPECT_NE(base::TextureType::TwoDArray, base::TextureType::ExternalImage);
  EXPECT_NE(base::TextureType::ThreeD, base::TextureType::Cube);
  EXPECT_NE(base::TextureType::ThreeD, base::TextureType::ExternalImage);
  EXPECT_NE(base::TextureType::Cube, base::TextureType::ExternalImage);
}

// ---------------------------------------------------------------------------
// base::kMaxColorAttachments
// ---------------------------------------------------------------------------

TEST(BaseMaxColorAttachmentsTest, ValueIsFour) {
  EXPECT_EQ(base::kMaxColorAttachments, 4u);
}

// ---------------------------------------------------------------------------
// base::AttachmentInteropDesc defaults
// ---------------------------------------------------------------------------

TEST(BaseAttachmentInteropDescTest, DefaultConstruction) {
  const base::AttachmentInteropDesc desc;
  EXPECT_EQ(desc.width, 1u);
  EXPECT_EQ(desc.height, 1u);
  EXPECT_EQ(desc.depth, 1u);
  EXPECT_EQ(desc.numLayers, 1u);
  EXPECT_EQ(desc.numSamples, 1u);
  EXPECT_EQ(desc.numMipLevels, 1u);
  EXPECT_EQ(desc.type, base::TextureType::TwoD);
  EXPECT_EQ(desc.format, base::TextureFormat::Invalid);
  EXPECT_TRUE(desc.isSampled);
}

TEST(BaseAttachmentInteropDescTest, DesignatedInitializer) {
  const base::AttachmentInteropDesc desc{
      .width = 128,
      .height = 256,
      .depth = 1,
      .numLayers = 2,
      .numSamples = 4,
      .numMipLevels = 3,
      .type = base::TextureType::TwoDArray,
      .format = base::TextureFormat::RGBA_UNorm8,
      .isSampled = false,
  };
  EXPECT_EQ(desc.width, 128u);
  EXPECT_EQ(desc.height, 256u);
  EXPECT_EQ(desc.depth, 1u);
  EXPECT_EQ(desc.numLayers, 2u);
  EXPECT_EQ(desc.numSamples, 4u);
  EXPECT_EQ(desc.numMipLevels, 3u);
  EXPECT_EQ(desc.type, base::TextureType::TwoDArray);
  EXPECT_EQ(desc.format, base::TextureFormat::RGBA_UNorm8);
  EXPECT_FALSE(desc.isSampled);
}

// ---------------------------------------------------------------------------
// base::FramebufferInteropDesc defaults
// ---------------------------------------------------------------------------

TEST(BaseFramebufferInteropDescTest, DefaultConstruction) {
  const base::FramebufferInteropDesc desc{};
  for (const auto& colorAttachment : desc.colorAttachments) {
    EXPECT_EQ(colorAttachment, nullptr);
  }
  EXPECT_EQ(desc.depthAttachment, nullptr);
  EXPECT_EQ(desc.stencilAttachment, nullptr);
}

} // namespace igl::tests
