/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/SlangShowcaseSession.h>

#include <IGLU/slang/SlangProgram.h>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>
#include <shell/renderSessions/slang/generated/ShowcaseInverseSlang.h>
#include <shell/renderSessions/slang/generated/ShowcaseMaterialsPassSlang.h>
#include <shell/renderSessions/slang/generated/ShowcasePostSlang.h>
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <igl/Timer.h>

namespace igl::shell {

namespace {

constexpr uint32_t kSegments = 48;
constexpr uint32_t kRings = 24;
constexpr uint32_t kMaterialTypes = 4;
constexpr uint32_t kSpheresPerType = 4;
constexpr uint32_t kSpheres = kMaterialTypes * kSpheresPerType;
constexpr uint64_t kFramesPerMode = 240;
constexpr uint32_t kStepsPerFrame = 8;
constexpr float kEyeDistance = 16.0f;
constexpr float kPi = 3.14159265f;

constexpr std::array<const char*, kMaterialTypes> kSpecializedEntryPoints = {"fragmentMetalRough",
                                                                             "fragmentClearcoat",
                                                                             "fragmentSheen",
                                                                             "fragmentAnisotropic"};
constexpr std::array<const char*, kMaterialTypes> kHalfEntryPoints = {"fragmentMetalRoughHalf",
                                                                      "fragmentClearcoatHalf",
                                                                      "fragmentSheenHalf",
                                                                      "fragmentAnisotropicHalf"};
constexpr std::array<const char*, 3> kPostEntryPoints = {"postNeutral", "postFilm", "postNoir"};
constexpr std::array<const char*, 3> kPostChainNames = {
    "Neutral: exposure, ACES",
    "Film: chromatic aberration, exposure, ACES, vignette, grain",
    "Noir: exposure, ACES, monochrome, posterize, tint, vignette"};

struct Vertex {
  std::array<float, 3> position;
  std::array<float, 3> normal;
  std::array<float, 4> tangent;
};

// Mirrors ShowcaseMaterials.slang, ShowcaseMaterialsPass.slang and ShowcaseInverse.slang.
struct MaterialData {
  std::array<float, 4> base;
  std::array<float, 4> extra;
};

struct SphereInstance {
  std::array<float, 4> placement;
  MaterialData material;
  std::array<uint32_t, 4> type;
};
static_assert(sizeof(SphereInstance) == 64);

constexpr uint32_t kFitSize = 128;
constexpr size_t kAdamStateSize = 80;
constexpr size_t kPixelGradientSize = 48;

struct Lighting {
  std::array<glm::vec4, 3> lights;
  std::array<glm::vec4, 3> lightColors;
  glm::vec4 sky;
  glm::vec4 ground;
};

struct SceneUniforms {
  glm::mat4 viewProjection;
  glm::vec4 camera;
  Lighting lighting;
  glm::vec4 display;
};

struct InverseUniforms {
  glm::mat4 inverseViewProjection;
  glm::vec4 camera;
  Lighting lighting;
};

// Mirrors ShowcasePost.slang.
struct PostUniforms {
  glm::vec4 params;
};

glm::mat4 viewProjection(float aspect) {
  return glm::perspectiveLH_ZO(glm::radians(38.0f), aspect, 0.1f, 100.0f) *
         glm::lookAtLH(
             glm::vec3(0.0f, 0.0f, -kEyeDistance), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

// Three lights circling the spheres.
Lighting lighting(float seconds) {
  const std::array<glm::vec3, 3> colors = {
      glm::vec3(1.0f, 0.85f, 0.7f), glm::vec3(0.6f, 0.75f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f)};
  Lighting result{.sky = glm::vec4(0.25f, 0.35f, 0.55f, 0.0f),
                  .ground = glm::vec4(0.08f, 0.07f, 0.06f, 0.0f)};
  for (size_t i = 0; i < 3; ++i) {
    const float angle = 0.5f * seconds + 2.0944f * static_cast<float>(i);
    result.lights[i] = glm::vec4(7.0f * std::cos(angle),
                                 3.0f + 1.5f * static_cast<float>(i),
                                 -6.0f + 4.0f * std::sin(angle),
                                 70.0f);
    result.lightColors[i] = glm::vec4(colors[i], 0.0f);
  }
  return result;
}

// A unit sphere with tangents along the parallels, for anisotropic highlights.
void buildSphere(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
  for (uint32_t ring = 0; ring <= kRings; ++ring) {
    const float theta = kPi * static_cast<float>(ring) / kRings;
    for (uint32_t segment = 0; segment <= kSegments; ++segment) {
      const float phi = 2.0f * kPi * static_cast<float>(segment) / kSegments;
      const std::array<float, 3> p = {
          std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
      vertices.push_back(
          {.position = p, .normal = p, .tangent = {-std::sin(phi), 0.0f, std::cos(phi), 1.0f}});
    }
  }
  for (uint32_t ring = 0; ring < kRings; ++ring) {
    for (uint32_t segment = 0; segment < kSegments; ++segment) {
      const auto a = static_cast<uint16_t>(ring * (kSegments + 1) + segment);
      const auto b = static_cast<uint16_t>(a + kSegments + 1);
      indices.insert(indices.end(),
                     {a,
                      b,
                      static_cast<uint16_t>(a + 1),
                      static_cast<uint16_t>(a + 1),
                      b,
                      static_cast<uint16_t>(b + 1)});
    }
  }
}

// Columns: gold, clear-coated paint, velvet, brushed aluminum. Rows: roughness.
std::vector<SphereInstance> buildSpheres() {
  const std::array<std::array<float, 3>, kMaterialTypes> colors = {
      {{1.0f, 0.78f, 0.34f}, {0.65f, 0.04f, 0.03f}, {0.08f, 0.06f, 0.35f}, {0.92f, 0.92f, 0.94f}}};
  const std::array<std::array<float, 4>, kMaterialTypes> extras = {{{1.0f, 0.0f, 0.0f, 0.0f},
                                                                    {0.0f, 1.0f, 0.08f, 0.0f},
                                                                    {0.9f, 0.85f, 1.0f, 0.35f},
                                                                    {1.0f, 0.85f, 0.0f, 0.0f}}};
  const std::array<float, kSpheresPerType> roughness = {0.15f, 0.35f, 0.6f, 0.85f};
  std::vector<SphereInstance> spheres;
  for (uint32_t type = 0; type < kMaterialTypes; ++type) {
    for (uint32_t row = 0; row < kSpheresPerType; ++row) {
      const float x = (static_cast<float>(type) - 1.5f) * 2.5f;
      const float y = (1.5f - static_cast<float>(row)) * 2.5f;
      spheres.push_back(
          {.placement = {x, y, 0.0f, 1.0f},
           .material = {.base = {colors[type][0], colors[type][1], colors[type][2], roughness[row]},
                        .extra = extras[type]},
           .type = {type, 0, 0, 0}});
    }
  }
  return spheres;
}

std::shared_ptr<IComputePipelineState> createComputePipeline(IDevice& device,
                                                             const iglu::slang::Program& program,
                                                             const char* entryPoint) {
  Result ret;
  std::shared_ptr<IShaderStages> stages =
      iglu::slang::createComputeStages(device, program, entryPoint, &ret);
  std::shared_ptr<IComputePipelineState> pipeline;
  if (ret.isOk()) {
    pipeline =
        device.createComputePipeline(ComputePipelineDesc{.shaderStages = std::move(stages)}, &ret);
  }
  if (!ret.isOk()) {
    IGL_LOG_INFO("SlangShowcaseSession: %s: %s\n", entryPoint, ret.message.c_str());
  }
  return pipeline;
}

Dimensions workgroupSize(const iglu::slang::Program& program, const char* entryPoint) {
  const iglu::slang::EntryPoint* entry = iglu::slang::findEntryPoint(program, entryPoint);
  return entry != nullptr ? iglu::slang::workgroupSize(*entry) : Dimensions(1, 1, 1);
}

const char* modeName(SlangShowcaseSession::ShadingMode mode) {
  switch (mode) {
  case SlangShowcaseSession::ShadingMode::Specialized:
    return "a pipeline per material (static specialization)";
  case SlangShowcaseSession::ShadingMode::Dynamic:
    return "one pipeline, dynamic dispatch on a type id";
  case SlangShowcaseSession::ShadingMode::Half:
    return "a pipeline per material, every lobe in half";
  }
  return "";
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
void SlangShowcaseSession::initialize() noexcept {
  IDevice& device = getPlatform().getDevice();
  commandQueue_ = device.createCommandQueue({}, nullptr);
  supported_ = device.getBackendType() == BackendType::Metal ||
               device.getBackendType() == BackendType::Vulkan;
  imgui_ = std::make_unique<iglu::imgui::Session>(device, getPlatform().getInputDispatcher());
  if (!supported_) {
    return;
  }

  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;
  buildSphere(vertices, indices);
  indexCount_ = indices.size();
  vertexBuffer_ = device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Vertex,
                                                 .data = vertices.data(),
                                                 .length = vertices.size() * sizeof(Vertex)},
                                      nullptr);
  indexBuffer_ = device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Index,
                                                .data = indices.data(),
                                                .length = indices.size() * sizeof(uint16_t)},
                                     nullptr);
  const std::vector<SphereInstance> spheres = buildSpheres();
  spheres_ = device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Storage,
                                            .data = spheres.data(),
                                            .length = spheres.size() * sizeof(SphereInstance),
                                            .storageStride = sizeof(SphereInstance)},
                                 nullptr);
  sceneUniforms_ =
      device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                     .length = sizeof(SceneUniforms),
                                     .storage = ResourceStorage::Shared,
                                     .hint = BufferDesc::BufferAPIHintBits::UniformBlock},
                          nullptr);
  depthState_ = device.createDepthStencilState(
      {.compareFunction = CompareFunction::Less, .isDepthWriteEnabled = true}, nullptr);

  const iglu::slang::Program& program = slang::kShowcaseMaterialsPassProgram;
  VertexInputStateDesc inputDesc{.numAttributes = 3, .numInputBindings = 1};
  const std::array<VertexAttributeFormat, 3> formats = {
      VertexAttributeFormat::Float3, VertexAttributeFormat::Float3, VertexAttributeFormat::Float4};
  const std::array<size_t, 3> offsets = {
      offsetof(Vertex, position), offsetof(Vertex, normal), offsetof(Vertex, tangent)};
  for (size_t i = 0; i < 3; ++i) {
    inputDesc.attributes[i] = {.bufferIndex = 0,
                               .format = formats[i],
                               .offset = offsets[i],
                               .name = program.vertexAttributes[i].name,
                               .location = program.vertexAttributes[i].location};
  }
  inputDesc.inputBindings[0] = {.stride = sizeof(Vertex)};
  vertexInput_ = device.createVertexInputState(inputDesc, nullptr);

  createInverseResources(device);
  postSampler_ = device.createSamplerState(SamplerStateDesc::newLinear(), nullptr);
  postUniforms_ =
      device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                     .length = sizeof(PostUniforms),
                                     .storage = ResourceStorage::Shared,
                                     .hint = BufferDesc::BufferAPIHintBits::UniformBlock},
                          nullptr);

  Result ret;
  timer_ = device.createTimer(&ret);
}

