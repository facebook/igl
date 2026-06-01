/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/util/SpvReflection.h>

#include "../util/SpvModules.h"

namespace igl::tests {

TEST(SpvReflectionTest, UniformBufferTest) {
  using namespace vulkan::util;

  const auto& spvWords = getUniformBufferSpvWords();
  SpvModuleInfo spvModuleInfo =
      getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.buffers.size(), 2);
  EXPECT_EQ(spvModuleInfo.buffers[0].bindingLocation, 0);
  EXPECT_EQ(spvModuleInfo.buffers[1].bindingLocation, 3);
  EXPECT_EQ(spvModuleInfo.buffers[0].isStorage, false);
  EXPECT_EQ(spvModuleInfo.buffers[1].isStorage, false);
}

TEST(SpvReflectionTest, TextureTest) {
  using namespace vulkan::util;

  const auto& spvWords = getTextureSpvWords();
  SpvModuleInfo spvModuleInfo =
      getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.buffers.size(), 0);
  EXPECT_EQ(spvModuleInfo.textures.size(), 4);
  EXPECT_EQ(spvModuleInfo.textures[0].bindingLocation, kNoBindingLocation);
  EXPECT_EQ(spvModuleInfo.textures[0].descriptorSet, kNoDescriptorSet);
  EXPECT_EQ(spvModuleInfo.textures[1].bindingLocation, 1);
  EXPECT_EQ(spvModuleInfo.textures[1].descriptorSet, 0);
  EXPECT_EQ(spvModuleInfo.textures[2].bindingLocation, kNoBindingLocation);
  EXPECT_EQ(spvModuleInfo.textures[2].descriptorSet, kNoBindingLocation);
  EXPECT_EQ(spvModuleInfo.textures[3].bindingLocation, 3);
  EXPECT_EQ(spvModuleInfo.textures[3].descriptorSet, 0);

  EXPECT_EQ(spvModuleInfo.textures[0].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[1].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[2].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[3].type, TextureType::TwoD);
}

TEST(SpvReflectionTest, TextureDescriptorSetTest) {
  using namespace vulkan::util;

  const auto& spvWords = getTextureWithDescriptorSetSpvWords();
  SpvModuleInfo spvModuleInfo =
      getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.buffers.size(), 0);
  EXPECT_EQ(spvModuleInfo.textures.size(), 2);
  EXPECT_EQ(spvModuleInfo.textures[0].bindingLocation, 1);
  EXPECT_EQ(spvModuleInfo.textures[0].descriptorSet, 0);
  EXPECT_EQ(spvModuleInfo.textures[1].bindingLocation, 2);
  EXPECT_EQ(spvModuleInfo.textures[1].descriptorSet, 1);

  EXPECT_EQ(spvModuleInfo.textures[0].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[1].type, TextureType::TwoD);
}

TEST(SpvReflectionTest, TinyMeshFragmentShaderTest) {
  using namespace vulkan::util;

  const auto& spvWords = getTinyMeshFragmentShaderSpvWords();
  SpvModuleInfo spvModuleInfo =
      getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.buffers.size(), 1);
  EXPECT_EQ(spvModuleInfo.textures.size(), 2);
  EXPECT_EQ(spvModuleInfo.buffers[0].bindingLocation, 0);
  EXPECT_EQ(spvModuleInfo.buffers[0].descriptorSet, 1);
  EXPECT_EQ(spvModuleInfo.buffers[0].isStorage, false);
  EXPECT_EQ(spvModuleInfo.textures[0].bindingLocation, 0);
  EXPECT_EQ(spvModuleInfo.textures[0].descriptorSet, 0);
  EXPECT_EQ(spvModuleInfo.textures[1].bindingLocation, 4);
  EXPECT_EQ(spvModuleInfo.textures[1].descriptorSet, 0);
}

TEST(SpvReflectionTest, MergeDistinctModules) {
  using namespace vulkan::util;

  const auto& ubSpvWords = getUniformBufferSpvWords();
  const SpvModuleInfo ubInfo =
      getReflectionData(ubSpvWords.data(), ubSpvWords.size() * sizeof(uint32_t));

  const auto& texSpvWords = getTextureSpvWords();
  const SpvModuleInfo texInfo =
      getReflectionData(texSpvWords.data(), texSpvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(ubInfo.buffers.size(), 2);
  ASSERT_EQ(ubInfo.textures.size(), 0);
  ASSERT_EQ(texInfo.buffers.size(), 0);

  const SpvModuleInfo merged = mergeReflectionData(ubInfo, texInfo);

  EXPECT_EQ(merged.buffers.size(), 2);
  EXPECT_EQ(merged.textures.size(), 3);
  EXPECT_EQ(merged.buffers[0].bindingLocation, 0);
  EXPECT_EQ(merged.buffers[1].bindingLocation, 3);
}

TEST(SpvReflectionTest, MergeSameModuleDeduplicates) {
  using namespace vulkan::util;

  const auto& spvWords = getUniformBufferSpvWords();
  const SpvModuleInfo info = getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  const SpvModuleInfo merged = mergeReflectionData(info, info);

  EXPECT_EQ(merged.buffers.size(), info.buffers.size());
}

TEST(SpvReflectionTest, MergePushConstantsAndUsageMasks) {
  using namespace vulkan::util;

  SpvModuleInfo info1;
  info1.hasPushConstants = true;
  info1.usageMaskBuffers = 0x01;
  info1.usageMaskTextures = 0x00;

  SpvModuleInfo info2;
  info2.hasPushConstants = false;
  info2.usageMaskBuffers = 0x02;
  info2.usageMaskTextures = 0x04;

  const SpvModuleInfo merged = mergeReflectionData(info1, info2);

  EXPECT_TRUE(merged.hasPushConstants);
  EXPECT_EQ(merged.usageMaskBuffers, 0x03u);
  EXPECT_EQ(merged.usageMaskTextures, 0x04u);
}

} // namespace igl::tests
