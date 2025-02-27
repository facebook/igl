/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "TinyMeshSession.h"

#include <cmath>
#include <cstddef>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <igl/FPSCounter.h>
#include <shell/shared/platform/DisplayContext.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/Device.h>
#include <igl/opengl/RenderCommandEncoder.h>
#endif // IGL_BACKEND_OPENGL
#if IGL_BACKEND_VULKAN
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/VulkanContext.h>
#endif // IGL_BACKEND_VULKAN

#if defined(IGL_CMAKE_BUILD)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#endif // __clang__
#include <stb/stb_image.h>
#define TINY_TEST_USE_DEPTH_BUFFER 1

namespace {

[[maybe_unused]] std::string stringReplaceAll(const char* input,
                                              const char* searchString,
                                              const char* replaceString) {
  std::string s(input);
  const size_t len = strlen(searchString);
  size_t pos = 0;
  while ((pos = s.find(searchString, pos)) != std::string::npos) {
    s.replace(pos, len, replaceString);
  }
  return s;
}

} // namespace

namespace igl::shell {

using namespace igl;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

constexpr uint32_t kNumBufferedFrames = 3;

namespace {
int width_ = 0;
int height_ = 0;
} // namespace

constexpr uint32_t kNumCubes = 256;

struct VertexPosUvw {
  vec3 position;
  vec3 color;
  vec2 uv;
};

struct UniformsPerFrame {
  mat4 proj;
  mat4 view;
};
struct UniformsPerObject {
  mat4 model;
};

// from igl/shell/renderSessions/Textured3DCubeSession.cpp
const float half = 1.0f;

// UV-mapped cube with indices: 24 vertices, 36 indices
static VertexPosUvw vertexData0[] = {
    // top
    {{-half, -half, +half}, {0.0, 0.0, 1.0}, {0, 0}}, // 0
    {{+half, -half, +half}, {1.0, 0.0, 1.0}, {1, 0}}, // 1
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 2
    {{-half, +half, +half}, {0.0, 1.0, 1.0}, {0, 1}}, // 3
    // bottom
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 4
    {{-half, +half, -half}, {0.0, 1.0, 0.0}, {0, 1}}, // 5
    {{+half, +half, -half}, {1.0, 1.0, 0.0}, {1, 1}}, // 6
    {{+half, -half, -half}, {1.0, 0.0, 0.0}, {1, 0}}, // 7
    // left
    {{+half, +half, -half}, {1.0, 1.0, 0.0}, {1, 0}}, // 8
    {{-half, +half, -half}, {0.0, 1.0, 0.0}, {0, 0}}, // 9
    {{-half, +half, +half}, {0.0, 1.0, 1.0}, {0, 1}}, // 10
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 11
    // right
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 12
    {{+half, -half, -half}, {1.0, 0.0, 0.0}, {1, 0}}, // 13
    {{+half, -half, +half}, {1.0, 0.0, 1.0}, {1, 1}}, // 14
    {{-half, -half, +half}, {0.0, 0.0, 1.0}, {0, 1}}, // 15
    // front
    {{+half, -half, -half}, {1.0, 0.0, 0.0}, {0, 0}}, // 16
    {{+half, +half, -half}, {1.0, 1.0, 0.0}, {1, 0}}, // 17
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 18
    {{+half, -half, +half}, {1.0, 0.0, 1.0}, {0, 1}}, // 19
    // back
    {{-half, +half, -half}, {0.0, 1.0, 0.0}, {1, 0}}, // 20
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 21
    {{-half, -half, +half}, {0.0, 0.0, 1.0}, {0, 1}}, // 22
    {{-half, +half, +half}, {0.0, 1.0, 1.0}, {1, 1}}, // 23
};

static uint16_t indexData[] = {0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,
                               8,  9,  10, 10, 11, 8,  12, 13, 14, 14, 15, 12,
                               16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

namespace {
UniformsPerFrame perFrame;
UniformsPerObject perObject[kNumCubes];
vec3 axis_[kNumCubes];
} // namespace

#if IGL_BACKEND_METAL
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
#endif // IGL_BACKEND_METAL

static const char* getVulkanVertexShaderSource() {
  return R"(
layout (location=0) in vec3 pos;
layout (location=1) in vec3 col;
layout (location=2) in vec2 st;
layout (location=0) out vec3 color;
layout (location=1) out vec2 uv;

#if VULKAN
layout (set = 1, binding = 0, std140)
#else
layout (binding = 0, std140)
#endif
uniform UniformsPerFrame {
  mat4 proj;
  mat4 view;
} perFrame;

#if VULKAN
layout (set = 1, binding = 1, std140)
#else
layout (binding = 1, std140)
#endif
uniform UniformsPerObject {
  mat4 model;
} perObject;

void main() {
  mat4 proj = perFrame.proj;
  mat4 view = perFrame.view;
  mat4 model = perObject.model;
  gl_Position = proj * view * model * vec4(pos, 1.0);
  color = col;
  uv = st;
}
)";
}

static const char* getVulkanFragmentShaderSource() {
  return R"(
layout (location=0) in vec3 color;
layout (location=1) in vec2 uv;
layout (location=0) out vec4 out_FragColor;

#if VULKAN
layout (set = 0, binding = 0) uniform sampler2D uTex0;
layout (set = 0, binding = 1) uniform sampler2D uTex1;
#else
layout (binding = 0) uniform sampler2D uTex0;
layout (binding = 1) uniform sampler2D uTex1;
#endif

void main() {
  vec4 t0 = texture(uTex0, 2.0 * uv);
  vec4 t1 = texture(uTex1,  uv);
  out_FragColor = vec4(color * (t0.rgb + t1.rgb), 1.0);
};
)";
}

