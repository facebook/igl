/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "TinyRenderable.hpp"

#include <cstring>
#import <igl/IGL.h>
#import <igl/NameHandle.h>
#import <igl/ShaderCreator.h>
#include <memory>
#import <simd/simd.h>

namespace {

std::shared_ptr<igl::ITexture> createCheckerboardTexture(igl::IDevice& device) {
  const size_t kWidth = 4, kHeight = 4;
  const uint32_t kData[kWidth][kHeight] = {
      {0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFFFFF},
      {0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFFFFF},
      {0xFFFFFFFF, 0xFFFFFFFF, 0xFF000000, 0xFF000000},
      {0xFFFFFFFF, 0xFFFFFFFF, 0xFF000000, 0xFF000000},
  };

  igl::Result result;
  const igl::TextureDesc desc =
      igl::TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                              kWidth,
                              kHeight,
                              igl::TextureDesc::TextureUsageBits::Sampled);
  auto texture = device.createTexture(desc, &result);
  IGL_DEBUG_ASSERT(result.isOk(), "Create texture failed: %s\n", result.message.c_str());

  const auto range = igl::TextureRangeDesc::new2D(0, 0, kWidth, kHeight);
  texture->upload(range, kData);
  return texture;
}

const char kMSLShaderSource[] =
    R"(#include <metal_stdlib>
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
      uint vid [[vertex_id]],
      constant VertexIn * vertices [[buffer(0)]]) {
    VertexOut out;
    out.position = float4(vertices[vid].position, 1.0);
    out.uv = vertices[vid].uv;
    return out;
  }

  fragment float4 fragmentShader(
      VertexOut IN [[stage_in]],
      texture2d<float> diffuseTex [[texture(0)]]) {
    constexpr sampler linearSampler(
        mag_filter::linear, min_filter::linear);
    return diffuseTex.sample(linearSampler, IN.uv);
  })";
// const size_t kMSLShaderSourceLen = sizeof(kMSLShaderSource)/sizeof(kMSLShaderSource[0]) - 1;

const char kGLSLShaderSourceVertex[] =
    R"(#line 0
  precision highp float;

  attribute vec3 position;
  attribute vec2 uv_in;
  varying vec2 uv;

  void main() {
    gl_Position = vec4(position, 1.0);
    uv = uv_in;
  })";
// const size_t kGLSLShaderSourceVertexLen =
// sizeof(kGLSLShaderSourceVertex)/sizeof(kGLSLShaderSourceVertex[0]) - 1;

const char kGLSLShaderSourceFragment[] =
    R"(#line 0
  precision highp float;

  uniform sampler2D inputImage;
  varying vec2 uv;

  void main() {
    gl_FragColor = texture2D(inputImage, uv);
  })";
// const size_t kGLSLShaderSourceFragmentLen =
// sizeof(kGLSLShaderSourceFragment)/sizeof(kGLSLShaderSourceFragment[0]) - 1;

std::unique_ptr<igl::IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
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
  case igl::BackendType::Metal: {
    igl::Result result;
    auto stages = igl::ShaderStagesCreator::fromLibraryStringInput(
        device, kMSLShaderSource, "vertexShader", "fragmentShader", "", &result);
    IGL_DEBUG_ASSERT(result.isOk(), "Shader stage creation failed: %s\n", result.message.c_str());
    return stages;
  }
  case igl::BackendType::OpenGL: {
    igl::Result result;
    auto stages = igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                                  kGLSLShaderSourceVertex,
                                                                  "main",
                                                                  "",
                                                                  kGLSLShaderSourceFragment,
                                                                  "main",
                                                                  "",
                                                                  &result);
    IGL_DEBUG_ASSERT(result.isOk(), "Shader stage creation failed: %s\n", result.message.c_str());
    return stages;
  }
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

const size_t kTextureUnit = 0;

} // namespace

// ----------------------------------------------------------------------------

