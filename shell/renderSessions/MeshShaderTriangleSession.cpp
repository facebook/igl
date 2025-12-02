/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/MeshShaderTriangleSession.h>

#include <shell/shared/renderSession/ShellParams.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>

namespace igl::shell {

namespace {

std::string getMetalTaskShaderSource() {
  return R"(
using namespace metal;

[[object]]
void taskMain(mesh_grid_properties meshGridProperties) {
  meshGridProperties.set_threadgroups_per_grid(uint3(1, 1, 1));
}
)";
}

std::string getMetalMeshShaderSource() {
  return R"(
using namespace metal;

struct VertexOut{
  float4 position [[position]];
  float4 color [[user(locn0)]];
};

struct UniformBlock {
  float4x4 mvpMatrix;
};

using TriangleMeshType = metal::mesh<VertexOut, void, 64, 64, metal::topology::triangle>;

constant float4 vertexData[3] = {{-0.6f, -0.4f, 0.0, 1.0}, {0.6f, -0.4f, 0.0, 1.0}, {0.0f, 0.6f, 0.0, 1.0}};
constant float4 colorData[3]  = {{1.0, 0.0, 0.0, 1.0}, {0.0, 1.0, 0.0, 1.0}, {0.0, 0.0, 1.0, 1.0}};

[[mesh]]
void meshMain(TriangleMeshType output, constant UniformBlock &vUniform[[buffer(1)]]) {
  output.set_primitive_count(1);

  for (int i = 0; i != 3; ++i){
    VertexOut v;
    v.position = vUniform.mvpMatrix * vertexData[i];
    v.color = colorData[i];
    output.set_vertex(i, v);
  }

  output.set_index(0, 0);
  output.set_index(1, 1);
  output.set_index(2, 2);
}
)";
}

std::string getMetalFragmentShaderSource() {
  return R"(
using namespace metal;

struct FS_IN{
  float4 color [[user(locn0)]];
};

fragment float4 fragmentMain(FS_IN in [[stage_in]]) {
  return in.color;
}
)";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device) {
  auto taskModule = igl::ShaderModuleCreator::fromStringInput(
      device,
      getMetalTaskShaderSource().c_str(),
      {.stage = igl::ShaderStage::Task, .entryPoint = "taskMain"},
      "task shader",
      nullptr);
  auto meshModule = igl::ShaderModuleCreator::fromStringInput(
      device,
      getMetalMeshShaderSource().c_str(),
      {.stage = igl::ShaderStage::Mesh, .entryPoint = "meshMain"},
      "mesh shader",
      nullptr);
  auto fragmentModule = igl::ShaderModuleCreator::fromStringInput(
      device,
      getMetalFragmentShaderSource().c_str(),
      {.stage = igl::ShaderStage::Fragment, .entryPoint = "fragmentMain"},
      "fragment shader",
      nullptr);

  Result result;
  auto shaderStages = igl::ShaderStagesCreator::fromMeshRenderModules(
      device, taskModule, meshModule, fragmentModule, &result);

  IGL_DEBUG_ASSERT(shaderStages);

  return shaderStages;
}
} // namespace

void MeshShaderTriangleSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  if (!device.hasFeature(DeviceFeatures::MeshShaders)) {
    IGL_DEBUG_ABORT("Mesh shaders are not supported.\n");
    return;
  };

  shaderStages_ = getShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(shaderStages_ != nullptr);

  BufferDesc uboDesc;
  uboDesc.type = igl::BufferDesc::BufferTypeBits::Uniform;
  uboDesc.storage = igl::ResourceStorage::Shared;
  uboDesc.length = sizeof(glm::mat4);
  ubo_ = device.createBuffer(uboDesc, nullptr);
  IGL_DEBUG_ASSERT(ubo_ != nullptr);

  // Command queue
  commandQueue_ = device.createCommandQueue({}, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  renderPass_.colorAttachments = {{
      .loadAction = LoadAction::Clear,
      .storeAction = StoreAction::Store,
      .clearColor = getPreferredClearColor(),
  }};
  renderPass_.depthAttachment = {
      .loadAction = LoadAction::Clear,
      .clearDepth = 1.0,
  };
}

void MeshShaderTriangleSession::update(SurfaceTextures surfaceTextures) noexcept {
  Result ret;
  if (!framebuffer_) {
    const FramebufferDesc framebufferDesc = {
        .colorAttachments = {{.texture = surfaceTextures.color}},
        .depthAttachment = {.texture = surfaceTextures.depth},
        .stencilAttachment =
            (surfaceTextures.depth && surfaceTextures.depth->getProperties().hasStencil())
                ? igl::FramebufferDesc::AttachmentDesc{.texture = surfaceTextures.depth}
                : igl::FramebufferDesc::AttachmentDesc{},
    };
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures);
  }

  // Graphics pipeline
  if (!pipelineState_) {
    const RenderPipelineDesc graphicsDesc = {
        .shaderStages = shaderStages_,
        .targetDesc =
            {
                .colorAttachments = {{.textureFormat =
                                          framebuffer_->getColorAttachment(0)->getFormat()}},
                .depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat(),
                .stencilAttachmentFormat = framebuffer_->getStencilAttachment()
                                               ? framebuffer_->getStencilAttachment()->getFormat()
                                               : igl::TextureFormat::Invalid,
            },
        .cullMode = igl::CullMode::Disabled,
        .frontFaceWinding = igl::WindingMode::Clockwise,
    };
    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(pipelineState_ != nullptr);
  }

  // Command Buffers
  auto buffer = commandQueue_->createCommandBuffer({}, &ret);
  IGL_DEBUG_ASSERT(ret.isOk());
  IGL_DEBUG_ASSERT(buffer != nullptr);
  auto drawableSurface = framebuffer_->getColorAttachment(0);

  frameNum_ = (++frameNum_) % 360;
  const float angle = (float)frameNum_ * M_PI / 180.0f;
  const glm::mat4 matrix = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));
  ubo_->upload(&matrix, {sizeof(matrix)});

  // Submit commands
  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(commands != nullptr);
  commands->bindRenderPipelineState(pipelineState_);
  commands->bindBuffer(1, BindTarget::kMesh, ubo_.get());
  commands->drawMeshTasks({1, 1, 1}, {1, 1, 1}, {1, 1, 1});
  commands->endEncoding();

  IGL_DEBUG_ASSERT(buffer != nullptr);
  if (shellParams().shouldPresent) {
    buffer->present(drawableSurface);
  }

  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);
  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

} // namespace igl::shell
