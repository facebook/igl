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
      void main0(mesh_grid_properties meshGridProperties){
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
    void main0(TriangleMeshType output, constant UniformBlock &vUniform[[buffer(1)]]) {
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

    fragment float4 main0(FS_IN in [[stage_in]]){
        return in.color;
    }
)";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device) {
    igl::ShaderModuleInfo info;
    info.entryPoint = "main0"; //Metal object function cannot be called 'main'.
    
    info.stage = igl::ShaderStage::Task;
    auto taskModule = igl::ShaderModuleCreator::fromStringInput(device, getMetalTaskShaderSource().c_str(), info, "task shader", NULL);
    
    info.stage = igl::ShaderStage::Mesh;
    auto meshModule = igl::ShaderModuleCreator::fromStringInput(device, getMetalMeshShaderSource().c_str(), info, "mesh shader", NULL);
    
    info.stage = igl::ShaderStage::Fragment;
    auto fragmentModule = igl::ShaderModuleCreator::fromStringInput(device, getMetalFragmentShaderSource().c_str(), info, "fragment shader", NULL);
    
    igl::Result result;
    auto shader_stages = igl::ShaderStagesCreator::fromMeshRenderModules(device, taskModule, meshModule, fragmentModule, &result);

    IGL_DEBUG_ASSERT(shader_stages);
    
    return shader_stages;
}
} // namespace

void MeshShaderTriangleSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  shaderStages_ = getShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(shaderStages_ != nullptr);
    
  BufferDesc uboDesc;
  uboDesc.type = igl::BufferDesc::BufferTypeBits::Uniform;
  uboDesc.storage = igl::ResourceStorage::Shared;
  uboDesc.length = sizeof(glm::mat4);
  ubo_ = device.createBuffer(uboDesc, nullptr);
  IGL_DEBUG_ASSERT(ubo_ != nullptr);

  // Command queue
  const CommandQueueDesc desc{};
  commandQueue_ = device.createCommandQueue(desc, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPreferredClearColor();
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
}

void MeshShaderTriangleSession::update(SurfaceTextures surfaceTextures) noexcept {
  Result ret;
  if (framebuffer_ == nullptr) {
    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
    if (surfaceTextures.depth && surfaceTextures.depth->getProperties().hasStencil()) {
      framebufferDesc.stencilAttachment.texture = surfaceTextures.depth;
    }
    IGL_DEBUG_ASSERT(ret.isOk());
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures);
  }

  // Graphics pipeline
  if (pipelineState_ == nullptr) {
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getFormat();
    graphicsDesc.targetDesc.depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat();
    graphicsDesc.targetDesc.stencilAttachmentFormat =
        framebuffer_->getStencilAttachment() ? framebuffer_->getStencilAttachment()->getFormat()
                                             : igl::TextureFormat::Invalid;
    graphicsDesc.cullMode = igl::CullMode::Disabled;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;

    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
    IGL_DEBUG_ASSERT(pipelineState_ != nullptr);
  }

  // Command Buffers
  const CommandBufferDesc cbDesc;
  auto buffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);
  IGL_DEBUG_ASSERT(buffer != nullptr);
  auto drawableSurface = framebuffer_->getColorAttachment(0);
    
  frameNum_ = (++frameNum_) % 360;
  float angle = (float)frameNum_ * M_PI / 180.0f;
  glm::mat4 matrix = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));
  ubo_->upload(&matrix, {sizeof(matrix)});

  // Submit commands
  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(commands != nullptr);
  if (commands) {
    commands->bindRenderPipelineState(pipelineState_);
    commands->bindBuffer(1, BindTarget::kMesh, ubo_.get());
    commands->drawMesh(Dimensions(1,1,1), Dimensions(1,1,1), Dimensions(1,1,1));

    commands->endEncoding();
  }

  IGL_DEBUG_ASSERT(buffer != nullptr);
  if (shellParams().shouldPresent) {
    buffer->present(drawableSurface);
  }

  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);
  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

} // namespace igl::shell
