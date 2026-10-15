/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/util/SpvReflection.h>

#include "../util/SpvModules.h"

#include <algorithm>
#include <igl/glslang/GlslCompiler.h>
#include <igl/glslang/GlslangHelpers.h>

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

TEST(SpvReflectionTest, MergeEmptyModules) {
  using namespace vulkan::util;

  const SpvModuleInfo empty1;
  const SpvModuleInfo empty2;

  const SpvModuleInfo merged = mergeReflectionData(empty1, empty2);

  EXPECT_TRUE(merged.buffers.empty());
  EXPECT_TRUE(merged.textures.empty());
  EXPECT_FALSE(merged.hasPushConstants);
  EXPECT_EQ(merged.usageMaskBuffers, 0u);
  EXPECT_EQ(merged.usageMaskTextures, 0u);
}

TEST(SpvReflectionTest, MergePushConstantsPreservedFromEitherSide) {
  using namespace vulkan::util;

  SpvModuleInfo withPush;
  withPush.hasPushConstants = true;

  SpvModuleInfo noPush;
  noPush.hasPushConstants = false;

  const SpvModuleInfo merged1 = mergeReflectionData(withPush, noPush);
  EXPECT_TRUE(merged1.hasPushConstants);

  const SpvModuleInfo merged2 = mergeReflectionData(noPush, withPush);
  EXPECT_TRUE(merged2.hasPushConstants);
}

TEST(SpvReflectionTest, UsageMasksMergeWithBitwiseOr) {
  using namespace vulkan::util;

  SpvModuleInfo info1;
  info1.usageMaskBuffers = 0x0F;
  info1.usageMaskTextures = 0xF0;

  SpvModuleInfo info2;
  info2.usageMaskBuffers = 0xF0;
  info2.usageMaskTextures = 0x0F;

  const SpvModuleInfo merged = mergeReflectionData(info1, info2);

  EXPECT_EQ(merged.usageMaskBuffers, 0xFFu);
  EXPECT_EQ(merged.usageMaskTextures, 0xFFu);
}

TEST(SpvReflectionTest, ReflectsSingleInputAttachment) {
  using namespace vulkan::util;

  glslang::initializeCompiler();

  const char* fragmentShader = R"(#version 460
layout(input_attachment_index = 1, set = 2, binding = 17) uniform subpassInput inA;
layout(location = 0) out vec4 outColor;
void main() {
  outColor = subpassLoad(inA);
})";

  glslang_resource_t resource{};
  glslangGetDefaultResource(&resource);

  std::vector<uint32_t> spvWords;
  Result ret = glslang::compileShader(ShaderStage::Fragment, fragmentShader, spvWords, &resource);
  glslang::finalizeCompiler();
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  SpvModuleInfo spvModuleInfo =
      getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.inputAttachments.size(), 1);
  EXPECT_EQ(spvModuleInfo.inputAttachments[0].bindingLocation, 17);
  EXPECT_EQ(spvModuleInfo.inputAttachments[0].descriptorSet, 2);
  EXPECT_EQ(spvModuleInfo.inputAttachments[0].inputAttachmentIndex, 1);
  EXPECT_TRUE(spvModuleInfo.images.empty());
  EXPECT_TRUE(spvModuleInfo.textures.empty());
  EXPECT_TRUE(spvModuleInfo.buffers.empty());
}

TEST(SpvReflectionTest, ReflectsMultipleInputAttachments) {
  using namespace vulkan::util;

  glslang::initializeCompiler();

  const char* fragmentShader = R"(#version 460
layout(input_attachment_index = 1, set = 2, binding = 17) uniform subpassInput inA;
layout(input_attachment_index = 2, set = 2, binding = 18) uniform subpassInput inB;
layout(input_attachment_index = 3, set = 2, binding = 19) uniform subpassInput inC;
layout(location = 0) out vec4 outColor;
void main() {
  outColor = subpassLoad(inA) + subpassLoad(inB) + subpassLoad(inC);
})";

  glslang_resource_t resource{};
  glslangGetDefaultResource(&resource);

  std::vector<uint32_t> spvWords;
  Result ret = glslang::compileShader(ShaderStage::Fragment, fragmentShader, spvWords, &resource);
  glslang::finalizeCompiler();
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  SpvModuleInfo spvModuleInfo =
      getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.inputAttachments.size(), 3);

  // Check all three input attachments are present (order-independent)
  std::vector<uint32_t> indices;
  std::vector<uint32_t> bindings;
  for (const auto& ia : spvModuleInfo.inputAttachments) {
    indices.push_back(ia.inputAttachmentIndex);
    bindings.push_back(ia.bindingLocation);
    EXPECT_EQ(ia.descriptorSet, 2);
  }

  std::sort(indices.begin(), indices.end());
  std::sort(bindings.begin(), bindings.end());

  EXPECT_EQ(indices[0], 1);
  EXPECT_EQ(indices[1], 2);
  EXPECT_EQ(indices[2], 3);
  EXPECT_EQ(bindings[0], 17);
  EXPECT_EQ(bindings[1], 18);
  EXPECT_EQ(bindings[2], 19);
  EXPECT_TRUE(spvModuleInfo.images.empty());
  EXPECT_TRUE(spvModuleInfo.textures.empty());
}

