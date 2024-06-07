/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "HelloWorldSession.h"

#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <regex>

namespace igl::shell {

static std::string getMetalShaderSource() {
  return R"(
          #include <metal_stdlib>
          #include <simd/simd.h>
          using namespace metal;

          constant float2 pos[3] = {
            float2(-0.6, -0.4),
            float2( 0.6, -0.4),
            float2( 0.0,  0.6)
          };
          constant float3 col[3] = {
            float3(1.0, 0.0, 0.0),
            float3(0.0, 1.0, 0.0),
            float3(0.0, 0.0, 1.0)
          };

          struct VertexOut {
            float4 position [[position]];
            float3 uvw;
          };

          vertex VertexOut vertexShader(uint vid [[vertex_id]]) {
            VertexOut out;
            out.position = float4(pos[vid], 0.0, 1.0);
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

static const char* getVulkanVertexShaderSource() {
  return R"(
#version 460
layout (location=0) out vec3 color;
const vec2 pos[3] = vec2[3](
	vec2(-0.6, -0.4),
	vec2( 0.6, -0.4),
	vec2( 0.0,  0.6)
);
const vec3 col[3] = vec3[3](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);
void main() {
	gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
	color = col[gl_VertexIndex];
}
)";
}
static const char* getVulkanFragmentShaderSource() {
  return R"(
#version 460
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main() {
	out_FragColor = vec4(color, 1.0);
}
)";
}

static std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
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
        device, getMetalShaderSource().c_str(), "vertexShader", "fragmentShader", "", nullptr);
  case igl::BackendType::OpenGL:
#if IGL_BACKEND_OPENGL
    auto glVersion =
        static_cast<igl::opengl::Device&>(device).getContext().deviceFeatures().getGLVersion();
    if (glVersion > igl::opengl::GLVersion::v2_1) {
      auto codeVS1 = std::regex_replace(
          getVulkanVertexShaderSource(), std::regex("gl_VertexIndex"), "gl_VertexID");
      auto codeVS2 = std::regex_replace(codeVS1.c_str(), std::regex("460"), "410");

      auto codeFS = std::regex_replace(getVulkanFragmentShaderSource(), std::regex("460"), "410");

      return igl::ShaderStagesCreator::fromModuleStringInput(
          device, codeVS2.c_str(), "main", "", codeFS.c_str(), "main", "", nullptr);
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

void HelloWorldSession::initialize() noexcept {
  // Command queue: backed by different types of GPU HW queues
  CommandQueueDesc desc{CommandQueueType::Graphics};
  commandQueue_ = getPlatform().getDevice().createCommandQueue(desc, nullptr);

  renderPass_.colorAttachments.resize(1);

  renderPass_.colorAttachments[0] = igl::RenderPassDesc::ColorAttachmentDesc{};
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPlatform().getDevice().backendDebugColor();
  renderPass_.depthAttachment.loadAction = LoadAction::DontCare;
}

void HelloWorldSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;

  const auto dimensions = surfaceTextures.color->getDimensions();
  framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
  IGL_ASSERT(framebuffer_);

  if (!renderPipelineState_Triangle_) {
    RenderPipelineDesc desc;

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

  framebuffer_->updateDrawable(surfaceTextures.color);

  // Command buffers (1-N per thread): create, submit and forget
  CommandBufferDesc cbDesc;
  std::shared_ptr<ICommandBuffer> buffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);

  const igl::Viewport viewport = {
      0.0f, 0.0f, (float)dimensions.width, (float)dimensions.height, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, (uint32_t)dimensions.width, (uint32_t)dimensions.height};

  // This will clear the framebuffer
  auto commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  commands->bindRenderPipelineState(renderPipelineState_Triangle_);
  commands->bindViewport(viewport);
  commands->bindScissorRect(scissor);
  commands->pushDebugGroupLabel("Render Triangle", igl::Color(1, 0, 0));
  commands->draw(3);
  commands->popDebugGroupLabel();
  commands->endEncoding();

  buffer->present(surfaceTextures.color);

  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

} // namespace igl::shell
