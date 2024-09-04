/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "util/TextureFormatTestBase.h"

namespace igl::tests {

class TextureFormatTest : public util::TextureFormatTestBase {
 private:
 public:
  TextureFormatTest() = default;
  ~TextureFormatTest() override = default;
};

TEST_F(TextureFormatTest, Sampled) {
#if IGL_PLATFORM_LINUX_SWIFTSHADER && IGL_BACKEND_VULKAN
  // Leak sanitizer crashes with "LeakSanitizer has encountered a fatal error" in these tests
  // for SwiftShader Vulkan in Linux.
  GTEST_SKIP() << "Leak sanitizer crashes for these tests if SwiftShader is in use.";
#endif
  testUsage(TextureDesc::TextureUsageBits::Sampled, "Sampled");
}

TEST_F(TextureFormatTest, SampledAttachment) {
#if IGL_PLATFORM_LINUX_SWIFTSHADER && IGL_BACKEND_VULKAN
  // Leak sanitizer crashes with "LeakSanitizer has encountered a fatal error" in these tests
  // for SwiftShader Vulkan in Linux.
  GTEST_SKIP() << "Leak sanitizer crashes for these tests if SwiftShader is in use.";
#endif
  testUsage(TextureDesc::TextureUsageBits::Sampled | TextureDesc::TextureUsageBits::Attachment,
            "SampledAttachment");
}

TEST_F(TextureFormatTest, Attachment) {
#if IGL_PLATFORM_LINUX_SWIFTSHADER && IGL_BACKEND_VULKAN
  // Leak sanitizer crashes with "LeakSanitizer has encountered a fatal error" in these tests
  // for SwiftShader Vulkan in Linux.
  GTEST_SKIP() << "Leak sanitizer crashes for these tests if SwiftShader is in use.";
#endif
  testUsage(TextureDesc::TextureUsageBits::Attachment, "Attachment");
}

TEST_F(TextureFormatTest, Storage) {
#if IGL_PLATFORM_LINUX_SWIFTSHADER && IGL_BACKEND_VULKAN
  // Leak sanitizer crashes with "LeakSanitizer has encountered a fatal error" in these tests
  // for SwiftShader Vulkan in Linux.
  GTEST_SKIP() << "Leak sanitizer crashes for these tests if SwiftShader is in use.";
#endif
  testUsage(TextureDesc::TextureUsageBits::Storage, "Storage");
}

} // namespace igl::tests
