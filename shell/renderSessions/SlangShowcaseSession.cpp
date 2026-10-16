/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/SlangShowcaseSession.h>

#include <IGLU/slang/SlangProgram.h>
#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>
#include <shell/renderSessions/slang/generated/ShowcaseDeferredSlang.h>
#include <shell/renderSessions/slang/generated/ShowcaseInverseSlang.h>
#include <shell/renderSessions/slang/generated/ShowcaseMaterialsPassSlang.h>
#include <shell/renderSessions/slang/generated/ShowcasePostSlang.h>
#include <shell/renderSessions/slang/generated/ShowcaseTerrainCullSlang.h>
#include <shell/renderSessions/slang/generated/ShowcaseTerrainSlang.h>
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
constexpr Color kClearColor(0.02f, 0.02f, 0.03f, 1.0f);

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

// Mirrors ShowcaseTerrainCommon.slang.
constexpr uint32_t kTerrainMeshlets = 73 * 73;
constexpr uint32_t kTerrainMeshletTriangles = 98;
constexpr uint32_t kTerrainTaskGroupSize = 32;
struct TerrainUniforms {
  glm::mat4 viewProjection;
  glm::vec4 camera;
  std::array<glm::vec4, 6> cullPlanes;
  Lighting lighting;
  glm::vec4 display;
};

// Mirrors ShowcaseDeferred.slang.
constexpr uint32_t kMaxLights = 64;
struct PointLight {
  glm::vec4 position; // position, intensity
  glm::vec4 color;
};
struct DeferredUniforms {
  std::array<uint32_t, 4> lightCount;
};
constexpr TextureFormat kGBufferFormat = TextureFormat::RGBA_F16;
constexpr TextureDesc::TextureUsage kTileGBufferUsage =
    TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::InputAttachment;
constexpr TextureDesc::TextureUsage kStoredGBufferUsage =
    TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled;

// The planes bounding a [0, 1]-depth clip volume, pointing inward.
std::array<glm::vec4, 6> frustumPlanes(const glm::mat4& m) {
  const glm::mat4 t = glm::transpose(m);
  std::array<glm::vec4, 6> planes = {
      t[3] + t[0], t[3] - t[0], t[3] + t[1], t[3] - t[1], t[2], t[3] - t[2]};
  for (glm::vec4& plane : planes) {
    plane /= glm::length(glm::vec3(plane));
  }
  return planes;
}

// A low sun, a cool fill and a sky, far enough to light the whole terrain.
Lighting terrainLighting() {
  return {.lights = {glm::vec4(70.0f, 45.0f, -50.0f, 11000.0f),
                     glm::vec4(-80.0f, 60.0f, 70.0f, 3500.0f),
                     glm::vec4(0.0f, 100.0f, 0.0f, 0.0f)},
          .lightColors = {glm::vec4(1.0f, 0.86f, 0.68f, 0.0f),
                          glm::vec4(0.45f, 0.55f, 0.8f, 0.0f),
                          glm::vec4(0.0f)},
          .sky = glm::vec4(0.3f, 0.38f, 0.5f, 0.0f),
          .ground = glm::vec4(0.06f, 0.05f, 0.04f, 0.0f)};
}

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

