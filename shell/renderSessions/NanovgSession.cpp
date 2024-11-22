/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "NanovgSession.h"

#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <regex>
#include <chrono>
#include <shell/shared/renderSession/ShellParams.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/fileLoader/FileLoader.h>
#include <filesystem>

namespace igl::shell {

double getMilliSeconds() {
  return std::chrono::duration<double>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch())
      .count();
}

namespace {
std::string getMetalShaderSource() {
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

const char* getVulkanVertexShaderSource() {
  return R"(#version 460
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
const char* getVulkanFragmentShaderSource() {
  return R"(#version 460
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main() {
	out_FragColor = vec4(color, 1.0);
}
)";
}

const char* getOpenGLESVertexShaderSource() {
  return R"(#version 300 es
out vec3 color;
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
	gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
	color = col[gl_VertexID];
}
)";
}

const char* getOpenGLESFragmentShaderSource() {
  return R"(#version 300 es
  precision highp float;
  in vec3 color;
  out vec4 out_FragColor;
  void main() {
	out_FragColor = vec4(color, 1.0);
}
)";
}
} // namespace

static std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_DEBUG_ASSERT_NOT_REACHED();
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
      std::string version = "410";
      if (glVersion == igl::opengl::GLVersion::v3_0_ES ||
          glVersion == igl::opengl::GLVersion::v3_1_ES ||
          glVersion == igl::opengl::GLVersion::v3_2_ES) {
        return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                               getOpenGLESVertexShaderSource(),
                                                               "main",
                                                               "",
                                                               getOpenGLESFragmentShaderSource(),
                                                               "main",
                                                               "",
                                                               nullptr);
      }
      auto codeVS1 = std::regex_replace(
          getVulkanVertexShaderSource(), std::regex("gl_VertexIndex"), "gl_VertexID");
      auto codeVS2 = std::regex_replace(codeVS1.c_str(), std::regex("460"), version);

      auto codeFS = std::regex_replace(getVulkanFragmentShaderSource(), std::regex("460"), version);

      return igl::ShaderStagesCreator::fromModuleStringInput(
          device, codeVS2.c_str(), "main", "", codeFS.c_str(), "main", "", nullptr);
    } else {
      IGL_DEBUG_ABORT("This sample is incompatible with OpenGL 2.1");
      return nullptr;
    }
#else
    return nullptr;
#endif // IGL_BACKEND_OPENGL
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

void NanovgSession::initialize() noexcept {
  // Command queue: backed by different types of GPU HW queues
  const CommandQueueDesc desc{CommandQueueType::Graphics};
  commandQueue_ = getPlatform().getDevice().createCommandQueue(desc, nullptr);

  renderPass_.colorAttachments.resize(1);

  renderPass_.colorAttachments[0] = igl::RenderPassDesc::ColorAttachmentDesc{};
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = igl::Color(0.3f, 0.3f, 0.32f, 1.0f);//getPreferredClearColor();
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
    
    nvgContext_ = getPlatform().nanovgContext;
    
//    auto bb = getPlatform().loadTexture("orange.png");
//    
    auto aa = getPlatform().getImageLoader().fileLoader().fullPath("image1.jpg");
    
    SetImageFullPathCallback([this](const std::string & name){
#if IGL_PLATFORM_ANDROID
        auto fullPath = std::filesystem::path("/data/data/com.facebook.igl.shell/files/") / name;
        if (std::filesystem::exists(fullPath)) {
            return fullPath.string();
        } else {
            IGL_DEBUG_ASSERT(false);
            return std::string("");
        }
#else
        return getPlatform().getImageLoader().fileLoader().fullPath(name);
#endif
    });
    
    if (loadDemoData(nvgContext_, &nvgDemoData_) == -1){
        assert(false);
    }
    
    initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");
    initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");
    initGraph(&gpuGraph, GRAPH_RENDER_MS, "GPU Time");
    times_ = 0;
}

void NanovgSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
    drawTriangle(surfaceTextures);
    
//    const auto dimensions = surfaceTextures.color->getDimensions();
//    drawNanovg((float)dimensions.width, (float)dimensions.height, surfaceTextures);
}