void SlangShowcaseSession::createInverseResources(IDevice& device) {
  fitted_ = device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Storage,
                                           .length = kSpheres * sizeof(MaterialData),
                                           .storage = ResourceStorage::Shared,
                                           .storageStride = sizeof(MaterialData)},
                                nullptr);
  adam_ = device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Storage,
                                         .length = kSpheres * kAdamStateSize,
                                         .storage = ResourceStorage::Shared,
                                         .storageStride = kAdamStateSize},
                              nullptr);
  pixelGradients_ = device.createBuffer(
      BufferDesc{.type = BufferDesc::BufferTypeBits::Storage,
                 .length = static_cast<size_t>(kFitSize) * kFitSize * kPixelGradientSize,
                 .storageStride = kPixelGradientSize},
      nullptr);
  // The fit renders square images from the scene's camera.
  const InverseUniforms uniforms{
      .inverseViewProjection = glm::inverse(viewProjection(1.0f)),
      .camera = glm::vec4(0.0f, 0.0f, -kEyeDistance, 1.0f),
      .lighting = lighting(0.0f),
  };
  inverseUniforms_ =
      device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                     .data = &uniforms,
                                     .length = sizeof(uniforms),
                                     .hint = BufferDesc::BufferAPIHintBits::UniformBlock},
                          nullptr);
  restartFit();
  if (device.hasFeature(DeviceFeatures::Compute)) {
    gradientsPipeline_ =
        createComputePipeline(device, slang::kShowcaseInverseProgram, "inverseGradients");
    adamPipeline_ = createComputePipeline(device, slang::kShowcaseInverseProgram, "inverseAdam");
  }
}

