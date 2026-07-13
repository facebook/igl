/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/base/IStagingBufferInterop.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// StagingBufferRegionInfo
// ---------------------------------------------------------------------------

TEST(StagingBufferRegionInfoTest, DefaultConstruction) {
  const base::StagingBufferRegionInfo info;
  EXPECT_EQ(info.handle, 0u);
  EXPECT_EQ(info.size, 0u);
}

TEST(StagingBufferRegionInfoTest, LargeHandle) {
  base::StagingBufferRegionInfo info;
  info.handle = 0xFFFFFFFFFFFFFFFFull;
  EXPECT_EQ(info.handle, 0xFFFFFFFFFFFFFFFFull);
}

TEST(StagingBufferRegionInfoTest, CopySemantics) {
  base::StagingBufferRegionInfo a;
  a.handle = 7u;
  a.size = 512u;

  const base::StagingBufferRegionInfo b = a;
  EXPECT_EQ(b.handle, 7u);
  EXPECT_EQ(b.size, 512u);
}

// ---------------------------------------------------------------------------
// UploadDestinationInfo
// ---------------------------------------------------------------------------

TEST(UploadDestinationInfoTest, DefaultConstruction) {
  const base::UploadDestinationInfo info;
  EXPECT_EQ(info.nativeHandle, nullptr);
  EXPECT_EQ(info.offset, 0u);
  EXPECT_EQ(info.size, 0u);
  EXPECT_EQ(info.imageData, nullptr);
}

TEST(UploadDestinationInfoTest, CopySemantics) {
  base::UploadDestinationInfo a;
  int handle = 0;
  float img = 0.0f;
  a.nativeHandle = &handle;
  a.offset = 32u;
  a.size = 64u;
  a.imageData = &img;

  const base::UploadDestinationInfo b = a;
  EXPECT_EQ(b.nativeHandle, &handle);
  EXPECT_EQ(b.offset, 32u);
  EXPECT_EQ(b.size, 64u);
  EXPECT_EQ(b.imageData, &img);
}

} // namespace igl::tests
