/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/Common.h"

#include <IGLU/simple_renderer/ShaderUniforms.h>

namespace igl::tests {

namespace {
class TestRenderPipelineReflection final : public IRenderPipelineReflection {
 public:
  [[nodiscard]] const std::vector<BufferArgDesc>& allUniformBuffers() const override {
    return bufferArguments_;
  }
  [[nodiscard]] const std::vector<SamplerArgDesc>& allSamplers() const override {
    return samplerArguments_;
  }
  [[nodiscard]] const std::vector<TextureArgDesc>& allTextures() const override {
    return textureArguments_;
  }

  TestRenderPipelineReflection(std::vector<BufferArgDesc> bufferArguments,
                               std::vector<SamplerArgDesc> samplerArguments,
                               std::vector<TextureArgDesc> textureArguments) :
    bufferArguments_(std::move(bufferArguments)),
    samplerArguments_(std::move(samplerArguments)),
    textureArguments_(std::move(textureArguments)) {}
  ~TestRenderPipelineReflection() override = default;

 private:
  std::vector<BufferArgDesc> bufferArguments_;
  std::vector<SamplerArgDesc> samplerArguments_;
  std::vector<TextureArgDesc> textureArguments_;
};
} // namespace

class ShaderUniformsTest : public ::testing::Test {
 public:
  ShaderUniformsTest() = default;
  ~ShaderUniformsTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
  }

  void TearDown() override {}

  // Member variables
 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(ShaderUniformsTest, SettersCoverage) {
  TestRenderPipelineReflection reflection{
      std::vector<BufferArgDesc>{},
      std::vector<SamplerArgDesc>{},
      std::vector<TextureArgDesc>{},
  };
  iglu::material::ShaderUniforms shaderUniforms(*iglDev_, reflection);

  NameHandle testName;
  bool boolValue = true;
  shaderUniforms.setBool(testName, boolValue);
  shaderUniforms.setBool(testName, testName, testName, boolValue);
  shaderUniforms.setBoolArray(testName, &boolValue, 1);
  shaderUniforms.setBoolArray(testName, testName, testName, &boolValue, 1);

  iglu::simdtypes::float1 floatValue = 1.0f;
  shaderUniforms.setFloat(testName, floatValue);
  shaderUniforms.setFloat(testName, testName, testName, floatValue);
  shaderUniforms.setFloatArray(testName, &floatValue, 1);
  shaderUniforms.setFloatArray(testName, testName, testName, &floatValue, 1);

  iglu::simdtypes::float2 float2Value = {1.0f, 2.0f};
  shaderUniforms.setFloat2(testName, float2Value);
  shaderUniforms.setFloat2(testName, testName, testName, float2Value);
  shaderUniforms.setFloat2Array(testName, &float2Value, 1);
  shaderUniforms.setFloat2Array(testName, testName, testName, &float2Value, 1);

  iglu::simdtypes::float3 float3Value = {1.0f, 2.0f, 3.0f};
  shaderUniforms.setFloat3(testName, float3Value);
  shaderUniforms.setFloat3Array(testName, &float3Value, 1);

  iglu::simdtypes::float4 float4Value = {1.0f, 2.0f, 3.0f, 4.0f};
  shaderUniforms.setFloat4(testName, float4Value);
  shaderUniforms.setFloat4(testName, testName, testName, float4Value);
  shaderUniforms.setFloat4Array(testName, &float4Value, 1);
  shaderUniforms.setFloat4Array(testName, testName, testName, &float4Value, 1);

  iglu::simdtypes::int1 intValue = 1;
  shaderUniforms.setInt(testName, intValue);
  shaderUniforms.setInt(testName, testName, testName, intValue);
  shaderUniforms.setIntArray(testName, &intValue, 1);
  shaderUniforms.setIntArray(testName, testName, testName, &intValue, 1);

  iglu::simdtypes::int2 int2Value = {1, 2};
  shaderUniforms.setInt2(testName, int2Value);
  shaderUniforms.setInt2(testName, testName, testName, int2Value);

  iglu::simdtypes::float2x2 float2x2Value = {float2Value, float2Value};
  shaderUniforms.setFloat2x2(testName, float2x2Value);
  shaderUniforms.setFloat2x2(testName, testName, testName, float2x2Value);
  shaderUniforms.setFloat2x2Array(testName, &float2x2Value, 1);
  shaderUniforms.setFloat2x2Array(testName, testName, testName, &float2x2Value, 1);

  iglu::simdtypes::float3x3 float3x3Value = {float3Value, float3Value, float3Value};
  shaderUniforms.setFloat3x3(testName, float3x3Value);
  shaderUniforms.setFloat3x3(testName, testName, testName, float3x3Value);
  shaderUniforms.setFloat3x3Array(testName, &float3x3Value, 1);
  shaderUniforms.setFloat3x3Array(testName, testName, testName, &float3x3Value, 1);

  iglu::simdtypes::float4x4 float4x4Value = {float4Value, float4Value, float4Value, float4Value};
  shaderUniforms.setFloat4x4(testName, float4x4Value);
  shaderUniforms.setFloat4x4(testName, testName, testName, float4x4Value);
  shaderUniforms.setFloat4x4Array(testName, &float4x4Value, 1);
  shaderUniforms.setFloat4x4Array(testName, testName, testName, &float4x4Value, 1);

  std::shared_ptr<ITexture> texture;
  std::shared_ptr<ISamplerState> sampler;
  shaderUniforms.setTexture("test", texture, sampler);
  shaderUniforms.setTexture("test", nullptr, sampler);
  shaderUniforms.setTexture("test", nullptr, nullptr);
}

} // namespace igl::tests
