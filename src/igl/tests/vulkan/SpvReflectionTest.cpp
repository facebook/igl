/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/util/SpvReflection.h>

#include "../util/SpvModules.h"

#include <gtest/gtest.h>

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

} // namespace igl::tests
