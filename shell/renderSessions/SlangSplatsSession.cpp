/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/SlangSplatsSession.h>

#include <IGLU/slang/SlangProgram.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <glm/packing.hpp>
#include <initializer_list>
#include <vector>
#include <shell/renderSessions/slang/generated/SplatAdamSlang.h>
#include <shell/renderSessions/slang/generated/SplatBackwardSlang.h>
#include <shell/renderSessions/slang/generated/SplatBinSlang.h>
#include <shell/renderSessions/slang/generated/SplatForwardSlang.h>
#include <shell/renderSessions/slang/generated/SplatRenderSlang.h>
#include <shell/renderSessions/slang/generated/SplatsBaked.h>
#include <shell/shared/imageLoader/ImageLoader.h>

namespace igl::shell {

namespace {

// Mirrors SplatCommon.slang.
constexpr uint32_t kImageSize = 256;
constexpr uint32_t kSplatCount = 2048;
constexpr uint32_t kTilesPerRow = 32;
constexpr uint32_t kTileStride = 256;
constexpr uint32_t kIterationsPerFrame = 4;
constexpr uint32_t kBakeIterations = 1200;
// Baked splats store 10 half floats each: center, log scale, rotation, opacity logit, color, 0.
constexpr size_t kBakedWordsPerSplat = 5;
constexpr float kPi = 3.14159265f;

struct SplatRecord {
  std::array<float, 4> shape;
  std::array<float, 4> extra;
  std::array<float, 4> color;
};
static_assert(sizeof(SplatRecord) == 48);
constexpr size_t kPixelStateSize = 32;
constexpr size_t kMomentsSize = 112;

struct RenderParams {
  std::array<float, 4> flip;
  std::array<float, 4> gains;
};

// The target image, resampled to kImageSize x kImageSize and composited over black.
struct Target {
  std::vector<uint8_t> rgba8;
  std::vector<float> rgba32f;
};

Target loadTarget(Platform& platform) {
  Target target{.rgba8 = std::vector<uint8_t>(static_cast<size_t>(kImageSize) * kImageSize * 4),
                .rgba32f = std::vector<float>(static_cast<size_t>(kImageSize) * kImageSize * 4)};
  const ImageData image = platform.getImageLoader().loadImageData("igl.png");
  const uint32_t width = image.desc.width;
  const uint32_t height = image.desc.height;
  const bool rgba8 = image.desc.format == TextureFormat::RGBA_UNorm8 ||
                     image.desc.format == TextureFormat::RGBA_SRGB;
  const uint8_t* source =
      image.data != nullptr && rgba8 && width > 0 && height > 0 ? image.data->data() : nullptr;
  for (uint32_t y = 0; y < kImageSize; ++y) {
    for (uint32_t x = 0; x < kImageSize; ++x) {
      const size_t i = (static_cast<size_t>(y) * kImageSize + x) * 4;
      std::array<float, 4> rgba = {
          static_cast<float>(x) / kImageSize, static_cast<float>(y) / kImageSize, 0.5f, 1.0f};
      if (source != nullptr) {
        const uint8_t* p =
            source +
            (static_cast<size_t>(y * height / kImageSize) * width + x * width / kImageSize) * 4;
        const float alpha = p[3] / 255.0f;
        rgba = {p[0] / 255.0f * alpha, p[1] / 255.0f * alpha, p[2] / 255.0f * alpha, 1.0f};
      }
      for (size_t c = 0; c < 4; ++c) {
        target.rgba32f[i + c] = rgba[c];
        target.rgba8[i + c] = static_cast<uint8_t>(std::lround(rgba[c] * 255.0f));
      }
    }
  }
  return target;
}

// Splats scattered over the image with the target's color at their centers (a fixed LCG keeps runs
// identical).
std::vector<SplatRecord> initialSplats(const Target& target) {
  uint32_t state = 12345u;
  const auto uniform = [&state]() {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / 16777216.0f;
  };
  std::vector<SplatRecord> splats(kSplatCount);
  for (SplatRecord& splat : splats) {
    const float x = uniform() * kImageSize;
    const float y = uniform() * kImageSize;
    const float logScale = std::log(2.5f + 3.0f * uniform());
    const size_t texel =
        (static_cast<size_t>(std::min(static_cast<uint32_t>(y), kImageSize - 1)) * kImageSize +
         std::min(static_cast<uint32_t>(x), kImageSize - 1)) *
        4;
    splat.shape = {x, y, logScale, logScale + 0.4f * (uniform() - 0.5f)};
    splat.extra = {uniform() * kPi, 1.5f, 0.0f, 0.0f};
    splat.color = {
        target.rgba32f[texel], target.rgba32f[texel + 1], target.rgba32f[texel + 2], 0.0f};
  }
  return splats;
}

std::vector<SplatRecord> bakedSplats() {
  static_assert(slang::kBakedSplatCount == kSplatCount,
                "SplatsBaked.h is stale; regenerate it with --bake-splats");
  std::vector<SplatRecord> splats(kSplatCount);
  for (size_t i = 0; i < splats.size(); ++i) {
    std::array<float, kBakedWordsPerSplat * 2> v{};
    for (size_t w = 0; w < kBakedWordsPerSplat; ++w) {
      const glm::vec2 pair = glm::unpackHalf2x16(slang::kBakedSplats[i * kBakedWordsPerSplat + w]);
      v[w * 2] = pair.x;
      v[w * 2 + 1] = pair.y;
    }
    splats[i] = {.shape = {v[0], v[1], v[2], v[3]},
                 .extra = {v[4], v[5], 0.0f, 0.0f},
                 .color = {v[6], v[7], v[8], 0.0f}};
  }
  return splats;
}

std::shared_ptr<IBuffer> createBuffer(IDevice& device,
                                      BufferDesc::BufferType type,
                                      const void* data,
                                      size_t length,
                                      size_t stride,
                                      ResourceStorage storage = ResourceStorage::Invalid) {
  Result ret;
  auto buffer = device.createBuffer(BufferDesc{.type = type,
                                               .data = data,
                                               .length = length,
                                               .storage = storage,
                                               .storageStride = stride},
                                    &ret);
  IGL_DEBUG_ASSERT(ret.isOk(), "%s", ret.message.c_str());
  return buffer;
}

std::shared_ptr<IComputePipelineState> createComputePipeline(IDevice& device,
                                                             const iglu::slang::Program& program,
                                                             const char* entryPoint) {
  Result ret;
  std::shared_ptr<IShaderStages> stages =
      iglu::slang::createComputeStages(device, program, entryPoint, &ret);
  std::shared_ptr<IComputePipelineState> pipeline;
  if (ret.isOk()) {
    ComputePipelineDesc desc{.shaderStages = std::move(stages)};
    iglu::slang::addGlResourceNames(program, entryPoint, desc);
    pipeline = device.createComputePipeline(desc, &ret);
  }
  if (!ret.isOk()) {
    IGL_LOG_INFO("SlangSplatsSession: %s: %s\n", entryPoint, ret.message.c_str());
  }
  return pipeline;
}

Dimensions workgroupSize(const iglu::slang::Program& program, const char* entryPoint) {
  const iglu::slang::EntryPoint* entry = iglu::slang::findEntryPoint(program, entryPoint);
  return entry != nullptr ? iglu::slang::workgroupSize(*entry) : Dimensions(1, 1, 1);
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
void SlangSplatsSession::initialize() noexcept {
  IDevice& device = getPlatform().getDevice();
  commandQueue_ = device.createCommandQueue({}, nullptr);

  for (int i = 1; i + 1 < Platform::argc(); ++i) {
    if (std::strcmp(Platform::argv()[i], "--bake-splats") == 0) {
      bakePath_ = Platform::argv()[i + 1];
    }
  }

  // Only backends with compute train; the others draw splats trained ahead of time.
  const bool training = device.hasFeature(DeviceFeatures::Compute);
  const Target target = loadTarget(getPlatform());
  const std::vector<SplatRecord> splats = training ? initialSplats(target) : bakedSplats();
  constexpr size_t kPixels = static_cast<size_t>(kImageSize) * kImageSize;
  const std::vector<uint8_t> zeros(kSplatCount * kMomentsSize, 0);

  splats_ = createBuffer(device,
                         training ? BufferDesc::BufferTypeBits::Storage |
                                        BufferDesc::BufferTypeBits::Vertex
                                  : BufferDesc::BufferTypeBits::Vertex,
                         splats.data(),
                         splats.size() * sizeof(SplatRecord),
                         sizeof(SplatRecord),
                         bakePath_.empty() ? ResourceStorage::Invalid : ResourceStorage::Shared);
  if (training) {
    tiles_ = createBuffer(device,
                          BufferDesc::BufferTypeBits::Storage,
                          nullptr,
                          static_cast<size_t>(kTilesPerRow) * kTilesPerRow * kTileStride * 4,
                          4);
    overflowCount_ = createBuffer(device, BufferDesc::BufferTypeBits::Storage, zeros.data(), 4, 4);
    targetPixels_ = createBuffer(device,
                                 BufferDesc::BufferTypeBits::Storage,
                                 target.rgba32f.data(),
                                 target.rgba32f.size() * sizeof(float),
                                 sizeof(float) * 4);
    pixels_ = createBuffer(device,
                           BufferDesc::BufferTypeBits::Storage,
                           nullptr,
                           kPixels * kPixelStateSize,
                           kPixelStateSize);
    gradients_ = createBuffer(device,
                              BufferDesc::BufferTypeBits::Storage,
                              nullptr,
                              kSplatCount * sizeof(SplatRecord),
                              sizeof(SplatRecord));
    moments_ = createBuffer(device,
                            BufferDesc::BufferTypeBits::Storage,
                            zeros.data(),
                            kSplatCount * kMomentsSize,
                            kMomentsSize);
  }

  targetTexture_ = device.createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                           kImageSize,
                                                           kImageSize,
                                                           TextureDesc::TextureUsageBits::Sampled),
                                        nullptr);
  targetTexture_->upload(TextureRangeDesc::new2D(0, 0, kImageSize, kImageSize),
                         target.rgba8.data());
  // Signed sums need a float target; the sensitivity panel is meaningless without one.
  const TextureFormat sumFormat =
      (device.getTextureFormatCapabilities(TextureFormat::RGBA_F16) &
       ICapabilities::TextureFormatCapabilityBits::SampledAttachment) != 0
          ? TextureFormat::RGBA_F16
          : TextureFormat::RGBA_UNorm8;
  const TextureDesc sumDesc = TextureDesc::new2D(sumFormat,
                                                 kImageSize,
                                                 kImageSize,
                                                 TextureDesc::TextureUsageBits::Sampled |
                                                     TextureDesc::TextureUsageBits::Attachment);
  accumulationTexture_ = device.createTexture(sumDesc, nullptr);
  sensitivityTexture_ = device.createTexture(sumDesc, nullptr);
  splatFramebuffer_ = device.createFramebuffer(
      FramebufferDesc{.colorAttachments = {{.texture = accumulationTexture_},
                                           {.texture = sensitivityTexture_}}},
      nullptr);
  sampler_ = device.createSamplerState(SamplerStateDesc::newLinear(), nullptr);

