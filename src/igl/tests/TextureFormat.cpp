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
  testUsage(TextureDesc::TextureUsageBits::Sampled, "Sampled");
}

TEST_F(TextureFormatTest, SampledAttachment) {
  testUsage(TextureDesc::TextureUsageBits::Sampled | TextureDesc::TextureUsageBits::Attachment,
            "SampledAttachment");
}

TEST_F(TextureFormatTest, Attachment) {
  testUsage(TextureDesc::TextureUsageBits::Attachment, "Attachment");
}

TEST_F(TextureFormatTest, Storage) {
  testUsage(TextureDesc::TextureUsageBits::Storage, "Storage");
}

} // namespace igl::tests
