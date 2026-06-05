/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/VertexInputState.h>

#include <functional>

namespace igl::tests {

//
// VertexInputStateTest
//
// Tests types related to abstraction of vertex input state (attribute formats,
// attributes, bindings, and input state descriptors).
//
//
class VertexInputStateTest : public ::testing::Test {
 public:
  VertexInputStateTest() = default;
  ~VertexInputStateTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
  }

  void TearDown() override {}

  // Member variables
 protected:
  const std::string backend_ = IGL_BACKEND_TYPE;
};

//
// VertexAttributeSizes
//
// Verifies that all vertex attribute formats report sizes that agree with what is expected by
// extant backends.
//
TEST_F(VertexInputStateTest, VertexAttributeSizes) {
  ASSERT_EQ(4, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Float1));
  ASSERT_EQ(8, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Float2));
  ASSERT_EQ(12, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Float3));
  ASSERT_EQ(16, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Float4));
  ASSERT_EQ(1, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Byte1));
  ASSERT_EQ(2, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Byte2));
  ASSERT_EQ(3, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Byte3));
  ASSERT_EQ(4, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Byte4));
  ASSERT_EQ(1, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UByte1));
  ASSERT_EQ(2, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UByte2));
  ASSERT_EQ(3, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UByte3));
  ASSERT_EQ(4, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UByte4));
  ASSERT_EQ(2, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Short1));
  ASSERT_EQ(4, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Short2));
  ASSERT_EQ(6, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Short3));
  ASSERT_EQ(8, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Short4));
  ASSERT_EQ(2, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UShort1));
  ASSERT_EQ(4, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UShort2));
  ASSERT_EQ(6, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UShort3));
  ASSERT_EQ(8, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UShort4));
  ASSERT_EQ(1,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Byte1Norm));
  ASSERT_EQ(2,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Byte2Norm));
  ASSERT_EQ(3,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Byte3Norm));
  ASSERT_EQ(4,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Byte4Norm));
  ASSERT_EQ(1,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UByte1Norm));
  ASSERT_EQ(2,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UByte2Norm));
  ASSERT_EQ(3,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UByte3Norm));
  ASSERT_EQ(4,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UByte4Norm));
  ASSERT_EQ(2,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Short1Norm));
  ASSERT_EQ(4,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Short2Norm));
  ASSERT_EQ(6,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Short3Norm));
  ASSERT_EQ(8,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Short4Norm));
  ASSERT_EQ(2,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UShort1Norm));
  ASSERT_EQ(4,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UShort2Norm));
  ASSERT_EQ(6,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UShort3Norm));
  ASSERT_EQ(8,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UShort4Norm));
  ASSERT_EQ(2,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::HalfFloat1));
  ASSERT_EQ(4,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::HalfFloat2));
  ASSERT_EQ(6,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::HalfFloat3));
  ASSERT_EQ(8,
            VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::HalfFloat4));

  ASSERT_EQ(4,
            VertexInputStateDesc::sizeForVertexAttributeFormat(
                VertexAttributeFormat::Int_2_10_10_10_REV));

  ASSERT_EQ(4, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Int1));
  ASSERT_EQ(8, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Int2));
  ASSERT_EQ(12, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Int3));
  ASSERT_EQ(16, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::Int4));

  ASSERT_EQ(4, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UInt1));
  ASSERT_EQ(8, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UInt2));
  ASSERT_EQ(12, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UInt3));
  ASSERT_EQ(16, VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat::UInt4));
}

