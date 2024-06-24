/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "DrawInstancedSession.h"

#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

namespace {

[[maybe_unused]] void stringReplaceAll(std::string& s,
                                       const std::string& searchString,
                                       const std::string& replaceString) {
  size_t pos = 0;
  while ((pos = s.find(searchString, pos)) != std::string::npos) {
    s.replace(pos, searchString.length(), replaceString);
  }
}

const char* getMetalShaderSource() {
  return R"(
          #include <metal_stdlib>
          #include <simd/simd.h>
          using namespace metal;

          constant float2 pos[6] = {
            float2(-0.05f,  0.05f),
            float2( 0.05f, -0.05f),
            float2( -0.05f, -0.05f),
            float2(-0.05f,  0.05f),
            float2(0.05f, -0.05f),
            float2(0.05f,  0.05f)
          };
          constant float3 col[6] = {
            float3(1.0, 0.0, 0.0),
            float3(0.0, 1.0, 0.0),
            float3(0.0, 0.0, 1.0),
            float3(1.0, 0.0, 0.0),
            float3(0.0, 1.0, 0.0),
            float3(0.0, 0.0, 1.0)
          };
        
         struct VertexIn{
            float2 offset [[attribute(0)]];
         };

          struct VertexOut {
            float4 position [[position]];
            float3 uvw;
          };

          vertex VertexOut vertexShader(uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(1)]],
                                        VertexIn in [[stage_in]]) {
            VertexOut out;
            out.position = float4(pos[vid] + in.offset, 0.0, 1.0);
            out.uvw = col[vid];
            return out;
           }

           fragment float4 fragmentShader(
                 VertexOut in[[stage_in]]) {

             float4 tex = float4(in.uvw,1.0);
             return tex;
           }
        )";
}

const char* getVulkanVertexShaderSource() {
  return R"(#version 460
layout (location=0) in vec2 offset;
layout (location=0) out vec3 color;
const vec2 pos[6] = vec2[6](
    vec2(-0.05f,  0.05f),
    vec2( 0.05f, -0.05f),
    vec2( -0.05f, -0.05f),
    vec2(-0.05f,  0.05f),
    vec2(0.05f, -0.05f),
    vec2(0.05f,  0.05f)
);
const vec3 col[6] = vec3[6](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0),
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);
void main() {
	gl_Position = vec4(pos[gl_VertexIndex] + offset , 0.0, 1.0);
	color = col[gl_VertexIndex];
}
)";
}

const char* getVulkanFragmentShaderSource() {
  return R"(#version 460
precision mediump float;
precision highp int;
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main() {
	out_FragColor = vec4(color, 1.0);
}
)";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getVulkanVertexShaderSource(),
                                                           "main",
                                                           "",
                                                           getVulkanFragmentShaderSource(),
                                                           "main",
                                                           "",
                                                           nullptr);
    return nullptr;
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Metal:
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, getMetalShaderSource(), "vertexShader", "fragmentShader", "", nullptr);
  case igl::BackendType::OpenGL:
#if IGL_BACKEND_OPENGL
    auto glVersion =
        static_cast<igl::opengl::Device&>(device).getContext().deviceFeatures().getGLVersion();
    if (glVersion > igl::opengl::GLVersion::v2_1) {
      auto usesOpenGLES =
          static_cast<igl::opengl::Device&>(device).getContext().deviceFeatures().usesOpenGLES();
      std::string codeVS(getVulkanVertexShaderSource());
      stringReplaceAll(codeVS, "gl_VertexIndex", "gl_VertexID");
      stringReplaceAll(codeVS, "460", usesOpenGLES ? "300 es" : "410");

      std::string codeFS(getVulkanFragmentShaderSource());
      stringReplaceAll(codeFS, "460", usesOpenGLES ? "300 es" : "410");

      if (usesOpenGLES) {
        stringReplaceAll(codeVS, "layout (location=0) out", "out");
        stringReplaceAll(codeFS, "layout (location=0) out", "out");
        stringReplaceAll(codeFS, "layout (location=0) in", "in");
      }

      return igl::ShaderStagesCreator::fromModuleStringInput(
          device, codeVS.c_str(), "main", "", codeFS.c_str(), "main", "", nullptr);
    } else {
      IGL_ASSERT_MSG(0, "This sample is incompatible with OpenGL 2.1");
      return nullptr;
    }
#else
    return nullptr;
#endif // IGL_BACKEND_OPENGL
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

} // namespace

