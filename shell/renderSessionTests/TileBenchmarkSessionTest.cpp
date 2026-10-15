/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <shell/renderSessions/TileBenchmarkSession.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
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

// The scenarios of `summaries`, each once, in the order of their first summary
std::vector<std::string> getScenarios(const Summaries& summaries) {
  std::vector<std::string> scenarios;
  for (const auto& summary : summaries) {
    if (std::find(scenarios.begin(), scenarios.end(), summary.scenario) == scenarios.end()) {
      scenarios.push_back(summary.scenario);
    }
  }
  return scenarios;
}

// The largest power of two that is at most `count`, which is at least 1
size_t floorToPowerOfTwo(size_t count) {
  size_t result = 1;
  while (count / 2 >= result) {
    result *= 2;
  }
  return result;
}

// Captures what IGLLog() writes while it exists, and passes it on to the previous handler
class LogCapture {
 public:
  LogCapture() {
    const std::lock_guard<std::mutex> lock(mutex());
    EXPECT_EQ(active(), nullptr) << "log captures do not nest";
    previous_ = IGLLogGetHandler();
    active() = this;
    IGLLogSetHandler(capture);
  }
  ~LogCapture() {
    const std::lock_guard<std::mutex> lock(mutex());
    IGLLogSetHandler(previous_);
    active() = nullptr;
  }
  LogCapture(const LogCapture&) = delete;
  LogCapture& operator=(const LogCapture&) = delete;

  // The lines that were logged so far, with their line breaks
  [[nodiscard]] std::vector<std::string> getLines() const {
    const std::lock_guard<std::mutex> lock(mutex());
    return lines_;
  }

  // The first line that starts with `prefix`, without its line break, or nothing
  [[nodiscard]] std::optional<std::string> findLine(std::string_view prefix) const {
    for (std::string line : getLines()) {
      if (line.rfind(prefix, 0) == 0) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
          line.pop_back();
        }
        return line;
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] bool hasLine(std::string_view prefix) const {
    return findLine(prefix).has_value();
  }

 private:
  // Guards active() and the lines of the active capture, so that capture() never uses a capture
  // that is being destroyed
  static std::mutex& mutex() {
    static auto* const kMutex = new std::mutex();
    return *kMutex;
  }

  static LogCapture*& active() {
    static LogCapture* sActive = nullptr;
    return sActive;
  }

  static int capture(IGLLogLevel level, const char* IGL_RESTRICT format, va_list ap) {
    va_list copy; // NOLINT(cppcoreguidelines-init-variables)
    va_copy(copy, ap);
    char buffer[2048] = {};
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, ap);
    {
      const std::lock_guard<std::mutex> lock(mutex());
      if (LogCapture* const capture = active()) {
        capture->lines_.emplace_back(buffer);
        if (capture->previous_ != nullptr) {
          capture->previous_(level, format, copy);
        }
      }
    }
    va_end(copy);
    return length;
  }

  IGLLogHandlerFunc previous_ = nullptr;
  std::vector<std::string> lines_;
};

// Splits a CSV row at its commas
std::vector<std::string> splitRow(const std::string& row) {
  std::vector<std::string> fields;
  size_t begin = 0;
  while (true) {
    const size_t end = row.find(',', begin);
    if (end == std::string::npos) {
      fields.push_back(row.substr(begin));
      return fields;
    }
    fields.push_back(row.substr(begin, end - begin));
    begin = end + 1;
  }
}

