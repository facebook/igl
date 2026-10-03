/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/ShaderModule.h>

#include <array>
#include <memory>
#include <string>
#include <igl/Shader.h>
#include <igl/tests/data/ShaderData.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>

namespace igl::tests {

namespace {

ShaderModuleDesc wgslDesc(const char* source, ShaderStage stage, std::string entryPoint) {
  return {
      .info = {.stage = stage, .entryPoint = std::move(entryPoint)},
      .input = {.source = source, .type = ShaderInputType::String},
      .debugName = "test",
  };
}

} // namespace

class WebGPUShaderModuleTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {
    EXPECT_EQ(device_->getContext().getUncapturedErrorCount(), 0u);
  }

 protected:
  std::unique_ptr<webgpu::Device> device_;
};

TEST_F(WebGPUShaderModuleTest, CompilesWgsl) {
  const std::string source(data::shader::kWgslSimpleFragShader);
  Result ret;
  auto module =
      device_->createShaderModule(wgslDesc(source.c_str(), ShaderStage::Fragment, "main"), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->info().entryPoint, "main");
  EXPECT_EQ(module->info().stage, ShaderStage::Fragment);
  EXPECT_EQ(device_->getShaderCompilationCount(), 1u);
}

TEST_F(WebGPUShaderModuleTest, CompileErrorsAreReturned) {
  Result ret;
  auto module = device_->createShaderModule(
      wgslDesc("@fragment fn main() -> @location(0) vec4f {\n  return undeclared;\n}\n",
               ShaderStage::Fragment,
               "main"),
      &ret);
  EXPECT_EQ(module, nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_NE(ret.message.find("undeclared"), std::string::npos) << ret.message;
  EXPECT_NE(ret.message.find("2:"), std::string::npos) << ret.message;
  EXPECT_EQ(device_->getShaderCompilationCount(), 0u);
}

TEST_F(WebGPUShaderModuleTest, BinaryInputIsUnsupported) {
  const std::array<uint32_t, 5> spirv = {0x07230203, 0x00010300, 0, 1, 0};
  Result ret;
  auto module = device_->createShaderModule(
      {
          .info = {.stage = ShaderStage::Vertex, .entryPoint = "main"},
          .input = {.data = spirv.data(), .length = sizeof(spirv), .type = ShaderInputType::Binary},
      },
      &ret);
  EXPECT_EQ(module, nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST_F(WebGPUShaderModuleTest, InvalidInputIsRejected) {
  Result ret;
  EXPECT_EQ(device_->createShaderModule(wgslDesc(nullptr, ShaderStage::Vertex, "main"), &ret),
            nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);

  const std::string source(data::shader::kWgslSimpleVertShader);
  EXPECT_EQ(device_->createShaderModule(wgslDesc(source.c_str(), ShaderStage::Vertex, ""), &ret),
            nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST_F(WebGPUShaderModuleTest, ShaderStages) {
  const std::string vertexSource(data::shader::kWgslSimpleVertShader);
  const std::string fragmentSource(data::shader::kWgslSimpleFragShader);
  Result ret;
  auto vertex = device_->createShaderModule(
      wgslDesc(vertexSource.c_str(), ShaderStage::Vertex, "main"), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto fragment = device_->createShaderModule(
      wgslDesc(fragmentSource.c_str(), ShaderStage::Fragment, "main"), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto stages =
      device_->createShaderStages(ShaderStagesDesc::fromRenderModules(vertex, fragment), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(stages, nullptr);
  EXPECT_EQ(stages->getVertexModule(), vertex);

  EXPECT_EQ(device_->createShaderStages(ShaderStagesDesc::fromRenderModules(vertex, nullptr), &ret),
            nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST_F(WebGPUShaderModuleTest, EntryPointMustExistForStage) {
  const std::string source(data::shader::kWgslSimpleFragShader);
  Result ret;
  EXPECT_EQ(
      device_->createShaderModule(wgslDesc(source.c_str(), ShaderStage::Fragment, "missing"), &ret),
      nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_NE(ret.message.find("missing"), std::string::npos) << ret.message;

  EXPECT_EQ(
      device_->createShaderModule(wgslDesc(source.c_str(), ShaderStage::Vertex, "main"), &ret),
      nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST_F(WebGPUShaderModuleTest, ModuleReflection) {
  const std::string source(data::shader::kWgslSimpleFragShader);
  Result ret;
  auto module = std::static_pointer_cast<webgpu::ShaderModule>(
      device_->createShaderModule(wgslDesc(source.c_str(), ShaderStage::Fragment, "main"), &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(module->getEntryPoint(), nullptr);
  EXPECT_EQ(module->getEntryPoint()->stage, ShaderStage::Fragment);
  EXPECT_EQ(module->getReflection().bindings.size(), 2u);
}

TEST_F(WebGPUShaderModuleTest, LibraryCompilesOnceForAllEntryPoints) {
  const std::string source(data::shader::kWgslSimpleShader);
  Result ret;
  auto library = device_->createShaderLibrary(
      ShaderLibraryDesc::fromStringInput(
          source.c_str(),
          {{.stage = ShaderStage::Vertex, .entryPoint = "vertexShader"},
           {.stage = ShaderStage::Fragment, .entryPoint = "fragmentShader"}},
          "library"),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(library, nullptr);
  EXPECT_EQ(device_->getShaderCompilationCount(), 1u);
  auto vertex =
      std::static_pointer_cast<webgpu::ShaderModule>(library->getShaderModule("vertexShader"));
  auto fragment =
      std::static_pointer_cast<webgpu::ShaderModule>(library->getShaderModule("fragmentShader"));
  ASSERT_TRUE(vertex && fragment);
  EXPECT_EQ(vertex->getWGPUShaderModule(), fragment->getWGPUShaderModule());

  EXPECT_EQ(device_->createShaderLibrary(
                ShaderLibraryDesc::fromStringInput(
                    source.c_str(),
                    {{.stage = ShaderStage::Fragment, .entryPoint = "vertexShader"}},
                    "library"),
                &ret),
            nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

} // namespace igl::tests