//
// VertexInputBindingEquality
//
// Exercises == and != operators for the VertexInputBinding type
//
TEST_F(VertexInputStateTest, VertexInputBindingEquality) {
  VertexInputBinding binding1, binding2;

  // Default-constructed bindings should be equal.
  ASSERT_EQ(binding1, binding2);

  // Bindings that differ in sample function should compare non-equal.
  binding1.sampleFunction = VertexSampleFunction::Constant;
  binding2.sampleFunction = VertexSampleFunction::PerVertex;
  ASSERT_NE(binding1, binding2);
  binding1.sampleFunction = binding2.sampleFunction;
  ASSERT_EQ(binding1, binding2);

  // Bindings that differ in stride should compare non-equal.
  binding1.stride = 16;
  binding2.stride = 12;
  ASSERT_NE(binding1, binding2);
  binding1.stride = binding2.stride;
  ASSERT_EQ(binding1, binding2);

  // Bindings that differ in stride should compare non-equal.
  binding1.sampleRate = 4;
  binding2.sampleRate = 1;
  ASSERT_NE(binding1, binding2);
  binding1.sampleRate = binding2.sampleRate;
  ASSERT_EQ(binding1, binding2);
}

//
// VertexAttributeEquality
//
// Exercises == and != operators for the VertexAttribute type
//
TEST_F(VertexInputStateTest, VertexAttributeEquality) {
  VertexAttribute attr1, attr2;

  // Default-constructed attributes should be equal.
  ASSERT_EQ(attr1, attr2);

  // Attributes with different buffer indices should compare non-equal.
  attr1.bufferIndex = 1;
  ASSERT_NE(attr1, attr2);
  attr1.bufferIndex = attr2.bufferIndex;
  ASSERT_EQ(attr1, attr2);

  // Attributes with different formats should compare non-equal.
  attr1.format = VertexAttributeFormat::Float4;
  ASSERT_NE(attr1, attr2);
  attr1.format = attr2.format;
  ASSERT_EQ(attr1, attr2);

  // Attributes with different offsets should compare non-equal.
  attr1.offset = 8;
  ASSERT_NE(attr1, attr2);
  attr1.offset = attr2.offset;
  ASSERT_EQ(attr1, attr2);

  // Attributes with different names should compare non-equal (OpenGL ES-only).
  attr1.name = "a";
  ASSERT_NE(attr1, attr2);
  attr1.name = attr2.name;
  ASSERT_EQ(attr1, attr2);

  // Attributes with different locations should compare non-equal (Metal-only).
  attr1.location = 2;
  ASSERT_NE(attr1, attr2);
  attr1.location = attr2.location;
  ASSERT_EQ(attr1, attr2);
}

//
// VertexInputStateDescEquality
//
// Exercises == and != operators for the VertexInputStateDesc type
//
TEST_F(VertexInputStateTest, VertexInputStateDescEquality) {
  VertexInputStateDesc desc1, desc2;

  // Default-constructed descriptors should be equal.
  ASSERT_EQ(desc1, desc2);

  // Descriptors populated with the same attributes and bindings should compare equal.
  desc1.numAttributes = 2;
  desc1.attributes[0].bufferIndex = 0;
  desc1.attributes[0].format = VertexAttributeFormat::Float4;
  desc1.attributes[0].offset = 0;
  desc1.attributes[1].bufferIndex = 1;
  desc1.attributes[1].format = VertexAttributeFormat::Float4;
  desc1.attributes[1].offset = 0;
  desc1.numInputBindings = 2;
  desc1.inputBindings[0].stride = 16;
  desc1.inputBindings[1].stride = 16;
  desc2.numAttributes = desc1.numAttributes;
  desc2.attributes[0].bufferIndex = desc1.attributes[0].bufferIndex;
  desc2.attributes[0].format = desc1.attributes[0].format;
  desc2.attributes[0].offset = desc1.attributes[0].offset;
  desc2.attributes[1].bufferIndex = desc1.attributes[1].bufferIndex;
  desc2.attributes[1].format = desc1.attributes[1].format;
  desc2.attributes[1].offset = desc1.attributes[1].offset;
  desc2.numInputBindings = desc1.numInputBindings;
  desc2.inputBindings[0].stride = desc1.inputBindings[0].stride;
  desc2.inputBindings[1].stride = desc1.inputBindings[1].stride;
  ASSERT_EQ(desc1, desc2);

  // Descriptors with different attribute counts should compare non-equal.
  desc1.numAttributes = 1;
  ASSERT_NE(desc1, desc2);
  desc1.numAttributes = desc2.numAttributes;
  ASSERT_EQ(desc1, desc2);

  // Descriptors with differing attributes should compare non-equal.
  desc1.attributes[0].format = VertexAttributeFormat::Float2;
  ASSERT_NE(desc1, desc2);
  desc1.attributes[0].format = desc2.attributes[0].format;
  ASSERT_EQ(desc1, desc2);

  // Descriptors with different binding counts should compare non-equal.
  desc1.numInputBindings = 1;
  ASSERT_NE(desc1, desc2);
  desc1.numInputBindings = desc2.numInputBindings;
  ASSERT_EQ(desc1, desc2);

  // Descriptors with differing bindings should compare non-equal.
  desc1.inputBindings[0].stride = 32;
  ASSERT_NE(desc1, desc2);
  desc1.inputBindings[0].stride = desc2.inputBindings[0].stride;
  ASSERT_EQ(desc1, desc2);
}

