/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "EngineTestSession.h"
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <shell/engine/graphics/Material.h>
#include <igl/ShaderCreator.h>
#include <igl/NameHandle.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <IGLU/texture_accessor/TextureAccessorFactory.h>
#include <stb/stb_image_write.h>

namespace igl::shell {

namespace {
struct VertexPosColor {
  glm::vec3 position;
  glm::vec4 color;
};

std::string getMetalShaderSource() {
  return R"(
      using namespace metal;

      typedef struct {
        float3 position [[attribute(0)]];
        float3 normal [[attribute(1)]];
        float2 texCoord [[attribute(2)]];
        float3 tangent [[attribute(3)]];
      } VertexIn;

      typedef struct {
        float4 position [[position]];
        float3 normal;
        float2 texCoord;
      } VertexOut;

      typedef struct {
        float4x4 mvpMatrix;
      } Uniforms;

      vertex VertexOut vertexShader(
          VertexIn in [[stage_in]],
          constant Uniforms & uniforms [[buffer(1)]]) {
        VertexOut out;
        out.position = uniforms.mvpMatrix * float4(in.position, 1.0);
        out.normal = in.normal;
        out.texCoord = in.texCoord;
        return out;
      }

      fragment float4 fragmentShader(
          VertexOut IN [[stage_in]],
          texture2d<float> baseColorTexture [[texture(0)]],
          sampler baseColorSampler [[sampler(0)]]) {
          float4 baseColor = baseColorTexture.sample(baseColorSampler, IN.texCoord);
          float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
          float diff = max(dot(normalize(IN.normal), lightDir), 0.3);
          return float4(baseColor.rgb * diff, baseColor.a);
      }
  )";
}

std::string getOpenGLVertexShaderSource() {
  return R"(#version 100
      precision highp float;
      attribute vec3 position;
      attribute vec3 normal;
      attribute vec2 texCoord;

      varying vec3 vNormal;

      void main() {
        gl_Position = vec4(position, 1.0);
        vNormal = normal;
      })";
}

std::string getOpenGLFragmentShaderSource() {
  return R"(#version 100
      precision highp float;

      varying vec3 vNormal;

      void main() {
        vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
        float diff = max(dot(normalize(vNormal), lightDir), 0.3);
        gl_FragColor = vec4(diff, diff, diff, 1.0);
      })";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device) {
  switch (device.getBackendType()) {
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
  default:
    return nullptr;
  }
}
} // namespace

EngineTestSession::EngineTestSession(std::shared_ptr<Platform> platform)
    : RenderSession(std::move(platform)) {}

