/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/simple_renderer/Material.h>
#include <IGLU/simple_renderer/ParametricVertexData.h>

namespace igl::tests {

// --- PrimitiveDesc ---

TEST(PrimitiveDescTest, DefaultConstruction) {
  const iglu::vertexdata::PrimitiveDesc desc;
  EXPECT_EQ(desc.numEntries, 0u);
  EXPECT_EQ(desc.offset, 0u);
  EXPECT_EQ(desc.frontFaceWinding, igl::WindingMode::CounterClockwise);
}

TEST(PrimitiveDescTest, DesignatedInitializer) {
  const iglu::vertexdata::PrimitiveDesc desc{
      .numEntries = 42, .offset = 10, .frontFaceWinding = igl::WindingMode::Clockwise};
  EXPECT_EQ(desc.numEntries, 42u);
  EXPECT_EQ(desc.offset, 10u);
  EXPECT_EQ(desc.frontFaceWinding, igl::WindingMode::Clockwise);
}

TEST(PrimitiveDescTest, CopyPreservesFields) {
  const iglu::vertexdata::PrimitiveDesc original{
      .numEntries = 99, .offset = 7, .frontFaceWinding = igl::WindingMode::Clockwise};
  const iglu::vertexdata::PrimitiveDesc copy = original;
  EXPECT_EQ(copy.numEntries, original.numEntries);
  EXPECT_EQ(copy.offset, original.offset);
  EXPECT_EQ(copy.frontFaceWinding, original.frontFaceWinding);
}

// --- Quad::inputStateDesc() ---

TEST(QuadInputStateDescTest, AttributeCount) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  EXPECT_EQ(desc.numAttributes, 2u);
}

TEST(QuadInputStateDescTest, PositionAttributeFormat) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 1u);
  EXPECT_EQ(desc.attributes[0].format, igl::VertexAttributeFormat::Float3);
}

TEST(QuadInputStateDescTest, PositionAttributeName) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 1u);
  EXPECT_EQ(desc.attributes[0].name, "a_position");
}

TEST(QuadInputStateDescTest, PositionAttributeLocation) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 1u);
  EXPECT_EQ(desc.attributes[0].location, 0);
}

TEST(QuadInputStateDescTest, PositionAttributeBufferIndex) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 1u);
  EXPECT_EQ(desc.attributes[0].bufferIndex, 0u);
}

TEST(QuadInputStateDescTest, PositionAttributeOffset) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 1u);
  EXPECT_EQ(desc.attributes[0].offset, offsetof(iglu::vertexdata::VertexPosUv, position));
}

TEST(QuadInputStateDescTest, UvAttributeFormat) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 2u);
  EXPECT_EQ(desc.attributes[1].format, igl::VertexAttributeFormat::Float2);
}

TEST(QuadInputStateDescTest, UvAttributeName) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 2u);
  EXPECT_EQ(desc.attributes[1].name, "a_uv");
}

TEST(QuadInputStateDescTest, UvAttributeLocation) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 2u);
  EXPECT_EQ(desc.attributes[1].location, 1);
}

TEST(QuadInputStateDescTest, UvAttributeBufferIndex) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 2u);
  EXPECT_EQ(desc.attributes[1].bufferIndex, 0u);
}

TEST(QuadInputStateDescTest, UvAttributeOffset) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numAttributes, 2u);
  EXPECT_EQ(desc.attributes[1].offset, offsetof(iglu::vertexdata::VertexPosUv, uv));
}

TEST(QuadInputStateDescTest, InputBindingCount) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  EXPECT_EQ(desc.numInputBindings, 1u);
}

TEST(QuadInputStateDescTest, InputBindingStride) {
  const igl::VertexInputStateDesc desc = iglu::vertexdata::Quad::inputStateDesc();
  ASSERT_GE(desc.numInputBindings, 1u);
  EXPECT_EQ(desc.inputBindings[0].stride, sizeof(iglu::vertexdata::VertexPosUv));
}

TEST(QuadInputStateDescTest, DescriptorConsistency) {
  const igl::VertexInputStateDesc desc1 = iglu::vertexdata::Quad::inputStateDesc();
  const igl::VertexInputStateDesc desc2 = iglu::vertexdata::Quad::inputStateDesc();
  EXPECT_EQ(desc1, desc2);
}

// --- RenderToTextureQuad ---

TEST(RenderToTextureQuadTest, InputStateDescMatchesQuad) {
  const igl::VertexInputStateDesc quadDesc = iglu::vertexdata::Quad::inputStateDesc();
  const igl::VertexInputStateDesc rttDesc = iglu::vertexdata::RenderToTextureQuad::inputStateDesc();
  EXPECT_EQ(quadDesc, rttDesc);
}

// --- DepthTestConfig ---

TEST(DepthTestConfigTest, AllValuesDistinct) {
  EXPECT_NE(iglu::material::DepthTestConfig::Disable, iglu::material::DepthTestConfig::Enable);
  EXPECT_NE(iglu::material::DepthTestConfig::Disable,
            iglu::material::DepthTestConfig::EnableNoWrite);
  EXPECT_NE(iglu::material::DepthTestConfig::Enable,
            iglu::material::DepthTestConfig::EnableNoWrite);
}

// --- BlendMode two-argument constructor ---

TEST(BlendModeConstructionTest, TwoArgSetsSymmetricBlendFactors) {
  const iglu::material::BlendMode mode(igl::BlendFactor::SrcAlpha,
                                       igl::BlendFactor::OneMinusSrcAlpha);
  EXPECT_EQ(mode.srcRGB, igl::BlendFactor::SrcAlpha);
  EXPECT_EQ(mode.dstRGB, igl::BlendFactor::OneMinusSrcAlpha);
  EXPECT_EQ(mode.opRGB, igl::BlendOp::Add);
  EXPECT_EQ(mode.srcAlpha, igl::BlendFactor::SrcAlpha);
  EXPECT_EQ(mode.dstAlpha, igl::BlendFactor::OneMinusSrcAlpha);
  EXPECT_EQ(mode.opAlpha, igl::BlendOp::Add);
}

} // namespace igl::tests