// Every fit starts from the same neutral guess.
void SlangShowcaseSession::restartFit() {
  const std::vector<MaterialData> fit(
      kSpheres, {.base = {0.5f, 0.5f, 0.5f, 0.5f}, .extra = {0.5f, 0.5f, 0.5f, 0.5f}});
  fitted_->upload(fit.data(), BufferRange(fit.size() * sizeof(MaterialData)));
  const std::vector<uint8_t> zeros(kSpheres * kAdamStateSize, 0);
  adam_->upload(zeros.data(), BufferRange(zeros.size()));
  steps_ = 0;
}

void SlangShowcaseSession::encodeInverseStep(ICommandBuffer& commandBuffer) {
  const auto dispatch = [this,
                         &commandBuffer](const std::shared_ptr<IComputePipelineState>& pipeline,
                                         const char* entryPoint,
                                         const Dimensions& groups,
                                         const Dependencies& dependencies) {
    auto encoder = commandBuffer.createComputeCommandEncoder();
    encoder->bindComputePipelineState(pipeline);
    const std::array<IBuffer*, 5> buffers = {
        spheres_.get(), fitted_.get(), adam_.get(), pixelGradients_.get(), inverseUniforms_.get()};
    for (uint32_t index = 0; index < buffers.size(); ++index) {
      encoder->bindBuffer(index, buffers[index]);
    }
    encoder->dispatchThreadGroups(
        groups, workgroupSize(slang::kShowcaseInverseProgram, entryPoint), dependencies);
    encoder->endEncoding();
  };
  dispatch(gradientsPipeline_,
           "inverseGradients",
           Dimensions(kFitSize / 8, kFitSize / 8, 1),
           {.buffers = {fitted_.get(), adam_.get()}});
  dispatch(adamPipeline_,
           "inverseAdam",
           Dimensions(kSpheres, 1, 1),
           {.buffers = {pixelGradients_.get()}});
  ++steps_;
}

