/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/tests/webgpu/TriangleRender.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <igl/tests/util/device/webgpu/TestDevice.h>

namespace igl::tests {

// The native half of the wasm comparison: with IGL_RENDER_SNAPSHOT_DIR set, writes the frame to
// <dir>/TriangleWebGPU.rgba (256x256, top row first) for comparison with the wasm sample's frame.
TEST(WebGPUTriangleRenderTest, RendersReferenceFrame) {
  auto device = util::device::webgpu::createTestDevice();
  ASSERT_NE(device, nullptr);
  constexpr uint32_t kSize = 256;
  std::vector<uint8_t> rgba;
  const Result ret = webgpu::renderTriangle(*device, kSize, rgba);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_EQ(rgba.size(), size_t{kSize} * kSize * 4);
  // Clear color in the corner, the blend of the three vertex colors at the centroid.
  EXPECT_NEAR(rgba[0], 51, 1);
  EXPECT_NEAR(rgba[1], 77, 1);
  EXPECT_NEAR(rgba[2], 102, 1);
  const size_t center = (size_t{kSize} * (kSize / 2 + 10) + kSize / 2) * 4;
  EXPECT_GT(rgba[center], 40);
  EXPECT_GT(rgba[center + 1], 40);
  EXPECT_GT(rgba[center + 2], 40);
  EXPECT_EQ(device->getContext().getUncapturedErrorCount(), 0u);

  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (const char* dir = std::getenv("IGL_RENDER_SNAPSHOT_DIR"); dir != nullptr && *dir != '\0') {
    std::ofstream file(std::string(dir) + "/TriangleWebGPU.rgba", std::ios::binary);
    file.write(reinterpret_cast<const char*>(rgba.data()),
               static_cast<std::streamsize>(rgba.size()));
  }
}

} // namespace igl::tests