static std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return nullptr;

#if IGL_BACKEND_VULKAN
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
#endif // IGL_BACKEND_VULKAN

// @fb-only
  // @fb-only
    // @fb-only
    // @fb-only
// @fb-only

#if IGL_BACKEND_METAL
  case igl::BackendType::Metal:
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, getMetalShaderSource().c_str(), "vertexShader", "fragmentShader", "", nullptr);
#endif // IGL_BACKEND_METAL

#if IGL_BACKEND_OPENGL
  case igl::BackendType::OpenGL: {
    auto glVersion =
        static_cast<igl::opengl::Device&>(device).getContext().deviceFeatures().getGLVersion();

    if (glVersion > igl::opengl::GLVersion::v2_1) {
      const std::string codeVS1 =
          stringReplaceAll(getVulkanVertexShaderSource(), "gl_VertexIndex", "gl_VertexID");
      auto codeVS2 = "#version 460\n" + codeVS1;
      auto codeFS = "#version 460\n" + std::string(getVulkanFragmentShaderSource());

      return igl::ShaderStagesCreator::fromModuleStringInput(
          device, codeVS2.c_str(), "main", "", codeFS.c_str(), "main", "", nullptr);
    } else {
      IGL_DEBUG_ABORT("This sample is incompatible with OpenGL 2.1");
      return nullptr;
    }
  }
#endif // IGL_BACKEND_OPENGL

  default:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return nullptr;
  }
}

TinyMeshSession::TinyMeshSession(std::shared_ptr<Platform> platform) :
  RenderSession(std::move(platform)) {
  listener_ = std::make_shared<Listener>(*this);
  getPlatform().getInputDispatcher().addKeyListener(listener_);
  imguiSession_ = std::make_unique<iglu::imgui::Session>(getPlatform().getDevice(),
                                                         getPlatform().getInputDispatcher());
}