  const iglu::slang::Program& render = slang::kSplatRenderProgram;
  const auto* attributes = render.vertexAttributes;
  VertexInputStateDesc inputDesc{.numAttributes = 3, .numInputBindings = 1};
  for (size_t i = 0; i < 3; ++i) {
    inputDesc.attributes[i] = {.bufferIndex = 0,
                               .format = VertexAttributeFormat::Float4,
                               .offset = i * sizeof(float) * 4,
                               .name = attributes[i].name,
                               .location = attributes[i].location};
  }
  inputDesc.inputBindings[0] = {.stride = sizeof(SplatRecord),
                                .sampleFunction = VertexSampleFunction::Instance};
  splatInput_ = device.createVertexInputState(inputDesc, nullptr);

  const RenderPipelineDesc::TargetDesc::ColorAttachment additive{
      .textureFormat = sumFormat,
      .blendEnabled = true,
      .srcRGBBlendFactor = BlendFactor::One,
      .srcAlphaBlendFactor = BlendFactor::One,
      .dstRGBBlendFactor = BlendFactor::One,
      .dstAlphaBlendFactor = BlendFactor::One,
  };
  RenderPipelineDesc splatDesc{
      .topology = PrimitiveType::TriangleStrip,
      .vertexInputState = splatInput_,
      .shaderStages =
          iglu::slang::createRenderStages(device, render, "splatVertex", "splatFragment", nullptr),
      .targetDesc = {.colorAttachments = {additive, additive}},
      .cullMode = CullMode::Disabled,
  };
  iglu::slang::addGlResourceNames(render, "splatVertex", "splatFragment", splatDesc);
  if (splatDesc.shaderStages != nullptr) {
    splatPipeline_ = device.createRenderPipeline(splatDesc, nullptr);
  }

