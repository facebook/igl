/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalShaderLibraryTest
//
// This test covers creation and error paths of shader libraries on Metal.
//
class MetalShaderLibraryTest : public ::testing::Test {
 public:
  MetalShaderLibraryTest() = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// CreateFromSource
//
// Test creating a shader library from a Metal source string.
//
TEST_F(MetalShaderLibraryTest, CreateFromSource) {
  Result res;

  const auto shaderSource = std::string(data::shader::kMtlSimpleShader);

  std::vector<ShaderModuleInfo> moduleInfo = {
      {.stage = ShaderStage::Vertex, .entryPoint = std::string(data::shader::kSimpleVertFunc)},
      {.stage = ShaderStage::Fragment, .entryPoint = std::string(data::shader::kSimpleFragFunc)},
  };

  auto libraryDesc =
      ShaderLibraryDesc::fromStringInput(shaderSource.c_str(), moduleInfo, "testLibrary");

  auto library = device_->createShaderLibrary(libraryDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(library, nullptr);

  // Verify we can retrieve individual shader modules
  auto vertModule = library->getShaderModule(std::string(data::shader::kSimpleVertFunc));
  ASSERT_NE(vertModule, nullptr);

  auto fragModule = library->getShaderModule(std::string(data::shader::kSimpleFragFunc));
  ASSERT_NE(fragModule, nullptr);
}

//
// InvalidSourceReturnsError
//
// Test that creating a shader library with invalid shader source returns an error.
//
TEST_F(MetalShaderLibraryTest, InvalidSourceReturnsError) {
  Result res;

  const char* invalidSource = "this is not valid Metal shader code!!!";

  std::vector<ShaderModuleInfo> moduleInfo = {
      {.stage = ShaderStage::Vertex, .entryPoint = "nonExistentFunc"},
  };

  auto libraryDesc =
      ShaderLibraryDesc::fromStringInput(invalidSource, moduleInfo, "invalidLibrary");

  auto library = device_->createShaderLibrary(libraryDesc, &res);
  ASSERT_FALSE(res.isOk());
}

//
// ShaderCompilationCountIncrements
//
// Test that shader compilation count increments after compiling a shader.
//
TEST_F(MetalShaderLibraryTest, ShaderCompilationCountIncrements) {
  const size_t countBefore = device_->getShaderCompilationCount();

  Result res;
  const auto shaderSource = std::string(data::shader::kMtlSimpleShader);

  std::vector<ShaderModuleInfo> moduleInfo = {
      {.stage = ShaderStage::Vertex, .entryPoint = std::string(data::shader::kSimpleVertFunc)},
      {.stage = ShaderStage::Fragment, .entryPoint = std::string(data::shader::kSimpleFragFunc)},
  };

  auto libraryDesc =
      ShaderLibraryDesc::fromStringInput(shaderSource.c_str(), moduleInfo, "countTestLibrary");

  auto library = device_->createShaderLibrary(libraryDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  const size_t countAfter = device_->getShaderCompilationCount();
  ASSERT_GT(countAfter, countBefore);
}

//
// FastMathPolicy
//
// Diff-3 plumbing (unit): shouldEnableFastMath() maps each ShaderOptimization onto Metal's
// fast-math lever. There is no true -O0 at runtime, so fast math is the debug/release lever:
// Default preserves the caller's explicit fastMathEnabled flag (historical behavior, existing
// clients unaffected), NoOpt (debug) forces it OFF for reference numerics, and Performance
// (release) forces it ON. Pure mapping logic — no Metal device required.
//
TEST(MetalShaderCompilerOptions, FastMathPolicy) {
  // Default honors the caller's explicit flag in both directions.
  EXPECT_TRUE(igl::metal::shouldEnableFastMath(
      ShaderCompilerOptions{.fastMathEnabled = true, .optimization = ShaderOptimization::Default}));
  EXPECT_FALSE(igl::metal::shouldEnableFastMath(ShaderCompilerOptions{
      .fastMathEnabled = false, .optimization = ShaderOptimization::Default}));

  // NoOpt (debug) forces fast math OFF regardless of the explicit flag.
  EXPECT_FALSE(igl::metal::shouldEnableFastMath(
      ShaderCompilerOptions{.fastMathEnabled = true, .optimization = ShaderOptimization::NoOpt}));

  // Performance (release) forces fast math ON regardless of the explicit flag.
  EXPECT_TRUE(igl::metal::shouldEnableFastMath(ShaderCompilerOptions{
      .fastMathEnabled = false, .optimization = ShaderOptimization::Performance}));
}

//
// CompilesForEachOptimization
//
// Diff-3 plumbing (integration): every ShaderOptimization compiles end-to-end through Metal's
// createShaderLibrary() via the options carried on ShaderInput. The release-vs-debug
// numerics/timing delta is validated on-device per the diff test plan, not here.
//
TEST_F(MetalShaderLibraryTest, CompilesForEachOptimization) {
  const auto shaderSource = std::string(data::shader::kMtlSimpleShader);
  const std::vector<ShaderModuleInfo> moduleInfo = {
      {.stage = ShaderStage::Vertex, .entryPoint = std::string(data::shader::kSimpleVertFunc)},
      {.stage = ShaderStage::Fragment, .entryPoint = std::string(data::shader::kSimpleFragFunc)},
  };

  for (const ShaderOptimization optimization :
       {ShaderOptimization::Default, ShaderOptimization::NoOpt, ShaderOptimization::Performance}) {
    auto libraryDesc =
        ShaderLibraryDesc::fromStringInput(shaderSource.c_str(), moduleInfo, "optTestLibrary");
    libraryDesc.input.options.optimization = optimization;

    Result res;
    auto library = device_->createShaderLibrary(libraryDesc, &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    EXPECT_NE(library, nullptr);
  }
}

//
// FunctionConstantSpecialization
//
// Regression coverage for the two ways createShaderLibrary() can hand back an MTLFunction that
// is unusable for pipeline creation. Both entry points live in one library, so a single test
// pins the whole discrimination in Device.mm::createShaderLibrary():
//
//  * A constant-free entry point must be looked up with plain newFunctionWithName:. Routing it
//    through newFunctionWithName:constantValues:error: regressed the Edits camera tab on iOS 16
// @fb-only
//  * An entry point that declares a function constant must be specialized even when the caller
//    supplies no values, because Metal refuses to build a pipeline from the unspecialized
//    function. Note this half fails hard: Metal raises `validateWithDevice ... failed assertion`
//    and aborts the test binary rather than returning an error.
//
TEST_F(MetalShaderLibraryTest, FunctionConstantSpecialization) {
  const char* source = R"(
    #include <metal_stdlib>
    using namespace metal;

    struct VertexOut { float4 position [[position]]; };

    vertex VertexOut vertexMain(uint vid [[vertex_id]]) {
      VertexOut out;
      out.position = float4(0.0, 0.0, 0.0, 1.0);
      return out;
    }

    fragment float4 fragmentNoConstants(VertexOut in [[stage_in]]) {
      return float4(1.0);
    }

    // Declares a function constant that has a default, so the plain lookup succeeds but still
    // reports the constant.
    constant bool kUseTint [[function_constant(0)]];
    fragment float4 fragmentDefaultedConstant(VertexOut in [[stage_in]]) {
      return kUseTint ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 1.0, 0.0, 1.0);
    }
  )";

  const std::vector<ShaderModuleInfo> moduleInfo = {
      {.stage = ShaderStage::Vertex, .entryPoint = "vertexMain"},
      {.stage = ShaderStage::Fragment, .entryPoint = "fragmentNoConstants"},
      {.stage = ShaderStage::Fragment, .entryPoint = "fragmentDefaultedConstant"},
  };

  Result res;
  auto library = device_->createShaderLibrary(
      ShaderLibraryDesc::fromStringInput(source, moduleInfo, "functionConstantLibrary"), &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(library, nullptr);

  auto vertexModule = library->getShaderModule("vertexMain");
  ASSERT_NE(vertexModule, nullptr);

  for (const char* fragmentEntryPoint : {"fragmentNoConstants", "fragmentDefaultedConstant"}) {
    auto fragmentModule = library->getShaderModule(fragmentEntryPoint);
    ASSERT_NE(fragmentModule, nullptr) << fragmentEntryPoint;

    RenderPipelineDesc pipelineDesc;
    pipelineDesc.shaderStages = device_->createShaderStages(
        ShaderStagesDesc::fromRenderModules(vertexModule, fragmentModule), &res);
    ASSERT_TRUE(res.isOk()) << fragmentEntryPoint << ": " << res.message;
    pipelineDesc.targetDesc.colorAttachments.resize(1);
    pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;

    auto pipeline = device_->createRenderPipeline(pipelineDesc, &res);
    EXPECT_TRUE(res.isOk()) << fragmentEntryPoint << ": " << res.message;
    EXPECT_NE(pipeline, nullptr) << fragmentEntryPoint;
  }
}

} // namespace igl::tests
