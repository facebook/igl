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
#include <igl/opengl/Device.h>

namespace igl {
namespace tests {
namespace util {

//
// Creates an IGL device and a command queue
//
void createDeviceAndQueue(std::shared_ptr<IDevice>& dev, std::shared_ptr<ICommandQueue>& cq) {
  Result ret;

  // Create Device
  dev = util::createTestDevice();
  ASSERT_TRUE(dev != nullptr);

  // Create Command Queue
  CommandQueueDesc cqDesc = {CommandQueueType::Graphics};
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
    auto context = &static_cast<opengl::Device&>(*dev).getContext();
    bool isGles3 = (opengl::DeviceFeatureSet::usesOpenGLES() &&
                    context->deviceFeatures().getGLVersion() >= igl::opengl::GLVersion::v3_0_ES);
#else
    bool isGles3 = false;
#endif // IGL_BACKEND_OPENGL
    auto vertexShader = isGles3 ? igl::tests::data::shader::OGL_SIMPLE_VERT_SHADER_ES3
                                : igl::tests::data::shader::OGL_SIMPLE_VERT_SHADER;
    auto fragmentShader = isGles3 ? igl::tests::data::shader::OGL_SIMPLE_FRAG_SHADER_ES3
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

void validateTextureRange(IDevice& device,
                          ICommandQueue& cmdQueue,
                          const std::shared_ptr<ITexture>& texture,
                          bool isRenderTarget,
                          const TextureRangeDesc& range,
                          const uint32_t* expectedData,
                          const char* message) {
  Result ret;
  // Dummy command buffer to wait for completion.
  auto cmdBuf = cmdQueue.createCommandBuffer({}, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf != nullptr);
  cmdQueue.submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  ASSERT_EQ(range.numLayers, 1);
  ASSERT_EQ(range.numMipLevels, 1);
  ASSERT_EQ(range.depth, 1);

  const auto expectedDataSize = range.width * range.height;
  std::vector<uint32_t> actualData;
  actualData.resize(expectedDataSize);

  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = texture;
  auto fb = device.createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(fb != nullptr);

  fb->copyBytesColorAttachment(cmdQueue, 0, actualData.data(), range);

  if (!isRenderTarget && (device.getBackendType() == igl::BackendType::Metal ||
                          device.getBackendType() == igl::BackendType::Vulkan)) {
    // The Vulkan and Metal implementations of copyBytesColorAttachment flip the returned image
    // vertically. This is the desired behavior for render targets, but for non-render target
    // textures, we want the unflipped data. This flips the output image again to get the unmodified
    // data.
    std::vector<uint32_t> tmpData;
    tmpData.resize(actualData.size());
    for (size_t h = 0; h < range.height; ++h) {
      size_t src = (range.height - 1 - h) * range.width;
      size_t dst = h * range.width;
      for (size_t w = 0; w < range.width; ++w) {
        tmpData[dst++] = actualData[src++];
      }
    }
    actualData = std::move(tmpData);
  }

  for (size_t i = 0; i < expectedDataSize; i++) {
    ASSERT_EQ(expectedData[i], actualData[i])
        << message << ": Mismatch at index " << i << ": Expected: " << std::hex << expectedData[i]
        << " Actual: " << std::hex << actualData[i];
  }
}

void validateFramebufferTextureRange(IDevice& device,
                                     ICommandQueue& cmdQueue,
                                     const IFramebuffer& framebuffer,
                                     const TextureRangeDesc& range,
                                     const uint32_t* expectedData,
                                     const char* message) {
  validateTextureRange(
      device, cmdQueue, framebuffer.getColorAttachment(0), true, range, expectedData, message);
}

void validateFramebufferTexture(IDevice& device,
                                ICommandQueue& cmdQueue,
                                const IFramebuffer& framebuffer,
                                const uint32_t* expectedData,
                                const char* message) {
  validateFramebufferTextureRange(device,
                                  cmdQueue,
                                  framebuffer,
                                  framebuffer.getColorAttachment(0)->getFullRange(),
                                  expectedData,
                                  message);
}

void validateUploadedTextureRange(IDevice& device,
                                  ICommandQueue& cmdQueue,
                                  const std::shared_ptr<ITexture>& texture,
                                  const TextureRangeDesc& range,
                                  const uint32_t* expectedData,
                                  const char* message) {
  validateTextureRange(device, cmdQueue, texture, false, range, expectedData, message);
}

void validateUploadedTexture(IDevice& device,
                             ICommandQueue& cmdQueue,
                             const std::shared_ptr<ITexture>& texture,
                             const uint32_t* expectedData,
                             const char* message) {
  validateTextureRange(
      device, cmdQueue, texture, false, texture->getFullRange(), expectedData, message);
}
} // namespace util
} // namespace tests
} // namespace igl