void TinyMeshSession::initialize() noexcept {
  device_ = &getPlatform().getDevice();

  // Vertex buffer, Index buffer and Vertex Input. Buffers are allocated in GPU memory.
  vb0_ = device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Vertex,
                                          vertexData0,
                                          sizeof(vertexData0),
                                          ResourceStorage::Private,
                                          0,
                                          "Buffer: vertex"),
                               nullptr);
  ib0_ = device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Index,
                                          indexData,
                                          sizeof(indexData),
                                          ResourceStorage::Private,
                                          0,
                                          "Buffer: index"),
                               nullptr);
  // create an Uniform buffers to store uniforms for 2 objects
  for (uint32_t i = 0; i != kNumBufferedFrames; i++) {
    ubPerFrame_.push_back(
        device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Uniform,
                                         &perFrame,
                                         sizeof(UniformsPerFrame),
                                         ResourceStorage::Shared,
                                         BufferDesc::BufferAPIHintBits::UniformBlock,
                                         "Buffer: uniforms (per frame)"),
                              nullptr));
    ubPerObject_.push_back(
        device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Uniform,
                                         perObject,
                                         kNumCubes * sizeof(UniformsPerObject),
                                         ResourceStorage::Shared,
                                         BufferDesc::BufferAPIHintBits::UniformBlock,
                                         "Buffer: uniforms (per object)"),
                              nullptr));
  }

  {
    VertexInputStateDesc desc;
    desc.numAttributes = 3;
    desc.attributes[0].format = VertexAttributeFormat::Float3;
    desc.attributes[0].offset = offsetof(VertexPosUvw, position);
    desc.attributes[0].name = "pos";
    desc.attributes[0].bufferIndex = 0;
    desc.attributes[0].location = 0;
    desc.attributes[1].format = VertexAttributeFormat::Float3;
    desc.attributes[1].offset = offsetof(VertexPosUvw, color);
    desc.attributes[1].name = "col";
    desc.attributes[1].bufferIndex = 0;
    desc.attributes[1].location = 1;
    desc.attributes[2].format = VertexAttributeFormat::Float2;
    desc.attributes[2].offset = offsetof(VertexPosUvw, uv);
    desc.attributes[2].name = "st";
    desc.attributes[2].bufferIndex = 0;
    desc.attributes[2].location = 2;
    desc.numInputBindings = 1;
    desc.inputBindings[0].stride = sizeof(VertexPosUvw);
    vertexInput0_ = device_->createVertexInputState(desc, nullptr);
  }

  {
    DepthStencilStateDesc desc;
    desc.isDepthWriteEnabled = true;
    desc.compareFunction = igl::CompareFunction::Less;
    depthStencilState_ = device_->createDepthStencilState(desc, nullptr);
  }

  {
    const uint32_t texWidth = 256;
    const uint32_t texHeight = 256;
    const TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::BGRA_SRGB,
                                                texWidth,
                                                texHeight,
                                                TextureDesc::TextureUsageBits::Sampled,
                                                "XOR pattern");
    texture0_ = device_->createTexture(desc, nullptr);
    std::vector<uint32_t> pixels(
        static_cast<std::vector<unsigned int>::size_type>(texWidth * texHeight));

    for (uint32_t y = 0; y != texHeight; y++) {
      for (uint32_t x = 0; x != texWidth; x++) {
        // create a XOR pattern
        pixels[y * texWidth + x] = 0xFF000000 + ((x ^ y) << 16) + ((x ^ y) << 8) + (x ^ y);
      }
    }
    texture0_->upload(TextureRangeDesc::new2D(0, 0, texWidth, texHeight), pixels.data());
  }
  {
    fs::path dir = fs::current_path();
    // find IGLU somewhere above our current directory
    // @fb-only
    const char* contentFolder = "third-party/content/src/";
    // @fb-only
    while (dir != fs::current_path().root_path() && !fs::exists(dir / fs::path(contentFolder))) {
      dir = dir.parent_path();
    }
    int32_t texWidth = 0;
    int32_t texHeight = 0;
    int32_t channels = 0;
    uint8_t* pixels = stbi_load((dir / fs::path(contentFolder) /
                                 fs::path("bistro/BuildingTextures/wood_polished_01_diff.png"))
                                    .string()
                                    .c_str(),
                                &texWidth,
                                &texHeight,
                                &channels,
                                4);
    IGL_DEBUG_ASSERT(pixels,
                     "Cannot load textures. Run `deploy_content.py` before running this app.");
    const TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::RGBA_SRGB,
                                                texWidth,
                                                texHeight,
                                                TextureDesc::TextureUsageBits::Sampled,
                                                "wood_polished_01_diff.png");
    texture1_ = device_->createTexture(desc, nullptr);
    texture1_->upload(TextureRangeDesc::new2D(0, 0, texWidth, texHeight), pixels);
    stbi_image_free(pixels);
  }
  {
    SamplerStateDesc desc = igl::SamplerStateDesc::newLinear();
    desc.addressModeU = igl::SamplerAddressMode::Repeat;
    desc.addressModeV = igl::SamplerAddressMode::Repeat;
    desc.debugName = "Sampler: linear";
    sampler_ = device_->createSamplerState(desc, nullptr);
  }

  // Command queue: backed by different types of GPU HW queues
  commandQueue_ = device_->createCommandQueue({}, nullptr);

  renderPass_.colorAttachments.emplace_back();
  renderPass_.colorAttachments.back().loadAction = LoadAction::Clear;
  renderPass_.colorAttachments.back().storeAction = StoreAction::Store;
  renderPass_.colorAttachments.back().clearColor = {1.0f, 0.0f, 0.0f, 1.0f};