void DrawInstancedSession::initialize() noexcept {
  // Command queue: backed by different types of GPU HW queues
  commandQueue_ =
      getPlatform().getDevice().createCommandQueue({CommandQueueType::Graphics}, nullptr);

  renderPass_.colorAttachments.resize(1);

  renderPass_.colorAttachments[0] = igl::RenderPassDesc::ColorAttachmentDesc{};
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPlatform().getDevice().backendDebugColor();
  renderPass_.depthAttachment.loadAction = LoadAction::DontCare;

  // Create Index Buffer
  const int16_t indexes[6] = {0, 1, 2, 3, 4, 5};

  BufferDesc buffer_desc;
  buffer_desc.type = BufferDesc::BufferTypeBits::Index;
  buffer_desc.length = sizeof(indexes);
  buffer_desc.data = &indexes;
  index_buffer_ = getPlatform().getDevice().createBuffer(buffer_desc, nullptr);
  IGL_ASSERT(index_buffer_);
}

void DrawInstancedSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;

  const auto dimensions = surfaceTextures.color->getDimensions();
  framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
  IGL_ASSERT(framebuffer_);

  if (!renderPipelineState_Triangle_) {
    VertexInputStateDesc inputDesc;
    inputDesc.numAttributes = 1;
    inputDesc.attributes[0] = VertexAttribute(1, VertexAttributeFormat::Float2, 0, "offset", 0);
    inputDesc.numInputBindings = 1;
    inputDesc.inputBindings[1].stride = sizeof(float) * 2;
    inputDesc.inputBindings[1].sampleFunction = igl::VertexSampleFunction::Instance;
    auto vertexInput0_ = getPlatform().getDevice().createVertexInputState(inputDesc, nullptr);
    IGL_ASSERT(vertexInput0_ != nullptr);

    RenderPipelineDesc desc;
    desc.vertexInputState = vertexInput0_;

    desc.targetDesc.colorAttachments.resize(1);

    if (framebuffer_->getColorAttachment(0)) {
      desc.targetDesc.colorAttachments[0].textureFormat =
          framebuffer_->getColorAttachment(0)->getProperties().format;
    }

    if (framebuffer_->getDepthAttachment()) {
      desc.targetDesc.depthAttachmentFormat =
          framebuffer_->getDepthAttachment()->getProperties().format;
    }

    desc.shaderStages = getShaderStagesForBackend(getPlatform().getDevice());
    renderPipelineState_Triangle_ = getPlatform().getDevice().createRenderPipeline(desc, nullptr);
    IGL_ASSERT(renderPipelineState_Triangle_);
  }

  if (!vertex_buffer_) {
    glm::vec2 translations[100];
    int index = 0;
    const float offset = 0.1f;
    for (int y = -10; y < 10; y += 2) {
      for (int x = -10; x < 10; x += 2) {
        glm::vec2 translation;
        translation.x = (float)x / 10.0f + offset;
        translation.y = (float)y / 10.0f + offset;
        translations[index++] = translation;
      }
    }

    BufferDesc desc;
    desc.type = BufferDesc::BufferTypeBits::Vertex;
    desc.length = sizeof(glm::vec2) * 100;
    desc.data = translations;
    vertex_buffer_ = getPlatform().getDevice().createBuffer(desc, nullptr);
    IGL_ASSERT(vertex_buffer_);
  }

  framebuffer_->updateDrawable(surfaceTextures.color);

  // Command buffers (1-N per thread): create, submit and forget
  const std::shared_ptr<ICommandBuffer> buffer = commandQueue_->createCommandBuffer({}, nullptr);

  const igl::Viewport viewport = {
      0.0f, 0.0f, (float)dimensions.width, (float)dimensions.height, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, (uint32_t)dimensions.width, (uint32_t)dimensions.height};

  // This will clear the framebuffer
  auto commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  commands->bindRenderPipelineState(renderPipelineState_Triangle_);
  commands->bindViewport(viewport);
  commands->bindScissorRect(scissor);
  commands->pushDebugGroupLabel("Render Triangle", igl::Color(1, 0, 0));
  commands->bindVertexBuffer(1, *vertex_buffer_);
  commands->bindIndexBuffer(*index_buffer_, IndexFormat::UInt16);
  commands->drawIndexed(6, 100);
  commands->popDebugGroupLabel();
  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(surfaceTextures.color);
  }

  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

} // namespace igl::shell
