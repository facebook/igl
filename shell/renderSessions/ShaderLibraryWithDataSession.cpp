/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>
#if defined(IGL_PLATFORM_UWP)
#include "ShaderLibraryWithDataSession.h"
#define M_PI 3.14159265358979323846
#else
#include <shell/renderSessions/ShaderLibraryWithDataSession.h>
#include <shell/shared/fileLoader/FileLoader.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#endif
#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>

namespace igl {
namespace shell {

struct VertexPosUvw {
  glm::vec3 position;
  glm::vec3 uvw;
};

const float half = 1.0f;
static VertexPosUvw vertexData0[] = {
    {{-half, half, -half}, {0.0, 1.0, 0.0}},
    {{half, half, -half}, {1.0, 1.0, 0.0}},
    {{-half, -half, -half}, {0.0, 0.0, 0.0}},
    {{half, -half, -half}, {1.0, 0.0, 0.0}},
    {{half, half, half}, {1.0, 1.0, 1.0}},
    {{-half, half, half}, {0.0, 1.0, 1.0}},
    {{half, -half, half}, {1.0, 0.0, 1.0}},
    {{-half, -half, half}, {0.0, 0.0, 1.0}},
};
static uint16_t indexData[] = {0, 1, 2, 1, 3, 2, 1, 4, 3, 4, 6, 3, 4, 5, 6, 5, 7, 6,
                               5, 0, 7, 0, 2, 7, 5, 4, 0, 4, 1, 0, 2, 3, 7, 3, 6, 7};

static bool isDeviceCompatible(IDevice& device) noexcept {
  const auto backendtype = device.getBackendType();
  if (backendtype != BackendType::Metal) {
    IGL_LOG_INFO_ONCE("Creating Shader Library from data is supported only on Metal");
    return false;
  }

  return true;
}

void ShaderLibraryWithDataSession::createSamplerAndTextures(const igl::IDevice& device) {
  // Sampler & Texture
  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  samplerDesc.addressModeU = SamplerAddressMode::MirrorRepeat;
  samplerDesc.addressModeV = SamplerAddressMode::MirrorRepeat;
  samplerDesc.addressModeW = SamplerAddressMode::MirrorRepeat;
  samp0_ = device.createSamplerState(samplerDesc, nullptr);

  uint32_t width = 256;
  uint32_t height = 256;
  uint32_t depth = 256;
  uint32_t bytesPerPixel = 4;
  auto textureData = std::vector<uint8_t>((size_t)width * height * depth * bytesPerPixel);
  for (uint32_t k = 0; k < depth; ++k) {
    for (uint32_t j = 0; j < height; ++j) {
      for (uint32_t i = 0; i < width; ++i) {
        const uint32_t index = (i + width * j + width * height * k) * bytesPerPixel;
        const float d = sqrtf((i - 128.0f) * (i - 128.0f) + (j - 128.0f) * (j - 128.0f) +
                              (k - 128.0f) * (k - 128.0f)) /
                        16.0f;
        if (d > 7.0f) {
          textureData[index + 0] = 148;
          textureData[index + 1] = 0;
          textureData[index + 2] = 211;
          textureData[index + 3] = 255;
        } else if (d > 6.0f) {
          textureData[index + 0] = 75;
          textureData[index + 1] = 0;
          textureData[index + 2] = 130;
          textureData[index + 3] = 255;
        } else if (d > 5.0f) {
          textureData[index + 0] = 0;
          textureData[index + 1] = 0;
          textureData[index + 2] = 255;
          textureData[index + 3] = 255;
        } else if (d > 4.0f) {
          textureData[index + 0] = 0;
          textureData[index + 1] = 255;
          textureData[index + 2] = 0;
          textureData[index + 3] = 255;
        } else if (d > 3.0f) {
          textureData[index + 0] = 255;
          textureData[index + 1] = 255;
          textureData[index + 2] = 0;
          textureData[index + 3] = 255;
        } else if (d > 2.0f) {
          textureData[index + 0] = 255;
          textureData[index + 1] = 127;
          textureData[index + 2] = 0;
          textureData[index + 3] = 255;
        } else {
          textureData[index + 0] = 255;
          textureData[index + 1] = 0;
          textureData[index + 2] = 0;
          textureData[index + 3] = 255;
        }
      }
    }
  }

  igl::TextureDesc texDesc = igl::TextureDesc::new3D(igl::TextureFormat::RGBA_UNorm8,
                                                     width,
                                                     height,
                                                     depth,
                                                     igl::TextureDesc::TextureUsageBits::Sampled);
  tex0_ = getPlatform().getDevice().createTexture(texDesc, nullptr);
  const auto range = igl::TextureRangeDesc::new2D(0, 0, width, height, depth);
  tex0_->upload(range, textureData.data());
}

void ShaderLibraryWithDataSession::createShaders(igl::IDevice& device) {
  if (device.getBackendType() != igl::BackendType::Metal) {
    IGL_LOG_ERROR("ShaderLibraryWithData is supported only on Metal");
    return;
  }

#if defined(IGL_PLATFORM_IOS) && IGL_PLATFORM_IOS
  std::string metalLibFile = "ShaderLibraryTest-ios.metallib";
#else
  std::string metalLibFile = "ShaderLibraryTest-macos.metallib";
#endif
  auto data = getPlatform().getFileLoader().loadBinaryData(metalLibFile);

  Result result;
  shaderStages_ = ShaderStagesCreator::fromLibraryBinaryInput(
      device, data.data(), data.size(), "vertexShader", "fragmentShader", "", &result);
  if (!result.isOk()) {
    return;
  }
}

void ShaderLibraryWithDataSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }
  // Vertex buffer, Index buffer and Vertex Input
  BufferDesc vb0Desc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData0, sizeof(vertexData0));
  vb0_ = device.createBuffer(vb0Desc, nullptr);
  BufferDesc ibDesc = BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  ib0_ = device.createBuffer(ibDesc, nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[0].offset = offsetof(VertexPosUvw, position);
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].name = "position";
  inputDesc.attributes[0].location = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[1].offset = offsetof(VertexPosUvw, uvw);
  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].name = "uvw_in";
  inputDesc.attributes[1].location = 1;
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(VertexPosUvw);
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);

  createSamplerAndTextures(device);
  createShaders(device);

  // Command queue: backed by different types of GPU HW queues
  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = device.createCommandQueue(desc, nullptr);

  // Set up vertex uniform data
  vertexParameters_.scaleZ = 1.0f;

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPlatform().getDevice().backendDebugColor();
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
}