TEST(SpvReflectionTest, MergeIncludesInputAttachments) {
  using namespace vulkan::util;

  glslang::initializeCompiler();

  glslang_resource_t resource{};
  glslangGetDefaultResource(&resource);

  // Vertex shader with no input attachments
  const char* vertexShader = R"(#version 460
layout(location = 0) in vec3 position;
layout(location = 0) out vec3 vColor;
void main() {
  gl_Position = vec4(position, 1.0);
  vColor = position;
})";

  std::vector<uint32_t> vertSpvWords;
  Result ret = glslang::compileShader(ShaderStage::Vertex, vertexShader, vertSpvWords, &resource);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  SpvModuleInfo vertInfo =
      getReflectionData(vertSpvWords.data(), vertSpvWords.size() * sizeof(uint32_t));

  // Fragment shader with input attachments
  const char* fragmentShader = R"(#version 460
layout(input_attachment_index = 1, set = 2, binding = 17) uniform subpassInput inA;
layout(location = 0) in vec3 vColor;
layout(location = 0) out vec4 outColor;
void main() {
  outColor = subpassLoad(inA);
})";

  std::vector<uint32_t> fragSpvWords;
  ret = glslang::compileShader(ShaderStage::Fragment, fragmentShader, fragSpvWords, &resource);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  SpvModuleInfo fragInfo =
      getReflectionData(fragSpvWords.data(), fragSpvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(fragInfo.inputAttachments.size(), 1);

  // Merge should preserve input attachments
  SpvModuleInfo merged = mergeReflectionData(vertInfo, fragInfo);

  glslang::finalizeCompiler();

  ASSERT_EQ(merged.inputAttachments.size(), 1);
  EXPECT_EQ(merged.inputAttachments[0].bindingLocation, 17);
  EXPECT_EQ(merged.inputAttachments[0].descriptorSet, 2);
  EXPECT_EQ(merged.inputAttachments[0].inputAttachmentIndex, 1);
}

TEST(SpvReflectionTest, ReflectsSubpassInputArrayAsArray) {
  using namespace vulkan::util;

  glslang::initializeCompiler();

  const char* fragmentShader = R"(#version 460
layout(input_attachment_index = 0, set = 2, binding = 16) uniform subpassInput inA[2];
layout(location = 0) out vec4 outColor;
void main() {
  outColor = subpassLoad(inA[0]);
})";

  glslang_resource_t resource{};
  glslangGetDefaultResource(&resource);

  std::vector<uint32_t> spvWords;
  Result ret = glslang::compileShader(ShaderStage::Fragment, fragmentShader, spvWords, &resource);
  glslang::finalizeCompiler();
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  SpvModuleInfo spvModuleInfo =
      getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.inputAttachments.size(), 1);
  EXPECT_TRUE(spvModuleInfo.inputAttachments[0].isArray);
  EXPECT_FALSE(spvModuleInfo.inputAttachments[0].isMultisampled);
}

TEST(SpvReflectionTest, ReflectsStorageImageAndInputAttachmentSeparately) {
  using namespace vulkan::util;

  glslang::initializeCompiler();

  const char* fragmentShader = R"(#version 460
layout(set = 2, binding = 0, rgba8) uniform image2D img;
layout(input_attachment_index = 1, set = 2, binding = 17) uniform subpassInput inA;
layout(location = 0) out vec4 outColor;
void main() {
  outColor = subpassLoad(inA);
})";

  glslang_resource_t resource{};
  glslangGetDefaultResource(&resource);

  std::vector<uint32_t> spvWords;
  Result ret = glslang::compileShader(ShaderStage::Fragment, fragmentShader, spvWords, &resource);
  glslang::finalizeCompiler();
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  SpvModuleInfo spvModuleInfo =
      getReflectionData(spvWords.data(), spvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.images.size(), 1);
  ASSERT_EQ(spvModuleInfo.inputAttachments.size(), 1);
  EXPECT_EQ(spvModuleInfo.images[0].bindingLocation, 0);
  EXPECT_EQ(spvModuleInfo.images[0].descriptorSet, 2);
  EXPECT_EQ(spvModuleInfo.inputAttachments[0].bindingLocation, 17);
  EXPECT_EQ(spvModuleInfo.inputAttachments[0].descriptorSet, 2);
}

} // namespace igl::tests