// Point lights circling the grid in front of the spheres, on four rings that turn in alternate
// directions, in colors around the hue circle. Each of `count` lights is dimmer the more there are,
// so the image brightens only gently as lights are added.
std::array<PointLight, kMaxLights> pointLights(float seconds, uint32_t count) {
  const float intensity = 9.0f / std::sqrt(static_cast<float>(std::max(count, 1u)));
  std::array<PointLight, kMaxLights> lights{};
  for (uint32_t i = 0; i < count; ++i) {
    const auto index = static_cast<float>(i);
    const float radius = 1.6f + 1.3f * static_cast<float>(i % 4);
    const float angle = (i % 2 == 0 ? 0.4f : -0.3f) * seconds + 2.4f * index;
    const float hue = 6.0f * std::fmod(0.618034f * index, 1.0f);
    const glm::vec3 color = glm::clamp(
        glm::abs(glm::mod(glm::vec3(hue, hue + 4.0f, hue + 2.0f), 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f);
    lights[i] = {.position = glm::vec4(radius * std::cos(angle),
                                       radius * std::sin(angle),
                                       -2.2f + 0.6f * std::sin(0.7f * seconds + index),
                                       intensity),
                 .color = glm::vec4(glm::mix(glm::vec3(1.0f), color, 0.6f), 0.0f)};
  }
  return lights;
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

// Chapter 5 renders into the G-buffer and the image, and a pipeline used in a subpass declares
// every color attachment of the render pass.
std::shared_ptr<IRenderPipelineState> createDeferredPipeline(
    IDevice& device,
    const char* vertexEntryPoint,
    const char* fragmentEntryPoint,
    const std::vector<TextureFormat>& colorFormats,
    const std::shared_ptr<IVertexInputState>& vertexInput) {
  RenderPipelineDesc::TargetDesc targetDesc{.depthAttachmentFormat = TextureFormat::Z_UNorm32};
  for (const TextureFormat format : colorFormats) {
    targetDesc.colorAttachments.push_back({.textureFormat = format});
  }
  Result ret;
  const RenderPipelineDesc desc{
      .vertexInputState = vertexInput,
      .shaderStages = iglu::slang::createRenderStages(
          device, slang::kShowcaseDeferredProgram, vertexEntryPoint, fragmentEntryPoint, &ret),
      .targetDesc = std::move(targetDesc),
      .cullMode = CullMode::Disabled,
  };
  std::shared_ptr<IRenderPipelineState> pipeline;
  if (ret.isOk()) {
    pipeline = device.createRenderPipeline(desc, &ret);
  }
  if (!ret.isOk() || pipeline == nullptr) {
    IGL_LOG_INFO("SlangShowcaseSession: %s: %s\n", fragmentEntryPoint, ret.message.c_str());
    return nullptr;
  }
  return pipeline;
}

// A size-dependent attachment of chapter 5.
std::shared_ptr<ITexture> createAttachment(IDevice& device,
                                           const Dimensions& size,
                                           TextureFormat format,
                                           TextureDesc::TextureUsage usage,
                                           ResourceStorage storage,
                                           Result* outResult) {
  return device.createTexture(TextureDesc{.width = size.width,
                                          .height = size.height,
                                          .usage = usage,
                                          .type = TextureType::TwoD,
                                          .format = format,
                                          .storage = storage},
                              outResult);
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
  createDeferredResources(device);
  postSampler_ = device.createSamplerState(SamplerStateDesc::newLinear(), nullptr);
  terrainUniforms_ =
      device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                     .length = sizeof(TerrainUniforms),
                                     .storage = ResourceStorage::Shared,
                                     .hint = BufferDesc::BufferAPIHintBits::UniformBlock},
                          nullptr);
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

void SlangShowcaseSession::createDeferredResources(IDevice& device) {
  const auto check = [](const Result& ret, const char* what) {
    if (!ret.isOk()) {
      IGL_LOG_INFO("SlangShowcaseSession: %s: %s\n", what, ret.message.c_str());
    }
  };
  Result ret;
  lights_ = device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Storage,
                                           .length = kMaxLights * sizeof(PointLight),
                                           .storage = ResourceStorage::Shared,
                                           .storageStride = sizeof(PointLight)},
                                &ret);
  check(ret, "lights");
  deferredUniforms_ =
      device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                     .length = sizeof(DeferredUniforms),
                                     .storage = ResourceStorage::Shared,
                                     .hint = BufferDesc::BufferAPIHintBits::UniformBlock},
                          &ret);
  check(ret, "light count");
  lightingDepthState_ = device.createDepthStencilState(
      {.compareFunction = CompareFunction::AlwaysPass, .isDepthWriteEnabled = false}, &ret);
  check(ret, "lighting depth state");
  if (!device.hasFeature(DeviceFeatures::Subpasses)) {
    noSubpassesReason_ = "the device does not support them";
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
  terrainVertexPipeline_ = create(
      slang::kShowcaseTerrainProgram, "terrainVertexFallback", "terrainFragment", format, nullptr);
  if (device.hasFeature(DeviceFeatures::MeshShaders)) {
    Result ret;
    RenderPipelineDesc desc{
        .shaderStages = iglu::slang::createMeshRenderStages(device,
                                                            slang::kShowcaseTerrainCullProgram,
                                                            "terrainTask",
                                                            "terrainMesh",
                                                            slang::kShowcaseTerrainProgram,
                                                            "terrainFragment",
                                                            &ret),
        .targetDesc = {.colorAttachments = {{.textureFormat = format}},
                       .depthAttachmentFormat = TextureFormat::Z_UNorm32},
        .cullMode = CullMode::Disabled,
    };
    if (ret.isOk()) {
      terrainMeshPipeline_ = device.createRenderPipeline(desc, nullptr);
    } else {
      IGL_LOG_INFO("SlangShowcaseSession: %s\n", ret.message.c_str());
    }
  }
  for (size_t chain = 0; chain < kPostEntryPoints.size(); ++chain) {
    postPipelines_[chain] =
        create(slang::kShowcasePostProgram, "postVertex", kPostEntryPoints[chain], format, nullptr);
  }
  gbufferStoredPipeline_ = createDeferredPipeline(device,
                                                  "deferredVertex",
                                                  "gbufferStored",
                                                  {kGBufferFormat, kGBufferFormat, kGBufferFormat},
                                                  vertexInput_);
  lightingStoredPipeline_ =
      createDeferredPipeline(device, "lightingVertex", "lightingStored", {format}, nullptr);
  if (device.hasFeature(DeviceFeatures::Subpasses)) {
    const std::vector<TextureFormat> formats = {
        format, kGBufferFormat, kGBufferFormat, kGBufferFormat};
    gbufferSubpassPipeline_ =
        createDeferredPipeline(device, "deferredVertex", "gbufferSubpass", formats, vertexInput_);
    lightingSubpassPipeline_ =
        createDeferredPipeline(device, "lightingVertex", "lightingSubpass", formats, nullptr);
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

void SlangShowcaseSession::drawTerrain(IRenderCommandEncoder& encoder,
                                       float aspect,
                                       bool encodeGamma) {
  // The camera flies a circle over the terrain, looking ahead and down.
  const float angle = 0.12f * static_cast<float>(frame_) / 60.0f;
  const glm::vec3 eye(34.0f * std::cos(angle), 20.0f, 34.0f * std::sin(angle));
  const glm::vec3 ahead(14.0f * std::cos(angle + 1.1f), 2.0f, 14.0f * std::sin(angle + 1.1f));
  const glm::mat4 flying = glm::perspectiveLH_ZO(glm::radians(50.0f), aspect, 0.5f, 400.0f) *
                           glm::lookAtLH(eye, ahead, glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::vec3 overviewEye(0.0f, 115.0f, -70.0f);
  const TerrainUniforms uniforms{
      .viewProjection =
          terrainOverview_
              ? glm::perspectiveLH_ZO(glm::radians(50.0f), aspect, 1.0f, 500.0f) *
                    glm::lookAtLH(overviewEye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f))
              : flying,
      .camera = glm::vec4(terrainOverview_ ? overviewEye : eye, 1.0f),
      .cullPlanes = frustumPlanes(flying),
      .lighting = terrainLighting(),
      .display = glm::vec4(encodeGamma ? 1.0f : 0.0f,
                           terrainMeshletColors_ ? 1.0f : 0.0f,
                           terrainOverview_ ? 0.0f : 0.006f,
                           0.0f),
  };
  terrainUniforms_->upload(&uniforms, BufferRange(sizeof(uniforms)));
  encoder.bindDepthStencilState(depthState_);
  encoder.bindBuffer(1,
                     BindTarget::kVertex | BindTarget::kFragment | BindTarget::kTask |
                         BindTarget::kMesh,
                     terrainUniforms_.get());
  if (terrainMeshPipeline_ && !terrainVertexFallback_) {
    const iglu::slang::Program& cull = slang::kShowcaseTerrainCullProgram;
    encoder.bindRenderPipelineState(terrainMeshPipeline_);
    encoder.drawMeshTasks(
        Dimensions((kTerrainMeshlets + kTerrainTaskGroupSize - 1) / kTerrainTaskGroupSize, 1, 1),
        workgroupSize(cull, "terrainTask"),
        workgroupSize(cull, "terrainMesh"));
  } else if (terrainVertexPipeline_) {
    encoder.bindRenderPipelineState(terrainVertexPipeline_);
    encoder.draw(kTerrainMeshletTriangles * 3, kTerrainMeshlets);
  }
}

// The stored path, which needs no subpasses.
bool SlangShowcaseSession::canRenderDeferred() const {
  return lights_ && deferredUniforms_ && lightingDepthState_ && gbufferStoredPipeline_ &&
         lightingStoredPipeline_;
}

void SlangShowcaseSession::bindLightingResources(IRenderCommandEncoder& encoder) const {
  encoder.bindDepthStencilState(lightingDepthState_);
  encoder.bindBuffer(1, sceneUniforms_.get());
  encoder.bindBuffer(2, spheres_.get());
  encoder.bindBuffer(3, lights_.get());
  encoder.bindBuffer(4, deferredUniforms_.get());
}

// One render pass keeps the G-buffer in tile memory: the first subpass writes it, and the second
// reads it as input attachments to light the image. Returns false, and stops trying subpasses, if
// the device cannot run them.
bool SlangShowcaseSession::renderInTileMemory(ICommandBuffer& commandBuffer,
                                              const std::shared_ptr<ITexture>& surface) {
  if (!gbufferSubpassPipeline_ || !lightingSubpassPipeline_) {
    return disableSubpasses("cannot create the pipelines");
  }
  if (!subpassFramebuffer_) {
    IDevice& device = getPlatform().getDevice();
    const Dimensions size = surface->getDimensions();
    FramebufferDesc desc{.colorAttachments = {{.texture = surface}}};
    Result ret;
    for (size_t i = 1; i <= gbuffer_.size(); ++i) {
      desc.colorAttachments[i].texture = createAttachment(
          device, size, kGBufferFormat, kTileGBufferUsage, ResourceStorage::Memoryless, &ret);
      if (!ret.isOk() || !desc.colorAttachments[i].texture) {
        return disableSubpasses("cannot create the memoryless G-buffer: " + ret.message);
      }
    }
    desc.depthAttachment.texture = createAttachment(device,
                                                    size,
                                                    TextureFormat::Z_UNorm32,
                                                    TextureDesc::TextureUsageBits::Attachment,
                                                    ResourceStorage::Memoryless,
                                                    &ret);
    if (!ret.isOk() || !desc.depthAttachment.texture) {
      return disableSubpasses("cannot create the memoryless depth: " + ret.message);
    }
    subpassFramebuffer_ = device.createFramebuffer(desc, &ret);
    if (!ret.isOk() || !subpassFramebuffer_) {
      return disableSubpasses("cannot create the framebuffer: " + ret.message);
    }
  } else {
    subpassFramebuffer_->updateDrawable(surface);
  }

  // The G-buffer is cleared to zero, which marks the background, and never stored.
  const RenderPassDesc::AttachmentDesc tile{.loadAction = LoadAction::Clear,
                                            .storeAction = StoreAction::DontCare};
  const RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear,
                            .storeAction = StoreAction::Store,
                            .clearColor = kClearColor},
                           tile,
                           tile,
                           tile},
      .depthAttachment = {.loadAction = LoadAction::Clear,
                          .storeAction = StoreAction::DontCare,
                          .clearDepth = 1.0f},
      .subpasses = {{.colorAttachmentMask = 0b1110},
                    {.colorAttachmentMask = 0b0001, .inputAttachmentMask = 0b1110}},
  };
  Result ret;
  const auto encoder =
      commandBuffer.createRenderCommandEncoder(renderPass, subpassFramebuffer_, &ret);
  if (!encoder) {
    return disableSubpasses(ret.message);
  }
  bindSphereResources(*encoder);
  encoder->bindRenderPipelineState(gbufferSubpassPipeline_);
  encoder->drawIndexed(indexCount_, kSpheres);
  encoder->nextSubpass();
  encoder->bindRenderPipelineState(lightingSubpassPipeline_);
  bindLightingResources(*encoder);
  encoder->draw(3);
  encoder->endEncoding();
  return true;
}