template<typename T>
std::optional<T> parseNumber(std::string_view text) {
  T value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc() || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

// A time in seconds with a fraction of up to 6 digits, as a count of microseconds
std::optional<int64_t> parseMicroseconds(std::string_view seconds) {
  const size_t point = seconds.find('.');
  const std::string_view fraction = point == std::string_view::npos ? std::string_view()
                                                                    : seconds.substr(point + 1);
  const std::optional<int64_t> whole = parseNumber<int64_t>(seconds.substr(0, point));
  if (!whole.has_value() || fraction.size() > 6) {
    return std::nullopt;
  }
  int64_t micros = 0;
  if (!fraction.empty()) {
    const std::optional<int64_t> digits = parseNumber<int64_t>(fraction);
    if (!digits.has_value()) {
      return std::nullopt;
    }
    micros = *digits;
    for (size_t i = fraction.size(); i != 6; ++i) {
      micros *= 10;
    }
  }
  const bool negative = seconds.starts_with('-');
  return *whole * 1000000 + (negative ? -micros : micros);
}

// A `TILEBENCH_V1,phase,<begin|end>,<scenario>,<size>,<variant>,<seconds>,<iterations>` row, whose
// last column is the number of executions of the variant in the phase
struct PhaseRow {
  std::string edge;
  std::string scenario;
  std::string size;
  std::string variant;
  int64_t microseconds = 0;
  uint64_t executions = 0;
};

// The phase rows of `lines`, in order
std::vector<PhaseRow> getPhaseRows(const std::vector<std::string>& lines) {
  std::vector<PhaseRow> rows;
  for (std::string line : lines) {
    if (line.rfind("TILEBENCH_V1,phase,", 0) != 0) {
      continue;
    }
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    const std::vector<std::string> fields = splitRow(line);
    if (fields.size() != 8) {
      ADD_FAILURE() << "a phase row has 8 columns: " << line;
      continue;
    }
    const std::optional<int64_t> microseconds = parseMicroseconds(fields[6]);
    const std::optional<uint64_t> executions = parseNumber<uint64_t>(fields[7]);
    if (!microseconds.has_value() || !executions.has_value()) {
      ADD_FAILURE() << "a phase row has numbers: " << line;
      continue;
    }
    rows.push_back({.edge = fields[2],
                    .scenario = fields[3],
                    .size = fields[4],
                    .variant = fields[5],
                    .microseconds = *microseconds,
                    .executions = *executions});
  }
  return rows;
}

// The rows `TILEBENCH_V1,summary,<scenario>,<size>,<variant>,...`: the modeled bytes of a variant
// per frame, which are the read bytes and the write bytes columns, or nothing if it has no row
std::optional<uint64_t> getModeledBytes(const LogCapture& capture,
                                        const std::string& scenario,
                                        const std::string& size,
                                        const std::string& variant) {
  const std::optional<std::string> line =
      capture.findLine("TILEBENCH_V1,summary," + scenario + "," + size + "," + variant + ",");
  if (!line.has_value()) {
    return std::nullopt;
  }
  const std::vector<std::string> fields = splitRow(*line);
  constexpr size_t kReadBytes = 12;
  constexpr size_t kWriteBytes = 13;
  if (fields.size() <= kWriteBytes) {
    return std::nullopt;
  }
  const std::optional<uint64_t> read = parseNumber<uint64_t>(fields[kReadBytes]);
  const std::optional<uint64_t> write = parseNumber<uint64_t>(fields[kWriteBytes]);
  if (!read.has_value() || !write.has_value()) {
    return std::nullopt;
  }
  return *read + *write;
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

  // Whether some depth format can be an attachment, as the session needs for `deferred` and `msaa`
  [[nodiscard]] bool supportsDepth() const {
    const IDevice& device = platform_->getDevice();
    return std::any_of(kDepthFormats.begin(), kDepthFormats.end(), [&device](TextureFormat format) {
      return contains(device.getTextureFormatCapabilities(format),
                      ICapabilities::TextureFormatCapabilityBits::Attachment);
    });
  }

  [[nodiscard]] bool supportsDeferred() const {
    return supportsDepth() &&
           platform_->getDevice().hasFeature(DeviceFeatures::MultipleRenderTargets);
  }

  // The most samples of the device, or 0 if it has no multisampling
  [[nodiscard]] size_t getMaxSamples() const {
    size_t maxSamples = 0;
    return platform_->getDevice().getFeatureLimits(DeviceFeatureLimits::MaxMultisampleCount,
                                                   maxSamples)
               ? maxSamples
               : 0;
  }

  [[nodiscard]] bool supportsMsaa() const {
    return getMaxSamples() >= 2 && supportsDepth();
  }

  // The sample count that the session uses for `requested`, which is at least 2: the next power of
  // two, limited by the device
  [[nodiscard]] size_t getSamples(size_t requested) const {
    size_t samples = 1;
    while (samples < requested) {
      samples *= 2;
    }
    return std::min(samples, floorToPowerOfTwo(getMaxSamples()));
  }

  // The scenarios that the device can run, in the order of the session
  [[nodiscard]] std::vector<std::string> getRunnableScenarios() const {
    std::vector<std::string> scenarios = {"chain"};
    if (supportsDeferred()) {
      scenarios.emplace_back("deferred");
    }
    if (supportsMsaa()) {
      scenarios.emplace_back("msaa");
    }
    return scenarios;
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

  // Every variant of `scenario` that the device can run was validated at `size` and passed,
  // `classic` first
  void expectEveryVariantPassed(const TileBenchmarkSession& session,
                                const std::string& scenario,
                                const std::string& size) {
    EXPECT_TRUE(session.validationPassed());
    const Summaries& summaries = session.validationSummaries();
    ASSERT_FALSE(summaries.empty());
    EXPECT_EQ(summaries.front().variant, "classic");
    for (const auto& summary : summaries) {
      EXPECT_EQ(summary.scenario, scenario);
      EXPECT_EQ(summary.size, size);
      EXPECT_TRUE(summary.passed) << summary.variant << ": max difference " << summary.maxDiff
                                  << ", " << summary.fractionWithinOne << " within 1, "
                                  << summary.fractionOutliers << " outliers";
    }
    if (scenario == "msaa") {
      EXPECT_TRUE(hasVariant(summaries, "resolve"));
      EXPECT_TRUE(hasVariant(summaries, "memoryless"));
      return;
    }
    EXPECT_TRUE(hasVariant(summaries, scenario == "chain" ? "fused" : "forward"));
    EXPECT_EQ(hasVariant(summaries, "tiled"), supportsTileVariants());
    EXPECT_EQ(hasVariant(summaries, "tiled-private"), supportsTileVariants());
  }

  // The session had nothing to validate for `scenario` at `size`, and its report says that each of
  // `variants` is N/A
  void expectNotAvailable(const TileBenchmarkSession& session,
                          const LogCapture& capture,
                          const std::string& scenario,
                          const std::string& size,
                          const std::vector<std::string>& variants) {
    EXPECT_TRUE(session.validationSummaries().empty());
    for (const std::string& variant : variants) {
      std::string row = "TILEBENCH_V1,meta,na.";
      row += scenario;
      row += '.';
      row += size;
      row += '.';
      row += variant;
      row += ',';
      EXPECT_TRUE(capture.hasLine(row)) << variant;
    }
  }

  // A session keeps a pointer to its parameters
  ShellParams shellParams_;

 private:
  // The depth formats that the session tries
  static constexpr std::array<TextureFormat, 3> kDepthFormats = {TextureFormat::Z_UNorm24,
                                                                 TextureFormat::Z_UNorm32,
                                                                 TextureFormat::Z_UNorm16};
};

TEST_F(TileBenchmarkSessionTest, ValidatesEveryVariant) {
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"}, {"tile-sizes", "256x256"}, {"tile-scenarios", "chain"}});

  expectEveryVariantPassed(session, "chain", "256x256");
  EXPECT_TRUE(session.appParams().exitRequested);
}

// Eight stages are more than one render pass runs as subpasses
TEST_F(TileBenchmarkSessionTest, ValidatesASplitChain) {
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"},
               {"tile-sizes", "256x256"},
               {"tile-scenarios", "chain"},
               {"tile-chain-length", "7"}});

  expectEveryVariantPassed(session, "chain", "256x256");
}

