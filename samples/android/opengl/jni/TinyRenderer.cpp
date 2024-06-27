/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/samples/android/opengl/TinyRenderer.h>

#include <EGL/egl.h>
#include <android/log.h>
#include <igl/IGL.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/egl/HWDevice.h>
#include <igl/opengl/egl/PlatformDevice.h>
#include <memory>
#include <sstream>
#include <stdexcept>

#define IGL_SAMPLE_LOG_INFO(...) \
  __android_log_print(ANDROID_LOG_INFO, "libsampleOpenGLJni", __VA_ARGS__)
#define IGL_SAMPLE_LOG_ERROR(...) \
  __android_log_print(ANDROID_LOG_ERROR, "libsampleOpenGLJni", __VA_ARGS__)

namespace igl_samples::android {

using namespace igl;

namespace {

void throwOnBadResult(const Result& result) {
  if (result.code != Result::Code::Ok) {
    std::stringstream errorMsg;
    errorMsg << "IGL error:\nCode: " << static_cast<int>(result.code)
             << "\nMessage: " << result.message;
    IGL_SAMPLE_LOG_ERROR("%s", errorMsg.str().c_str());
    throw std::runtime_error(errorMsg.str());
  }
}

const std::string kVertexShader = R"(
  precision highp float;

  attribute vec3 position;
  attribute vec2 uv_in;
  varying vec2 uv;

  void main() {
    gl_Position = vec4(position, 1.0);
    uv = uv_in;
  }
)";

const std::string kFragmentShader = R"(
  precision highp float;

  varying vec2 uv;

  void main() {
    gl_FragColor = vec4(uv, 0, 1);
  }
)";

} // namespace

void TinyRenderer::init() {
  Result result;
  { // Initialize the device
    const igl::HWDeviceQueryDesc queryDesc(HWDeviceType::IntegratedGpu);
    auto hwDevice = opengl::egl::HWDevice();
    auto hwDevices = hwDevice.queryDevices(queryDesc, &result);
    throwOnBadResult(result);
    device_ = hwDevice.create(hwDevices[0], igl::opengl::RenderingAPI::GLES2, nullptr, &result);
    throwOnBadResult(result);
  }

  { // Initialize the vertex buffers, index buffers, and shaders
    struct VertexPosUv {
      std::array<float, 3> position;
      std::array<float, 2> uv;
    };
    static VertexPosUv vertexData[] = {
        {{-0.8f, 0.8f, 0.0}, {0.0, 1.0}},
        {{0.8f, 0.8f, 0.0}, {1.0, 1.0}},
        {{-0.8f, -0.8f, 0.0}, {0.0, 0.0}},
        {{0.8f, -0.8f, 0.0}, {1.0, 0.0}},
    };
    static uint16_t indexData[] = {
        0,
        1,
        2,
        1,
        3,
        2,
    };

    const BufferDesc vertexBufferDesc =
        BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData));
    vertexBuffer_ = device_->createBuffer(vertexBufferDesc, &result);
    throwOnBadResult(result);

    const BufferDesc indexBufferDesc =
        BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
    indexBuffer_ = device_->createBuffer(indexBufferDesc, &result);
    throwOnBadResult(result);

    VertexInputStateDesc vertexInputDesc;
    vertexInputDesc.numAttributes = 2;
    vertexInputDesc.attributes[0] = VertexAttribute(
        0, VertexAttributeFormat::Float3, offsetof(VertexPosUv, position), "position");
    vertexInputDesc.attributes[1] =
        VertexAttribute(0, VertexAttributeFormat::Float2, offsetof(VertexPosUv, uv), "uv_in");
    vertexInputDesc.numInputBindings = 1;
    vertexInputDesc.inputBindings[0].stride = sizeof(VertexPosUv);
    vertexInputState_ = device_->createVertexInputState(vertexInputDesc, &result);
    throwOnBadResult(result);
  }

  {
    shaderStages_ = ShaderStagesCreator::fromModuleStringInput(
        *device_, kVertexShader.c_str(), "main", "", kFragmentShader.c_str(), "main", "", &result);
    throwOnBadResult(result);
  }

  { // Initialize command queue
    const CommandQueueDesc commandQueueDesc = {CommandQueueType::Graphics};
    commandQueue_ = device_->createCommandQueue(commandQueueDesc, &result);
    throwOnBadResult(result);
  }

  { // Set up our render pass descriptor
    renderPassDesc_.colorAttachments.resize(1);
    renderPassDesc_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPassDesc_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPassDesc_.colorAttachments[0].clearColor = {0.0, 0.0, 0.5, 1.0};
  }
}