// The classic path stores the G-buffer for the final encoder to sample.
bool SlangShowcaseSession::renderGBuffer(ICommandBuffer& commandBuffer) {
  if (!gbufferFramebuffer_) {
    IDevice& device = getPlatform().getDevice();
    const Dimensions size = framebuffer_->getColorAttachment(0)->getDimensions();
    FramebufferDesc desc{.depthAttachment = {.texture = framebuffer_->getDepthAttachment()}};
    Result ret;
    for (size_t i = 0; i < gbuffer_.size(); ++i) {
      gbuffer_[i] = createAttachment(
          device, size, kGBufferFormat, kStoredGBufferUsage, ResourceStorage::Private, &ret);
      if (!ret.isOk() || !gbuffer_[i]) {
        IGL_LOG_INFO("SlangShowcaseSession: G-buffer: %s\n", ret.message.c_str());
        return false;
      }
      desc.colorAttachments[i].texture = gbuffer_[i];
    }
    gbufferFramebuffer_ = device.createFramebuffer(desc, &ret);
    if (!ret.isOk() || !gbufferFramebuffer_) {
      IGL_LOG_INFO("SlangShowcaseSession: G-buffer framebuffer: %s\n", ret.message.c_str());
      gbufferFramebuffer_ = nullptr;
      return false;
    }
  }
  const RenderPassDesc::AttachmentDesc stored{.loadAction = LoadAction::Clear,
                                              .storeAction = StoreAction::Store};
  Result ret;
  const auto encoder = commandBuffer.createRenderCommandEncoder(
      RenderPassDesc{.colorAttachments = {stored, stored, stored},
                     .depthAttachment = {.loadAction = LoadAction::Clear, .clearDepth = 1.0f}},
      gbufferFramebuffer_,
      &ret);
  if (!encoder) {
    IGL_LOG_INFO("SlangShowcaseSession: G-buffer pass: %s\n", ret.message.c_str());
    return false;
  }
  bindSphereResources(*encoder);
  encoder->bindRenderPipelineState(gbufferStoredPipeline_);
  encoder->drawIndexed(indexCount_, kSpheres);
  encoder->endEncoding();
  return true;
}

