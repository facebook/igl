/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/VertexInputState.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// VertexSampleFunction
// ---------------------------------------------------------------------------

TEST(VertexSampleFunctionTest, EnumValues) {
  EXPECT_EQ(static_cast<int>(VertexSampleFunction::Constant), 0);
  EXPECT_EQ(static_cast<int>(VertexSampleFunction::PerVertex), 1);
  EXPECT_EQ(static_cast<int>(VertexSampleFunction::Instance), 2);
}

TEST(VertexSampleFunctionTest, ValuesAreDistinct) {
  EXPECT_NE(VertexSampleFunction::Constant, VertexSampleFunction::PerVertex);
  EXPECT_NE(VertexSampleFunction::PerVertex, VertexSampleFunction::Instance);
  EXPECT_NE(VertexSampleFunction::Constant, VertexSampleFunction::Instance);
}

// ---------------------------------------------------------------------------
// VertexAttributeFormat
// ---------------------------------------------------------------------------

TEST(VertexAttributeFormatTest, FirstValueIsZero) {
  EXPECT_EQ(static_cast<int>(VertexAttributeFormat::Float1), 0);
}

TEST(VertexAttributeFormatTest, FloatSequence) {
  EXPECT_EQ(static_cast<int>(VertexAttributeFormat::Float2),
            static_cast<int>(VertexAttributeFormat::Float1) + 1);
  EXPECT_EQ(static_cast<int>(VertexAttributeFormat::Float3),
            static_cast<int>(VertexAttributeFormat::Float1) + 2);
  EXPECT_EQ(static_cast<int>(VertexAttributeFormat::Float4),
            static_cast<int>(VertexAttributeFormat::Float1) + 3);
}

// ---------------------------------------------------------------------------
// VertexAttribute
// ---------------------------------------------------------------------------

TEST(VertexAttributeTest, DefaultConstruction) {
  const VertexAttribute attr;
  EXPECT_EQ(attr.bufferIndex, 0u);
  EXPECT_EQ(attr.format, VertexAttributeFormat::Float1);
  EXPECT_EQ(attr.offset, 0u);
  EXPECT_TRUE(attr.name.empty());
  EXPECT_EQ(attr.location, -1);
}

// ---------------------------------------------------------------------------
// VertexInputBinding
// ---------------------------------------------------------------------------

TEST(VertexInputBindingTest, DefaultConstruction) {
  const VertexInputBinding binding;
  EXPECT_EQ(binding.stride, 0u);
  EXPECT_EQ(binding.sampleFunction, VertexSampleFunction::PerVertex);
  EXPECT_EQ(binding.sampleRate, 1u);
}

// ---------------------------------------------------------------------------
// VertexInputStateDesc
// ---------------------------------------------------------------------------

TEST(VertexInputStateDescTest, DefaultConstruction) {
  const VertexInputStateDesc desc;
  EXPECT_EQ(desc.numAttributes, 0u);
  EXPECT_EQ(desc.numInputBindings, 0u);
}

TEST(VertexInputStateDescTest, ArraySizesMatchMax) {
  const VertexInputStateDesc desc;
  EXPECT_EQ(std::size(desc.attributes), IGL_VERTEX_ATTRIBUTES_MAX);
  EXPECT_EQ(std::size(desc.inputBindings), IGL_BUFFER_BINDINGS_MAX);
}

TEST(VertexInputStateDescTest, DefaultAttributesAreDefault) {
  const VertexInputStateDesc desc;
  for (const auto& attribute : desc.attributes) {
    EXPECT_EQ(attribute.bufferIndex, 0u);
    EXPECT_EQ(attribute.format, VertexAttributeFormat::Float1);
    EXPECT_EQ(attribute.offset, 0u);
  }
}

TEST(VertexInputStateDescTest, DefaultBindingsAreDefault) {
  const VertexInputStateDesc desc;
  for (const auto& binding : desc.inputBindings) {
    EXPECT_EQ(binding.stride, 0u);
    EXPECT_EQ(binding.sampleFunction, VertexSampleFunction::PerVertex);
    EXPECT_EQ(binding.sampleRate, 1u);
  }
}

} // namespace igl::tests
