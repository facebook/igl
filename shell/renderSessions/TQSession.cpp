/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <IGLU/simdtypes/SimdTypes.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/GLIncludes.h>
#include <shell/renderSessions/TQSession.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

struct VertexPosUv {
  iglu::simdtypes::float3 position;
  iglu::simdtypes::float2 uv;
};
static VertexPosUv vertexData[] = {
    {{-0.8f, 0.8f, 0.0}, {0.0, 0.0}},
    {{0.8f, 0.8f, 0.0}, {1.0, 0.0}},
    {{-0.8f, -0.8f, 0.0}, {0.0, 1.0}},
    {{0.8f, -0.8f, 0.0}, {1.0, 1.0}},
};
static uint16_t indexData[] = {
    0,
    1,
    2,
    1,
    3,
    2,
};

static std::string getVersion() {
  return {"#version 100"};
}

static std::string getMetalShaderSource() {
  return R"(
              using namespace metal;

              typedef struct { float3 color; } UniformBlock;

              typedef struct {
                float3 position [[attribute(0)]];
                float2 uv [[attribute(1)]];
              } VertexIn;

              typedef struct {
                float4 position [[position]];
                float2 uv;
              } VertexOut;

              vertex VertexOut vertexShader(
                  uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(1)]]) {
                VertexOut out;
                out.position = float4(vertices[vid].position, 1.0);
                out.uv = vertices[vid].uv;
                return out;
              }

              fragment float4 fragmentShader(
                  VertexOut IN [[stage_in]],
                  texture2d<float> diffuseTex [[texture(0)]],
                  sampler linearSampler [[sampler(0)]],
                  constant UniformBlock * color [[buffer(0)]]) {
                float4 tex = diffuseTex.sample(linearSampler, IN.uv);
                return float4(color->color.r, color->color.g, color->color.b, 1.0) *
                      tex;
              }
    )";
}

static std::string getOpenGLVertexShaderSource() {
  return getVersion() + R"(
                precision highp float;
                attribute vec3 position;
                attribute vec2 uv_in;

                varying vec2 uv;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in; // position.xy * 0.5 + 0.5;
                })";
}

static std::string getOpenGLFragmentShaderSource() {
  return getVersion() + std::string(R"(
                precision highp float;
                uniform vec3 color;
                uniform sampler2D inputImage;

                varying vec2 uv;

                void main() {
                  gl_FragColor =
                      vec4(color, 1.0) * texture2D(inputImage, uv);
                })");
}

static std::string getVulkanVertexShaderSource() {
  return R"(
                layout(location = 0) in vec3 position;
                layout(location = 1) in vec2 uv_in;
                layout(location = 0) out vec2 uv;
                layout(location = 1) out vec3 color;

                struct UniformsPerObject {
                  vec3 color;
                };

                layout (set = 1, binding = 0, std140) uniform PerObject {
                  UniformsPerObject perObject;
                } object;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in;
                  color = object.perObject.color;
                }
                )";
}

static std::string getVulkanFragmentShaderSource() {
  return R"(
                layout(location = 0) in vec2 uv;
                layout(location = 1) in vec3 color;
                layout(location = 0) out vec4 out_FragColor;

                layout(set = 0, binding = 0) uniform sampler2D in_texture;

                void main() {
                  out_FragColor = vec4(color, 1.0) * texture(in_texture, uv);
                }
                )";
}
// @fb-only

static std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getVulkanVertexShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           getVulkanFragmentShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           nullptr);
  // @fb-only
    // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
  case igl::BackendType::Metal:
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, getMetalShaderSource().c_str(), "vertexShader", "fragmentShader", "", nullptr);
  case igl::BackendType::OpenGL:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getOpenGLVertexShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           getOpenGLFragmentShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           nullptr);
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

void TQSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  // Vertex & Index buffer
  const BufferDesc vbDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData));
  _vb0 = device.createBuffer(vbDesc, nullptr);
  IGL_ASSERT(_vb0 != nullptr);
  const BufferDesc ibDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  _ib0 = device.createBuffer(ibDesc, nullptr);
  IGL_ASSERT(_ib0 != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0] = VertexAttribute(
      1, VertexAttributeFormat::Float3, offsetof(VertexPosUv, position), "position", 0);
  inputDesc.attributes[1] =
      VertexAttribute(1, VertexAttributeFormat::Float2, offsetof(VertexPosUv, uv), "uv_in", 1);
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[1].stride = sizeof(VertexPosUv);
  _vertexInput0 = device.createVertexInputState(inputDesc, nullptr);
  IGL_ASSERT(_vertexInput0 != nullptr);

  // Sampler & Texture
  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  samplerDesc.debugName = "Sampler: linear";
  _samp0 = device.createSamplerState(samplerDesc, nullptr);
  IGL_ASSERT(_samp0 != nullptr);
  _tex0 = getPlatform().loadTexture("igl.png");

  _shaderStages = getShaderStagesForBackend(device);
  IGL_ASSERT(_shaderStages != nullptr);

  // Command queue
  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  _commandQueue = device.createCommandQueue(desc, nullptr);
  IGL_ASSERT(_commandQueue != nullptr);

  _renderPass.colorAttachments.resize(1);
  _renderPass.colorAttachments[0].loadAction = LoadAction::Clear;
  _renderPass.colorAttachments[0].storeAction = StoreAction::Store;
  _renderPass.colorAttachments[0].clearColor = getPlatform().getDevice().backendDebugColor();
  _renderPass.depthAttachment.loadAction = LoadAction::Clear;
  _renderPass.depthAttachment.clearDepth = 1.0;

  // init uniforms
  _fragmentParameters = FragmentFormat{{1.0f, 1.0f, 1.0f}};

  BufferDesc fpDesc;
  fpDesc.type = BufferDesc::BufferTypeBits::Uniform;
  fpDesc.data = &_fragmentParameters;
  fpDesc.length = sizeof(_fragmentParameters);
  fpDesc.storage = ResourceStorage::Shared;

  _fragmentParamBuffer = device.createBuffer(fpDesc, nullptr);
  IGL_ASSERT(_fragmentParamBuffer != nullptr);
}

void TQSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  igl::Result ret;
  if (_framebuffer == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
    IGL_ASSERT(ret.isOk());
    _framebuffer = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_ASSERT(ret.isOk());
    IGL_ASSERT(_framebuffer != nullptr);
  } else {
    _framebuffer->updateDrawable(surfaceTextures.color);
  }

  const size_t _textureUnit = 0;

  // Graphics pipeline
  if (_pipelineState == nullptr) {
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = _vertexInput0;
    graphicsDesc.shaderStages = _shaderStages;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        _framebuffer->getColorAttachment(0)->getProperties().format;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        _framebuffer->getDepthAttachment()->getProperties().format;
    graphicsDesc.fragmentUnitSamplerMap[_textureUnit] = IGL_NAMEHANDLE("inputImage");
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;

    _pipelineState = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
    IGL_ASSERT(_pipelineState != nullptr);

    // Set up uniformdescriptors
    _fragmentUniformDescriptors.emplace_back();
  }

  // Command Buffers
  const CommandBufferDesc cbDesc;
  auto buffer = _commandQueue->createCommandBuffer(cbDesc, nullptr);
  IGL_ASSERT(buffer != nullptr);
  auto drawableSurface = _framebuffer->getColorAttachment(0);

  _framebuffer->updateDrawable(drawableSurface);

  // Uniform: "color"
  if (!_fragmentUniformDescriptors.empty()) {
    // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
    // @fb-only
      if (getPlatform().getDevice().hasFeature(DeviceFeatures::BindUniform)) {
        _fragmentUniformDescriptors.back().location =
            _pipelineState->getIndexByName("color", igl::ShaderStage::Fragment);
      }
    _fragmentUniformDescriptors.back().type = UniformType::Float3;
    _fragmentUniformDescriptors.back().offset = offsetof(FragmentFormat, color);
  }

  // Submit commands
  const std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(_renderPass, _framebuffer);
  IGL_ASSERT(commands != nullptr);
  if (commands) {
    commands->bindVertexBuffer(1, *_vb0);
    commands->bindRenderPipelineState(_pipelineState);
    if (getPlatform().getDevice().hasFeature(DeviceFeatures::BindUniform)) {
      // Bind non block uniforms
      for (const auto& uniformDesc : _fragmentUniformDescriptors) {
        commands->bindUniform(uniformDesc, &_fragmentParameters);
      }
    } else if (getPlatform().getDevice().hasFeature(DeviceFeatures::UniformBlocks)) {
      commands->bindBuffer(0, _fragmentParamBuffer, 0);
    } else {
      IGL_ASSERT_NOT_REACHED();
    }

    commands->bindTexture(_textureUnit, BindTarget::kFragment, _tex0.get());
    commands->bindSamplerState(_textureUnit, BindTarget::kFragment, _samp0.get());
    commands->bindIndexBuffer(*_ib0, IndexFormat::UInt16);
    commands->drawIndexed(6);

    commands->endEncoding();
  }

  IGL_ASSERT(buffer != nullptr);
  if (shellParams().shouldPresent) {
    buffer->present(drawableSurface);
  }

  IGL_ASSERT(_commandQueue != nullptr);
  _commandQueue->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

} // namespace igl::shell
