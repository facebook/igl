/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/tests/data/ShaderData.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <igl/glslang/GlslCompiler.h>
#include <igl/glslang/GlslangHelpers.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/vulkan/util/SpvReflection.h>
#include <igl/webgpu/ShaderModule.h>
#include <igl/webgpu/WgslReflection.h>

namespace igl::tests {

namespace {

struct ShaderPair {
  const char* name;
  ShaderStage stage;
  std::string_view vulkan;
  std::string_view wgsl;
};

namespace shader = data::shader;

constexpr ShaderPair kShaderPairs[] = {
    {.name = "SimpleVert",
     .stage = ShaderStage::Vertex,
     .vulkan = shader::kVulkanSimpleVertShader,
     .wgsl = shader::kWgslSimpleVertShader},
    {.name = "SimpleFrag",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShader,
     .wgsl = shader::kWgslSimpleFragShader},
    {.name = "SimpleFragFloat",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderFloat,
     .wgsl = shader::kWgslSimpleFragShaderFloat},
    {.name = "SimpleFragFloat2",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderFloat2,
     .wgsl = shader::kWgslSimpleFragShaderFloat2},
    {.name = "SimpleFragFloat3",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderFloat3,
     .wgsl = shader::kWgslSimpleFragShaderFloat3},
    {.name = "SimpleFragFloat4",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderFloat4,
     .wgsl = shader::kWgslSimpleFragShaderFloat4},
    {.name = "SimpleFragUint",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderUint,
     .wgsl = shader::kWgslSimpleFragShaderUint},
    {.name = "SimpleFragUint2",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderUint2,
     .wgsl = shader::kWgslSimpleFragShaderUint2},
    {.name = "SimpleFragUint4",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderUint4,
     .wgsl = shader::kWgslSimpleFragShaderUint4},
    {.name = "SimpleVertTex2dArray",
     .stage = ShaderStage::Vertex,
     .vulkan = shader::kVulkanSimpleVertShaderTex2dArray,
     .wgsl = shader::kWgslSimpleVertShaderTex2dArray},
    {.name = "SimpleFragTex2dArray",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderTex2dArray,
     .wgsl = shader::kWgslSimpleFragShaderTex2dArray},
    {.name = "SimpleVertCube",
     .stage = ShaderStage::Vertex,
     .vulkan = shader::kVulkanSimpleVertShaderCube,
     .wgsl = shader::kWgslSimpleVertShaderCube},
    {.name = "SimpleFragCube",
     .stage = ShaderStage::Fragment,
     .vulkan = shader::kVulkanSimpleFragShaderCube,
     .wgsl = shader::kWgslSimpleFragShaderCube},
    {.name = "SimpleCompute",
     .stage = ShaderStage::Compute,
     .vulkan = shader::kVulkanSimpleComputeShader,
     .wgsl = shader::kWgslSimpleComputeShader},
};

WGPUTextureViewDimension toViewDimension(TextureType type) {
  switch (type) {
  case TextureType::TwoD:
    return WGPUTextureViewDimension_2D;
  case TextureType::TwoDArray:
    return WGPUTextureViewDimension_2DArray;
  case TextureType::Cube:
    return WGPUTextureViewDimension_Cube;
  case TextureType::ThreeD:
    return WGPUTextureViewDimension_3D;
  case TextureType::Invalid:
  case TextureType::ExternalImage:
    return WGPUTextureViewDimension_Undefined;
  }
  return WGPUTextureViewDimension_Undefined;
}

} // namespace

class WebGPUShaderDataTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    glslang::initializeCompiler();
  }
  static void TearDownTestSuite() {
    glslang::finalizeCompiler();
  }
};

// Every kWgsl* shader compiles on the device and has a `main` entry point for its stage.
TEST_F(WebGPUShaderDataTest, Compiles) {
  auto device = util::device::webgpu::createTestDevice();
  ASSERT_NE(device, nullptr);
  for (const ShaderPair& pair : kShaderPairs) {
    const std::string source(pair.wgsl);
    Result ret;
    auto module = device->createShaderModule(
        {
            .info = {.stage = pair.stage, .entryPoint = "main"},
            .input = {.source = source.c_str(), .type = ShaderInputType::String},
            .debugName = pair.name,
        },
        &ret);
    EXPECT_TRUE(ret.isOk()) << pair.name << ": " << ret.message;
    EXPECT_NE(module, nullptr) << pair.name;
  }
  EXPECT_EQ(device->getContext().getUncapturedErrorCount(), 0u);
}

// Drift guard: each kWgsl* shader declares exactly the resources of its kVulkan* counterpart,
// remapped by the bind convention (see ShaderData.h).
TEST_F(WebGPUShaderDataTest, MatchesVulkanBindings) {
  glslang_resource_t resource = {};
  glslangGetDefaultResource(&resource);

  for (const ShaderPair& pair : kShaderPairs) {
    const std::string glsl = "#version 460\n" + std::string(pair.vulkan);
    std::vector<uint32_t> spirv;
    const Result compiled = glslang::compileShader(pair.stage, glsl.c_str(), spirv, &resource);
    ASSERT_TRUE(compiled.isOk()) << pair.name << ": " << compiled.message;
    const vulkan::util::SpvModuleInfo spv =
        vulkan::util::getReflectionData(spirv.data(), spirv.size() * sizeof(uint32_t));

    webgpu::WgslReflection wgsl;
    const Result parsed = webgpu::parseWgslReflection(pair.wgsl, wgsl);
    ASSERT_TRUE(parsed.isOk()) << pair.name << ": " << parsed.message;

    EXPECT_EQ(wgsl.bindings.size(), 2 * spv.textures.size() + spv.buffers.size()) << pair.name;
    for (const vulkan::util::TextureDescription& texture : spv.textures) {
      EXPECT_EQ(texture.descriptorSet, webgpu::kTextureGroup) << pair.name;
      const webgpu::WgslBinding* image =
          wgsl.findBinding(webgpu::kTextureGroup, 2 * texture.bindingLocation);
      const webgpu::WgslBinding* sampler =
          wgsl.findBinding(webgpu::kTextureGroup, 2 * texture.bindingLocation + 1);
      ASSERT_NE(image, nullptr) << pair.name;
      ASSERT_NE(sampler, nullptr) << pair.name;
      EXPECT_EQ(image->kind, webgpu::WgslBindingKind::Texture) << pair.name;
      EXPECT_EQ(image->viewDimension, toViewDimension(texture.type)) << pair.name;
      EXPECT_EQ(sampler->kind, webgpu::WgslBindingKind::Sampler) << pair.name;
    }
    for (const vulkan::util::BufferDescription& buffer : spv.buffers) {
      const webgpu::WgslBinding* binding =
          wgsl.findBinding(buffer.descriptorSet, buffer.bindingLocation);
      ASSERT_NE(binding, nullptr) << pair.name;
      EXPECT_EQ(binding->group, webgpu::kBufferGroup) << pair.name;
      EXPECT_EQ(binding->kind != webgpu::WgslBindingKind::UniformBuffer, buffer.isStorage)
          << pair.name;
    }
    const webgpu::WgslEntryPoint* entryPoint = wgsl.findEntryPoint("main");
    ASSERT_NE(entryPoint, nullptr) << pair.name;
    EXPECT_EQ(entryPoint->stage, pair.stage) << pair.name;
  }
}

} // namespace igl::tests
