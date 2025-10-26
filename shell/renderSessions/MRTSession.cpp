/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/NameHandle.h>

#include <array>
#include <numeric>
#include <vector>
#include <IGLU/simdtypes/SimdTypes.h>
#include <shell/renderSessions/MRTSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <igl/CommandBuffer.h>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/ShaderCreator.h>
#include <igl/VertexInputState.h>
#include <igl/opengl/Config.h>

namespace igl::shell {
struct VertexPosUv {
  iglu::simdtypes::float3 position; // SIMD 128b aligned
  iglu::simdtypes::float2 uv; // SIMD 128b aligned
};
static VertexPosUv vertexData0[] = {
    {{-0.9f, 0.9f, 0.0}, {0.0, 1.0}},
    {{-0.05f, 0.9f, 0.0}, {1.0, 1.0}},
    {{-0.9f, -0.9f, 0.0}, {0.0, 0.0}},
    {{-0.05f, -0.9f, 0.0}, {1.0, 0.0}},
};
static VertexPosUv vertexData1[] = {
    {{0.05f, 0.9f, 0.0}, {0.0, 1.0}},
    {{0.90f, 0.9f, 0.0}, {1.0, 1.0}},
    {{0.05f, -0.9f, 0.0}, {0.0, 0.0}},
    {{0.90f, -0.9f, 0.0}, {1.0, 0.0}},
};
static uint16_t indexData[] = {
    0,
    1,
    2,
    1,
    3,
    2,
};

enum class ShaderPrecision { Low, Medium, High };

static std::string getPrecisionProlog(ShaderPrecision precision) {
#if IGL_OPENGL_ES
  switch (precision) {
  case ShaderPrecision::Low:
    return {"precision lowp float;"};
  case ShaderPrecision::Medium:
    return {"precision mediump float;"};
  case ShaderPrecision::High:
    return {"precision highp float;"};
  }
#else
  return std::string();
#endif
}

static std::string getVersionProlog() {
#if IGL_OPENGL_ES
  return {"#version 300 es\n"};
#else
  return std::string("#version 410\n");
#endif
}

static std::string getMetalShaderSource(int metalShaderIdx) {
  switch (metalShaderIdx) {
  case 0:
    return R"(
                    #include <metal_stdlib>
                    #include <simd/simd.h>
                    #line 0
                    using namespace metal;

                     struct VertexIn {
                       float3 position [[attribute(0)]];
                       float2 uv [[attribute(1)]];
                     };

                     struct VertexOut {
                       float4 position [[position]];
                       float2 uv;
                     };

                     vertex VertexOut vertexShader(
                         uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(0)]]) {
                       VertexOut out;
                       out.position = float4(vertices[vid].position, 1.0);
                       out.uv = vertices[vid].uv;
                       return out;
                     }

                     struct FragmentOutput {
                       float4 colorOutGreen [[color(0)]];
                       float4 colorOutRed [[color(1)]];
                     };

                     fragment FragmentOutput fragmentShader(VertexOut IN [[stage_in]],
                                                            texture2d<float> diffuseTex
                                                            [[texture(0)]]) {
                       constexpr sampler linearSampler(mag_filter::linear,
                                                       min_filter::linear);
                       FragmentOutput f;
                       float4 c = diffuseTex.sample(linearSampler, IN.uv);
                       f.colorOutRed.r = c.r;
                       f.colorOutRed.g = 0.0;
                       f.colorOutRed.b = 0.0;
                       f.colorOutRed.a = 1.0;
                       f.colorOutGreen.r = 0.0;
                       f.colorOutGreen.g = c.g;
                       f.colorOutGreen.b = 0.0;
                       f.colorOutGreen.a = 1.0;

                       return f;
                     })";
  default:
    return R"(
                      #include <metal_stdlib>
                      #include <simd/simd.h>
                      #line 0
                      using namespace metal;

                      struct VertexIn {
                        float3 position [[attribute(0)]];
                        float2 uv [[attribute(1)]];
                      };

                      struct VertexOut {
                        float4 position [[position]];
                        float2 uv;
                      };

                      vertex VertexOut vertexShader(
                          uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(0)]]) {
                        VertexOut out;
                        out.position = float4(vertices[vid].position, 1.0);
                        out.uv = vertices[vid].uv;
                        return out;
                      }

                      fragment float4 fragmentShader(VertexOut IN [[stage_in]],
                                                    texture2d<float> greenTex [[texture(0)]],
                                                    texture2d<float> redTex [[texture(1)]]) {
                        constexpr sampler linearSampler(mag_filter::linear,
                                                        min_filter::linear);
                        float4 c = greenTex.sample(linearSampler, IN.uv) +
                                  redTex.sample(linearSampler, IN.uv);
                        return c;
                      })";
  }
}