TEST_F(TileBenchmarkSessionTest, ValidatesHalfFloatTargets) {
  const ICapabilities::TextureFormatCapabilities capabilities =
      platform_->getDevice().getTextureFormatCapabilities(TextureFormat::RGBA_F16);
  const bool supported =
      contains(capabilities, ICapabilities::TextureFormatCapabilityBits::Sampled) &&
      contains(capabilities, ICapabilities::TextureFormatCapabilityBits::Attachment);

  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"},
               {"tile-sizes", "256x256"},
               {"tile-scenarios", "chain"},
               {"tile-format", "rgba16f"},
               {"tile-chain-length", "7"}});

  if (supported) {
    expectEveryVariantPassed(session, "chain", "256x256");
  } else {
    EXPECT_TRUE(session.validationSummaries().empty());
    EXPECT_TRUE(capture.hasLine("TILEBENCH_V1,meta,unsupported,"));
  }
}

// `classic` is the reference, so it is validated even if it is not requested
TEST_F(TileBenchmarkSessionTest, ValidatesOnlyRequestedVariants) {
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"},
               {"tile-sizes", "256x256"},
               {"tile-scenarios", "chain"},
               {"tile-variants", "fused"}});

  EXPECT_TRUE(session.validationPassed());
  const Summaries& summaries = session.validationSummaries();
  ASSERT_EQ(summaries.size(), 2u);
  EXPECT_EQ(summaries[0].variant, "classic");
  EXPECT_EQ(summaries[1].variant, "fused");
}

