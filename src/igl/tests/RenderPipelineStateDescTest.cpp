/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <functional>
#include <igl/ComputePipelineState.h>
#include <igl/RenderPipelineState.h>

namespace igl::tests {

// -------------------------------------------------------------------------
// RenderPipelineDesc::TargetDesc::ColorAttachment
// -------------------------------------------------------------------------

TEST(ColorAttachmentDescTest, DefaultConstruction) {
  const RenderPipelineDesc::TargetDesc::ColorAttachment ca;
  EXPECT_EQ(ca.textureFormat, TextureFormat::Invalid);
  EXPECT_EQ(ca.colorWriteMask, kColorWriteBitsAll);
  EXPECT_FALSE(ca.blendEnabled);
  EXPECT_EQ(ca.rgbBlendOp, BlendOp::Add);
  EXPECT_EQ(ca.alphaBlendOp, BlendOp::Add);
  EXPECT_EQ(ca.srcRGBBlendFactor, BlendFactor::One);
  EXPECT_EQ(ca.srcAlphaBlendFactor, BlendFactor::One);
  EXPECT_EQ(ca.dstRGBBlendFactor, BlendFactor::Zero);
  EXPECT_EQ(ca.dstAlphaBlendFactor, BlendFactor::Zero);
}

TEST(ColorAttachmentDescTest, EqualityOpReflexive) {
  const RenderPipelineDesc::TargetDesc::ColorAttachment ca;
  EXPECT_EQ(ca, ca);
}

TEST(ColorAttachmentDescTest, EqualityOpSameValues) {
  const RenderPipelineDesc::TargetDesc::ColorAttachment a;
  const RenderPipelineDesc::TargetDesc::ColorAttachment b;
  EXPECT_EQ(a, b);
}

TEST(ColorAttachmentDescTest, InequalityOpDifferentTextureFormat) {
  RenderPipelineDesc::TargetDesc::ColorAttachment a;
  RenderPipelineDesc::TargetDesc::ColorAttachment b;
  b.textureFormat = TextureFormat::RGBA_UNorm8;
  EXPECT_NE(a, b);
}

TEST(ColorAttachmentDescTest, InequalityOpDifferentColorWriteMask) {
  RenderPipelineDesc::TargetDesc::ColorAttachment a;
  RenderPipelineDesc::TargetDesc::ColorAttachment b;
  b.colorWriteMask = kColorWriteBitsRed;
  EXPECT_NE(a, b);
}

TEST(ColorAttachmentDescTest, InequalityOpDifferentBlendEnabled) {
  RenderPipelineDesc::TargetDesc::ColorAttachment a;
  RenderPipelineDesc::TargetDesc::ColorAttachment b;
  b.blendEnabled = true;
  EXPECT_NE(a, b);
}

TEST(ColorAttachmentDescTest, InequalityOpDifferentRgbBlendOp) {
  RenderPipelineDesc::TargetDesc::ColorAttachment a;
  RenderPipelineDesc::TargetDesc::ColorAttachment b;
  b.rgbBlendOp = BlendOp::Subtract;
  EXPECT_NE(a, b);
}

TEST(ColorAttachmentDescTest, InequalityOpDifferentAlphaBlendOp) {
  RenderPipelineDesc::TargetDesc::ColorAttachment a;
  RenderPipelineDesc::TargetDesc::ColorAttachment b;
  b.alphaBlendOp = BlendOp::Max;
  EXPECT_NE(a, b);
}

TEST(ColorAttachmentDescTest, InequalityOpDifferentSrcRGBBlendFactor) {
  RenderPipelineDesc::TargetDesc::ColorAttachment a;
  RenderPipelineDesc::TargetDesc::ColorAttachment b;
  b.srcRGBBlendFactor = BlendFactor::SrcAlpha;
  EXPECT_NE(a, b);
}

TEST(ColorAttachmentDescTest, InequalityOpDifferentDstRGBBlendFactor) {
  RenderPipelineDesc::TargetDesc::ColorAttachment a;
  RenderPipelineDesc::TargetDesc::ColorAttachment b;
  b.dstRGBBlendFactor = BlendFactor::OneMinusSrcAlpha;
  EXPECT_NE(a, b);
}

TEST(ColorAttachmentDescTest, HashConsistency) {
  const RenderPipelineDesc::TargetDesc::ColorAttachment ca;
  const std::hash<RenderPipelineDesc::TargetDesc::ColorAttachment> hasher;
  EXPECT_EQ(hasher(ca), hasher(ca));
}

TEST(ColorAttachmentDescTest, HashEqualObjectsHaveSameHash) {
  const RenderPipelineDesc::TargetDesc::ColorAttachment a;
  const RenderPipelineDesc::TargetDesc::ColorAttachment b;
  const std::hash<RenderPipelineDesc::TargetDesc::ColorAttachment> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

// -------------------------------------------------------------------------
// RenderPipelineDesc::TargetDesc
// -------------------------------------------------------------------------

TEST(TargetDescTest, DefaultConstruction) {
  const RenderPipelineDesc::TargetDesc td;
  EXPECT_TRUE(td.colorAttachments.empty());
  EXPECT_EQ(td.depthAttachmentFormat, TextureFormat::Invalid);
  EXPECT_EQ(td.stencilAttachmentFormat, TextureFormat::Invalid);
}

TEST(TargetDescTest, EqualityOpReflexive) {
  const RenderPipelineDesc::TargetDesc td;
  EXPECT_EQ(td, td);
}

TEST(TargetDescTest, EqualityOpSameValues) {
  const RenderPipelineDesc::TargetDesc a;
  const RenderPipelineDesc::TargetDesc b;
  EXPECT_EQ(a, b);
}

TEST(TargetDescTest, InequalityOpDifferentDepthFormat) {
  RenderPipelineDesc::TargetDesc a;
  RenderPipelineDesc::TargetDesc b;
  b.depthAttachmentFormat = TextureFormat::Z_UNorm16;
  EXPECT_NE(a, b);
}

TEST(TargetDescTest, InequalityOpDifferentStencilFormat) {
  RenderPipelineDesc::TargetDesc a;
  RenderPipelineDesc::TargetDesc b;
  b.stencilAttachmentFormat = TextureFormat::S_UInt8;
  EXPECT_NE(a, b);
}

TEST(TargetDescTest, InequalityOpDifferentColorAttachmentCount) {
  RenderPipelineDesc::TargetDesc a;
  RenderPipelineDesc::TargetDesc b;
  b.colorAttachments.push_back({});
  EXPECT_NE(a, b);
}

TEST(TargetDescTest, InequalityOpDifferentColorAttachmentFormat) {
  RenderPipelineDesc::TargetDesc a;
  RenderPipelineDesc::TargetDesc b;
  a.colorAttachments.push_back({});
  b.colorAttachments.push_back({.textureFormat = TextureFormat::RGBA_UNorm8});
  EXPECT_NE(a, b);
}

TEST(TargetDescTest, HashConsistency) {
  const RenderPipelineDesc::TargetDesc td;
  const std::hash<RenderPipelineDesc::TargetDesc> hasher;
  EXPECT_EQ(hasher(td), hasher(td));
}

TEST(TargetDescTest, HashEqualObjectsHaveSameHash) {
  const RenderPipelineDesc::TargetDesc a;
  const RenderPipelineDesc::TargetDesc b;
  const std::hash<RenderPipelineDesc::TargetDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

// -------------------------------------------------------------------------
// RenderPipelineDesc
// -------------------------------------------------------------------------

TEST(RenderPipelineDescTest, DefaultConstruction) {
  const RenderPipelineDesc desc;
  EXPECT_EQ(desc.topology, PrimitiveType::Triangle);
  EXPECT_EQ(desc.vertexInputState, nullptr);
  EXPECT_EQ(desc.shaderStages, nullptr);
  EXPECT_TRUE(desc.targetDesc.colorAttachments.empty());
  EXPECT_EQ(desc.cullMode, CullMode::Disabled);
  EXPECT_EQ(desc.frontFaceWinding, WindingMode::CounterClockwise);
  EXPECT_EQ(desc.polygonFillMode, PolygonFillMode::Fill);
  EXPECT_EQ(desc.sampleCount, 1u);
  EXPECT_EQ(desc.isDynamicBufferMask, 0u);
  EXPECT_FALSE(desc.alphaToCoverageEnabled);
  EXPECT_TRUE(desc.debugName.toString().empty());
}

TEST(RenderPipelineDescTest, EqualityOpReflexive) {
  const RenderPipelineDesc desc;
  EXPECT_EQ(desc, desc);
}

TEST(RenderPipelineDescTest, EqualityOpSameValues) {
  const RenderPipelineDesc a;
  const RenderPipelineDesc b;
  EXPECT_EQ(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentTopology) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.topology = PrimitiveType::Line;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentCullMode) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.cullMode = CullMode::Back;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentFrontFaceWinding) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.frontFaceWinding = WindingMode::Clockwise;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentPolygonFillMode) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.polygonFillMode = PolygonFillMode::Line;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentSampleCount) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.sampleCount = 4u;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentIsDynamicBufferMask) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.isDynamicBufferMask = 1u;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentTargetDesc) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.targetDesc.depthAttachmentFormat = TextureFormat::Z_UNorm16;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentAlphaToCoverageEnabled) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.alphaToCoverageEnabled = true;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentVertexUnitSamplerMap) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.vertexUnitSamplerMap[0] = IGL_NAMEHANDLE("vertSampler");
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentFragmentUnitSamplerMap) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("fragSampler");
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentUniformBlockBindingMap) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.uniformBlockBindingMap[0] = {{IGL_NAMEHANDLE("block"), IGL_NAMEHANDLE("instance")}};
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, DebugNameIncludedInEquality) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  a.debugName = IGL_NAMEHANDLE("pipeline_a");
  b.debugName = IGL_NAMEHANDLE("pipeline_b");
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, AlphaToCoverageDefaultFalse) {
  const RenderPipelineDesc desc;
  EXPECT_FALSE(desc.alphaToCoverageEnabled);
}