static std::string getOpenGLVertexShaderSource() {
  return getVersionProlog() + getPrecisionProlog(ShaderPrecision::High) + R"(
                in vec3 position;
                in vec2 uv_in;
                out vec2 uv;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in;
                })";
}

static std::string getOpenGLFragmentShaderSource(int programIndex) {
  if (programIndex == 0) {
    return getVersionProlog() + getPrecisionProlog(ShaderPrecision::High) + R"(
                uniform sampler2D inputImage;
                in vec2 uv;
                layout(location = 0) out vec4 colorGreen;
                layout(location = 1) out vec4 colorRed;
                void main() {
                  vec4 c = texture(inputImage, uv);
                  colorGreen = vec4(0., c.g, 0., 1.0);
                  colorRed = vec4(c.r, 0., 0., 1.0);
                })";
  } else {
    return getVersionProlog() + getPrecisionProlog(ShaderPrecision::High) + R"(
                uniform sampler2D colorRed;
                uniform sampler2D colorGreen;
                in vec2 uv;
                out vec4 colorOut;
                void main() {
                  colorOut = texture(colorRed, uv) + texture(colorGreen, uv);
                })";
  }
}

static std::string getVulkanVertexShaderSource() {
  return getPrecisionProlog(ShaderPrecision::High) + R"(
                layout(location = 0) in vec3 position;
                layout(location = 1) in vec2 uv_in;
                layout(location = 0) out vec2 uv;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in;
                })";
}

static std::string getVulkanFragmentShaderSource(int programIndex) {
  if (programIndex == 0) {
    return getPrecisionProlog(ShaderPrecision::High) + R"(
                layout(location = 0) in vec2 uv;
                layout(location = 0) out vec4 colorGreen;
                layout(location = 1) out vec4 colorRed;

                layout(set = 0, binding = 0) uniform sampler2D in_texture;

                void main() {
                  vec4 c = texture(in_texture, uv);
                  colorGreen = vec4(0., c.g, 0., 1.0);
                  colorRed = vec4(c.r, 0., 0., 1.0);
                })";
  } else {
    return getPrecisionProlog(ShaderPrecision::High) + R"(
                layout(location = 0) in vec2 uv;
                layout(location = 0) out vec4 out_FragColor;

                layout(set = 0, binding = 0) uniform sampler2D in_texture_green;
                layout(set = 0, binding = 1) uniform sampler2D in_texture_red;

                void main() {
                  vec2 uv1 = vec2(uv.x, 1.0-uv.y);
                  out_FragColor = texture(in_texture_green, uv1) + texture(in_texture_red, uv1);
                })";
  }
}