TEST_F(VertexInputStateTest, VertexInputStateDescHashConsistency) {
  const VertexInputStateDesc desc;
  const std::hash<VertexInputStateDesc> hasher;
  EXPECT_EQ(hasher(desc), hasher(desc));
}

TEST_F(VertexInputStateTest, VertexInputStateDescHashEqualObjectsHaveSameHash) {
  const VertexInputStateDesc a{
      .numAttributes = 1,
      .attributes = {{.format = VertexAttributeFormat::Float3, .offset = 0}},
      .numInputBindings = 1,
      .inputBindings = {{.stride = 12}},
  };
  const VertexInputStateDesc b{
      .numAttributes = 1,
      .attributes = {{.format = VertexAttributeFormat::Float3, .offset = 0}},
      .numInputBindings = 1,
      .inputBindings = {{.stride = 12}},
  };

  const std::hash<VertexInputStateDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST_F(VertexInputStateTest, VertexInputStateDescHashDifferentObjectsHaveDifferentHash) {
  const VertexInputStateDesc a{
      .numAttributes = 1,
      .attributes = {{.format = VertexAttributeFormat::Float3}},
      .numInputBindings = 1,
      .inputBindings = {{.stride = 12}},
  };
  const VertexInputStateDesc b{
      .numAttributes = 1,
      .attributes = {{.format = VertexAttributeFormat::Float4}},
      .numInputBindings = 1,
      .inputBindings = {{.stride = 16}},
  };

  const std::hash<VertexInputStateDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST_F(VertexInputStateTest, VertexInputBindingHashEqualObjectsHaveSameHash) {
  const VertexInputBinding a{
      .stride = 16,
      .sampleFunction = VertexSampleFunction::PerVertex,
      .sampleRate = 1,
  };
  const VertexInputBinding b{
      .stride = 16,
      .sampleFunction = VertexSampleFunction::PerVertex,
      .sampleRate = 1,
  };

  const std::hash<VertexInputBinding> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST_F(VertexInputStateTest, VertexInputBindingHashDifferentObjectsHaveDifferentHash) {
  const VertexInputBinding a{
      .stride = 16,
      .sampleFunction = VertexSampleFunction::PerVertex,
      .sampleRate = 1,
  };
  const VertexInputBinding b{
      .stride = 32,
      .sampleFunction = VertexSampleFunction::Instance,
      .sampleRate = 2,
  };

  const std::hash<VertexInputBinding> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST_F(VertexInputStateTest, VertexAttributeHashEqualObjectsHaveSameHash) {
  const VertexAttribute a{
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float3,
      .offset = 12,
      .name = "position",
      .location = 0,
  };
  const VertexAttribute b{
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float3,
      .offset = 12,
      .name = "position",
      .location = 0,
  };

  const std::hash<VertexAttribute> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST_F(VertexInputStateTest, VertexAttributeHashDifferentObjectsHaveDifferentHash) {
  const VertexAttribute a{
      .bufferIndex = 0,
      .format = VertexAttributeFormat::Float3,
      .offset = 0,
  };
  const VertexAttribute b{
      .bufferIndex = 0,
      .format = VertexAttributeFormat::Float4,
      .offset = 0,
  };

  const std::hash<VertexAttribute> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

//
// VertexAttributeHash
//
// Exercises the std::hash<VertexAttribute> specialization: hashing is
// deterministic, equal attributes hash equally, and attributes differing in any
// field hash differently.
//
TEST_F(VertexInputStateTest, VertexAttributeHash) {
  const std::hash<VertexAttribute> hasher;

  const VertexAttribute a{
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float3,
      .offset = 12,
      .name = "pos",
      .location = 0,
  };
  // Distinct object with identical field values.
  const VertexAttribute b{
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float3,
      .offset = 12,
      .name = "pos",
      .location = 0,
  };

  EXPECT_EQ(hasher(a), hasher(a));
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));

  // Each variant differs from `a` in exactly one hashed field, so a regression
  // that drops any single field from the hash combine surfaces here.
  const VertexAttribute differsInBufferIndex{
      .bufferIndex = 2,
      .format = VertexAttributeFormat::Float3,
      .offset = 12,
      .name = "pos",
      .location = 0,
  };
  EXPECT_NE(a, differsInBufferIndex);
  EXPECT_NE(hasher(a), hasher(differsInBufferIndex));

  const VertexAttribute differsInFormat{
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float4,
      .offset = 12,
      .name = "pos",
      .location = 0,
  };
  EXPECT_NE(a, differsInFormat);
  EXPECT_NE(hasher(a), hasher(differsInFormat));

  const VertexAttribute differsInOffset{
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float3,
      .offset = 16,
      .name = "pos",
      .location = 0,
  };
  EXPECT_NE(a, differsInOffset);
  EXPECT_NE(hasher(a), hasher(differsInOffset));

  const VertexAttribute differsInName{
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float3,
      .offset = 12,
      .name = "normal",
      .location = 0,
  };
  EXPECT_NE(a, differsInName);
  EXPECT_NE(hasher(a), hasher(differsInName));

  const VertexAttribute differsInLocation{
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float3,
      .offset = 12,
      .name = "pos",
      .location = 1,
  };
  EXPECT_NE(a, differsInLocation);
  EXPECT_NE(hasher(a), hasher(differsInLocation));
}

//
// VertexInputBindingHash
//
// Exercises the std::hash<VertexInputBinding> specialization: hashing is
// deterministic, equal bindings hash equally, and bindings differing in any
// field hash differently.
//
TEST_F(VertexInputStateTest, VertexInputBindingHash) {
  const std::hash<VertexInputBinding> hasher;

  const VertexInputBinding a{
      .stride = 16,
      .sampleFunction = VertexSampleFunction::PerVertex,
      .sampleRate = 1,
  };
  // Distinct object with identical field values.
  const VertexInputBinding b{
      .stride = 16,
      .sampleFunction = VertexSampleFunction::PerVertex,
      .sampleRate = 1,
  };

  EXPECT_EQ(hasher(a), hasher(a));
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));

  // Each variant differs from `a` in exactly one hashed field, so a regression
  // that drops any single field from the hash combine surfaces here.
  const VertexInputBinding differsInStride{
      .stride = 32,
      .sampleFunction = VertexSampleFunction::PerVertex,
      .sampleRate = 1,
  };
  EXPECT_NE(a, differsInStride);
  EXPECT_NE(hasher(a), hasher(differsInStride));

  const VertexInputBinding differsInSampleFunction{
      .stride = 16,
      .sampleFunction = VertexSampleFunction::Instance,
      .sampleRate = 1,
  };
  EXPECT_NE(a, differsInSampleFunction);
  EXPECT_NE(hasher(a), hasher(differsInSampleFunction));

  const VertexInputBinding differsInSampleRate{
      .stride = 16,
      .sampleFunction = VertexSampleFunction::PerVertex,
      .sampleRate = 4,
  };
  EXPECT_NE(a, differsInSampleRate);
  EXPECT_NE(hasher(a), hasher(differsInSampleRate));
}

} // namespace igl::tests