namespace iglu::kit {

const nlohmann::json& TinyRenderable::getProperties() const {
  static const nlohmann::json j;
  //  = {
  //    "name", "tiny",
  //    {"backends", {
  //      {"opengl", "metal"}
  //    }},
  //    {"snapshot-test", true},
  //  };

  return j;
}

void TinyRenderable::initialize(igl::IDevice& device, const igl::IFramebuffer& framebuffer) {
  IGL_DEBUG_ASSERT(device.verifyScope());

  igl::Result result;

  shaderStages_ = getShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(shaderStages_ != nullptr);

  // Vertex buffer
  struct VertexPosUv {
    simd::float3 position; // SIMD 128b aligned
    simd::float2 uv; // SIMD 128b aligned
  };

  const float kMax = 20;
  const VertexPosUv kVertexData[] = {
      {{-0.9f, 0.9f, 0.0}, {0.0, kMax}},
      {{0.9f, 0.9f, 0.0}, {kMax, kMax}},
      {{-0.9f, -0.9f, 0.0}, {0.0, 0.0}},
      {{0.9f, -0.9f, 0.0}, {kMax, 0.0}},
  };

  const igl::BufferDesc vbDesc =
      igl::BufferDesc(igl::BufferDesc::BufferTypeBits::Vertex, kVertexData, sizeof(kVertexData));
  vertexBuffer_ = device.createBuffer(vbDesc, &result);
  IGL_DEBUG_ASSERT(result.isOk(), "create buffer failed: %s\n", result.message.c_str());

  // Index buffer
  const uint16_t kIndexData[] = {0, 1, 2, 1, 3, 2};
  auto ibDesc =
      igl::BufferDesc(igl::BufferDesc::BufferTypeBits::Index, kIndexData, sizeof(kIndexData));
  indexBuffer_ = device.createBuffer(ibDesc, &result);
  IGL_DEBUG_ASSERT(result.isOk(), "create buffer failed: %s\n", result.message.c_str());

  // Vertex input state
  igl::VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0] = igl::VertexAttribute(
      0, igl::VertexAttributeFormat::Float3, offsetof(VertexPosUv, position), "position", 0);
  inputDesc.attributes[1] = igl::VertexAttribute(
      0, igl::VertexAttributeFormat::Float2, offsetof(VertexPosUv, uv), "uv_in", 1);
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(VertexPosUv);
  vertexInput_ = device.createVertexInputState(inputDesc, &result);
  IGL_DEBUG_ASSERT(result.isOk(), "create vertex state failed: %s\n", result.message.c_str());

  // Sampler & Texture
  igl::SamplerStateDesc samplerDesc;
  samplerDesc.addressModeU = igl::SamplerAddressMode::Repeat;
  samplerDesc.addressModeV = igl::SamplerAddressMode::Repeat;
  samplerDesc.minFilter = samplerDesc.magFilter = igl::SamplerMinMagFilter::Nearest;
  samplerDesc.magFilter = samplerDesc.magFilter = igl::SamplerMinMagFilter::Nearest;
  sampler_ = device.createSamplerState(samplerDesc, nullptr);
  texture_ = createCheckerboardTexture(device);

  igl::RenderPipelineDesc graphicsDesc;
  graphicsDesc.vertexInputState = vertexInput_;
  graphicsDesc.shaderStages = shaderStages_;
  auto indices = framebuffer.getColorAttachmentIndices();
  IGL_DEBUG_ASSERT(!indices.empty());
  graphicsDesc.targetDesc.colorAttachments.resize(1);
  auto textureFormat = framebuffer.getColorAttachment(indices[0])->getProperties().format;
  graphicsDesc.targetDesc.colorAttachments[0].textureFormat = textureFormat;
  graphicsDesc.targetDesc.colorAttachments[0].blendEnabled = true;
  graphicsDesc.targetDesc.colorAttachments[0].rgbBlendOp = igl::BlendOp::Add;
  graphicsDesc.targetDesc.colorAttachments[0].alphaBlendOp = igl::BlendOp::Add;
  graphicsDesc.targetDesc.colorAttachments[0].srcRGBBlendFactor = igl::BlendFactor::SrcAlpha;
  graphicsDesc.targetDesc.colorAttachments[0].srcAlphaBlendFactor = igl::BlendFactor::SrcAlpha;
  graphicsDesc.targetDesc.colorAttachments[0].dstRGBBlendFactor =
      igl::BlendFactor::OneMinusSrcAlpha;
  graphicsDesc.targetDesc.colorAttachments[0].dstAlphaBlendFactor =
      igl::BlendFactor::OneMinusSrcAlpha;

  graphicsDesc.fragmentUnitSamplerMap[kTextureUnit] = IGL_NAMEHANDLE("inputImage");
  graphicsDesc.cullMode = igl::CullMode::Back;
  graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;

  pipelineState_ = device.createRenderPipeline(graphicsDesc, &result);
  IGL_DEBUG_ASSERT(result.isOk(), "create pipeline failed: %s\n", result.message.c_str());
}

void TinyRenderable::update(igl::IDevice& device) {
  // no-op
}

void TinyRenderable::submit(igl::IRenderCommandEncoder& cmds) {
  // Draw call 0
  // clang-format off
  cmds.bindVertexBuffer(0, *vertexBuffer_);
  cmds.bindRenderPipelineState(pipelineState_);
  cmds.bindTexture(kTextureUnit, igl::BindTarget::kFragment, texture_.get());
  cmds.bindSamplerState(kTextureUnit, igl::BindTarget::kFragment, sampler_.get());
  cmds.bindIndexBuffer(*indexBuffer_, igl::IndexFormat::UInt16);
  cmds.drawIndexed(6);
  // clang-format on
}

} // namespace iglu::kit