#if TINY_TEST_USE_DEPTH_BUFFER
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
#else
  renderPass_.depthAttachment.loadAction = LoadAction::DontCare;
#endif // TINY_TEST_USE_DEPTH_BUFFER

  // initialize random rotation axes for all cubes
  for (auto& axi : axis_) {
    axi = glm::sphericalRand(1.0f);
  }
}

std::shared_ptr<ITexture> TinyMeshSession::getVulkanNativeDepth() {
#if IGL_BACKEND_VULKAN
  if (device_->getBackendType() == BackendType::Vulkan) {
    const auto& vkPlatformDevice = device_->getPlatformDevice<igl::vulkan::PlatformDevice>();

    IGL_DEBUG_ASSERT(vkPlatformDevice != nullptr);

    Result ret;
    std::shared_ptr<ITexture> drawable =
        vkPlatformDevice->createTextureFromNativeDepth(width_, height_, &ret);

    IGL_DEBUG_ASSERT(ret.isOk());
    return drawable;
  }
#endif // IGL_BACKEND_VULKAN

  // TODO: unhardcode Vulkan assumption above
  return nullptr;
}

void TinyMeshSession::update(SurfaceTextures surfaceTextures) noexcept {
  width_ = surfaceTextures.color->getSize().width;
  height_ = surfaceTextures.color->getSize().height;

  const float deltaSeconds = getDeltaSeconds();

  fps_.updateFPS(deltaSeconds);
  currentTime_ += deltaSeconds;

  if (!framebuffer_) {
    framebufferDesc_.colorAttachments[0].texture = surfaceTextures.color;

#if TINY_TEST_USE_DEPTH_BUFFER
    framebufferDesc_.depthAttachment.texture = getVulkanNativeDepth();
#endif // TINY_TEST_USE_DEPTH_BUFFER
    framebuffer_ = device_->createFramebuffer(framebufferDesc_, nullptr);
    IGL_DEBUG_ASSERT(framebuffer_);

    RenderPipelineDesc desc;

    desc.targetDesc.colorAttachments.resize(1);
    desc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;

    if (framebuffer_->getDepthAttachment()) {
      desc.targetDesc.depthAttachmentFormat =
          framebuffer_->getDepthAttachment()->getProperties().format;
    }

    desc.vertexInputState = vertexInput0_;
    desc.shaderStages = getShaderStagesForBackend(*device_);

#if !TINY_TEST_USE_DEPTH_BUFFER
    desc.cullMode = igl::CullMode::Back;
#endif // TINY_TEST_USE_DEPTH_BUFFER

    desc.frontFaceWinding = igl::WindingMode::Clockwise;
    desc.debugName = igl::genNameHandle("Pipeline: mesh");
    desc.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("uTex0");
    desc.fragmentUnitSamplerMap[1] = IGL_NAMEHANDLE("uTex1");
    renderPipelineState_Mesh_ = device_->createRenderPipeline(desc, nullptr);
  }

  framebuffer_->updateDrawable(surfaceTextures.color);

  // from igl/shell/renderSessions/Textured3DCubeSession.cpp
  const float fov = float(45.0f * (M_PI / 180.0f));
  const float aspectRatio = (float)width_ / (float)height_;
  perFrame.proj = glm::perspectiveLH(fov, aspectRatio, 0.1f, 500.0f);
  // place a "camera" behind the cubes, the distance depends on the total number of cubes
  perFrame.view =
      glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, sqrtf(kNumCubes / 16.0f) * 20.0f * half));
  ubPerFrame_[frameIndex_]->upload(&perFrame, BufferRange(sizeof(perFrame)));

  // rotate cubes around random axes
  for (uint32_t i = 0; i != kNumCubes; i++) {
    const float direction = powf(-1, (float)(i + 1));
    const uint32_t cubesInLine = (uint32_t)sqrt(kNumCubes);
    const vec3 offset =
        vec3(-1.5f * sqrt(kNumCubes) + 4.0f * static_cast<float>(i % cubesInLine),
             -1.5f * sqrt(kNumCubes) + 4.0f * std::floor(static_cast<float>(i) / cubesInLine),
             0);
    perObject[i].model =
        glm::rotate(glm::translate(mat4(1.0f), offset), float(direction * currentTime_), axis_[i]);
  }

  ubPerObject_[frameIndex_]->upload(&perObject, BufferRange(sizeof(perObject)));

  // Command buffers (1-N per thread): create, submit and forget
  const std::shared_ptr<ICommandBuffer> buffer = commandQueue_->createCommandBuffer({}, nullptr);

  const igl::Viewport viewport = {0.0f, 0.0f, (float)width_, (float)height_, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, (uint32_t)width_, (uint32_t)height_};

  // This will clear the framebuffer
  auto commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  commands->bindRenderPipelineState(renderPipelineState_Mesh_);
  commands->bindViewport(viewport);
  commands->bindScissorRect(scissor);
  commands->pushDebugGroupLabel("Render Mesh", Color(1, 0, 0));
  commands->bindVertexBuffer(0, *vb0_);
  commands->bindDepthStencilState(depthStencilState_);
  commands->bindBuffer(0, ubPerFrame_[frameIndex_].get());
  commands->bindTexture(0, igl::BindTarget::kFragment, texture0_.get());
  commands->bindTexture(1, igl::BindTarget::kFragment, texture1_.get());
  commands->bindSamplerState(0, igl::BindTarget::kFragment, sampler_.get());
  commands->bindSamplerState(1, igl::BindTarget::kFragment, sampler_.get());
  // Draw 2 cubes: we use uniform buffer to update matrices
  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
  for (uint32_t i = 0; i != kNumCubes; i++) {
    commands->bindBuffer(1, ubPerObject_[frameIndex_].get(), i * sizeof(UniformsPerObject));
    commands->drawIndexed(3u * 6u * 2u);
  }
  commands->popDebugGroupLabel();
  {
    imguiSession_->beginFrame(framebufferDesc_, getPlatform().getDisplayContext().pixelsPerPoint);
    ImGui::Begin("Texture Viewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Image(ImTextureID(texture1_.get()), ImVec2(512, 512));
    ImGui::End();
    imguiSession_->drawFPS(fps_.getAverageFPS());
    imguiSession_->endFrame(getPlatform().getDevice(), *commands);
  }
  commands->endEncoding();

  buffer->present(surfaceTextures.color);

  commandQueue_->submit(*buffer);

  frameIndex_ = (frameIndex_ + 1) % kNumBufferedFrames;
}

bool TinyMeshSession::Listener::process(const CharEvent& event) {
  if (event.character == 't') {
    session.texture1_.reset();
    return true;
  }
  return false;
}

} // namespace igl::shell
