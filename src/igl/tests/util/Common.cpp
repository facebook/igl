/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Common.h"

#include "../data/ShaderData.h"
#include "TestDevice.h"
#include <gtest/gtest.h>
#include <igl/ShaderCreator.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/Device.h>
#endif // IGL_BACKEND_OPENGL

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
  const CommandQueueDesc cqDesc = {CommandQueueType::Graphics};
  cq = dev->createCommandQueue(cqDesc, &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cq != nullptr); // Shouldn't trigger if above is okay
}

void createShaderStages(const std::shared_ptr<IDevice>& dev,
                        const char* vertexSource,
                        const char* vertexEntryPoint,
                        const char* fragmentSource,
                        const char* fragmentEntryPoint,
                        std::unique_ptr<IShaderStages>& stages) {
  Result ret;
  stages = ShaderStagesCreator::fromModuleStringInput(
      *dev, vertexSource, vertexEntryPoint, "", fragmentSource, fragmentEntryPoint, "", &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message.c_str();
  ASSERT_TRUE(stages != nullptr);
}

void createShaderStages(const std::shared_ptr<IDevice>& dev,
                        const char* librarySource,
                        const char* vertexEntryPoint,
                        const char* fragmentEntryPoint,
                        std::unique_ptr<IShaderStages>& stages) {
  Result ret;
  stages = ShaderStagesCreator::fromLibraryStringInput(
      *dev, librarySource, vertexEntryPoint, fragmentEntryPoint, "", &ret);

  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  ASSERT_TRUE(stages != nullptr);
}

void createSimpleShaderStages(const std::shared_ptr<IDevice>& dev,
                              std::unique_ptr<IShaderStages>& stages,
                              TextureFormat outputFormat) {
  const auto backend = dev->getBackendType();

  if (backend == igl::BackendType::OpenGL) {
#if IGL_BACKEND_OPENGL
    auto* context = &static_cast<opengl::Device&>(*dev).getContext();
    const bool isGles3 =
        (opengl::DeviceFeatureSet::usesOpenGLES() &&
         context->deviceFeatures().getGLVersion() >= igl::opengl::GLVersion::v3_0_ES);
#else
    bool isGles3 = false;
#endif // IGL_BACKEND_OPENGL
    const auto* vertexShader = isGles3 ? igl::tests::data::shader::OGL_SIMPLE_VERT_SHADER_ES3
                                       : igl::tests::data::shader::OGL_SIMPLE_VERT_SHADER;
    const auto* fragmentShader = isGles3 ? igl::tests::data::shader::OGL_SIMPLE_FRAG_SHADER_ES3
                                         : igl::tests::data::shader::OGL_SIMPLE_FRAG_SHADER;

    createShaderStages(dev,
                       vertexShader,
                       igl::tests::data::shader::shaderFunc,
                       fragmentShader,
                       igl::tests::data::shader::shaderFunc,
                       stages);
  } else if (backend == igl::BackendType::Metal) {
    const char* shader = igl::tests::data::shader::MTL_SIMPLE_SHADER;
    if (outputFormat == TextureFormat::RG_UInt16) {
      shader = igl::tests::data::shader::MTL_SIMPLE_SHADER_USHORT2;
    } else if (outputFormat == TextureFormat::R_UInt16) {
      shader = igl::tests::data::shader::MTL_SIMPLE_SHADER_USHORT2;
    } else if (outputFormat == TextureFormat::RGB10_A2_Uint_Rev) {
      shader = igl::tests::data::shader::MTL_SIMPLE_SHADER_USHORT4;
    } else if (outputFormat == TextureFormat::RGBA_UInt32) {
      shader = igl::tests::data::shader::MTL_SIMPLE_SHADER_UINT4;
    } else if (outputFormat != TextureFormat::Invalid) {
      auto components = TextureFormatProperties::fromTextureFormat(outputFormat).componentsPerPixel;
      switch (components) {
      case 1:
        shader = igl::tests::data::shader::MTL_SIMPLE_SHADER_FLOAT;
        break;
      case 2:
        shader = igl::tests::data::shader::MTL_SIMPLE_SHADER_FLOAT2;
        break;
      case 3:
        shader = igl::tests::data::shader::MTL_SIMPLE_SHADER_FLOAT3;
        break;
      case 4:
        shader = igl::tests::data::shader::MTL_SIMPLE_SHADER_FLOAT4;
        break;
      default:
        ASSERT_TRUE(false);
        break;
      }
    }
    createShaderStages(dev,
                       shader,
                       igl::tests::data::shader::simpleVertFunc,
                       igl::tests::data::shader::simpleFragFunc,
                       stages);
  } else if (backend == igl::BackendType::Vulkan) {
    // Output format-specific shaders needed for MoltenVK
    const char* fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER;
    if (outputFormat == TextureFormat::RG_UInt16) {
      fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_UINT2;
    } else if (outputFormat == TextureFormat::R_UInt16) {
      fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_UINT2;
    } else if (outputFormat == TextureFormat::RGB10_A2_Uint_Rev) {
      fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_UINT4;
    } else if (outputFormat == TextureFormat::RGBA_UInt32) {
      fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_UINT4;
    } else if (outputFormat != TextureFormat::Invalid) {
      auto components = TextureFormatProperties::fromTextureFormat(outputFormat).componentsPerPixel;
      switch (components) {
      case 1:
        fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_FLOAT;
        break;
      case 2:
        fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_FLOAT2;
        break;
      case 3:
        fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_FLOAT3;
        break;
      case 4:
        fragShader = igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_FLOAT4;
        break;
      default:
        ASSERT_TRUE(false);
        break;
      }
    }
    createShaderStages(dev,
                       igl::tests::data::shader::VULKAN_SIMPLE_VERT_SHADER,
                       igl::tests::data::shader::shaderFunc,
                       fragShader,
                       igl::tests::data::shader::shaderFunc,
                       stages);
  } else {
    ASSERT_TRUE(0);
  }
}

} // namespace igl::tests::util
