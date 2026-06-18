/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/RenderPipelineReflection.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// BufferArgDesc::BufferMemberDesc
// ---------------------------------------------------------------------------

TEST(BufferMemberDescTest, DefaultConstruction) {
  const BufferArgDesc::BufferMemberDesc desc;
  EXPECT_TRUE(desc.name.toString().empty());
  EXPECT_EQ(desc.type, UniformType::Invalid);
  EXPECT_EQ(desc.offset, 0u);
  EXPECT_EQ(desc.arrayLength, 1u);
}

TEST(BufferMemberDescTest, FieldMutationName) {
  BufferArgDesc::BufferMemberDesc desc;
  desc.name = IGL_NAMEHANDLE("myMember");
  EXPECT_STREQ(desc.name.c_str(), "myMember");
}

TEST(BufferMemberDescTest, FieldMutationType) {
  BufferArgDesc::BufferMemberDesc desc;
  desc.type = UniformType::Float;
  EXPECT_EQ(desc.type, UniformType::Float);
}

TEST(BufferMemberDescTest, FieldMutationOffset) {
  BufferArgDesc::BufferMemberDesc desc;
  desc.offset = 0x10u;
  EXPECT_EQ(desc.offset, 0x10u);
}

TEST(BufferMemberDescTest, FieldMutationArrayLength) {
  BufferArgDesc::BufferMemberDesc desc;
  desc.arrayLength = 4u;
  EXPECT_EQ(desc.arrayLength, 4u);
}

// ---------------------------------------------------------------------------
// BufferArgDesc
// ---------------------------------------------------------------------------

TEST(BufferArgDescTest, DefaultConstruction) {
  const BufferArgDesc desc;
  EXPECT_TRUE(desc.name.toString().empty());
  EXPECT_EQ(desc.bufferAlignment, 0u);
  EXPECT_EQ(desc.bufferDataSize, 0u);
  EXPECT_EQ(desc.bufferIndex, -1);
  EXPECT_EQ(desc.shaderStage, ShaderStage::Fragment);
  EXPECT_FALSE(desc.isUniformBlock);
  EXPECT_TRUE(desc.members.empty());
}

TEST(BufferArgDescTest, FieldMutationName) {
  BufferArgDesc desc;
  desc.name = IGL_NAMEHANDLE("myBuffer");
  EXPECT_STREQ(desc.name.c_str(), "myBuffer");
}

TEST(BufferArgDescTest, FieldMutationBufferAlignment) {
  BufferArgDesc desc;
  desc.bufferAlignment = 256u;
  EXPECT_EQ(desc.bufferAlignment, 256u);
}

TEST(BufferArgDescTest, FieldMutationBufferDataSize) {
  BufferArgDesc desc;
  desc.bufferDataSize = 1024u;
  EXPECT_EQ(desc.bufferDataSize, 1024u);
}

TEST(BufferArgDescTest, FieldMutationBufferIndex) {
  BufferArgDesc desc;
  desc.bufferIndex = 3;
  EXPECT_EQ(desc.bufferIndex, 3);
}

TEST(BufferArgDescTest, FieldMutationShaderStage) {
  BufferArgDesc desc;
  desc.shaderStage = ShaderStage::Vertex;
  EXPECT_EQ(desc.shaderStage, ShaderStage::Vertex);
}

TEST(BufferArgDescTest, FieldMutationIsUniformBlock) {
  BufferArgDesc desc;
  desc.isUniformBlock = true;
  EXPECT_TRUE(desc.isUniformBlock);
}

TEST(BufferArgDescTest, MembersPushAndAccess) {
  BufferArgDesc desc;
  BufferArgDesc::BufferMemberDesc member;
  member.name = IGL_NAMEHANDLE("u_color");
  member.type = UniformType::Float4;
  member.offset = 16u;
  member.arrayLength = 2u;
  desc.members.push_back(member);

  ASSERT_EQ(desc.members.size(), 1u);
  EXPECT_STREQ(desc.members[0].name.c_str(), "u_color");
  EXPECT_EQ(desc.members[0].type, UniformType::Float4);
  EXPECT_EQ(desc.members[0].offset, 16u);
  EXPECT_EQ(desc.members[0].arrayLength, 2u);
}

