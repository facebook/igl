/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <shell/renderSessions/TileBenchmarkSession.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <shell/shared/testShell/TestShell.h>

namespace igl::shell {

namespace {

using CustomParams = std::vector<std::pair<std::string, std::string>>;
using Summaries = std::vector<TileBenchmarkSession::ValidationSummary>;

bool hasVariant(const Summaries& summaries, const std::string& variant) {
  return std::any_of(summaries.begin(), summaries.end(), [&variant](const auto& summary) {
    return summary.variant == variant;
  });
}

} // namespace

class TileBenchmarkSessionTest : public ::testing::Test, public TestShellBase {
 public:
  void SetUp() override {
    setUpInternal({.width = 64, .height = 64});
    if (IsSkipped() || HasFatalFailure()) {
      return;
    }
    const IDevice& device = platform_->getDevice();
    const BackendType backend = device.getBackendType();
    if (backend != BackendType::Vulkan && backend != BackendType::Metal &&
        backend != BackendType::OpenGL) {
      GTEST_SKIP() << "The session only clears the surface on " << BackendTypeToString(backend);
    }
    if (backend == BackendType::OpenGL && device.getShaderVersion().majorVersion < 3) {
      GTEST_SKIP() << "The session needs GLSL ES 3.00 or GLSL 3.30";
    }
  }

 protected:
  // The conditions under which the session runs `tiled` and `tiled-private`
  [[nodiscard]] bool supportsTileVariants() const {
    const IDevice& device = platform_->getDevice();
    if (!device.hasFeature(DeviceFeatures::Subpasses) ||
        !device.hasFeature(DeviceFeatures::MultipleRenderTargets)) {
      return false;
    }
    if (device.getBackendType() == BackendType::OpenGL) {
      const BackendVersion version = device.getBackendVersion();
      return version.flavor == BackendFlavor::OpenGL_ES && version.majorVersion >= 3;
    }
    return true;
  }

  void initialize(TileBenchmarkSession& session, const CustomParams& customParams) {
    shellParams_.viewportSize = glm::vec2(64.0f, 64.0f);
    shellParams_.benchmarkParams = BenchmarkRenderSessionParams{.customParams = customParams};
    session.setShellParams(shellParams_);
    session.initialize();
  }

  void update(TileBenchmarkSession& session) {
    const DeviceScope scope(platform_->getDevice());
    session.update({.color = offscreenTexture_, .depth = offscreenDepthTexture_});
  }

  // A blocking run does all its work in one update()
  void runBlocking(TileBenchmarkSession& session, CustomParams customParams) {
    customParams.emplace_back("tile-blocking", "1");
    initialize(session, customParams);
    update(session);
    session.teardown();
  }

  // Every variant that the device can run was validated at `size` and passed, `classic` first
  void expectEveryVariantPassed(const TileBenchmarkSession& session, const std::string& size) {
    EXPECT_TRUE(session.validationPassed());
    const Summaries& summaries = session.validationSummaries();
    ASSERT_FALSE(summaries.empty());
    EXPECT_EQ(summaries.front().variant, "classic");
    for (const auto& summary : summaries) {
      EXPECT_EQ(summary.scenario, "chain");
      EXPECT_EQ(summary.size, size);
      EXPECT_TRUE(summary.passed) << summary.variant << ": max difference " << summary.maxDiff
                                  << ", " << summary.fractionWithinOne << " within 1, "
                                  << summary.fractionOutliers << " outliers";
    }
    EXPECT_TRUE(hasVariant(summaries, "fused"));
    EXPECT_EQ(hasVariant(summaries, "tiled"), supportsTileVariants());
    EXPECT_EQ(hasVariant(summaries, "tiled-private"), supportsTileVariants());
  }