bool SlangShowcaseSession::createPipelines(TextureFormat format) {
  IDevice& device = getPlatform().getDevice();
  const auto create = [&](const iglu::slang::Program& program,
                          const char* vertexEntryPoint,
                          const char* fragmentEntryPoint,
                          TextureFormat colorFormat,
                          const std::shared_ptr<IVertexInputState>& vertexInput)
      -> std::shared_ptr<IRenderPipelineState> {
    Result ret;
    RenderPipelineDesc desc{
        .vertexInputState = vertexInput,
        .shaderStages = iglu::slang::createRenderStages(
            device, program, vertexEntryPoint, fragmentEntryPoint, &ret),
        .targetDesc = {.colorAttachments = {{.textureFormat = colorFormat}},
                       .depthAttachmentFormat = TextureFormat::Z_UNorm32},
        .cullMode = CullMode::Disabled,
    };
    if (!ret.isOk()) {
      IGL_LOG_INFO("SlangShowcaseSession: %s\n", ret.message.c_str());
      return nullptr;
    }
    return device.createRenderPipeline(desc, nullptr);
  };
  const iglu::slang::Program& pass = slang::kShowcaseMaterialsPassProgram;
  dynamicPipeline_ = create(pass, "sphereVertex", "fragmentDynamic", format, vertexInput_);
  for (uint32_t type = 0; type < kMaterialTypes; ++type) {
    specializedPipelines_[type] =
        create(pass, "sphereVertex", kSpecializedEntryPoints[type], format, vertexInput_);
    halfPipelines_[type] =
        create(pass, "sphereVertex", kHalfEntryPoints[type], format, vertexInput_);
  }
  inversePipeline_ = create(pass, "sphereVertex", "fragmentInverse", format, vertexInput_);
  sensitivityPipeline_ = create(pass, "sphereVertex", "fragmentSensitivity", format, vertexInput_);
  radiancePipeline_ =
      create(pass, "sphereVertex", "fragmentRadiance", TextureFormat::RGBA_F16, vertexInput_);
  for (size_t chain = 0; chain < kPostEntryPoints.size(); ++chain) {
    postPipelines_[chain] =
        create(slang::kShowcasePostProgram, "postVertex", kPostEntryPoints[chain], format, nullptr);
  }
  return dynamicPipeline_ != nullptr && inversePipeline_ != nullptr &&
         sensitivityPipeline_ != nullptr && radiancePipeline_ != nullptr &&
         postPipelines_[0] != nullptr;
}