  if (training) {
    createTrainingPipelines(device);
  }
}

void SlangSplatsSession::createTrainingPipelines(IDevice& device) {
  binPipeline_ = createComputePipeline(device, slang::kSplatBinProgram, "binTiles");
  forwardPipeline_ = createComputePipeline(device, slang::kSplatForwardProgram, "forwardPixels");
  backwardPipeline_ = createComputePipeline(device, slang::kSplatBackwardProgram, "backwardSplats");
  adamPipeline_ = createComputePipeline(device, slang::kSplatAdamProgram, "adamStep");
  if (!binPipeline_ || !forwardPipeline_ || !backwardPipeline_ || !adamPipeline_) {
    binPipeline_ = forwardPipeline_ = backwardPipeline_ = adamPipeline_ = nullptr;
  }
}

void SlangSplatsSession::encodeTrainingStep(ICommandBuffer& commandBuffer) {
  const auto dispatch = [&commandBuffer](const std::shared_ptr<IComputePipelineState>& pipeline,
                                         std::initializer_list<IBuffer*> buffers,
                                         const Dimensions& groups,
                                         const Dimensions& groupSize,
                                         IBuffer* readsAfterWrite) {
    auto encoder = commandBuffer.createComputeCommandEncoder();
    encoder->bindComputePipelineState(pipeline);
    uint32_t index = 0;
    for (IBuffer* buffer : buffers) {
      encoder->bindBuffer(index++, buffer);
    }
    encoder->dispatchThreadGroups(groups, groupSize, {.buffers = {readsAfterWrite}});
    encoder->endEncoding();
  };
  dispatch(binPipeline_,
           {splats_.get(), tiles_.get(), overflowCount_.get()},
           Dimensions(kTilesPerRow * kTilesPerRow / 64, 1, 1),
           workgroupSize(slang::kSplatBinProgram, "binTiles"),
           splats_.get());
  dispatch(forwardPipeline_,
           {splats_.get(), tiles_.get(), targetPixels_.get(), pixels_.get()},
           Dimensions(kImageSize / 8, kImageSize / 8, 1),
           workgroupSize(slang::kSplatForwardProgram, "forwardPixels"),
           tiles_.get());
  dispatch(backwardPipeline_,
           {splats_.get(), pixels_.get(), gradients_.get()},
           Dimensions(kSplatCount, 1, 1),
           workgroupSize(slang::kSplatBackwardProgram, "backwardSplats"),
           pixels_.get());
  dispatch(adamPipeline_,
           {splats_.get(), gradients_.get(), moments_.get()},
           Dimensions(kSplatCount / 64, 1, 1),
           workgroupSize(slang::kSplatAdamProgram, "adamStep"),
           gradients_.get());
}

// NOLINTNEXTLINE(bugprone-exception-escape)
void SlangSplatsSession::update(SurfaceTextures surfaceTextures) noexcept {
  if (!surfaceTextures.color || !splatPipeline_) {
    return;
  }
  IDevice& device = getPlatform().getDevice();
  if (!framebuffer_) {
    framebuffer_ = device.createFramebuffer(
        FramebufferDesc{.colorAttachments = {{.texture = surfaceTextures.color}}}, nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }
  if (!compositePipeline_) {
    // OpenGL renders into textures bottom-up; sRGB surfaces encode gamma themselves.
    const bool bottomUp = device.getBackendType() == BackendType::OpenGL;
    const bool srgb = surfaceTextures.color->getProperties().isSRGB();
    const RenderParams params{.flip = {1.0f, bottomUp ? 1.0f : 0.0f, 0.0f, 0.0f},
                              .gains = {4.0f, 40.0f, srgb ? 1.0f : 0.0f, 0.0f}};
    renderParams_ =
        device.createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                       .data = &params,
                                       .length = sizeof(params),
                                       .hint = BufferDesc::BufferAPIHintBits::UniformBlock},
                            nullptr);
    const iglu::slang::Program& render = slang::kSplatRenderProgram;
    RenderPipelineDesc desc{
        .shaderStages = iglu::slang::createRenderStages(
            device, render, "compositeVertex", "compositeFragment", nullptr),
        .targetDesc = {.colorAttachments = {{.textureFormat =
                                                 surfaceTextures.color->getProperties().format}}},
        .cullMode = CullMode::Disabled,
    };
    iglu::slang::addGlResourceNames(render, "compositeVertex", "compositeFragment", desc);
    if (desc.shaderStages == nullptr) {
      return;
    }
    compositePipeline_ = device.createRenderPipeline(desc, nullptr);
  }