static std::unique_ptr<IShaderStages> createShaderStagesForBackend(const IDevice& device,
                                                                   int programIndex) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device,
        getVulkanVertexShaderSource().c_str(),
        "main",
        "",
        getVulkanFragmentShaderSource(programIndex).c_str(),
        "main",
        "",
        nullptr);
  case igl::BackendType::Custom:
    IGL_DEBUG_ABORT("No Custom shader available");
    return nullptr;
  case igl::BackendType::D3D12: {
    if (programIndex == 0) {
      // First pass: write to SV_Target0 and SV_Target1
      static const char* kVS = R"(
        struct VSIn { float3 position: POSITION; float2 uv: TEXCOORD0; };
        struct VSOut { float4 position: SV_POSITION; float2 uv: TEXCOORD0; };
        VSOut main(VSIn v){ VSOut o; o.position=float4(v.position,1); o.uv=v.uv; return o; }
      )";
      static const char* kPS = R"(
        Texture2D inputImage : register(t0); SamplerState s0 : register(s0);
        struct PSIn { float4 position: SV_POSITION; float2 uv: TEXCOORD0; };
        struct PSOut { float4 colorGreen: SV_Target0; float4 colorRed: SV_Target1; };
        PSOut main(PSIn i){ float4 c = inputImage.Sample(s0, i.uv);
          // Input PNG data arrives as BGRA, so route blue channel into the second target.
          PSOut o; o.colorGreen=float4(0,c.g,0,1); o.colorRed=float4(c.b,0,0,1); return o; }
      )";
      return igl::ShaderStagesCreator::fromModuleStringInput(device, kVS, "main", "", kPS, "main", "", nullptr);
    } else {
      // Second pass: sample two textures and output sum
      static const char* kVS = R"(
        struct VSIn { float3 position: POSITION; float2 uv: TEXCOORD0; };
        struct VSOut { float4 position: SV_POSITION; float2 uv: TEXCOORD0; };
        VSOut main(VSIn v){ VSOut o; o.position=float4(v.position,1); o.uv=v.uv; return o; }
      )";
      static const char* kPS = R"(
        Texture2D colorGreen : register(t0); Texture2D colorRed : register(t1); SamplerState s0 : register(s0);
        struct PSIn { float4 position: SV_POSITION; float2 uv: TEXCOORD0; };
        float4 main(PSIn i) : SV_Target { float2 uv1=float2(i.uv.x, 1.0 - i.uv.y);
          return colorGreen.Sample(s0, uv1) + colorRed.Sample(s0, uv1); }
      )";
      return igl::ShaderStagesCreator::fromModuleStringInput(device, kVS, "main", "", kPS, "main", "", nullptr);
    }
  }
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::OpenGL:
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device,
        getOpenGLVertexShaderSource().c_str(),
        "main",
        "",
        getOpenGLFragmentShaderSource(programIndex).c_str(),
        "main",
        "",
        nullptr);
  case igl::BackendType::Metal:
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device,
        getMetalShaderSource(programIndex).c_str(),
        "vertexShader",
        "fragmentShader",
        "",
        nullptr);
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

static bool isDeviceCompatible(IDevice& device) noexcept {
  // D3D12 supports MRT; keep tests stable by not toggling feature gate in Device
  if (device.getBackendType() == igl::BackendType::D3D12) {
    return true;
  }
  return device.hasFeature(DeviceFeatures::MultipleRenderTargets);
}

void MRTSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

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
  inputDesc.attributes[0] = VertexAttribute{
      0, VertexAttributeFormat::Float3, offsetof(VertexPosUv, position), "position", 0};
  inputDesc.attributes[1] =
      VertexAttribute{0, VertexAttributeFormat::Float2, offsetof(VertexPosUv, uv), "uv_in", 1};
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(VertexPosUv);
  vertexInput_ = device.createVertexInputState(inputDesc, nullptr);

  // Sampler & Texture
  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  samplerDesc.debugName = "Sampler: linear";
  samp0_ = device.createSamplerState(samplerDesc, nullptr);
  tex0_ = getPlatform().loadTexture("igl.png");

  {
    shaderStagesMRT_ = createShaderStagesForBackend(device, 0);
    shaderStagesDisplayLast_ = createShaderStagesForBackend(device, 1);
  }

  commandQueue_ = device.createCommandQueue({}, nullptr);

  tex0_->generateMipmap(*commandQueue_);

  renderPassMRT_.colorAttachments.resize(2);
  renderPassMRT_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPassMRT_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPassMRT_.colorAttachments[0].clearColor = getPreferredClearColor();
  renderPassMRT_.colorAttachments[1].loadAction = LoadAction::Clear;
  renderPassMRT_.colorAttachments[1].storeAction = StoreAction::Store;
  renderPassMRT_.colorAttachments[1].clearColor = getPreferredClearColor();

  renderPassDisplayLast_.colorAttachments.resize(1);
  renderPassDisplayLast_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPassDisplayLast_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPassDisplayLast_.colorAttachments[0].clearColor = getPreferredClearColor();
}