SlangShowcaseSession::ShadingMode SlangShowcaseSession::currentMode() const {
  if (hasFixedMode_) {
    return fixedMode_;
  }
  const auto mode = static_cast<ShadingMode>((frame_ / kFramesPerMode) % 3);
  // Half shading needs 16-bit float support; fall back to the float pipelines without it.
  return mode == ShadingMode::Half && !halfPipelines_[0] ? ShadingMode::Specialized : mode;
}

SlangShowcaseSession::PostChain SlangShowcaseSession::currentPostChain() const {
  return hasFixedPostChain_ ? fixedPostChain_
                            : static_cast<PostChain>((frame_ / kFramesPerMode) % 3);
}

void SlangShowcaseSession::bindSphereResources(IRenderCommandEncoder& encoder) const {
  encoder.bindDepthStencilState(depthState_);
  encoder.bindVertexBuffer(0, *vertexBuffer_);
  encoder.bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16);
  encoder.bindBuffer(1, sceneUniforms_.get());
  encoder.bindBuffer(2, spheres_.get());
  encoder.bindBuffer(3, fitted_.get());
}

void SlangShowcaseSession::drawOverlay(ShadingMode mode, PostChain chain) {
  constexpr std::array<const char*, 3> kTitles = {"Slang showcase 1/5: materials",
                                                  "Slang showcase 2/5: inverse rendering",
                                                  "Slang showcase 3/5: post effects"};
  ImGui::SetNextWindowPos(ImVec2(10, 10));
  ImGui::Begin("##SlangShowcase",
               nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoBackground);
  ImGui::TextUnformatted(kTitles[static_cast<size_t>(chapter_)]);
  if (!supported_) {
    ImGui::Text("Needs Metal or Vulkan");
    ImGui::End();
    return;
  }
  switch (chapter_) {
  case Chapter::Materials:
    ImGui::Text("Shading: %s", modeName(mode));
    break;
  case Chapter::InverseRendering:
    if (showSensitivity_) {
      ImGui::Text("d color / d roughness of the fit after %u steps", steps_);
    } else {
      ImGui::Text("Left halves: hidden parameters. Right: fit, step %u", steps_);
    }
    break;
  case Chapter::PostEffects:
    ImGui::Text("%s", kPostChainNames[static_cast<size_t>(chain)]);
    break;
  }
  if (timer_ != nullptr) {
    ImGui::Text("GPU frame: %.3f ms", gpuMilliseconds_);
  }
  ImGui::End();

  const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(10, displaySize.y - 10), ImGuiCond_Always, ImVec2(0, 1));
  ImGui::Begin("##SlangShowcaseControls",
               nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoBackground);
  int chapter = static_cast<int>(chapter_);
  ImGui::RadioButton("Materials", &chapter, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Inverse rendering", &chapter, 1);
  ImGui::SameLine();
  ImGui::RadioButton("Post effects", &chapter, 2);
  const auto previous = chapter_;
  chapter_ = static_cast<Chapter>(chapter);
  switch (previous) {
  case Chapter::Materials: {
    const std::array<const char*, 4> modes = {"Cycle", "Specialized", "Dynamic", "Half"};
    int selected = hasFixedMode_ ? static_cast<int>(fixedMode_) + 1 : 0;
    if (ImGui::Combo("Shading", &selected, modes.data(), static_cast<int>(modes.size()))) {
      hasFixedMode_ = selected > 0;
      fixedMode_ = hasFixedMode_ ? static_cast<ShadingMode>(selected - 1)
                                 : ShadingMode::Specialized;
    }
    break;
  }
  case Chapter::InverseRendering:
    ImGui::Checkbox("d/d roughness", &showSensitivity_);
    ImGui::SameLine();
    if (ImGui::Button("Restart")) {
      restartFit();
    }
    break;
  case Chapter::PostEffects: {
    const std::array<const char*, 4> chains = {"Cycle", "Neutral", "Film", "Noir"};
    int selected = hasFixedPostChain_ ? static_cast<int>(fixedPostChain_) + 1 : 0;
    if (ImGui::Combo("Chain", &selected, chains.data(), static_cast<int>(chains.size()))) {
      hasFixedPostChain_ = selected > 0;
      fixedPostChain_ = hasFixedPostChain_ ? static_cast<PostChain>(selected - 1)
                                           : PostChain::Neutral;
    }
    break;
  }
  }
  ImGui::End();
}

