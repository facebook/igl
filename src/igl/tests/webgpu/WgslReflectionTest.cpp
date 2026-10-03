/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/WgslReflection.h>

#include <string>
#include <igl/webgpu/RenderPipelineReflection.h>

namespace igl::tests {

namespace {

// Declarations in any order, aliases, consts, attributes and nested comments.
constexpr const char* kModule = R"(
enable f16;
/* block /* nested */ comment */
const kCount = 3u;
alias Color = vec4<f32>;

@group(1) @binding(0) var<uniform> uniforms : Uniforms;
@group(1) @binding(2) var<storage, read> particles : array<Particle>;
@group(1) @binding(3) var<storage, read_write> counters : Counters;
@group(0) @binding(0) var colorTex : texture_2d<f32>;
@group(0) @binding(1) var colorSampler : sampler;
@group(0) @binding(2) var depthTex : texture_depth_2d;
@group(0) @binding(3) var shadowSampler : sampler_comparison;
@group(0) @binding(4) var indexTex : texture_2d_array<u32>;
@group(0) @binding(6) var cubeTex : texture_cube<f32>;
@group(0) @binding(8) var msTex : texture_multisampled_2d<i32>;
@group(2) @binding(0) var outImage : texture_storage_2d<rgba8unorm, write>;
@group(3) @binding(0) var<uniform> pushConstants : vec4f;

var<private> scratch : vec4f;

@id(7) override kScale : f32 = 1.0;
override kEnabled : bool;

struct Particle {
  position : vec3f,
  mass : f32,
  color : Color,
  flags : vec2<u32>,
};

struct Counters {
  total : atomic<u32>,
  perBin : array<atomic<u32>, 4>,
};

struct Uniforms {
  mvp : mat4x4f,
  normal : mat3x3<f32>,
  tint : Color,
  offset : vec2f,
  @align(16) level : i32,
  weights : array<f32, kCount>,
  @size(32) enabled : u32,
  half : vec3h,
  small : mat2x2f,
};

struct VertexOut {
  @builtin(position) position : vec4f,
  @location(0) @interpolate(flat) id : u32,
};

fn helper(x : f32) -> f32 {
  if (x > 1.0) { return x >> 1; }
  return x;
}

@vertex
fn vs(@builtin(vertex_index) i : u32) -> VertexOut {
  return VertexOut(vec4f(0.0), i);
}

@fragment
fn fs(v : VertexOut) -> @location(0) vec4f {
  return textureSample(colorTex, colorSampler, vec2f(0.5));
}

@compute @workgroup_size(8, 4i)
fn cs() {}

@compute @workgroup_size(kCount, 2, 1)
fn cs2() {}
)";

const char* toString(webgpu::WgslBindingKind kind) {
  switch (kind) {
  case webgpu::WgslBindingKind::UniformBuffer:
    return "uniform";
  case webgpu::WgslBindingKind::StorageBuffer:
    return "storage";
  case webgpu::WgslBindingKind::ReadOnlyStorageBuffer:
    return "read-only-storage";
  case webgpu::WgslBindingKind::Texture:
    return "texture";
  case webgpu::WgslBindingKind::DepthTexture:
    return "depth-texture";
  case webgpu::WgslBindingKind::MultisampledTexture:
    return "multisampled-texture";
  case webgpu::WgslBindingKind::StorageTexture:
    return "storage-texture";
  case webgpu::WgslBindingKind::Sampler:
    return "sampler";
  case webgpu::WgslBindingKind::ComparisonSampler:
    return "comparison-sampler";
  }
  return "?";
}

// Raw WGPU enum values differ between webgpu.h versions, so the golden uses names.
const char* toString(WGPUTextureViewDimension dimension) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (dimension) {
  case WGPUTextureViewDimension_1D:
    return "1d";
  case WGPUTextureViewDimension_2D:
    return "2d";
  case WGPUTextureViewDimension_2DArray:
    return "2d-array";
  case WGPUTextureViewDimension_Cube:
    return "cube";
  case WGPUTextureViewDimension_CubeArray:
    return "cube-array";
  case WGPUTextureViewDimension_3D:
    return "3d";
  default:
    return "?";
  }
}