  auto commandBuffer = commandQueue_->createCommandBuffer({}, nullptr);
  if (binPipeline_) {
    for (uint32_t i = 0; i < kIterationsPerFrame; ++i) {
      encodeTrainingStep(*commandBuffer);
    }
  }

  RenderPassDesc splatPass;
  splatPass.colorAttachments.resize(2);
  for (auto& attachment : splatPass.colorAttachments) {
    attachment.loadAction = LoadAction::Clear;
    attachment.storeAction = StoreAction::Store;
    attachment.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
  }
  auto splatEncoder = commandBuffer->createRenderCommandEncoder(
      splatPass, splatFramebuffer_, {.buffers = {splats_.get()}});
  splatEncoder->bindRenderPipelineState(splatPipeline_);
  splatEncoder->bindVertexBuffer(0, *splats_);
  splatEncoder->bindBuffer(1, renderParams_.get());
  splatEncoder->draw(4, kSplatCount);
  splatEncoder->endEncoding();

  RenderPassDesc compositePass;
  compositePass.colorAttachments.resize(1);
  compositePass.colorAttachments[0].loadAction = LoadAction::DontCare;
  compositePass.colorAttachments[0].storeAction = StoreAction::Store;
  auto encoder = commandBuffer->createRenderCommandEncoder(
      compositePass,
      framebuffer_,
      {.textures = {accumulationTexture_.get(), sensitivityTexture_.get()}});
  encoder->bindRenderPipelineState(compositePipeline_);
  encoder->bindBuffer(1, renderParams_.get());
  const std::array<ITexture*, 3> textures = {
      accumulationTexture_.get(), sensitivityTexture_.get(), targetTexture_.get()};
  for (size_t unit = 0; unit < textures.size(); ++unit) {
    encoder->bindTexture(unit, BindTarget::kFragment, textures[unit]);
    encoder->bindSamplerState(unit, BindTarget::kFragment, sampler_.get());
  }
  encoder->draw(3);
  encoder->endEncoding();