// NOLINTNEXTLINE(bugprone-exception-escape)
void SlangShowcaseSession::update(SurfaceTextures surfaceTextures) noexcept {
  if (!surfaceTextures.color) {
    return;
  }
  IDevice& device = getPlatform().getDevice();
  const Size size = surfaceTextures.color->getSize();
  if (!framebuffer_ || framebuffer_->getColorAttachment(0)->getSize() != size) {
    auto depth = device.createTexture(TextureDesc::new2D(TextureFormat::Z_UNorm32,
                                                         static_cast<size_t>(size.width),
                                                         static_cast<size_t>(size.height),
                                                         TextureDesc::TextureUsageBits::Attachment),
                                      nullptr);
    framebuffer_ = device.createFramebuffer(
        FramebufferDesc{.colorAttachments = {{.texture = surfaceTextures.color}},
                        .depthAttachment = {.texture = depth}},
        nullptr);
    radianceFramebuffer_ = nullptr;
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }
  if (supported_ && !dynamicPipeline_ &&
      !createPipelines(surfaceTextures.color->getProperties().format)) {
    supported_ = false;
  }

  const ShadingMode mode = currentMode();
  const PostChain chain = currentPostChain();
  const bool post = supported_ && chapter_ == Chapter::PostEffects;
  if (supported_) {
    const bool encode = !surfaceTextures.color->getProperties().isSRGB();
    const SceneUniforms scene{
        .viewProjection = viewProjection(size.width / size.height),
        .camera = glm::vec4(0.0f, 0.0f, -kEyeDistance, 1.0f),
        .lighting = lighting(static_cast<float>(frame_) / 60.0f),
        .display = glm::vec4(encode ? 1.0f : 0.0f),
    };
    sceneUniforms_->upload(&scene, BufferRange(sizeof(scene)));
    const PostUniforms postParams{
        .params = glm::vec4(static_cast<float>(frame_) / 60.0f, encode ? 1.0f : 0.0f, 0.0f, 0.0f)};
    postUniforms_->upload(&postParams, BufferRange(sizeof(postParams)));
  }
  if (post && !radianceFramebuffer_) {
    radianceTexture_ =
        device.createTexture(TextureDesc::new2D(TextureFormat::RGBA_F16,
                                                static_cast<size_t>(size.width),
                                                static_cast<size_t>(size.height),
                                                TextureDesc::TextureUsageBits::Attachment |
                                                    TextureDesc::TextureUsageBits::Sampled),
                             nullptr);
    radianceFramebuffer_ = device.createFramebuffer(
        FramebufferDesc{.colorAttachments = {{.texture = radianceTexture_}},
                        .depthAttachment = {.texture = framebuffer_->getDepthAttachment()}},
        nullptr);
  }

  auto commandBuffer =
      commandQueue_->createCommandBuffer(CommandBufferDesc{.timer = timer_}, nullptr);
  if (supported_ && chapter_ == Chapter::InverseRendering && gradientsPipeline_ && adamPipeline_) {
    for (uint32_t i = 0; i < kStepsPerFrame; ++i) {
      encodeInverseStep(*commandBuffer);
    }
  }
  const RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear,
                            .storeAction = StoreAction::Store,
                            .clearColor = {0.02f, 0.02f, 0.03f, 1.0f}}},
      .depthAttachment = {.loadAction = LoadAction::Clear, .clearDepth = 1.0f},
  };
  if (post) {
    auto radianceEncoder =
        commandBuffer->createRenderCommandEncoder(renderPass, radianceFramebuffer_);
    bindSphereResources(*radianceEncoder);
    radianceEncoder->bindRenderPipelineState(radiancePipeline_);
    radianceEncoder->drawIndexed(indexCount_, kSpheres);
    radianceEncoder->endEncoding();
  }

  Dependencies dependencies{.buffers = {fitted_.get()}};
  if (post) {
    dependencies.textures[0] = radianceTexture_.get();
  }
  auto encoder = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer_, dependencies);
  if (post) {
    const auto& pipeline = postPipelines_[static_cast<size_t>(chain)];
    encoder->bindRenderPipelineState(pipeline ? pipeline : postPipelines_[0]);
    encoder->bindTexture(0, BindTarget::kFragment, radianceTexture_.get());
    encoder->bindSamplerState(0, BindTarget::kFragment, postSampler_.get());
    encoder->bindBuffer(1, postUniforms_.get());
    encoder->draw(3);
  } else if (supported_) {
    bindSphereResources(*encoder);
    if (chapter_ == Chapter::InverseRendering) {
      encoder->bindRenderPipelineState(showSensitivity_ ? sensitivityPipeline_ : inversePipeline_);
      encoder->drawIndexed(indexCount_, kSpheres);
    } else if (mode == ShadingMode::Dynamic) {
      encoder->bindRenderPipelineState(dynamicPipeline_);
      encoder->drawIndexed(indexCount_, kSpheres);
    } else {
      const auto& pipelines = mode == ShadingMode::Half ? halfPipelines_ : specializedPipelines_;
      for (uint32_t type = 0; type < kMaterialTypes; ++type) {
        encoder->bindRenderPipelineState(pipelines[type]);
        encoder->drawIndexed(indexCount_, kSpheresPerType, 0, 0, type * kSpheresPerType);
      }
    }
  }

  imgui_->beginFrame(
      FramebufferDesc{.colorAttachments = {{.texture = surfaceTextures.color}},
                      .depthAttachment = {.texture = framebuffer_->getDepthAttachment()}},
      getPlatform().getDisplayContext().pixelsPerPoint);
  drawOverlay(mode, chain);
  imgui_->endFrame(device, *encoder);
  encoder->endEncoding();

  if (shellParams().shouldPresent) {
    commandBuffer->present(surfaceTextures.color);
  }
  commandQueue_->submit(*commandBuffer, true);
  if (timer_ != nullptr && timer_->resultsAvailable()) {
    gpuMilliseconds_ = static_cast<double>(timer_->getElapsedTimeNanos()) / 1.0e6;
  }
  ++frame_;
}

} // namespace igl::shell