bool SlangShowcaseSession::disableSubpasses(const std::string& reason) {
  IGL_LOG_INFO("SlangShowcaseSession: no subpasses (%s); storing the G-buffer\n", reason.c_str());
  noSubpassesReason_ = reason.empty() ? "cannot begin the render pass" : reason;
  subpassFramebuffer_ = nullptr;
  return false;
}

void SlangShowcaseSession::drawOverlay(ShadingMode mode, PostChain chain) {
  constexpr std::array<const char*, 5> kTitles = {"Slang showcase 1/5: materials",
                                                  "Slang showcase 2/5: inverse rendering",
                                                  "Slang showcase 3/5: post effects",
                                                  "Slang showcase 4/5: GPU-driven terrain",
                                                  "Slang showcase 5/5: tile memory"};
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
  case Chapter::Terrain:
    ImGui::Text("%u meshlets, %u triangles: %s",
                kTerrainMeshlets,
                kTerrainMeshlets * kTerrainMeshletTriangles,
                terrainMeshPipeline_ && !terrainVertexFallback_
                    ? "task shader culls, mesh shader expands"
                    : "vertex shader, no culling");
    break;
  case Chapter::TileMemory:
    if (!canRenderDeferred()) {
      ImGui::Text("Cannot create the deferred resources; see the log");
      break;
    }
    ImGui::Text("%u point lights, G-buffer %s",
                std::min(lightCount_, kMaxLights),
                tileSubpasses_ && noSubpassesReason_.empty()
                    ? "in tile memory, read as input attachments"
                    : "stored, then sampled");
    if (tileSubpasses_ && !noSubpassesReason_.empty()) {
      ImGui::Text("No subpasses: %s", noSubpassesReason_.c_str());
    }
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
  ImGui::SameLine();
  ImGui::RadioButton("Terrain", &chapter, 3);
  ImGui::SameLine();
  ImGui::RadioButton("Tile memory", &chapter, 4);
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
  case Chapter::Terrain:
    ImGui::Checkbox("From above", &terrainOverview_);
    ImGui::SameLine();
    ImGui::Checkbox("Meshlets", &terrainMeshletColors_);
    if (terrainMeshPipeline_) {
      ImGui::SameLine();
      ImGui::Checkbox("Vertex shader", &terrainVertexFallback_);
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
  case Chapter::TileMemory: {
    int path = tileSubpasses_ ? 0 : 1;
    ImGui::RadioButton("Tile memory (subpasses)", &path, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Stored G-buffer", &path, 1);
    tileSubpasses_ = path == 0;
    int lights = static_cast<int>(std::min(lightCount_, kMaxLights));
    if (ImGui::SliderInt("Lights", &lights, 1, static_cast<int>(kMaxLights))) {
      lightCount_ = static_cast<uint32_t>(lights);
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
    subpassFramebuffer_ = nullptr;
    gbufferFramebuffer_ = nullptr;
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
  const bool deferred = supported_ && chapter_ == Chapter::TileMemory && canRenderDeferred();
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
  if (deferred) {
    const uint32_t count = std::min(lightCount_, kMaxLights);
    const std::array<PointLight, kMaxLights> lights =
        pointLights(static_cast<float>(frame_) / 60.0f, count);
    lights_->upload(lights.data(), BufferRange(sizeof(lights)));
    const DeferredUniforms uniforms{.lightCount = {count, 0, 0, 0}};
    deferredUniforms_->upload(&uniforms, BufferRange(sizeof(uniforms)));
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
  // Chapter 5 lights the image in tile memory, and the final encoder draws only the overlay over
  // it, or stores the G-buffer for the final encoder to light.
  const bool litInTileMemory = deferred && tileSubpasses_ && noSubpassesReason_.empty() &&
                               renderInTileMemory(*commandBuffer, surfaceTextures.color);
  const bool gbufferStored = deferred && !litInTileMemory && renderGBuffer(*commandBuffer);
  const RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = litInTileMemory ? LoadAction::Load : LoadAction::Clear,
                            .storeAction = StoreAction::Store,
                            .clearColor = kClearColor}},
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
  } else if (gbufferStored) {
    for (size_t i = 0; i < gbuffer_.size(); ++i) {
      dependencies.textures[i] = gbuffer_[i].get();
    }
  }
  auto encoder = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer_, dependencies);
  if (post) {
    const auto& pipeline = postPipelines_[static_cast<size_t>(chain)];
    encoder->bindRenderPipelineState(pipeline ? pipeline : postPipelines_[0]);
    encoder->bindTexture(0, BindTarget::kFragment, radianceTexture_.get());
    encoder->bindSamplerState(0, BindTarget::kFragment, postSampler_.get());
    encoder->bindBuffer(1, postUniforms_.get());
    encoder->draw(3);
  } else if (supported_ && chapter_ == Chapter::Terrain) {
    drawTerrain(
        *encoder, size.width / size.height, !surfaceTextures.color->getProperties().isSRGB());
  } else if (supported_ && chapter_ == Chapter::TileMemory) {
    if (gbufferStored) {
      encoder->bindRenderPipelineState(lightingStoredPipeline_);
      bindLightingResources(*encoder);
      for (size_t i = 0; i < gbuffer_.size(); ++i) {
        encoder->bindTexture(i, BindTarget::kFragment, gbuffer_[i].get());
        // Vulkan reads textures through combined samplers; loads ignore the filter.
        encoder->bindSamplerState(i, BindTarget::kFragment, postSampler_.get());
      }
      encoder->draw(3);
    }
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
