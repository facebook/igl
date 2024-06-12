/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "data/ShaderData.h"
#include "data/TextureData.h"
#include "data/VertexIndexData.h"
#include "util/Common.h"
#include "util/TestDevice.h"

#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/NameHandle.h>
#include <igl/opengl/Device.h>
#include <string>

namespace igl::tests {

TEST(TextureRangeDesc, Construction) {
  {
    const auto range = TextureRangeDesc::new1D(2, 3, 4, 5);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 0);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 3);
    EXPECT_EQ(range.height, 1);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 4);
    EXPECT_EQ(range.numMipLevels, 5);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::new1DArray(2, 3, 4, 5, 6, 7);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 0);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 3);
    EXPECT_EQ(range.height, 1);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 4);
    EXPECT_EQ(range.numLayers, 5);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::new2DArray(2, 3, 4, 5, 6, 7, 8, 9);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 6);
    EXPECT_EQ(range.numLayers, 7);
    EXPECT_EQ(range.mipLevel, 8);
    EXPECT_EQ(range.numMipLevels, 9);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::new3D(2, 3, 4, 5, 6, 7, 8, 9);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 4);
    EXPECT_EQ(range.width, 5);
    EXPECT_EQ(range.height, 6);
    EXPECT_EQ(range.depth, 7);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 8);
    EXPECT_EQ(range.numMipLevels, 9);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::newCube(2, 3, 4, 5, 7, 8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 7);
    EXPECT_EQ(range.numMipLevels, 8);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 6);
  }
  {
    const auto range = TextureRangeDesc::newCubeFace(2, 3, 4, 5, 1, 7, 8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 7);
    EXPECT_EQ(range.numMipLevels, 8);
    EXPECT_EQ(range.face, 1);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::newCubeFace(2, 3, 4, 5, TextureCubeFace::NegX, 7, 8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 7);
    EXPECT_EQ(range.numMipLevels, 8);
    EXPECT_EQ(range.face, 1);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, AtMipLevel) {
  {
    const auto initialRange = TextureRangeDesc::new3D(0, 2, 5, 2, 10, 16, 0, 2);
    const auto range = initialRange.atMipLevel(0);
    EXPECT_EQ(range.x, 0);
    EXPECT_EQ(range.y, 2);
    EXPECT_EQ(range.z, 5);
    EXPECT_EQ(range.width, 2);
    EXPECT_EQ(range.height, 10);
    EXPECT_EQ(range.depth, 16);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 0);
    EXPECT_EQ(range.numMipLevels, 1);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto initialRange = TextureRangeDesc::new3D(0, 2, 5, 2, 10, 16, 0);
    const auto range = initialRange.atMipLevel(1);
    EXPECT_EQ(range.x, 0);
    EXPECT_EQ(range.y, 1);
    EXPECT_EQ(range.z, 2);
    EXPECT_EQ(range.width, 1);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 8);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 1);
    EXPECT_EQ(range.numMipLevels, 1);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto initialRange = TextureRangeDesc::new2DArray(0, 5, 2, 10, 0, 2, 1);
    const auto range = initialRange.atMipLevel(3);
    EXPECT_EQ(range.x, 0);
    EXPECT_EQ(range.y, 1);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 1);
    EXPECT_EQ(range.height, 2);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 2);
    EXPECT_EQ(range.mipLevel, 3);
    EXPECT_EQ(range.numMipLevels, 1);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, WithNumMipLevels) {
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.withNumMipLevels(8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 8);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, AtLayer) {
  {
    const auto initialRange = TextureRangeDesc::new2DArray(2, 3, 4, 5, 6, 7, 8);
    const auto range = initialRange.atLayer(1);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 1);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 8);
    EXPECT_EQ(range.numMipLevels, 1);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, WithNumLayers) {
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.withNumLayers(8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 8);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, AtFace) {
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.atFace(1);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 1);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.atFace(TextureCubeFace::NegX);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 1);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, WithNumFaces) {
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.withNumFaces(8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 8);
  }
}

TEST(TextureRangeDesc, Validate) {
  igl::setDebugBreakEnabled(false);
  constexpr size_t kMax = std::numeric_limits<uint32_t>::max();
  constexpr size_t kMaxPlus1 = std::numeric_limits<uint32_t>::max() + 1;
  // Overflow validation logic doesn't work on 32bit architectures
  constexpr bool kRunOverflowTests = (sizeof(size_t) > sizeof(uint32_t));

  // 2D
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, 1024, 1024).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, 1024, 1024, 0, 11).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, 1, kMax).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, kMax, 1).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, 1024, 1024, kMax, 1).validate().isOk());

  if (kRunOverflowTests) {
    EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, kMax, 1024).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, 1024, kMax).validate().isOk());

    EXPECT_FALSE(TextureRangeDesc::new2D(1, 1, 1, kMax).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new2D(1, 1, kMax, 1).validate().isOk());

    EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, 1, kMaxPlus1).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, kMaxPlus1, 1).validate().isOk());
  }

  EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, 1024, 1024, 0, 12).validate().isOk());

  // 2D Array
  EXPECT_TRUE(TextureRangeDesc::new2DArray(0, 0, 1024, 1024, 0, 1024).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2DArray(0, 0, 1, 1, 0, kMax).validate().isOk());

  if (kRunOverflowTests) {
    EXPECT_FALSE(TextureRangeDesc::new2DArray(0, 0, 1024, 1024, 0, kMax).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new2DArray(0, 0, 1, 1, 1, kMax).validate().isOk());

    EXPECT_FALSE(TextureRangeDesc::new2DArray(0, 0, 1, 1, 0, kMaxPlus1).validate().isOk());
  }

  // 3D
  EXPECT_TRUE(TextureRangeDesc::new3D(0, 0, 0, 1024, 1024, 1024).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new3D(0, 0, 0, 1, 1, kMax).validate().isOk());

  if (kRunOverflowTests) {
    EXPECT_FALSE(TextureRangeDesc::new3D(0, 0, 0, 1024, 1024, kMax).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new3D(0, 0, 1, 1, 1, kMax).validate().isOk());

    EXPECT_FALSE(TextureRangeDesc::new3D(0, 0, 0, 1, 1, kMaxPlus1).validate().isOk());
  }

  // Cube
  EXPECT_TRUE(TextureRangeDesc::newCube(0, 0, 1024, 1024).validate().isOk());

  EXPECT_FALSE(TextureRangeDesc::newCube(0, 0, 1024, 1024).withNumFaces(7).validate().isOk());
  EXPECT_FALSE(TextureRangeDesc::newCube(0, 0, 1024, 1024).atFace(7).validate().isOk());
}

namespace {
TextureRangeDesc createTestRange() {
  TextureRangeDesc range;
  range.x = 1;
  range.y = 2;
  range.z = 3;
  range.width = 4;
  range.height = 5;
  range.depth = 6;
  range.layer = 7;
  range.numLayers = 8;
  range.mipLevel = 9;
  range.numMipLevels = 10;
  range.face = 11;
  range.numFaces = 12;

  return range;
}
} // namespace

TEST(TextureRangeDesc, Equality) {
  const auto rangeA = createTestRange();
  {
    const auto rangeB = createTestRange();
    EXPECT_TRUE(rangeA == rangeB);
    EXPECT_FALSE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.x = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.y = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.z = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.width = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.height = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.depth = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.layer = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.numLayers = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.mipLevel = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.numMipLevels = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.face = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.numFaces = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
}

} // namespace igl::tests