TEST_F(TileBenchmarkSessionTest, ReportsTheTileVariantsAsNotAvailable) {
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"}, {"tile-sizes", "64x64"}, {"tile-scenarios", "chain"}});

  EXPECT_TRUE(session.validationPassed());
  for (const std::string variant : {"tiled", "tiled-private"}) {
    EXPECT_EQ(capture.hasLine("TILEBENCH_V1,meta,na.chain.64x64." + variant + ","),
              !supportsTileVariants())
        << variant;
  }
}

TEST_F(TileBenchmarkSessionTest, ValidatesDeferred) {
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(
      session,
      {{"tile-mode", "validate"}, {"tile-sizes", "256x256"}, {"tile-scenarios", "deferred"}});

  if (supportsDeferred()) {
    expectEveryVariantPassed(session, "deferred", "256x256");
  } else {
    expectNotAvailable(
        session, capture, "deferred", "256x256", {"classic", "tiled", "tiled-private", "forward"});
  }
}

TEST_F(TileBenchmarkSessionTest, ValidatesMsaa) {
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"}, {"tile-sizes", "256x256"}, {"tile-scenarios", "msaa"}});

  if (supportsMsaa()) {
    expectEveryVariantPassed(session, "msaa", "256x256");
  } else {
    expectNotAvailable(session, capture, "msaa", "256x256", {"classic", "resolve", "memoryless"});
  }
}

// Only the scenarios in `tile-scenarios` run, in the order of the session. A scenario that is not
// known is ignored, and all scenarios run if none is known.
TEST_F(TileBenchmarkSessionTest, RunsOnlyTheRequestedScenarios) {
  const auto run = [this](const std::string& scenarios) {
    TileBenchmarkSession session(platform_);
    runBlocking(session,
                {{"tile-mode", "validate"},
                 {"tile-sizes", "64x64"},
                 {"tile-chain-length", "1"},
                 {"tile-variants", "classic"},
                 {"tile-scenarios", scenarios}});
    EXPECT_TRUE(session.validationPassed());
    return getScenarios(session.validationSummaries());
  };

  const std::vector<std::string> runnable = getRunnableScenarios();
  const auto select = [&runnable](const std::vector<std::string>& wanted) {
    std::vector<std::string> selected;
    std::copy_if(runnable.begin(),
                 runnable.end(),
                 std::back_inserter(selected),
                 [&wanted](const std::string& scenario) {
                   return std::find(wanted.begin(), wanted.end(), scenario) != wanted.end();
                 });
    return selected;
  };

  EXPECT_EQ(run("chain,bogus"), std::vector<std::string>{"chain"});
  EXPECT_EQ(run("msaa,chain"), select({"chain", "msaa"}));
  EXPECT_EQ(run("deferred"), select({"deferred"}));
  EXPECT_EQ(run("bogus"), runnable);
}

// A scenario that has none of the variants in `tile-variants` does not run
TEST_F(TileBenchmarkSessionTest, SkipsTheScenariosWithoutTheRequestedVariants) {
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"},
               {"tile-sizes", "64x64"},
               {"tile-scenarios", "chain,msaa"},
               {"tile-variants", "fused"}});

  EXPECT_TRUE(session.validationPassed());
  EXPECT_EQ(getScenarios(session.validationSummaries()), std::vector<std::string>{"chain"});
}