std::shared_ptr<engine::Mesh> EngineTestSession::createCubeMesh(igl::IDevice& device, float size) {
  auto mesh = std::make_shared<engine::Mesh>();

  // Cube vertices with normals and UVs
  float s = size / 2.0f;
  engine::Vertex vertices[] = {
      // Front face (Z+)
      {{-s, -s,  s}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{ s, -s,  s}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{ s,  s,  s}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
      {{-s,  s,  s}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

      // Back face (Z-)
      {{ s, -s, -s}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
      {{-s, -s, -s}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
      {{-s,  s, -s}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
      {{ s,  s, -s}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

      // Right face (X+)
      {{ s, -s,  s}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
      {{ s, -s, -s}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
      {{ s,  s, -s}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
      {{ s,  s,  s}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

      // Left face (X-)
      {{-s, -s, -s}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
      {{-s, -s,  s}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
      {{-s,  s,  s}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
      {{-s,  s, -s}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

      // Top face (Y+)
      {{-s,  s,  s}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{ s,  s,  s}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{ s,  s, -s}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
      {{-s,  s, -s}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

      // Bottom face (Y-)
      {{-s, -s, -s}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{ s, -s, -s}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{ s, -s,  s}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
      {{-s, -s,  s}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
  };

  uint32_t indices[] = {
      0, 1, 2,  0, 2, 3,    // Front
      4, 5, 6,  4, 6, 7,    // Back
      8, 9, 10, 8, 10, 11,  // Right
      12, 13, 14, 12, 14, 15, // Left
      16, 17, 18, 16, 18, 19, // Top
      20, 21, 22, 20, 22, 23  // Bottom
  };

  std::vector<engine::Vertex> vertexVec(vertices, vertices + 24);
  std::vector<uint32_t> indexVec(indices, indices + 36);

  mesh->setVertices(vertexVec);
  mesh->setIndices(indexVec);
  mesh->calculateBounds();

  BufferDesc vbDesc(BufferDesc::BufferTypeBits::Vertex, vertices, sizeof(vertices));
  auto vb = device.createBuffer(vbDesc, nullptr);
  mesh->setVertexBuffer(std::shared_ptr<IBuffer>(std::move(vb)));

  BufferDesc ibDesc(BufferDesc::BufferTypeBits::Index, indices, sizeof(indices));
  auto ib = device.createBuffer(ibDesc, nullptr);
  mesh->setIndexBuffer(std::shared_ptr<IBuffer>(std::move(ib)));

  return mesh;
}

void EngineTestSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  // Create a combined scene with glTF models and procedural geometry
  model_ = std::make_unique<engine::GLTFModel>();

  // Load test cube glTF (red cube with simple material)
  auto cubeModel = engine::GLTFLoader::loadFromFile(device, "/Users/alexeymedvedev/Desktop/sources/igl/test_cube.gltf");

  if (cubeModel) {
    // Create 2 instances of the glTF cube at different positions
    auto cubeNode1 = std::make_shared<engine::GLTFNode>();
    cubeNode1->name = "glTF Cube 1";
    cubeNode1->transform = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 0.0f, 0.0f));
    cubeNode1->children = cubeModel->rootNodes;
    model_->rootNodes.push_back(cubeNode1);

    auto cubeNode2 = std::make_shared<engine::GLTFNode>();
    cubeNode2->name = "glTF Cube 2";
    cubeNode2->transform = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f));
    cubeNode2->children = cubeModel->rootNodes;
    model_->rootNodes.push_back(cubeNode2);

    // Copy resources from the loaded model
    model_->meshes.insert(model_->meshes.end(), cubeModel->meshes.begin(), cubeModel->meshes.end());
    model_->materials.insert(model_->materials.end(), cubeModel->materials.begin(), cubeModel->materials.end());
  }

  // Create procedural cubes with different colors
  auto greenCubeMesh = createCubeMesh(device, 1.0f);
  auto greenMaterial = std::make_shared<engine::Material>();
  greenMaterial->setBaseColor(glm::vec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green
  greenCubeMesh->setMaterial(greenMaterial);
  model_->meshes.push_back(greenCubeMesh);
  model_->materials.push_back(greenMaterial);

  auto greenNode = std::make_shared<engine::GLTFNode>();
  greenNode->name = "Green Cube";
  greenNode->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
  greenNode->mesh = greenCubeMesh;
  model_->rootNodes.push_back(greenNode);

  auto blueCubeMesh = createCubeMesh(device, 0.8f);
  auto blueMaterial = std::make_shared<engine::Material>();
  blueMaterial->setBaseColor(glm::vec4(0.0f, 0.0f, 0.8f, 1.0f)); // Blue
  blueCubeMesh->setMaterial(blueMaterial);
  model_->meshes.push_back(blueCubeMesh);
  model_->materials.push_back(blueMaterial);

  auto blueNode = std::make_shared<engine::GLTFNode>();
  blueNode->name = "Blue Cube";
  blueNode->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));
  blueNode->mesh = blueCubeMesh;
  model_->rootNodes.push_back(blueNode);

  // Create vertex input state
  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 4;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[0].offset = offsetof(engine::Vertex, position);
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].name = "position";
  inputDesc.attributes[0].location = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[1].offset = offsetof(engine::Vertex, normal);
  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].name = "normal";
  inputDesc.attributes[1].location = 1;
  inputDesc.attributes[2].format = VertexAttributeFormat::Float2;
  inputDesc.attributes[2].offset = offsetof(engine::Vertex, texCoord);
  inputDesc.attributes[2].bufferIndex = 0;
  inputDesc.attributes[2].name = "texCoord";
  inputDesc.attributes[2].location = 2;
  inputDesc.attributes[3].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[3].offset = offsetof(engine::Vertex, tangent);
  inputDesc.attributes[3].bufferIndex = 0;
  inputDesc.attributes[3].name = "tangent";
  inputDesc.attributes[3].location = 3;
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(engine::Vertex);

  vertexInput_ = device.createVertexInputState(inputDesc, nullptr);

  // Create shaders
  shaderStages_ = getShaderStagesForBackend(device);

  // Create command queue
  const CommandQueueDesc queueDesc{};
  commandQueue_ = device.createCommandQueue(queueDesc, nullptr);

  if (!commandQueue_) {
    IGL_LOG_ERROR("Failed to create command queue\n");
  }

  // Create uniform buffer for MVP matrices (ring buffer for multiple draw calls)
  BufferDesc ubDesc;
  ubDesc.type = BufferDesc::BufferTypeBits::Uniform;
  ubDesc.data = nullptr;
  ubDesc.length = kMaxDrawCalls * kUniformAlignment;  // Large enough for many draw calls
  ubDesc.storage = ResourceStorage::Shared;
  uniformBuffer_ = device.createBuffer(ubDesc, nullptr);

  // Create sampler state for textures
  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  samplerDesc.addressModeU = SamplerAddressMode::Repeat;
  samplerDesc.addressModeV = SamplerAddressMode::Repeat;
  sampler_ = device.createSamplerState(samplerDesc, nullptr);

  // Create depth/stencil state for z-buffering
  DepthStencilStateDesc depthStencilDesc;
  depthStencilDesc.isDepthWriteEnabled = true;
  depthStencilDesc.compareFunction = CompareFunction::Less;
  depthStencilState_ = device.createDepthStencilState(depthStencilDesc, nullptr);

  // Create default white texture for meshes without textures
  TextureDesc texDesc;
  texDesc.type = TextureType::TwoD;
  texDesc.width = 1;
  texDesc.height = 1;
  texDesc.format = TextureFormat::RGBA_UNorm8;
  texDesc.usage = TextureDesc::TextureUsageBits::Sampled;
  texDesc.debugName = "DefaultWhiteTexture";
  defaultTexture_ = device.createTexture(texDesc, nullptr);

  if (defaultTexture_) {
    uint32_t whitePixel = 0xFFFFFFFF;  // White color (RGBA)
    defaultTexture_->upload(TextureRangeDesc::new2D(0, 0, 1, 1), &whitePixel);
  }
}

void EngineTestSession::renderNode(const std::shared_ptr<engine::GLTFNode>& node,
                                    const glm::mat4& parentTransform,
                                    IRenderCommandEncoder* commands) {
  // Calculate world transform for this node
  glm::mat4 worldTransform = node->getWorldTransform(parentTransform);

  // Render mesh if attached to this node
  if (node->mesh) {
    auto mesh = node->mesh;

    // Update MVP matrix with node's transform using unique offset
    glm::mat4 mvp = worldTransform;
    size_t offset = currentUniformOffset_ * kUniformAlignment;
    uniformBuffer_->upload(&mvp, BufferRange(sizeof(glm::mat4), offset));
    commands->bindBuffer(1, uniformBuffer_.get(), offset);
    currentUniformOffset_++;  // Increment for next draw call

    // Bind vertex and index buffers
    commands->bindVertexBuffer(0, *mesh->getVertexBuffer());
    commands->bindIndexBuffer(*mesh->getIndexBuffer(), IndexFormat::UInt32);

    // Bind material textures or default texture
    auto material = mesh->getMaterial();
    ITexture* textureToUse = defaultTexture_.get();

    if (material) {
      auto baseColorTex = material->getTexture("baseColor");
      if (baseColorTex) {
        textureToUse = baseColorTex.get();
      }
    }

    // Always bind a texture (either material texture or default white)
    if (textureToUse) {
      commands->bindTexture(0, igl::BindTarget::kFragment, textureToUse);
      commands->bindSamplerState(0, igl::BindTarget::kFragment, sampler_.get());
    }

    // Draw the mesh
    commands->drawIndexed(static_cast<uint32_t>(mesh->getIndices().size()));
  }

  // Recursively render children
  for (const auto& child : node->children) {
    renderNode(child, worldTransform, commands);
  }
}

void EngineTestSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  // Set the framebuffer from surface textures
  if (framebuffer_ == nullptr) {
    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    if (surfaceTextures.depth) {
      framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
      // If the depth texture has stencil, set it as stencil attachment too
      auto depthFormat = surfaceTextures.depth->getFormat();
      if (depthFormat == TextureFormat::S8_UInt_Z32_UNorm ||
          depthFormat == TextureFormat::S8_UInt_Z24_UNorm) {
        framebufferDesc.stencilAttachment.texture = surfaceTextures.depth;
      }
    }
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures);
  }

  // Create pipeline once we have framebuffer
  if (pipelineState_ == nullptr) {
    RenderPipelineDesc pipelineDesc;
    pipelineDesc.vertexInputState = vertexInput_;
    pipelineDesc.shaderStages = shaderStages_;
    pipelineDesc.targetDesc.colorAttachments.resize(1);
    pipelineDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getFormat();

    if (framebuffer_->getDepthAttachment()) {
      pipelineDesc.targetDesc.depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat();
    }
    if (framebuffer_->getStencilAttachment()) {
      pipelineDesc.targetDesc.stencilAttachmentFormat = framebuffer_->getStencilAttachment()->getFormat();
    }

    pipelineDesc.cullMode = igl::CullMode::Back;  // Enable back-face culling
    pipelineDesc.frontFaceWinding = igl::WindingMode::CounterClockwise;

    pipelineState_ = getPlatform().getDevice().createRenderPipeline(pipelineDesc, nullptr);
  }

  if (!pipelineState_ || !framebuffer_) {
    return;
  }

  // Update rotation angle
  rotationAngle_ += 0.01f;  // Rotate ~0.57 degrees per frame
  if (rotationAngle_ > glm::two_pi<float>()) {
    rotationAngle_ -= glm::two_pi<float>();
  }

  // Calculate MVP matrix
  auto aspect = (float)framebuffer_->getColorAttachment(0)->getDimensions().width /
                (float)framebuffer_->getColorAttachment(0)->getDimensions().height;

  glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
  glm::mat4 view = glm::lookAt(
      glm::vec3(4.0f, 3.0f, 6.0f),  // Camera position (moved back to see all cubes)
      glm::vec3(0.0f, 0.0f, 0.0f),  // Look at origin
      glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
  );

  // Apply rotation around Y axis for the entire scene
  glm::mat4 model = glm::rotate(glm::mat4(1.0f), rotationAngle_, glm::vec3(0.0f, 1.0f, 0.0f));

  glm::mat4 viewProjection = projection * view;

  // Create command buffer
  CommandBufferDesc cbDesc;
  auto commandBuffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);

  // Create render pass
  RenderPassDesc renderPass;
  renderPass.colorAttachments.resize(1);
  renderPass.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

  // Enable depth testing
  renderPass.depthAttachment.loadAction = LoadAction::Clear;
  renderPass.depthAttachment.storeAction = StoreAction::DontCare;
  renderPass.depthAttachment.clearDepth = 1.0;

  // Begin render pass
  auto commands = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer_);

  // Bind pipeline state (shared for all meshes)
  commands->bindRenderPipelineState(pipelineState_);
  commands->bindDepthStencilState(depthStencilState_);

  // Reset uniform buffer offset for this frame
  currentUniformOffset_ = 0;

  // Render scene graph
  if (model_ && !model_->rootNodes.empty()) {
    // Traverse and render all nodes in the scene graph
    // Apply global rotation to all objects
    for (const auto& rootNode : model_->rootNodes) {
      renderNode(rootNode, viewProjection * model, commands.get());
    }
  } else if (model_ && !model_->meshes.empty()) {
    // Fallback: render first mesh directly if no scene graph
    auto mesh = model_->meshes[0];
    if (mesh) {
      auto mvp = viewProjection * model;
      uniformBuffer_->upload(&mvp, BufferRange(sizeof(glm::mat4), 0));
      commands->bindBuffer(1, uniformBuffer_.get());
      commands->bindVertexBuffer(0, *mesh->getVertexBuffer());

      auto material = mesh->getMaterial();
      if (material) {
        auto baseColorTex = material->getTexture("baseColor");
        if (baseColorTex) {
          commands->bindTexture(0, igl::BindTarget::kFragment, baseColorTex.get());
          commands->bindSamplerState(0, igl::BindTarget::kFragment, sampler_.get());
        }
      }

      commands->bindIndexBuffer(*mesh->getIndexBuffer(), IndexFormat::UInt32);
      commands->drawIndexed(static_cast<uint32_t>(mesh->getIndices().size()));
    }
  }

  commands->endEncoding();

  // Present and submit
  auto drawableSurface = framebuffer_->getColorAttachment(0);
  if (shellParams().shouldPresent) {
    commandBuffer->present(drawableSurface);
  }

  commandQueue_->submit(*commandBuffer);

  frameCount_++;
  RenderSession::update(surfaceTextures);
}

} // namespace igl::shell