TEST(BufferArgDescTest, MembersMultiplePush) {
  BufferArgDesc desc;

  BufferArgDesc::BufferMemberDesc m0;
  m0.name = IGL_NAMEHANDLE("m0");
  m0.type = UniformType::Float;
  m0.offset = 0u;

  BufferArgDesc::BufferMemberDesc m1;
  m1.name = IGL_NAMEHANDLE("m1");
  m1.type = UniformType::Float2;
  m1.offset = 4u;

  desc.members.push_back(m0);
  desc.members.push_back(m1);

  ASSERT_EQ(desc.members.size(), 2u);
  EXPECT_EQ(desc.members[0].type, UniformType::Float);
  EXPECT_EQ(desc.members[0].offset, 0u);
  EXPECT_EQ(desc.members[1].type, UniformType::Float2);
  EXPECT_EQ(desc.members[1].offset, 4u);
}

TEST(BufferArgDescTest, NestedMemberDescDefaultsAfterPush) {
  BufferArgDesc desc;
  desc.members.push_back({});

  ASSERT_EQ(desc.members.size(), 1u);
  EXPECT_TRUE(desc.members[0].name.toString().empty());
  EXPECT_EQ(desc.members[0].type, UniformType::Invalid);
  EXPECT_EQ(desc.members[0].offset, 0u);
  EXPECT_EQ(desc.members[0].arrayLength, 1u);
}

// ---------------------------------------------------------------------------
// TextureArgDesc
// ---------------------------------------------------------------------------

TEST(TextureArgDescTest, DefaultConstruction) {
  const TextureArgDesc desc{};
  EXPECT_TRUE(desc.name.empty());
  EXPECT_EQ(desc.type, TextureType::Invalid);
  EXPECT_EQ(desc.textureIndex, -1);
  EXPECT_EQ(desc.shaderStage, ShaderStage::Fragment);
}

TEST(TextureArgDescTest, FieldMutationName) {
  TextureArgDesc desc;
  desc.name = "u_albedo";
  EXPECT_EQ(desc.name, "u_albedo");
}

TEST(TextureArgDescTest, FieldMutationType) {
  TextureArgDesc desc;
  desc.type = TextureType::TwoD;
  EXPECT_EQ(desc.type, TextureType::TwoD);
}

TEST(TextureArgDescTest, FieldMutationTextureIndex) {
  TextureArgDesc desc;
  desc.textureIndex = 5;
  EXPECT_EQ(desc.textureIndex, 5);
}

TEST(TextureArgDescTest, FieldMutationShaderStage) {
  TextureArgDesc desc;
  desc.shaderStage = ShaderStage::Vertex;
  EXPECT_EQ(desc.shaderStage, ShaderStage::Vertex);
}

// ---------------------------------------------------------------------------
// SamplerArgDesc
// ---------------------------------------------------------------------------

TEST(SamplerArgDescTest, DefaultConstruction) {
  const SamplerArgDesc desc;
  EXPECT_TRUE(desc.name.empty());
  EXPECT_EQ(desc.samplerIndex, -1);
  EXPECT_EQ(desc.shaderStage, ShaderStage::Fragment);
}

TEST(SamplerArgDescTest, FieldMutationName) {
  SamplerArgDesc desc;
  desc.name = "u_sampler";
  EXPECT_EQ(desc.name, "u_sampler");
}

TEST(SamplerArgDescTest, FieldMutationSamplerIndex) {
  SamplerArgDesc desc;
  desc.samplerIndex = 2;
  EXPECT_EQ(desc.samplerIndex, 2);
}

TEST(SamplerArgDescTest, FieldMutationShaderStage) {
  SamplerArgDesc desc;
  desc.shaderStage = ShaderStage::Vertex;
  EXPECT_EQ(desc.shaderStage, ShaderStage::Vertex);
}

} // namespace igl::tests