std::string serialize(const webgpu::WgslReflection& r) {
  std::string out;
  for (const auto& b : r.bindings) {
    out += "binding " + std::to_string(b.group) + "." + std::to_string(b.binding) + " " + b.name +
           " " + toString(b.kind) + " " + b.type;
    if (b.bufferSize != 0) {
      out += " size=" + std::to_string(b.bufferSize);
    }
    if (b.viewDimension != WGPUTextureViewDimension_Undefined) {
      out += std::string(" dim=") + toString(b.viewDimension) +
             " sampled=" + std::to_string(static_cast<int>(b.sampledType));
    }
    out += "\n";
  }
  for (const auto& s : r.structs) {
    out += "struct " + s.name + " size=" + std::to_string(s.size) +
           " align=" + std::to_string(s.align) + "\n";
    for (const auto& m : s.members) {
      out += "  " + m.name + " : " + m.type + " @" + std::to_string(m.offset) +
             " size=" + std::to_string(m.size) + " align=" + std::to_string(m.align) +
             " igl=" + std::to_string(static_cast<int>(m.uniformType));
      if (m.arrayStride != 0) {
        out += " len=" + std::to_string(m.arrayLength) + " stride=" + std::to_string(m.arrayStride);
      }
      out += "\n";
    }
  }
  for (const auto& e : r.entryPoints) {
    out += "entry " + e.name + " stage=" + std::to_string(static_cast<int>(e.stage)) +
           " wg=" + std::to_string(e.workgroupSize[0]) + "," + std::to_string(e.workgroupSize[1]) +
           "," + std::to_string(e.workgroupSize[2]) + "\n";
  }
  for (const auto& o : r.overrides) {
    out += "override " + o.name + " : " + o.type +
           (o.id ? " id=" + std::to_string(*o.id) : std::string()) + "\n";
  }
  return out;
}

// UniformType: Float=1 Float2=2 Float3=3 Float4=4 Boolean=5 Int=6 Mat2x2=10 Mat3x3=11 Mat4x4=12.
// ShaderStage: Vertex=0 Fragment=1 Compute=2.
constexpr const char* kGolden = R"(binding 0.0 colorTex texture texture_2d<f32> dim=2d sampled=0
binding 0.1 colorSampler sampler sampler
binding 0.2 depthTex depth-texture texture_depth_2d dim=2d sampled=0
binding 0.3 shadowSampler comparison-sampler sampler_comparison
binding 0.4 indexTex texture texture_2d_array<u32> dim=2d-array sampled=2
binding 0.6 cubeTex texture texture_cube<f32> dim=cube sampled=0
binding 0.8 msTex multisampled-texture texture_multisampled_2d<i32> dim=2d sampled=1
binding 1.0 uniforms uniform Uniforms size=224
binding 1.2 particles read-only-storage array<Particle> size=48
binding 1.3 counters storage Counters size=20
binding 2.0 outImage storage-texture texture_storage_2d<rgba8unorm, write> dim=2d sampled=0
binding 3.0 pushConstants uniform vec4f size=16
struct Particle size=48 align=16
  position : vec3f @0 size=12 align=16 igl=3
  mass : f32 @12 size=4 align=4 igl=1
  color : vec4<f32> @16 size=16 align=16 igl=4
  flags : vec2<u32> @32 size=8 align=8 igl=7
struct Counters size=20 align=4
  total : atomic<u32> @0 size=4 align=4 igl=6
  perBin : array<atomic<u32>, 4> @4 size=16 align=4 igl=6 len=4 stride=4
struct Uniforms size=224 align=16
  mvp : mat4x4f @0 size=64 align=16 igl=12
  normal : mat3x3<f32> @64 size=48 align=16 igl=11
  tint : vec4<f32> @112 size=16 align=16 igl=4
  offset : vec2f @128 size=8 align=8 igl=2
  level : i32 @144 size=4 align=16 igl=6
  weights : array<f32, kCount> @148 size=12 align=4 igl=1 len=3 stride=4
  enabled : u32 @160 size=32 align=4 igl=6
  half : vec3h @192 size=6 align=8 igl=0
  small : mat2x2f @200 size=16 align=8 igl=10
struct VertexOut size=32 align=16
  position : vec4f @0 size=16 align=16 igl=4
  id : u32 @16 size=4 align=4 igl=6
entry vs stage=0 wg=0,0,0
entry fs stage=1 wg=0,0,0
entry cs stage=2 wg=8,4,1
entry cs2 stage=2 wg=3,2,1
override kScale : f32 id=7
override kEnabled : bool
)";

} // namespace

TEST(WebGPUWgslReflectionTest, Golden) {
  webgpu::WgslReflection reflection;
  const Result ret = webgpu::parseWgslReflection(kModule, reflection);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(serialize(reflection), kGolden);

  ASSERT_NE(reflection.findEntryPoint("fs"), nullptr);
  EXPECT_EQ(reflection.findEntryPoint("helper"), nullptr);
  ASSERT_NE(reflection.findBinding(2, 0), nullptr);
  EXPECT_EQ(reflection.findBinding(2, 0)->storageFormat, WGPUTextureFormat_RGBA8Unorm);
  EXPECT_EQ(reflection.findBinding(2, 0)->storageAccess, WGPUStorageTextureAccess_WriteOnly);
  EXPECT_EQ(reflection.findBinding(0, 5), nullptr);
  ASSERT_NE(reflection.findStruct("Uniforms"), nullptr);
  EXPECT_EQ(reflection.findStruct("Uniforms")->size, 224u);
  EXPECT_EQ(reflection.findStruct("Missing"), nullptr);
}