// A run whose `tile-variants` has none of the variants of the requested scenarios logs an error,
// has nothing to validate, and still ends with the meta rows
TEST_F(TileBenchmarkSessionTest, LogsAnErrorWhenNoScenarioIsLeft) {
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"},
               {"tile-sizes", "64x64"},
               {"tile-scenarios", "msaa"},
               {"tile-variants", "fused"}});

  EXPECT_TRUE(session.validationSummaries().empty());
  EXPECT_FALSE(session.validationPassed());
  EXPECT_TRUE(capture.hasLine("TileBenchmarkSession: no scenario is left to run"));
  EXPECT_TRUE(capture.hasLine("TILEBENCH_V1,meta,tile-mode,validate"));
  EXPECT_TRUE(session.appParams().exitRequested);
}

// The msaa scenario needs at least 2 samples: it does not run with fewer
TEST_F(TileBenchmarkSessionTest, SkipsMsaaWithoutSamples) {
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"},
               {"tile-sizes", "64x64"},
               {"tile-chain-length", "1"},
               {"tile-variants", "classic"},
               {"tile-scenarios", "msaa,chain"},
               {"tile-msaa-samples", "1"}});

  EXPECT_EQ(getScenarios(session.validationSummaries()), std::vector<std::string>{"chain"});
  EXPECT_TRUE(capture.hasLine("TileBenchmarkSession: tile-msaa-samples is 1, which is below 2"));
  EXPECT_FALSE(capture.hasLine("TILEBENCH_V1,meta,na.msaa."));
}

// The report has the parameters that the session used: a number that the device cannot have is
// replaced, and a number that is not finite is the default
TEST_F(TileBenchmarkSessionTest, ReportsTheParametersItUsed) {
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "validate"},
               {"tile-sizes", "64x64"},
               {"tile-chain-length", "1"},
               {"tile-variants", "classic"},
               {"tile-scenarios", "chain,msaa"},
               {"tile-msaa-samples", "3"},
               {"tile-phase-seconds", "nan"}});

  // A device with multisampling raises 3 to 4, and limits it to what the device can do
  const size_t samples = getMaxSamples() >= 2 ? getSamples(3) : 3;
  EXPECT_TRUE(capture.hasLine("TILEBENCH_V1,meta,tile-msaa-samples," + std::to_string(samples)));
  EXPECT_TRUE(capture.hasLine("TILEBENCH_V1,meta,tile-phase-seconds,5"));
}

TEST_F(TileBenchmarkSessionTest, StepsOneSizePerUpdateWhenNotBlocking) {
  TileBenchmarkSession session(platform_);
  initialize(session,
             {{"tile-mode", "validate"},
              {"tile-blocking", "0"},
              {"tile-sizes", "64x64,96x96"},
              {"tile-scenarios", "chain"},
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
  initialize(session,
             {{"tile-mode", "validate"},
              {"tile-blocking", "0"},
              {"tile-sizes", "64x64"},
              {"tile-scenarios", "chain"}});

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
               {"tile-scenarios", "chain"},
               {"tile-rounds", "1"},
               {"tile-warmup", "0"},
               {"tile-iterations", "1"},
               {"tile-timer", "cpu"}});

  expectEveryVariantPassed(session, "chain", "64x64");
  EXPECT_TRUE(session.appParams().exitRequested);
}

// The modeled traffic of the msaa variants has the structure that the scenario is for, which a
// comparison of the images cannot show. With S samples of 4 bytes, `classic` stores the samples,
// loads them and writes the resolved image: (8 * S + 4) bytes for each pixel. The others write the
// resolved image only: 4 bytes.
TEST_F(TileBenchmarkSessionTest, ModelsTheTrafficOfMsaa) {
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "bench"},
               {"tile-sizes", "64x64"},
               {"tile-scenarios", "msaa"},
               {"tile-msaa-samples", "4"},
               {"tile-rounds", "1"},
               {"tile-warmup", "0"},
               {"tile-iterations", "1"},
               {"tile-timer", "cpu"}});

  if (!supportsMsaa()) {
    expectNotAvailable(session, capture, "msaa", "64x64", {"classic", "resolve", "memoryless"});
    return;
  }
  const uint64_t pixels = 64 * 64;
  const uint64_t samples = getSamples(4);
  EXPECT_EQ(getModeledBytes(capture, "msaa", "64x64", "classic"),
            std::make_optional((8 * samples + 4) * pixels));
  EXPECT_EQ(getModeledBytes(capture, "msaa", "64x64", "resolve"), std::make_optional(4 * pixels));
  EXPECT_EQ(getModeledBytes(capture, "msaa", "64x64", "memoryless"),
            std::make_optional(4 * pixels));
}