  // A session keeps a pointer to its parameters
  ShellParams shellParams_;
};

TEST_F(TileBenchmarkSessionTest, ValidatesEveryVariant) {
  TileBenchmarkSession session(platform_);
  runBlocking(session, {{"tile-mode", "validate"}, {"tile-sizes", "256x256"}});

  expectEveryVariantPassed(session, "256x256");
  EXPECT_TRUE(session.appParams().exitRequested);
}

// Eight stages are more than one render pass runs as subpasses
TEST_F(TileBenchmarkSessionTest, ValidatesASplitChain) {
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"}, {"tile-sizes", "256x256"}, {"tile-chain-length", "7"}});

  expectEveryVariantPassed(session, "256x256");
}

TEST_F(TileBenchmarkSessionTest, ValidatesHalfFloatTargets) {
  const ICapabilities::TextureFormatCapabilities capabilities =
      platform_->getDevice().getTextureFormatCapabilities(TextureFormat::RGBA_F16);
  if (!contains(capabilities, ICapabilities::TextureFormatCapabilityBits::Sampled) ||
      !contains(capabilities, ICapabilities::TextureFormatCapabilityBits::Attachment)) {
    GTEST_SKIP() << "RGBA_F16 cannot be a sampled render target";
  }

  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"},
               {"tile-sizes", "256x256"},
               {"tile-format", "rgba16f"},
               {"tile-chain-length", "7"}});

  expectEveryVariantPassed(session, "256x256");
}

// `classic` is the reference, so it is validated even if it is not requested
TEST_F(TileBenchmarkSessionTest, ValidatesOnlyRequestedVariants) {
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"}, {"tile-sizes", "256x256"}, {"tile-variants", "fused"}});

  EXPECT_TRUE(session.validationPassed());
  const Summaries& summaries = session.validationSummaries();
  ASSERT_EQ(summaries.size(), 2u);
  EXPECT_EQ(summaries[0].variant, "classic");
  EXPECT_EQ(summaries[1].variant, "fused");
}

TEST_F(TileBenchmarkSessionTest, StepsOneSizePerUpdateWhenNotBlocking) {
  TileBenchmarkSession session(platform_);
  initialize(session,
             {{"tile-mode", "validate"},
              {"tile-blocking", "0"},
              {"tile-sizes", "64x64,96x96"},
              {"tile-variants", "classic,fused"}});

  update(session);
  ASSERT_EQ(session.validationSummaries().size(), 2u);
  for (const auto& summary : session.validationSummaries()) {
    EXPECT_EQ(summary.size, "64x64");
  }

  update(session);
  const Summaries& summaries = session.validationSummaries();
  ASSERT_EQ(summaries.size(), 4u);
  EXPECT_EQ(summaries[2].size, "96x96");
  EXPECT_EQ(summaries[3].size, "96x96");
  EXPECT_TRUE(session.validationPassed());
  EXPECT_FALSE(session.appParams().exitRequested);

  session.teardown();
}

// A headless shell renders one frame, so a run that asks not to block blocks anyway
TEST_F(TileBenchmarkSessionTest, HeadlessRunsBlocking) {
  shellParams_.isHeadless = true;
  TileBenchmarkSession session(platform_);
  initialize(session, {{"tile-mode", "validate"}, {"tile-blocking", "0"}, {"tile-sizes", "64x64"}});

  update(session);

  EXPECT_TRUE(session.validationPassed());
  EXPECT_TRUE(session.appParams().exitRequested);
  session.teardown();
}

// A bench run validates every size before it times it
TEST_F(TileBenchmarkSessionTest, BenchModeCompletes) {
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "bench"},
               {"tile-sizes", "64x64"},
               {"tile-rounds", "1"},
               {"tile-warmup", "0"},
               {"tile-iterations", "1"},
               {"tile-timer", "cpu"}});

  expectEveryVariantPassed(session, "64x64");
  EXPECT_TRUE(session.appParams().exitRequested);
}

// For manual runs: logs the result table of the default benchmark at 1920x1080
TEST_F(TileBenchmarkSessionTest, DISABLED_Benchmark1080p) {
  TileBenchmarkSession session(platform_);
  runBlocking(session, {{"tile-sizes", "1920x1080"}});

  expectEveryVariantPassed(session, "1920x1080");
}

} // namespace igl::shell