TEST(RenderPipelineDescTest, InequalityOpDifferentAlphaToCoverage) {
  RenderPipelineDesc a;
  RenderPipelineDesc b;
  b.alphaToCoverageEnabled = true;
  EXPECT_NE(a, b);
}

TEST(RenderPipelineDescTest, ImmutableSamplersDefaultNull) {
  const RenderPipelineDesc desc;
  for (const auto& sampler : desc.immutableSamplers) {
    EXPECT_EQ(sampler, nullptr);
  }
}

TEST(RenderPipelineDescTest, DesignatedInitializerTargetDesc) {
  const RenderPipelineDesc::TargetDesc td{.depthAttachmentFormat = TextureFormat::Z_UNorm16,
                                          .stencilAttachmentFormat = TextureFormat::S_UInt8};
  EXPECT_EQ(td.depthAttachmentFormat, TextureFormat::Z_UNorm16);
  EXPECT_EQ(td.stencilAttachmentFormat, TextureFormat::S_UInt8);
  EXPECT_TRUE(td.colorAttachments.empty());
}

TEST(RenderPipelineDescTest, HashConsistency) {
  const RenderPipelineDesc desc;
  const std::hash<RenderPipelineDesc> hasher;
  EXPECT_EQ(hasher(desc), hasher(desc));
}