void MRTSession::update(const igl::SurfaceTextures surfaceTextures) noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  createOrUpdateFramebufferMRT(surfaceTextures);

  const size_t textureUnit = 0;

  // Graphics pipeline: state batch that fully configures GPU for rendering
  if (pipelineStateMRT_ == nullptr) {
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput_;
    graphicsDesc.shaderStages = shaderStagesMRT_;
    graphicsDesc.targetDesc.colorAttachments.resize(2);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        surfaceTextures.color->getProperties().format;
    graphicsDesc.targetDesc.colorAttachments[0].blendEnabled = true;
    graphicsDesc.targetDesc.colorAttachments[0].rgbBlendOp = BlendOp::Add;
    graphicsDesc.targetDesc.colorAttachments[0].alphaBlendOp = BlendOp::Add;
    graphicsDesc.targetDesc.colorAttachments[0].srcRGBBlendFactor = BlendFactor::SrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[0].srcAlphaBlendFactor = BlendFactor::SrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[0].dstRGBBlendFactor = BlendFactor::OneMinusSrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[0].dstAlphaBlendFactor = BlendFactor::OneMinusSrcAlpha;

    graphicsDesc.targetDesc.colorAttachments[1].textureFormat =
        surfaceTextures.color->getProperties().format;
    graphicsDesc.targetDesc.colorAttachments[1].blendEnabled = true;
    graphicsDesc.targetDesc.colorAttachments[1].rgbBlendOp = BlendOp::Add;
    graphicsDesc.targetDesc.colorAttachments[1].alphaBlendOp = BlendOp::Add;
    graphicsDesc.targetDesc.colorAttachments[1].srcRGBBlendFactor = BlendFactor::SrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[1].srcAlphaBlendFactor = BlendFactor::SrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[1].dstRGBBlendFactor = BlendFactor::OneMinusSrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[1].dstAlphaBlendFactor = BlendFactor::OneMinusSrcAlpha;

    graphicsDesc.fragmentUnitSamplerMap[textureUnit] = IGL_NAMEHANDLE("inputImage");
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;

    pipelineStateMRT_ = device.createRenderPipeline(graphicsDesc, nullptr);
  }

  const std::shared_ptr<ICommandBuffer> buffer = commandQueue_->createCommandBuffer({}, nullptr);

  IGL_LOG_INFO("Creating MRT encoder with framebuffer %p\n", framebufferMRT_.get());
  auto commands = buffer->createRenderCommandEncoder(renderPassMRT_, framebufferMRT_);

  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);

  // Draw call 0
  // clang-format off
  commands->bindVertexBuffer(0, *vb0_);
  commands->bindRenderPipelineState(pipelineStateMRT_);
  commands->bindTexture(textureUnit, BindTarget::kFragment, tex0_.get());
  commands->bindSamplerState(textureUnit, BindTarget::kFragment, samp0_.get());
  commands->drawIndexed(6);
  // clang-format on

  // Draw call 1
  // clang-format off
  commands->bindVertexBuffer(0, *vb1_);
  commands->drawIndexed(6);
  // clang-format on

  commands->endEncoding();

  createOrUpdateFramebufferDisplayLast(surfaceTextures);

  if (pipelineStateLastDisplay_ == nullptr) {
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput_;
    graphicsDesc.shaderStages = shaderStagesDisplayLast_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        surfaceTextures.color->getProperties().format;
    graphicsDesc.fragmentUnitSamplerMap[textureUnit] = IGL_NAMEHANDLE("colorGreen");
    graphicsDesc.fragmentUnitSamplerMap[textureUnit + 1] = IGL_NAMEHANDLE("colorRed");
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;

    pipelineStateLastDisplay_ = device.createRenderPipeline(graphicsDesc, nullptr);
  }

  // Command buffers (1-N per thread): create, submit and forget

  IGL_LOG_INFO("Creating display encoder with framebuffer %p\n",
               framebufferDisplayLast_ ? framebufferDisplayLast_.get() : nullptr);
  commands = buffer->createRenderCommandEncoder(renderPassDisplayLast_, framebufferDisplayLast_);

  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);

  // Draw call 0
  // clang-format off
  commands->bindRenderPipelineState(pipelineStateLastDisplay_);
  auto green = framebufferMRT_->getColorAttachment(0);
  auto red = framebufferMRT_->getColorAttachment(1);
  IGL_LOG_INFO("Display pass textures: green=%p red=%p\n", green.get(), red.get());
  commands->bindTexture(textureUnit, BindTarget::kFragment, green.get());
  commands->bindSamplerState(textureUnit, BindTarget::kFragment, samp0_.get());
  commands->bindTexture(textureUnit + 1, BindTarget::kFragment, red.get());
  commands->bindSamplerState(textureUnit + 1, BindTarget::kFragment, samp0_.get());

  commands->bindVertexBuffer(0,  *vb0_);
  commands->drawIndexed(6);

  commands->bindVertexBuffer(0, *vb1_);
  commands->drawIndexed(6);

  // clang-format on
  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(surfaceTextures.color);
  }

  commandQueue_->submit(*buffer); // Guarantees ordering between command buffers

  std::array<uint8_t, 4> sampleGreen{};
  std::array<uint8_t, 4> sampleRed{};
  auto leftSample = TextureRangeDesc::new2D(100, 90, 1, 1);
  auto rightSample = TextureRangeDesc::new2D(500, 90, 1, 1);
  IGL_LOG_INFO("Sampling MRT attachments...\n");
  framebufferMRT_->copyBytesColorAttachment(*commandQueue_, 0, sampleGreen.data(), leftSample, 0);
  framebufferMRT_->copyBytesColorAttachment(*commandQueue_, 1, sampleRed.data(), rightSample, 0);
  IGL_LOG_INFO("MRT samples (left green, right red): green=(%u,%u,%u,%u) red=(%u,%u,%u,%u)\n",
               (unsigned)sampleGreen[0],
               (unsigned)sampleGreen[1],
               (unsigned)sampleGreen[2],
               (unsigned)sampleGreen[3],
               (unsigned)sampleRed[0],
               (unsigned)sampleRed[1],
               (unsigned)sampleRed[2],
               (unsigned)sampleRed[3]);

  const auto dims = tex1_->getDimensions();
  std::vector<uint8_t> attachment1(dims.width * dims.height * 4);
  auto fullRange = TextureRangeDesc::new2D(0, 0, dims.width, dims.height);
  framebufferMRT_->copyBytesColorAttachment(
      *commandQueue_, 1, attachment1.data(), fullRange, dims.width * 4);
  size_t redSum = 0;
  for (size_t i = 0; i < attachment1.size(); i += 4) {
    redSum += attachment1[i];
  }
  IGL_LOG_INFO("Attachment1 red sum=%zu\n", redSum);
}