  commandBuffer->present(surfaceTextures.color);
  commandQueue_->submit(*commandBuffer, true);
  if (binPipeline_) {
    iterations_ += kIterationsPerFrame;
    if (!bakePath_.empty() && iterations_ == kBakeIterations) {
      commandBuffer->waitUntilCompleted();
      bake();
    }
  }
}

void SlangSplatsSession::bake() {
  Result ret;
  const auto* records = static_cast<const SplatRecord*>(
      splats_->map(BufferRange(kSplatCount * sizeof(SplatRecord)), &ret));
  if (records == nullptr) {
    IGL_LOG_INFO("SlangSplatsSession: cannot read the splats: %s\n", ret.message.c_str());
    return;
  }
  std::ofstream out(bakePath_);
  out << "/*\n"
         " * Copyright (c) Meta Platforms, Inc. and affiliates.\n"
         " *\n"
         " * This source code is licensed under the MIT license found in the\n"
         " * LICENSE file in the root directory of this source tree.\n"
         " */\n\n"
         "// @"
         "generated by SlangSplatsSession --bake-splats after "
      << kBakeIterations
      << " training iterations.\n"
         "// clang-format off\n\n"
         "#pragma once\n\n"
         "#include <cstdint>\n\n"
         "namespace igl::shell::slang {\n\n"
         "inline constexpr uint32_t kBakedSplatCount = "
      << kSplatCount << ";\ninline constexpr uint32_t kBakedSplats[] = {\n";
  char word[16];
  for (uint32_t i = 0; i < kSplatCount; ++i) {
    const SplatRecord& r = records[i];
    const std::array<float, kBakedWordsPerSplat * 2> v = {r.shape[0],
                                                          r.shape[1],
                                                          r.shape[2],
                                                          r.shape[3],
                                                          r.extra[0],
                                                          r.extra[1],
                                                          r.color[0],
                                                          r.color[1],
                                                          r.color[2],
                                                          0.0f};
    out << "   ";
    for (size_t w = 0; w < kBakedWordsPerSplat; ++w) {
      std::snprintf(
          word, sizeof(word), " 0x%08x,", glm::packHalf2x16(glm::vec2(v[w * 2], v[w * 2 + 1])));
      out << word;
    }
    out << "\n";
  }
  out << "};\n\n} // namespace igl::shell::slang\n";
  splats_->unmap();
  out.close();
  if (!out) {
    IGL_LOG_ERROR("SlangSplatsSession: cannot write %s\n", bakePath_.c_str());
    return;
  }
  IGL_LOG_INFO("SlangSplatsSession: baked %u iterations into %s\n", iterations_, bakePath_.c_str());
}

} // namespace igl::shell