void NanovgSession::drawTriangle(igl::SurfaceTextures surfaceTextures){
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
  framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
  framebufferDesc.stencilAttachment.texture = surfaceTextures.depth;

  const auto dimensions = surfaceTextures.color->getDimensions();
  framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
  IGL_DEBUG_ASSERT(framebuffer_);

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
      
      if (framebuffer_->getStencilAttachment()) {
        desc.targetDesc.stencilAttachmentFormat =
            framebuffer_->getStencilAttachment()->getProperties().format;
      }

    desc.shaderStages = getShaderStagesForBackend(getPlatform().getDevice());
    renderPipelineState_Triangle_ = getPlatform().getDevice().createRenderPipeline(desc, nullptr);
    IGL_DEBUG_ASSERT(renderPipelineState_Triangle_);
  }

  framebuffer_->updateDrawable(surfaceTextures.color);

  // Command buffers (1-N per thread): create, submit and forget
  const CommandBufferDesc cbDesc;
  const std::shared_ptr<ICommandBuffer> buffer =
      commandQueue_->createCommandBuffer(cbDesc, nullptr);

  const igl::Viewport viewport = {
      0.0f, 0.0f, (float)dimensions.width, (float)dimensions.height, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, (uint32_t)dimensions.width, (uint32_t)dimensions.height};

  // This will clear the framebuffer
    std::shared_ptr<igl::IRenderCommandEncoder> commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  commands->bindRenderPipelineState(renderPipelineState_Triangle_);
  commands->bindViewport(viewport);
  commands->bindScissorRect(scissor);
  commands->pushDebugGroupLabel("Render Triangle", igl::Color(1, 0, 0));
//  commands->draw(3);
  commands->popDebugGroupLabel();
    
    drawNanovg((float)dimensions.width, (float)dimensions.height, commands);
    
  commands->endEncoding();
    
  if (shellParams().shouldPresent) {
    buffer->present(surfaceTextures.color);
  }

  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

void NanovgSession::drawNanovg(float __width, float __height, std::shared_ptr<igl::IRenderCommandEncoder> command){
    NVGcontext* vg = nvgContext_;
    
    float pxRatio = 2.0f;
    
    const float width = __width / pxRatio;
    const float height = __height / pxRatio;

    int mx = 0;
    int my = 0;
    int blowup = 0;
    
    auto start_ms = getMilliSeconds();
    
    nvgBeginFrame(vg, width,height, pxRatio);
    nvgSetColorTexture(vg, framebuffer_, command);

    times_++;
    renderDemo(vg, mx,my, width,height, times_ / 60.0f, blowup, &nvgDemoData_);

    renderGraph(vg, 5,5, &fps);
    renderGraph(vg, 5+200+5,5, &cpuGraph);
    renderGraph(vg, 5+200+5+200+5,5, &gpuGraph);
    
    {
//        //绘制一个矩形
//        nvgBeginPath(vg);
//        nvgRect(vg, 100,100, 10,10);
//        nvgFillColor(vg, nvgRGBA(255,192,0,255));
//        nvgFill(vg);
//        
//        //绘制扣洞矩形
//        nvgBeginPath(vg);
//        nvgRect(vg, 100,100, 50,50);
//        nvgRect(vg, 110,110, 10,10);
//        nvgPathWinding(vg, NVG_HOLE);    // Mark circle as a hole.
//        nvgFillColor(vg, nvgRGBA(255,192,0,255));
//        nvgFill(vg);
//        
//        //绘制图片
//        NVGpaint imgPaint = nvgImagePattern(vg, 200, 200, 100,100, 0.0f/180.0f*NVG_PI, 2, 0.5);
//        nvgBeginPath(vg);
//        nvgRect(vg, 100,100, 500,500);
//        nvgFillPaint(vg, imgPaint);
//        nvgFill(vg);
////        
////        //绘制文字
//        nvgFontSize(vg, 150.0f);
//        nvgFontFace(vg, "sans-bold");
//        nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
//        nvgFontBlur(vg,2);
//        nvgFillColor(vg, nvgRGBA(256,0,0,128));
//        nvgText(vg, 200, 100, "Hello", NULL);
    }

    nvgEndFrame(vg);
    
    auto end_ms = getMilliSeconds();
    
    updateGraph(&fps, (start_ms - preTimestamp_));
    updateGraph(&cpuGraph, (end_ms - start_ms));
    
    preTimestamp_ = start_ms;
}

} // namespace igl::shell