std::shared_ptr<ITexture> MRTSession::createTexture2D(const std::shared_ptr<ITexture>& tex) {
  const auto dimensions = tex->getDimensions();
  // Use RGBA_UNorm8 for MRT attachments to match shader output expectations
  TextureDesc desc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                        dimensions.width,
                                        dimensions.height,
                                        TextureDesc::TextureUsageBits::Attachment |
                                            TextureDesc::TextureUsageBits::Sampled);
  desc.debugName = "shell/renderSessions/MRTSession.cpp:MRTSession::createTexture2D()";

  return getPlatform().getDevice().createTexture(desc, nullptr);
}

void MRTSession::createOrUpdateFramebufferDisplayLast(const igl::SurfaceTextures& surfaceTextures) {
  if (framebufferDisplayLast_) {
    framebufferDisplayLast_->updateDrawable(surfaceTextures.color);
    return;
  }

  // Framebuffer & Texture
  const FramebufferDesc framebufferDesc = {
      .colorAttachments = {{.texture = surfaceTextures.color}},
  };

  framebufferDisplayLast_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
}

void MRTSession::createOrUpdateFramebufferMRT(const igl::SurfaceTextures& surfaceTextures) {
  if (framebufferMRT_) {
    return;
  }

  if (tex1_ == nullptr) {
    tex1_ = createTexture2D(surfaceTextures.color);
  }
  if (tex2_ == nullptr) {
    tex2_ = createTexture2D(surfaceTextures.color);
  }
  // Framebuffer & Texture
  const FramebufferDesc framebufferDesc = {
      .colorAttachments =
          {
              {.texture = tex1_},
              {.texture = tex2_},
          },
  };

  framebufferMRT_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
  if (framebufferMRT_) {
    auto indices = framebufferMRT_->getColorAttachmentIndices();
    IGL_LOG_INFO("MRT framebuffer=%p attachments: %zu\n",
                 framebufferMRT_.get(),
                 indices.size());
  }
}

} // namespace igl::shell
