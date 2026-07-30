/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <string>
#include <igl/Texture.h>

namespace igl::tests {

namespace {
void expectRangeEq(const TextureRangeDesc& actual,
                   const TextureRangeDesc& expected,
                   const std::string& label) {
  EXPECT_EQ(actual.x, expected.x) << label << " x";
  EXPECT_EQ(actual.y, expected.y) << label << " y";
  EXPECT_EQ(actual.z, expected.z) << label << " z";
  EXPECT_EQ(actual.width, expected.width) << label << " width";
  EXPECT_EQ(actual.height, expected.height) << label << " height";
  EXPECT_EQ(actual.depth, expected.depth) << label << " depth";
  EXPECT_EQ(actual.layer, expected.layer) << label << " layer";
  EXPECT_EQ(actual.numLayers, expected.numLayers) << label << " numLayers";
  EXPECT_EQ(actual.mipLevel, expected.mipLevel) << label << " mipLevel";
  EXPECT_EQ(actual.numMipLevels, expected.numMipLevels) << label << " numMipLevels";
  EXPECT_EQ(actual.face, expected.face) << label << " face";
  EXPECT_EQ(actual.numFaces, expected.numFaces) << label << " numFaces";
}
} // namespace

// Exercises the uint32_t narrowing in atMipLevel(), atLayer() and atFace() across 3D,
// 2D-array and cube ranges. Every expected value below is hand-traced from the mip-delta
// right-shift (dimension >> delta, offsets x/y/z >> delta, clamped to a minimum of 1) and
// the single-slice narrowing performed by atLayer()/atFace().
TEST(TextureRangeDesc, NarrowingAcrossMipLevelLayerAndFace) {
  // 3D range with non-zero x/y/z offsets, dropping from base mip 1 to mip 4 (delta 3, >> 3).
  {
    const auto base = TextureRangeDesc::new3D(24, 40, 56, 80, 48, 32, 1, 4);
    const auto mipped = base.atMipLevel(4);
    const TextureRangeDesc expected{
        .x = 3, // 24 >> 3
        .y = 5, // 40 >> 3
        .z = 7, // 56 >> 3
        .width = 10, // 80 >> 3
        .height = 6, // 48 >> 3
        .depth = 4, // 32 >> 3
        .mipLevel = 4,
        .numMipLevels = 1,
    };
    expectRangeEq(mipped, expected, "3D atMipLevel(4)");
  }

  // 2D array: atMipLevel() shifts dims (delta 2, >> 2) while preserving layer/numLayers,
  // then atLayer() narrows the range to a single array layer.
  {
    const auto base = TextureRangeDesc::new2DArray(12, 20, 36, 52, 3, 6, 0, 3);
    const auto mipped = base.atMipLevel(2);
    const TextureRangeDesc expectedMipped{
        .x = 3, // 12 >> 2
        .y = 5, // 20 >> 2
        .width = 9, // 36 >> 2
        .height = 13, // 52 >> 2
        .layer = 3, // preserved by atMipLevel()
        .numLayers = 6, // preserved by atMipLevel()
        .mipLevel = 2,
        .numMipLevels = 1,
    };
    expectRangeEq(mipped, expectedMipped, "2DArray atMipLevel(2)");

    const auto layered = mipped.atLayer(4);
    const TextureRangeDesc expectedLayered{
        .x = 3,
        .y = 5,
        .width = 9,
        .height = 13,
        .layer = 4, // narrowed to a single layer
        .numLayers = 1,
        .mipLevel = 2,
        .numMipLevels = 1,
    };
    expectRangeEq(layered, expectedLayered, "2DArray atLayer(4)");
  }

  // Cube: atMipLevel() shifts dims (delta 3, >> 3) while preserving numFaces == 6, then
  // atFace(TextureCubeFace) narrows to a single face via the uint32_t cast of the enum.
  {
    const auto base = TextureRangeDesc::newCube(8, 16, 100, 140, 0, 5);
    const auto mipped = base.atMipLevel(3);
    const TextureRangeDesc expectedMipped{
        .x = 1, // 8 >> 3
        .y = 2, // 16 >> 3
        .width = 12, // 100 >> 3
        .height = 17, // 140 >> 3
        .mipLevel = 3,
        .numMipLevels = 1,
        .face = 0,
        .numFaces = 6, // preserved by atMipLevel()
    };
    expectRangeEq(mipped, expectedMipped, "Cube atMipLevel(3)");

    const auto faced = mipped.atFace(TextureCubeFace::NegZ);
    const TextureRangeDesc expectedFaced{
        .x = 1,
        .y = 2,
        .width = 12,
        .height = 17,
        .mipLevel = 3,
        .numMipLevels = 1,
        .face = 5, // TextureCubeFace::NegZ narrowed to uint32_t
        .numFaces = 1,
    };
    expectRangeEq(faced, expectedFaced, "Cube atFace(NegZ)");
  }
}

} // namespace igl::tests
