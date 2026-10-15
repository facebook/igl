/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/gpu_benchmark/TrafficModel.h>

#include "Fakes.h"

#include <igl/IGL.h>

namespace iglu::gpu_benchmark::tests {

TEST(TrafficModelTest, ClassicVersusTile) {
  constexpr uint32_t w = 100;
  constexpr uint32_t h = 50;
  const std::vector<PassTraffic> classic = {
      {.name = "a",
       .attachments = {{.bytesPerPixel = 4,
                        .width = w,
                        .height = h,
                        .loadAction = igl::LoadAction::Clear,
                        .storeAction = igl::StoreAction::Store}}},
      {.name = "b",
       .attachments = {{.bytesPerPixel = 4,
                        .width = w,
                        .height = h,
                        .loadAction = igl::LoadAction::Load,
                        .storeAction = igl::StoreAction::Store}},
       .sampledTextures = {{.bytesPerPixel = 4, .width = w, .height = h}}},
  };
  const TrafficEstimate classicEstimate = estimateTraffic(classic);
  EXPECT_EQ(classicEstimate.readBytes, 2u * 4 * w * h);
  EXPECT_EQ(classicEstimate.writeBytes, 2u * 4 * w * h);
  EXPECT_EQ(classicEstimate.totalBytes(), 4u * 4 * w * h);

  const std::vector<PassTraffic> tile = {
      {.name = "tile",
       .attachments = {{.bytesPerPixel = 4,
                        .width = w,
                        .height = h,
                        .loadAction = igl::LoadAction::Load,
                        .storeAction = igl::StoreAction::Store,
                        .memoryless = true},
                       {.bytesPerPixel = 4,
                        .width = w,
                        .height = h,
                        .loadAction = igl::LoadAction::Clear,
                        .storeAction = igl::StoreAction::Store}}},
  };
  const TrafficEstimate tileEstimate = estimateTraffic(tile);
  EXPECT_EQ(tileEstimate.readBytes, 0u);
  EXPECT_EQ(tileEstimate.writeBytes, 4u * w * h);
}

TEST(TrafficModelTest, MsaaRules) {
  constexpr uint32_t w = 100;
  constexpr uint32_t h = 50;
  const auto estimate = [&](igl::LoadAction load, igl::StoreAction store, bool memoryless) {
    return estimateTraffic({{.name = "p",
                             .attachments = {{.bytesPerPixel = 4,
                                              .samples = 4,
                                              .width = w,
                                              .height = h,
                                              .loadAction = load,
                                              .storeAction = store,
                                              .memoryless = memoryless,
                                              .resolveBytesPerPixel = 4}}}});
  };
  const TrafficEstimate clearResolve =
      estimate(igl::LoadAction::Clear, igl::StoreAction::MsaaResolve, false);
  EXPECT_EQ(clearResolve.readBytes, 0u);
  EXPECT_EQ(clearResolve.writeBytes, 4u * w * h);
  const TrafficEstimate loadStore = estimate(igl::LoadAction::Load, igl::StoreAction::Store, false);
  EXPECT_EQ(loadStore.readBytes, 16u * w * h);
  EXPECT_EQ(loadStore.writeBytes, 16u * w * h);
  const TrafficEstimate dontCare =
      estimate(igl::LoadAction::DontCare, igl::StoreAction::DontCare, false);
  EXPECT_EQ(dontCare.totalBytes(), 0u);
  const TrafficEstimate memorylessResolve =
      estimate(igl::LoadAction::Clear, igl::StoreAction::MsaaResolve, true);
  EXPECT_EQ(memorylessResolve.readBytes, 0u);
  EXPECT_EQ(memorylessResolve.writeBytes, 4u * w * h);
  const TrafficEstimate memorylessLoadResolve =
      estimate(igl::LoadAction::Load, igl::StoreAction::MsaaResolve, true);
  EXPECT_EQ(memorylessLoadResolve.readBytes, 0u);
  EXPECT_EQ(memorylessLoadResolve.writeBytes, 4u * w * h);
  EXPECT_EQ(estimate(igl::LoadAction::Load, igl::StoreAction::Store, true).totalBytes(), 0u);
  EXPECT_EQ(estimate(igl::LoadAction::Clear, igl::StoreAction::DontCare, true).totalBytes(), 0u);
  const TrafficEstimate noResolveTarget =
      estimateTraffic({{.name = "p",
                        .attachments = {{.bytesPerPixel = 4,
                                         .samples = 4,
                                         .width = w,
                                         .height = h,
                                         .storeAction = igl::StoreAction::MsaaResolve,
                                         .memoryless = true}}}});
  EXPECT_EQ(noResolveTarget.totalBytes(), 0u);
}

TEST(TrafficModelTest, MemorylessMsaaResolveExample) {
  constexpr uint32_t w = 123;
  constexpr uint32_t h = 45;
  const TrafficEstimate memoryless =
      estimateTraffic({{.name = "resolve",
                        .attachments = {{.bytesPerPixel = 4,
                                         .samples = 4,
                                         .width = w,
                                         .height = h,
                                         .loadAction = igl::LoadAction::Clear,
                                         .storeAction = igl::StoreAction::MsaaResolve,
                                         .memoryless = true,
                                         .resolveBytesPerPixel = 4}}}});
  EXPECT_EQ(memoryless.readBytes, 0u);
  EXPECT_EQ(memoryless.writeBytes, 4u * w * h);

  const TrafficEstimate storedThenResolved =
      estimateTraffic({{.name = "store",
                        .attachments = {{.bytesPerPixel = 4,
                                         .samples = 4,
                                         .width = w,
                                         .height = h,
                                         .loadAction = igl::LoadAction::Clear,
                                         .storeAction = igl::StoreAction::Store}}},
                       {.name = "resolve",
                        .attachments = {{.bytesPerPixel = 4,
                                         .samples = 4,
                                         .width = w,
                                         .height = h,
                                         .loadAction = igl::LoadAction::Load,
                                         .storeAction = igl::StoreAction::MsaaResolve,
                                         .resolveBytesPerPixel = 4}}}});
  EXPECT_EQ(storedThenResolved.readBytes, 16u * w * h);
  EXPECT_EQ(storedThenResolved.writeBytes, (16u + 4u) * w * h);
  EXPECT_EQ(storedThenResolved.totalBytes(), (16u + 16u + 4u) * w * h);
}

TEST(TrafficModelTest, BytesPerPixel) {
  igl::setDebugBreakEnabled(false);
  EXPECT_EQ(bytesPerPixel(igl::TextureFormat::RGBA_UNorm8), 4u);
  EXPECT_EQ(bytesPerPixel(igl::TextureFormat::R_UNorm8), 1u);
  EXPECT_EQ(bytesPerPixel(igl::TextureFormat::RGBA_F16), 8u);
  EXPECT_EQ(bytesPerPixel(igl::TextureFormat::RGBA_F32), 16u);
  EXPECT_EQ(bytesPerPixel(igl::TextureFormat::Z_UNorm24), 3u);
  EXPECT_EQ(bytesPerPixel(igl::TextureFormat::RGBA_ASTC_4x4), 0u);
}

TEST(TrafficModelTest, DescribeDefaultsAndMip) {
  FakeFramebuffer fb;
  fb.colors = {makeTexture(igl::TextureFormat::RGBA_UNorm8, 100, 50),
               makeTexture(igl::TextureFormat::RGBA_F16, 100, 50)};

  igl::RenderPassDesc pass;
  pass.colorAttachments.push_back({.loadAction = igl::LoadAction::Load, .mipLevel = 1});
  pass.colorAttachments.push_back({});
  const PassTraffic described = describeRenderPass("p", pass, fb);
  EXPECT_EQ(described.name, "p");
  ASSERT_EQ(described.attachments.size(), 2u);
  EXPECT_EQ(described.attachments[0].bytesPerPixel, 4u);
  EXPECT_EQ(described.attachments[0].width, 50u);
  EXPECT_EQ(described.attachments[0].height, 25u);
  EXPECT_EQ(described.attachments[0].loadAction, igl::LoadAction::Load);
  EXPECT_EQ(described.attachments[0].storeAction, igl::StoreAction::Store);
  EXPECT_EQ(described.attachments[1].bytesPerPixel, 8u);
  EXPECT_EQ(described.attachments[1].width, 100u);
  EXPECT_EQ(described.attachments[1].height, 50u);
  EXPECT_EQ(described.attachments[1].loadAction, igl::LoadAction::DontCare);
  EXPECT_EQ(described.attachments[1].storeAction, igl::StoreAction::Store);
  EXPECT_FALSE(described.attachments[0].memoryless);
  EXPECT_EQ(described.attachments[0].samples, 1u);

  igl::RenderPassDesc deepMip;
  deepMip.colorAttachments.push_back({.mipLevel = 200});
  deepMip.colorAttachments.push_back({});
  const PassTraffic deep = describeRenderPass("deep", deepMip, fb);
  EXPECT_EQ(deep.attachments[0].width, 1u);
  EXPECT_EQ(deep.attachments[0].height, 1u);
}

TEST(TrafficModelTest, DescribeMsaaResolveAndMemoryless) {
  FakeFramebuffer fb;
  fb.colors = {
      makeTexture(igl::TextureFormat::RGBA_UNorm8, 100, 50, 4, igl::ResourceStorage::Memoryless),
      makeTexture(igl::TextureFormat::RGBA_UNorm8, 100, 50, 4)};
  fb.colorResolves = {makeTexture(igl::TextureFormat::RGBA_UNorm8, 100, 50),
                      makeTexture(igl::TextureFormat::RGBA_F16, 100, 50)};
  igl::RenderPassDesc pass;
  pass.colorAttachments.push_back(
      {.loadAction = igl::LoadAction::Clear, .storeAction = igl::StoreAction::MsaaResolve});
  pass.colorAttachments.push_back(
      {.loadAction = igl::LoadAction::Clear, .storeAction = igl::StoreAction::MsaaResolve});
  const PassTraffic described = describeRenderPass("msaa", pass, fb);
  ASSERT_EQ(described.attachments.size(), 2u);
  EXPECT_TRUE(described.attachments[0].memoryless);
  EXPECT_EQ(described.attachments[0].samples, 4u);
  EXPECT_EQ(described.attachments[0].bytesPerPixel, 4u);
  EXPECT_EQ(described.attachments[0].resolveBytesPerPixel, 4u);
  EXPECT_EQ(described.attachments[0].storeAction, igl::StoreAction::MsaaResolve);
  EXPECT_FALSE(described.attachments[1].memoryless);
  EXPECT_EQ(described.attachments[1].samples, 4u);
  EXPECT_EQ(described.attachments[1].resolveBytesPerPixel, 8u);
  const TrafficEstimate estimate = estimateTraffic({described});
  EXPECT_EQ(estimate.readBytes, 0u);
  EXPECT_EQ(estimate.writeBytes, (4u + 8u) * 100 * 50);
}

TEST(TrafficModelTest, DescribeDepthStencil) {
  const auto combined = makeTexture(igl::TextureFormat::S8_UInt_Z24_UNorm, 64, 64);
  FakeFramebuffer fb;
  fb.depth = combined;
  fb.stencil = combined;
  igl::RenderPassDesc pass;
  pass.depthAttachment = {.loadAction = igl::LoadAction::Clear,
                          .storeAction = igl::StoreAction::DontCare};
  pass.stencilAttachment = {.loadAction = igl::LoadAction::Load,
                            .storeAction = igl::StoreAction::Store};
  const PassTraffic described = describeRenderPass("ds", pass, fb);
  ASSERT_EQ(described.attachments.size(), 1u);
  EXPECT_EQ(described.attachments[0].bytesPerPixel, 4u);
  EXPECT_EQ(described.attachments[0].loadAction, igl::LoadAction::Load);
  EXPECT_EQ(described.attachments[0].storeAction, igl::StoreAction::Store);

  pass.depthAttachment = {.loadAction = igl::LoadAction::Clear,
                          .storeAction = igl::StoreAction::MsaaResolve};
  pass.stencilAttachment = {.loadAction = igl::LoadAction::DontCare,
                            .storeAction = igl::StoreAction::DontCare};
  const PassTraffic resolveOnly = describeRenderPass("ds", pass, fb);
  ASSERT_EQ(resolveOnly.attachments.size(), 1u);
  EXPECT_EQ(resolveOnly.attachments[0].loadAction, igl::LoadAction::Clear);
  EXPECT_EQ(resolveOnly.attachments[0].storeAction, igl::StoreAction::MsaaResolve);

  pass.depthAttachment = {.loadAction = igl::LoadAction::Clear,
                          .storeAction = igl::StoreAction::MsaaResolve};
  pass.stencilAttachment = {.loadAction = igl::LoadAction::Clear,
                            .storeAction = igl::StoreAction::Store};
  EXPECT_EQ(describeRenderPass("ds", pass, fb).attachments[0].storeAction, igl::StoreAction::Store);

  FakeFramebuffer separate;
  separate.depth = makeTexture(igl::TextureFormat::Z_UNorm32, 64, 64);
  separate.stencil = makeTexture(igl::TextureFormat::S_UInt8, 64, 64);
  const PassTraffic two = describeRenderPass("two", pass, separate);
  ASSERT_EQ(two.attachments.size(), 2u);
  EXPECT_EQ(two.attachments[0].bytesPerPixel, 4u);
  EXPECT_EQ(two.attachments[1].bytesPerPixel, 1u);

  FakeFramebuffer stencilOnly;
  stencilOnly.stencil = makeTexture(igl::TextureFormat::S_UInt8, 64, 64);
  EXPECT_EQ(describeRenderPass("s", pass, stencilOnly).attachments.size(), 1u);
}

TEST(TrafficModelTest, DescribeSampledAndNullColor) {
  FakeFramebuffer fb;
  fb.colors = {nullptr, makeTexture(igl::TextureFormat::RGBA_UNorm8, 8, 8)};
  fb.colorResolves = {makeTexture(igl::TextureFormat::RGBA_UNorm8, 8, 8)};
  igl::RenderPassDesc pass;
  pass.colorAttachments.push_back({});
  pass.colorAttachments.push_back({});
  const PassTraffic described =
      describeRenderPass("s",
                         pass,
                         fb,
                         {makeTexture(igl::TextureFormat::RGBA_F16, 16, 4),
                          nullptr,
                          makeTexture(igl::TextureFormat::R_UNorm8, 2, 2)});
  ASSERT_EQ(described.attachments.size(), 1u);
  ASSERT_EQ(described.sampledTextures.size(), 2u);
  EXPECT_EQ(described.sampledTextures[0].bytesPerPixel, 8u);
  EXPECT_EQ(described.sampledTextures[0].width, 16u);
  EXPECT_EQ(described.sampledTextures[0].height, 4u);
  EXPECT_EQ(described.sampledTextures[1].bytesPerPixel, 1u);
  EXPECT_EQ(estimateTraffic({described}).readBytes, 8u * 16 * 4 + 4);
}

TEST(TrafficModelTest, Footprint) {
  const auto a = makeTexture(igl::TextureFormat::RGBA_UNorm8, 10, 10);
  const auto msaa =
      makeTexture(igl::TextureFormat::RGBA_UNorm8, 10, 10, 4, igl::ResourceStorage::Private, 2, 3);
  const auto memoryless =
      makeTexture(igl::TextureFormat::RGBA_UNorm8, 10, 10, 1, igl::ResourceStorage::Memoryless);

  const uint64_t aSize = 400u;
  const uint64_t msaaSize = (400u + 100u + 16u) * 2 * 4;
  const uint64_t memorylessSize = 400u;

  const FootprintEstimate withFeature =
      estimateFootprint({a, msaa, a, nullptr, memoryless, memoryless}, true);
  EXPECT_EQ(withFeature.bytes, aSize + msaaSize);
  EXPECT_EQ(withFeature.memorylessBytes, memorylessSize);
  EXPECT_FALSE(withFeature.memorylessBacked);

  const FootprintEstimate withoutFeature =
      estimateFootprint({a, msaa, memoryless, memoryless}, false);
  EXPECT_EQ(withoutFeature.bytes, aSize + msaaSize + memorylessSize);
  EXPECT_EQ(withoutFeature.memorylessBytes, memorylessSize);
  EXPECT_TRUE(withoutFeature.memorylessBacked);

  const FootprintEstimate none = estimateFootprint({a}, false);
  EXPECT_EQ(none.bytes, aSize);
  EXPECT_EQ(none.memorylessBytes, 0u);
  EXPECT_FALSE(none.memorylessBacked);
  EXPECT_EQ(estimateFootprint({}, false).bytes, 0u);
}

TEST(TrafficModelTest, SamplesAndLayersCountAsAtLeastOne) {
  constexpr uint32_t w = 10;
  constexpr uint32_t h = 4;
  const TrafficEstimate zeroed =
      estimateTraffic({{.name = "p",
                        .attachments = {{.bytesPerPixel = 4,
                                         .samples = 0,
                                         .width = w,
                                         .height = h,
                                         .layers = 0,
                                         .loadAction = igl::LoadAction::Load,
                                         .storeAction = igl::StoreAction::Store}}}});
  EXPECT_EQ(zeroed.readBytes, 4u * w * h);
  EXPECT_EQ(zeroed.writeBytes, 4u * w * h);
}

TEST(TrafficModelTest, LayersScaleStoresLoadsAndResolves) {
  constexpr uint32_t w = 10;
  constexpr uint32_t h = 4;
  const TrafficEstimate estimate =
      estimateTraffic({{.name = "p",
                        .attachments = {{.bytesPerPixel = 4,
                                         .samples = 4,
                                         .width = w,
                                         .height = h,
                                         .layers = 2,
                                         .loadAction = igl::LoadAction::Load,
                                         .storeAction = igl::StoreAction::Store},
                                        {.bytesPerPixel = 4,
                                         .samples = 4,
                                         .width = w,
                                         .height = h,
                                         .layers = 2,
                                         .storeAction = igl::StoreAction::MsaaResolve,
                                         .memoryless = true,
                                         .resolveBytesPerPixel = 4}}}});
  EXPECT_EQ(estimate.readBytes, 16u * w * h * 2);
  EXPECT_EQ(estimate.writeBytes, 16u * w * h * 2 + 4u * w * h * 2);
}

TEST(TrafficModelTest, AlsoResolveAddsTheResolveWrite) {
  constexpr uint32_t w = 10;
  constexpr uint32_t h = 4;
  const auto estimate = [&](igl::StoreAction store, bool alsoResolve, bool memoryless) {
    return estimateTraffic({{.name = "p",
                             .attachments = {{.bytesPerPixel = 4,
                                              .samples = 4,
                                              .width = w,
                                              .height = h,
                                              .storeAction = store,
                                              .memoryless = memoryless,
                                              .resolveBytesPerPixel = 4,
                                              .alsoResolve = alsoResolve}}}});
  };
  EXPECT_EQ(estimate(igl::StoreAction::Store, false, false).writeBytes, 16u * w * h);
  EXPECT_EQ(estimate(igl::StoreAction::Store, true, false).writeBytes, (16u + 4u) * w * h);
  EXPECT_EQ(estimate(igl::StoreAction::MsaaResolve, true, false).writeBytes, 4u * w * h);
  EXPECT_EQ(estimate(igl::StoreAction::DontCare, true, false).writeBytes, 4u * w * h);
  EXPECT_EQ(estimate(igl::StoreAction::Store, true, true).writeBytes, 4u * w * h);
}

TEST(TrafficModelTest, StereoDoublesEveryAttachment) {
  FakeFramebuffer fb;
  fb.mode = igl::FramebufferMode::Stereo;
  fb.colors = {makeTexture(igl::TextureFormat::RGBA_UNorm8, 8, 8)};
  fb.depth = makeTexture(igl::TextureFormat::Z_UNorm32, 8, 8);
  fb.stencil = makeTexture(igl::TextureFormat::S_UInt8, 8, 8);
  igl::RenderPassDesc pass;
  pass.colorAttachments.push_back({.loadAction = igl::LoadAction::Load});
  pass.depthAttachment = {.loadAction = igl::LoadAction::Load,
                          .storeAction = igl::StoreAction::Store};
  pass.stencilAttachment = {.loadAction = igl::LoadAction::Load,
                            .storeAction = igl::StoreAction::Store};
  const PassTraffic stereo = describeRenderPass("stereo", pass, fb);
  ASSERT_EQ(stereo.attachments.size(), 3u);
  for (const AttachmentTraffic& attachment : stereo.attachments) {
    EXPECT_EQ(attachment.layers, 2u);
  }
  fb.mode = igl::FramebufferMode::Mono;
  const PassTraffic mono = describeRenderPass("mono", pass, fb);
  for (const AttachmentTraffic& attachment : mono.attachments) {
    EXPECT_EQ(attachment.layers, 1u);
  }
  const TrafficEstimate stereoEstimate = estimateTraffic({stereo});
  const TrafficEstimate monoEstimate = estimateTraffic({mono});
  EXPECT_EQ(stereoEstimate.readBytes, 2 * monoEstimate.readBytes);
  EXPECT_EQ(stereoEstimate.writeBytes, 2 * monoEstimate.writeBytes);
  EXPECT_EQ(monoEstimate.readBytes, (4u + 4u + 1u) * 8 * 8);
}

TEST(TrafficModelTest, CombinedDepthStencilStoreAndResolveRules) {
  const auto combined = makeTexture(igl::TextureFormat::S8_UInt_Z24_UNorm, 16, 16, 4);
  const auto depthResolve = makeTexture(igl::TextureFormat::Z_UNorm32, 16, 16);
  FakeFramebuffer fb;
  fb.depth = combined;
  fb.stencil = combined;
  fb.depthResolve = depthResolve;

  using igl::LoadAction;
  using igl::StoreAction;
  struct Row {
    LoadAction depthLoad;
    StoreAction depthStore;
    LoadAction stencilLoad;
    StoreAction stencilStore;
    LoadAction expectedLoad;
    StoreAction expectedStore;
    bool expectedAlsoResolve;
  };
  const Row rows[] = {
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::Store,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::Store,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::Store,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::Store,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::MsaaResolve,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::Store,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::MsaaResolve,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::Store,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::Store,
       .expectedAlsoResolve = true},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::MsaaResolve,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::MsaaResolve,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::MsaaResolve,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::MsaaResolve,
       .stencilLoad = LoadAction::DontCare,
       .stencilStore = StoreAction::DontCare,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::MsaaResolve,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::DontCare,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::MsaaResolve,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::DontCare,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::DontCare,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::Store,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::Store,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::Store,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::DontCare,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::Store,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::DontCare,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::DontCare,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::DontCare,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Load,
       .depthStore = StoreAction::DontCare,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::DontCare,
       .expectedLoad = LoadAction::Load,
       .expectedStore = StoreAction::DontCare,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::DontCare,
       .depthStore = StoreAction::DontCare,
       .stencilLoad = LoadAction::Load,
       .stencilStore = StoreAction::DontCare,
       .expectedLoad = LoadAction::Load,
       .expectedStore = StoreAction::DontCare,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::DontCare,
       .depthStore = StoreAction::DontCare,
       .stencilLoad = LoadAction::Clear,
       .stencilStore = StoreAction::DontCare,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::DontCare,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::Clear,
       .depthStore = StoreAction::DontCare,
       .stencilLoad = LoadAction::DontCare,
       .stencilStore = StoreAction::DontCare,
       .expectedLoad = LoadAction::Clear,
       .expectedStore = StoreAction::DontCare,
       .expectedAlsoResolve = false},
      {.depthLoad = LoadAction::DontCare,
       .depthStore = StoreAction::DontCare,
       .stencilLoad = LoadAction::DontCare,
       .stencilStore = StoreAction::DontCare,
       .expectedLoad = LoadAction::DontCare,
       .expectedStore = StoreAction::DontCare,
       .expectedAlsoResolve = false},
  };
  for (const Row& row : rows) {
    igl::RenderPassDesc pass;
    pass.depthAttachment = {.loadAction = row.depthLoad, .storeAction = row.depthStore};
    pass.stencilAttachment = {.loadAction = row.stencilLoad, .storeAction = row.stencilStore};
    const PassTraffic described = describeRenderPass("ds", pass, fb);
    ASSERT_EQ(described.attachments.size(), 1u);
    const AttachmentTraffic& attachment = described.attachments[0];
    EXPECT_EQ(attachment.loadAction, row.expectedLoad);
    EXPECT_EQ(attachment.storeAction, row.expectedStore);
    EXPECT_EQ(attachment.alsoResolve, row.expectedAlsoResolve);
    EXPECT_EQ(attachment.resolveBytesPerPixel, 4u);
    EXPECT_EQ(attachment.samples, 4u);
  }

  igl::RenderPassDesc storeAndResolve;
  storeAndResolve.depthAttachment = {.loadAction = LoadAction::Clear,
                                     .storeAction = StoreAction::MsaaResolve};
  storeAndResolve.stencilAttachment = {.loadAction = LoadAction::Clear,
                                       .storeAction = StoreAction::Store};
  const TrafficEstimate estimate = estimateTraffic({describeRenderPass("ds", storeAndResolve, fb)});
  EXPECT_EQ(estimate.readBytes, 0u);
  EXPECT_EQ(estimate.writeBytes, (4u * 4 + 4u) * 16 * 16);
}

} // namespace iglu::gpu_benchmark::tests