// A phase run validates the size, then logs the begin and the end of the phase of each variant, one
// after the other
TEST_F(TileBenchmarkSessionTest, PhaseModeLogsPhases) {
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  runBlocking(session,
              {{"tile-mode", "phases"},
               {"tile-phase-seconds", "0.05"},
               {"tile-iterations", "1"},
               {"tile-sizes", "64x64"},
               {"tile-scenarios", "chain"},
               {"tile-variants", "classic,fused"}});

  EXPECT_TRUE(session.validationPassed());
  EXPECT_TRUE(session.appParams().exitRequested);
  EXPECT_FALSE(capture.hasLine("TILEBENCH_V1,summary,"));

  const std::vector<PhaseRow> rows = getPhaseRows(capture.getLines());
  ASSERT_EQ(rows.size(), 4u);
  int64_t previousEnd = std::numeric_limits<int64_t>::min();
  const std::array<std::string, 2> variants = {"classic", "fused"};
  for (size_t i = 0; i != variants.size(); ++i) {
    const PhaseRow& begin = rows[2 * i];
    const PhaseRow& end = rows[2 * i + 1];
    EXPECT_EQ(begin.edge, "begin");
    EXPECT_EQ(end.edge, "end");
    for (const PhaseRow* row : {&begin, &end}) {
      EXPECT_EQ(row->scenario, "chain");
      EXPECT_EQ(row->size, "64x64");
      EXPECT_EQ(row->variant, variants[i]);
    }
    EXPECT_EQ(begin.executions, 0u);
    EXPECT_GT(end.executions, 0u) << variants[i] << " did not execute";
    // The rows have the steady clock in seconds, to microseconds
    EXPECT_GE(end.microseconds - begin.microseconds, 49000) << variants[i];
    EXPECT_GE(begin.microseconds, previousEnd)
        << variants[i] << " began before the phase before it";
    previousEnd = end.microseconds;
  }
}

// A run that does not block gives each update() at most a slice of a phase, so a phase spans
// several of them, and its rows still bracket all of its work
TEST_F(TileBenchmarkSessionTest, PhaseModeResumesAcrossUpdates) {
  constexpr size_t kMaxUpdates = 2000;
  const LogCapture capture;
  TileBenchmarkSession session(platform_);
  initialize(session,
             {{"tile-mode", "phases"},
              {"tile-blocking", "0"},
              {"tile-phase-seconds", "0.2"},
              {"tile-iterations", "1"},
              {"tile-sizes", "64x64"},
              {"tile-scenarios", "chain"},
              {"tile-variants", "classic"}});

  std::optional<size_t> beginUpdate;
  std::optional<size_t> endUpdate;
  size_t numUpdates = 0;
  while (!endUpdate.has_value() && numUpdates < kMaxUpdates) {
    update(session);
    ++numUpdates;
    const std::vector<PhaseRow> rows = getPhaseRows(capture.getLines());
    if (!rows.empty() && !beginUpdate.has_value()) {
      beginUpdate = numUpdates;
    }
    if (rows.size() == 2) {
      endUpdate = numUpdates;
    }
  }
  session.teardown();

  ASSERT_TRUE(endUpdate.has_value()) << "the phase did not end in " << kMaxUpdates << " updates";
  EXPECT_GE(*endUpdate - *beginUpdate, 2u);
  EXPECT_FALSE(session.appParams().exitRequested);
  const std::vector<PhaseRow> rows = getPhaseRows(capture.getLines());
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].edge, "begin");
  EXPECT_EQ(rows[1].edge, "end");
  EXPECT_EQ(rows[1].variant, "classic");
  EXPECT_GT(rows[1].executions, 0u);
  EXPECT_GE(rows[1].microseconds - rows[0].microseconds, 199000);
}

// For manual runs: logs the result table of the default benchmark at 1920x1080
TEST_F(TileBenchmarkSessionTest, DISABLED_Benchmark1080p) {
  TileBenchmarkSession session(platform_);
  runBlocking(session, {{"tile-sizes", "1920x1080"}, {"tile-scenarios", "chain"}});

  expectEveryVariantPassed(session, "chain", "1920x1080");
}

} // namespace igl::shell
