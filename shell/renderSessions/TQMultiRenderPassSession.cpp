/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <IGLU/simdtypes/SimdTypes.h>
#include <cmath>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/GLIncludes.h>
#include <shell/renderSessions/TQMultiRenderPassSession.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {
struct VertexPosUv {
  iglu::simdtypes::float3 position; // SIMD 128b aligned
  iglu::simdtypes::float2 uv; // SIMD 128b aligned
};
static VertexPosUv vertexData0[] = {
    {{-1.0f, 1.0f, 0.0}, {0.0, 1.0}},
    {{1.0f, 1.0f, 0.0}, {1.0, 1.0}},
    {{-1.0f, -1.0, 0.0}, {0.0, 0.0}},
    {{1.0f, -1.0f, 0.0}, {1.0, 0.0}},
};
static VertexPosUv vertexData1[] = {
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
                  uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(0)]]) {
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
  return R"(#version 100
                attribute vec3 position;
                attribute vec2 uv_in;

                varying vec2 uv;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in; // position.xy * 0.5 + 0.5;
                })";
}

static std::string getOpenGLFragmentShaderSource() {
  return R"(#version 100
                precision highp float;

                uniform vec3 color;
                uniform sampler2D inputImage;

                varying vec2 uv;

                void main() {
                  gl_FragColor =
                      vec4(color, 1.0) * texture2D(inputImage, uv);
                })";
}

static std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan:
    IGL_DEBUG_ABORT("IGLSamples not set up for Vulkan");
    return nullptr;
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

static void render(std::shared_ptr<ICommandBuffer>& buffer,
                   std::shared_ptr<IBuffer>& vertexBuffer,
                   std::shared_ptr<ITexture>& inputTexture,
                   std::shared_ptr<IRenderPipelineState>& pipelineState,
                   std::shared_ptr<IFramebuffer>& framebuffer,
                   RenderPassDesc& renderPass,
                   std::shared_ptr<ISamplerState>& samplerState,
                   std::shared_ptr<IBuffer>& ib,
                   size_t textureUnit_,
                   igl::BackendType& backend,
                   std::shared_ptr<IBuffer>& fragmentParamBuffer,
                   std::vector<igl::UniformDesc>& fragmentUniformDescriptors,
                   FragmentFormat fragmentParameters) {
  // Submit commands
  const std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass, framebuffer);

  commands->bindRenderPipelineState(pipelineState);

  if (backend != igl::BackendType::OpenGL) {
    commands->bindBuffer(0, fragmentParamBuffer.get());
  } else {
    // Bind non block uniforms
    for (const auto& uniformDesc : fragmentUniformDescriptors) {
      commands->bindUniform(uniformDesc, &fragmentParameters);
    }
  }
  commands->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture.get());
  commands->bindSamplerState(textureUnit_, BindTarget::kFragment, samplerState.get());
  commands->bindVertexBuffer(0, *vertexBuffer);
  commands->bindIndexBuffer(*ib, IndexFormat::UInt16);
  commands->drawIndexed(6);
  commands->endEncoding();
}

void TQMultiRenderPassSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  // Vertex buffer, Index buffer and Vertex Input
  const BufferDesc vb0Desc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData0, sizeof(vertexData0));
  vb0_ = device.createBuffer(vb0Desc, nullptr);
  const BufferDesc vb1Desc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData1, sizeof(vertexData1));
  vb1_ = device.createBuffer(vb1Desc, nullptr);
  const BufferDesc ibDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  ib0_ = device.createBuffer(ibDesc, nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0] = VertexAttribute(
      0, VertexAttributeFormat::Float3, offsetof(VertexPosUv, position), "position", 0);
  inputDesc.attributes[1] =
      VertexAttribute(0, VertexAttributeFormat::Float2, offsetof(VertexPosUv, uv), "uv_in", 1);
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(VertexPosUv);
  vertexInputState_ = device.createVertexInputState(inputDesc, nullptr);

  // Sampler & Texture
  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  samplerDesc.debugName = "Sampler: linear";
  samplerState_ = device.createSamplerState(samplerDesc, nullptr);
  tex0_ = getPlatform().loadTexture("igl.png");

  shaderStages_ = getShaderStagesForBackend(device);

  // Command queue
  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = device.createCommandQueue(desc, nullptr);

  renderPass0_.colorAttachments.resize(1);
  renderPass0_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass0_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass0_.colorAttachments[0].clearColor = getPreferredClearColor();
  renderPass0_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass0_.depthAttachment.clearDepth = 1.0;

  renderPass1_.colorAttachments.resize(1);
  renderPass1_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass1_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass1_.colorAttachments[0].clearColor = getPreferredClearColor();
  renderPass1_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass1_.depthAttachment.clearDepth = 1.0;

  // init uniforms
  fragmentParameters_ = FragmentFormat{{1.0f, 1.0f, 1.0f}};

  BufferDesc fpDesc;
  fpDesc.type = BufferDesc::BufferTypeBits::Uniform;
  fpDesc.data = &fragmentParameters_;
  fpDesc.length = sizeof(fragmentParameters_);
  fpDesc.storage = ResourceStorage::Shared;

  fragmentParamBuffer_ = device.createBuffer(fpDesc, nullptr);
}

void TQMultiRenderPassSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  igl::Result ret;
  if (framebuffer0_ == nullptr) {
    const auto dimensions = surfaceTextures.color->getDimensions();
    const igl::TextureDesc desc1 =
        igl::TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                dimensions.width,
                                dimensions.height,
                                igl::TextureDesc::TextureUsageBits::Sampled |
                                    igl::TextureDesc::TextureUsageBits::Attachment);
    tex1_ = getPlatform().getDevice().createTexture(desc1, nullptr);

    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = tex1_;

    igl::TextureDesc desc = igl::TextureDesc::new2D(igl::TextureFormat::Z_UNorm24,
                                                    dimensions.width,
                                                    dimensions.height,
                                                    igl::TextureDesc::TextureUsageBits::Attachment);
    desc.storage = igl::ResourceStorage::Private;
    framebufferDesc.depthAttachment.texture = getPlatform().getDevice().createTexture(desc, &ret);

    framebuffer0_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer0_ != nullptr);
  }

  if (framebuffer1_ == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;

    framebuffer1_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer1_ != nullptr);
  }
  const size_t _textureUnit = 0;

  // Graphics pipeline
  if (pipelineState0_ == nullptr) {
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInputState_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat = tex1_->getProperties().format;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        framebuffer0_->getDepthAttachment()->getProperties().format;
    graphicsDesc.fragmentUnitSamplerMap[_textureUnit] = IGL_NAMEHANDLE("inputImage");
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;

    pipelineState0_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);

    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer1_->getColorAttachment(0)->getProperties().format;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        framebuffer1_->getDepthAttachment()->getProperties().format;

    pipelineState1_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);

    // Set up uniformdescriptors
    fragmentUniformDescriptors_.emplace_back();
  }

  // Command buffer
  const CommandBufferDesc cbDesc;
  auto buffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);

  // Draw render pass 0
  auto drawableSurface = framebuffer1_->getColorAttachment(0);
  framebuffer1_->updateDrawable(drawableSurface);

  if (!fragmentUniformDescriptors_.empty()) {
    fragmentUniformDescriptors_.back().location =
        pipelineState0_->getIndexByName("color", igl::ShaderStage::Fragment);
    fragmentUniformDescriptors_.back().type = UniformType::Float3;
    fragmentUniformDescriptors_.back().offset = offsetof(FragmentFormat, color);
  }

  igl::BackendType backendType = getPlatform().getDevice().getBackendType();

  // Draw render pass 0
  render(buffer,
         vb0_,
         tex0_,
         pipelineState0_,
         framebuffer0_,
         renderPass0_,
         samplerState_,
         ib0_,
         _textureUnit,
         backendType,
         fragmentParamBuffer_,
         fragmentUniformDescriptors_,
         fragmentParameters_);

  // Draw render pass 1
  render(buffer,
         vb1_,
         tex1_,
         pipelineState1_,
         framebuffer1_,
         renderPass1_,
         samplerState_,
         ib0_,
         _textureUnit,
         backendType,
         fragmentParamBuffer_,
         fragmentUniformDescriptors_,
         fragmentParameters_);

  if (shellParams().shouldPresent) {
    buffer->present(drawableSurface);
  }

  commandQueue_->submit(*buffer);
}

} // namespace igl::shell