TEST(RenderPipelineDescTest, HashEqualObjectsHaveSameHash) {
  const RenderPipelineDesc a;
  const RenderPipelineDesc b;
  const std::hash<RenderPipelineDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

// -------------------------------------------------------------------------
// ComputePipelineDesc
// -------------------------------------------------------------------------

TEST(ComputePipelineDescTest, DefaultConstruction) {
  const ComputePipelineDesc desc;
  EXPECT_EQ(desc.shaderStages, nullptr);
  EXPECT_TRUE(desc.imagesMap.empty());
  EXPECT_TRUE(desc.buffersMap.empty());
  EXPECT_TRUE(desc.debugName.empty());
}

TEST(ComputePipelineDescTest, EqualityOpReflexive) {
  const ComputePipelineDesc desc;
  EXPECT_EQ(desc, desc);
}

TEST(ComputePipelineDescTest, EqualityOpSameValues) {
  const ComputePipelineDesc a;
  const ComputePipelineDesc b;
  EXPECT_EQ(a, b);
}

TEST(ComputePipelineDescTest, InequalityOpDifferentShaderStages) {
  ComputePipelineDesc a;
  ComputePipelineDesc b;
  static int sentinel = 0;
  b.shaderStages = std::shared_ptr<IShaderStages>(reinterpret_cast<IShaderStages*>(&sentinel),
                                                  [](IShaderStages*) {});
  EXPECT_NE(a, b);
}

TEST(ComputePipelineDescTest, InequalityOpDifferentDebugName) {
  ComputePipelineDesc a;
  ComputePipelineDesc b;
  b.debugName = "compute_pass";
  EXPECT_NE(a, b);
}

TEST(ComputePipelineDescTest, InequalityOpDifferentBuffersMap) {
  ComputePipelineDesc a;
  ComputePipelineDesc b;
  b.buffersMap[0] = IGL_NAMEHANDLE("input");
  EXPECT_NE(a, b);
}

TEST(ComputePipelineDescTest, InequalityOpDifferentImagesMap) {
  ComputePipelineDesc a;
  ComputePipelineDesc b;
  b.imagesMap[0] = IGL_NAMEHANDLE("storageImage");
  EXPECT_NE(a, b);
}

TEST(ComputePipelineDescTest, HashConsistency) {
  const ComputePipelineDesc desc;
  const std::hash<ComputePipelineDesc> hasher;
  EXPECT_EQ(hasher(desc), hasher(desc));
}

TEST(ComputePipelineDescTest, HashEqualObjectsHaveSameHash) {
  const ComputePipelineDesc a;
  const ComputePipelineDesc b;
  const std::hash<ComputePipelineDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

} // namespace igl::tests