TEST(WebGPUWgslReflectionTest, Errors) {
  webgpu::WgslReflection reflection;
  Result ret = webgpu::parseWgslReflection(
      "@group(0) @binding(0) var a : sampler;\n@group(0) @binding(0) var b : sampler;\n",
      reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_NE(ret.message.find("declared twice"), std::string::npos) << ret.message;

  ret = webgpu::parseWgslReflection("\n@group(1) @binding(0) var<uniform> u : Missing;\n",
                                    reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_NE(ret.message.find("Missing"), std::string::npos) << ret.message;

  ret = webgpu::parseWgslReflection("struct A { b : B, };\nstruct B { a : A, };\n", reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);

  ret = webgpu::parseWgslReflection("/* unterminated", reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);

  ret = webgpu::parseWgslReflection(
      "@group(0) @binding(0) var<uniform> u : array<vec4f, 4294967296>;\n", reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);

  ret = webgpu::parseWgslReflection(
      "@group(0) @binding(0) var<uniform> u : array<vec4f, 268435456>;\n", reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_NE(ret.message.find("too large"), std::string::npos) << ret.message;

  ret = webgpu::parseWgslReflection(
      "@group(0) @binding(0) var t : texture_storage_2d<rgba8unorm, readwrite>;\n", reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);

  ret = webgpu::parseWgslReflection(
      "override g : u32 = 0;\n@group(g) @binding(g) var s : sampler;\n", reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_NE(ret.message.find("literal integers or consts"), std::string::npos) << ret.message;

  ret = webgpu::parseWgslReflection("\n\nbogus;\n", reflection);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_NE(ret.message.find("line 3"), std::string::npos) << ret.message;
  EXPECT_TRUE(reflection.bindings.empty());
}

TEST(WebGPUWgslReflectionTest, PipelineReflectionFollowsBindConvention) {
  webgpu::WgslReflection reflection;
  ASSERT_TRUE(webgpu::parseWgslReflection(kModule, reflection).isOk());
  const webgpu::RenderPipelineReflection pipeline({{ShaderStage::Fragment, &reflection}});

  // Buffers: group 1 only.
  ASSERT_EQ(pipeline.allUniformBuffers().size(), 3u);
  const BufferArgDesc& uniforms = pipeline.allUniformBuffers()[0];
  EXPECT_EQ(uniforms.name.toString(), "uniforms");
  EXPECT_EQ(uniforms.bufferIndex, 0);
  EXPECT_EQ(uniforms.bufferDataSize, 224u);
  EXPECT_EQ(uniforms.bufferAlignment, 16u);
  ASSERT_EQ(uniforms.members.size(), 9u);
  EXPECT_EQ(uniforms.members[5].name.toString(), "weights");
  EXPECT_EQ(uniforms.members[5].type, UniformType::Float);
  EXPECT_EQ(uniforms.members[5].offset, 148u);
  EXPECT_EQ(uniforms.members[5].arrayLength, 3u);
  EXPECT_EQ(uniforms.members[5].arrayStride, 4u);

  // Texture unit i at binding 2i, its sampler at 2i+1; storage texture i at group 2 binding i.
  ASSERT_EQ(pipeline.allTextures().size(), 6u);
  EXPECT_EQ(pipeline.allTextures()[0].name, "colorTex");
  EXPECT_EQ(pipeline.allTextures()[0].textureIndex, 0);
  EXPECT_EQ(pipeline.allTextures()[3].name, "cubeTex");
  EXPECT_EQ(pipeline.allTextures()[3].type, TextureType::Cube);
  EXPECT_EQ(pipeline.allTextures()[3].textureIndex, 3);
  EXPECT_EQ(pipeline.allTextures()[5].name, "outImage");
  EXPECT_EQ(pipeline.allTextures()[5].textureIndex, 0);
  ASSERT_EQ(pipeline.allSamplers().size(), 2u);
  EXPECT_EQ(pipeline.allSamplers()[1].name, "shadowSampler");
  EXPECT_EQ(pipeline.allSamplers()[1].samplerIndex, 1);

  EXPECT_EQ(pipeline.getIndexByName("counters", ShaderStage::Fragment), 3);
  EXPECT_EQ(pipeline.getIndexByName("indexTex", ShaderStage::Fragment), 2);
  EXPECT_EQ(pipeline.getIndexByName("colorSampler", ShaderStage::Fragment), 0);
  EXPECT_EQ(pipeline.getIndexByName("colorTex", ShaderStage::Vertex), -1);
  EXPECT_EQ(pipeline.getIndexByName("pushConstants", ShaderStage::Fragment), -1);
}

} // namespace igl::tests