void TinyRenderer::render() {
  Result result;

  // Create or update the framebuffer for the current frame
  auto viewTexture =
      device_->getPlatformDevice<opengl::egl::PlatformDevice>()->createTextureFromNativeDrawable(
          &result);
  throwOnBadResult(result);

  if (framebuffer_ == nullptr) {
    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = viewTexture;
    framebuffer_ = device_->createFramebuffer(framebufferDesc, &result);
    throwOnBadResult(result);
  } else {
    framebuffer_->updateDrawable(viewTexture);
  }

  // Create pipeline state object if needed
  if (pipelineState_ == nullptr) {
    RenderPipelineDesc pipelineDesc;
    pipelineDesc.vertexInputState = vertexInputState_;
    pipelineDesc.shaderStages = shaderStages_;
    pipelineDesc.targetDesc.colorAttachments.resize(1);
    pipelineDesc.targetDesc.colorAttachments[0].textureFormat = viewTexture->getProperties().format;
    pipelineDesc.targetDesc.colorAttachments[0].blendEnabled = true;
    pipelineDesc.targetDesc.colorAttachments[0].rgbBlendOp = BlendOp::Add;
    pipelineDesc.targetDesc.colorAttachments[0].alphaBlendOp = BlendOp::Add;
    pipelineDesc.targetDesc.colorAttachments[0].srcRGBBlendFactor = BlendFactor::SrcAlpha;
    pipelineDesc.targetDesc.colorAttachments[0].srcAlphaBlendFactor = BlendFactor::SrcAlpha;
    pipelineDesc.targetDesc.colorAttachments[0].dstRGBBlendFactor = BlendFactor::OneMinusSrcAlpha;
    pipelineDesc.targetDesc.colorAttachments[0].dstAlphaBlendFactor = BlendFactor::OneMinusSrcAlpha;

    pipelineDesc.cullMode = CullMode::Back;
    pipelineDesc.frontFaceWinding = WindingMode::Clockwise;

    pipelineState_ = device_->createRenderPipeline(pipelineDesc, &result);
    throwOnBadResult(result);
  }

  // Create and submit command buffers
  const CommandBufferDesc commandBufferDesc;
  const std::shared_ptr<ICommandBuffer> buffer =
      commandQueue_->createCommandBuffer(commandBufferDesc, &result);
  throwOnBadResult(result);

  auto cmds = buffer->createRenderCommandEncoder(renderPassDesc_, framebuffer_);

  cmds->bindVertexBuffer(0, *vertexBuffer_);
  cmds->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16);
  cmds->bindRenderPipelineState(pipelineState_);
  cmds->drawIndexed(6);

  cmds->endEncoding();
  buffer->present(viewTexture);

  commandQueue_->submit(*buffer);
}

void TinyRenderer::onSurfacesChanged() {
  auto* readSurface = eglGetCurrentSurface(EGL_READ);
  auto* drawSurface = eglGetCurrentSurface(EGL_DRAW);

  Result result;
  device_->getPlatformDevice<opengl::egl::PlatformDevice>()->updateSurfaces(
      readSurface, drawSurface, &result);
  throwOnBadResult(result);
}

} // namespace igl_samples::android
