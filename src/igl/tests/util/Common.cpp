/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "Common.h"

#include "../data/ShaderData.h"
#include "TestDevice.h"

#include <igl/ShaderCreator.h>

namespace igl::tests::util {

//
// Creates an IGL device and a command queue
//
void createDeviceAndQueue(std::shared_ptr<IDevice>& dev, std::shared_ptr<ICommandQueue>& cq) {
  Result ret;

  // Create Device
  dev = util::createTestDevice();
  ASSERT_TRUE(dev != nullptr);

  // Create Command Queue
  const CommandQueueDesc cqDesc = {};
  cq = dev->createCommandQueue(cqDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cq != nullptr); // Shouldn't trigger if above is okay
}

void createShaderStages(const std::shared_ptr<IDevice>& dev,
                        std::string_view vertexSource,
                        std::string_view vertexEntryPoint,
                        std::string_view fragmentSource,
                        std::string_view fragmentEntryPoint,
                        std::unique_ptr<IShaderStages>& stages) {
  Result ret;
  stages = ShaderStagesCreator::fromModuleStringInput(*dev,
                                                      vertexSource.data(),
                                                      std::string(vertexEntryPoint),
                                                      "",
                                                      fragmentSource.data(),
                                                      std::string(fragmentEntryPoint),
                                                      "",
                                                      &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message.c_str();
  ASSERT_TRUE(stages != nullptr);
}

void createShaderStages(const std::shared_ptr<IDevice>& dev,
                        std::string_view librarySource,
                        std::string_view vertexEntryPoint,
                        std::string_view fragmentEntryPoint,
                        std::unique_ptr<IShaderStages>& stages) {
  Result ret;
  stages = ShaderStagesCreator::fromLibraryStringInput(*dev,
                                                       librarySource.data(),
                                                       std::string(vertexEntryPoint),
                                                       std::string(fragmentEntryPoint),
                                                       "",
                                                       &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  ASSERT_TRUE(stages != nullptr);
}

void createSimpleShaderStages(const std::shared_ptr<IDevice>& dev,
                              std::unique_ptr<IShaderStages>& stages,
                              TextureFormat outputFormat) {
  const auto backendVersion = dev->getBackendVersion();

  if (backendVersion.flavor == igl::BackendFlavor::OpenGL ||
      backendVersion.flavor == igl::BackendFlavor::OpenGL_ES) {
    const bool isGles3 = backendVersion.flavor == igl::BackendFlavor::OpenGL_ES &&
                         backendVersion.majorVersion >= 3;
    std::string_view vertexShader = isGles3 ? igl::tests::data::shader::kOglSimpleVertShaderEs3
                                            : igl::tests::data::shader::kOglSimpleVertShader;
    std::string_view fragmentShader = isGles3 ? igl::tests::data::shader::kOglSimpleFragShaderEs3
                                              : igl::tests::data::shader::kOglSimpleFragShader;

    createShaderStages(dev,
                       vertexShader,
                       igl::tests::data::shader::kShaderFunc,
                       fragmentShader,
                       igl::tests::data::shader::kShaderFunc,
                       stages);
  } else if (backendVersion.flavor == igl::BackendFlavor::Metal) {
    std::string_view shader = igl::tests::data::shader::kMtlSimpleShader;
    if (outputFormat == TextureFormat::RG_UInt16) {
      shader = igl::tests::data::shader::kMtlSimpleShaderUshort2;
    } else if (outputFormat == TextureFormat::R_UInt16) {
      shader = igl::tests::data::shader::kMtlSimpleShaderUshort2;
    } else if (outputFormat == TextureFormat::RGB10_A2_Uint_Rev) {
      shader = igl::tests::data::shader::kMtlSimpleShaderUshort4;
    } else if (outputFormat == TextureFormat::RGBA_UInt32) {
      shader = igl::tests::data::shader::kMtlSimpleShaderUint4;
    } else if (outputFormat == TextureFormat::R_UInt32) {
      shader = igl::tests::data::shader::kMtlSimpleShaderUint;
    } else if (outputFormat != TextureFormat::Invalid) {
      auto components = TextureFormatProperties::fromTextureFormat(outputFormat).componentsPerPixel;
      switch (components) {
      case 1:
        shader = igl::tests::data::shader::kMtlSimpleShaderFloat;
        break;
      case 2:
        shader = igl::tests::data::shader::kMtlSimpleShaderFloat2;
        break;
      case 3:
        shader = igl::tests::data::shader::kMtlSimpleShaderFloat3;
        break;
      case 4:
        shader = igl::tests::data::shader::kMtlSimpleShaderFloat4;
        break;
      default:
        ASSERT_TRUE(false);
        break;
      }
    }
    createShaderStages(dev,
                       shader,
                       igl::tests::data::shader::kSimpleVertFunc,
                       igl::tests::data::shader::kSimpleFragFunc,
                       stages);
  } else if (backendVersion.flavor == igl::BackendFlavor::Vulkan) {
    // Output format-specific shaders needed for MoltenVK
    std::string_view fragShader = igl::tests::data::shader::kVulkanSimpleFragShader;
    if (outputFormat == TextureFormat::RG_UInt16) {
      fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderUint2;
    } else if (outputFormat == TextureFormat::R_UInt16) {
      fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderUint2;
    } else if (outputFormat == TextureFormat::RGB10_A2_Uint_Rev) {
      fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderUint4;
    } else if (outputFormat == TextureFormat::RGBA_UInt32) {
      fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderUint4;
    } else if (outputFormat == TextureFormat::R_UInt32) {
      fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderUint;
    } else if (outputFormat != TextureFormat::Invalid) {
      auto components = TextureFormatProperties::fromTextureFormat(outputFormat).componentsPerPixel;
      switch (components) {
      case 1:
        fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderFloat;
        break;
      case 2:
        fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderFloat2;
        break;
      case 3:
        fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderFloat3;
        break;
      case 4:
        fragShader = igl::tests::data::shader::kVulkanSimpleFragShaderFloat4;
        break;
      default:
        ASSERT_TRUE(false);
        break;
      }
    }
    createShaderStages(dev,
                       igl::tests::data::shader::kVulkanSimpleVertShader,
                       std::string(igl::tests::data::shader::kShaderFunc),
                       fragShader,
                       std::string(igl::tests::data::shader::kShaderFunc),
                       stages);
  } else if (backendVersion.flavor == igl::BackendFlavor::D3D12) {
    // Minimal HLSL equivalent used for D3D12 tests
    const char* vsHlsl = R"(
struct VSIn { float4 position_in : POSITION; float2 uv_in : TEXCOORD0; };
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
PSIn main(VSIn i) { PSIn o; o.position = i.position_in; o.uv = i.uv_in; return o; }
)";
    const char* psHlsl = R"(
Texture2D inputImage : register(t0);
SamplerState samp0 : register(s0);
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(PSIn i) : SV_TARGET { return inputImage.Sample(samp0, i.uv); }
)";
    createShaderStages(dev, vsHlsl, std::string("main"), psHlsl, std::string("main"), stages);
  } else {
    ASSERT_TRUE(0);
  }
}

} // namespace igl::tests::util