void ShaderLibraryWithDataSession::setVertexParams(float aspectRatio) {
  // perspective projection
  float fov = 45.0f * (M_PI / 180.0f);
  glm::mat4 projectionMat = glm::perspectiveLH(fov, aspectRatio, 0.1f, 100.0f);
  // rotating animation
  static float angle = 0.0f, scaleZ = 1.0f, ss = 0.005f;
  angle += 0.005f;
  scaleZ += ss;
  scaleZ = scaleZ < 0.0f ? 0.0f : scaleZ > 1.0 ? 1.0f : scaleZ;
  if (scaleZ <= 0.05f || scaleZ >= 1.0f) {
    ss *= -1.0f;
  }
  glm::mat4 xform = projectionMat * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.f, 8.0f)) *
                    glm::rotate(glm::mat4(1.0f), -0.2f, glm::vec3(1.0f, 0.0f, 0.0f)) *
                    glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, scaleZ));

  vertexParameters_.mvpMatrix = xform;
  vertexParameters_.scaleZ = scaleZ;
}

void ShaderLibraryWithDataSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  // cube animation
  setVertexParams(surfaceTextures.color->getAspectRatio());

  igl::Result ret;
  if (framebuffer_ == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;

    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_ASSERT(ret.isOk());
    IGL_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  size_t textureUnit = 0;
  if (pipelineState_ == nullptr) {
    // Graphics pipeline: state batch that fully configures GPU for rendering

    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput0_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    graphicsDesc.fragmentUnitSamplerMap[textureUnit] = IGL_NAMEHANDLE("inputVolume");
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;
    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
  }

  // Command buffers (1-N per thread): create, submit and forget
  CommandBufferDesc cbDesc;
  auto buffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);

  std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  commands->bindBuffer(0, BindTarget::kVertex, vb0_, 0);

  // Bind Vertex Uniform Data
  iglu::ManagedUniformBufferInfo info;
  info.index = 1;
  info.length = sizeof(VertexFormat);
  info.uniforms = std::vector<igl::UniformDesc>{
      igl::UniformDesc{
          "mvpMatrix", -1, igl::UniformType::Mat4x4, 1, offsetof(VertexFormat, mvpMatrix), 0},
      igl::UniformDesc{
          "scaleZ", -1, igl::UniformType::Float, 1, offsetof(VertexFormat, scaleZ), 0}};

  std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer =
      std::make_shared<iglu::ManagedUniformBuffer>(device, info);
  IGL_ASSERT(vertUniformBuffer->result.isOk());
  *static_cast<VertexFormat*>(vertUniformBuffer->getData()) = vertexParameters_;
  vertUniformBuffer->bind(device, *pipelineState_, *commands);

  commands->bindTexture(textureUnit, BindTarget::kFragment, tex0_.get());
  commands->bindSamplerState(textureUnit, BindTarget::kFragment, samp0_);

  commands->bindRenderPipelineState(pipelineState_);

  commands->drawIndexed(PrimitiveType::Triangle, 3 * 6 * 2, IndexFormat::UInt16, *ib0_, 0);

  commands->endEncoding();

  buffer->present(framebuffer_->getColorAttachment(0));

  commandQueue_->submit(*buffer); // Guarantees ordering between command buffers
}

} // namespace shell
} // namespace igl
