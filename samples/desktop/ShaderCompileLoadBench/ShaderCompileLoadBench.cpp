/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#endif

#include <igl/Common.h>
#include <igl/ComputePipelineState.h>
#include <igl/Device.h>
#include <igl/RenderPipelineState.h>
#include <igl/ShaderCreator.h>
#include <igl/glslang/GlslCompiler.h>
#include <igl/glslang/GlslangHelpers.h>
#include <igl/tests/util/device/TestDevice.h>
#include <igl/vulkan/ComputePipelineState.h>

namespace {

constexpr size_t kDefaultIterations = 31;

constexpr std::string_view kVulkanComputeShader = R"(#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {}
)";

constexpr std::string_view kOpenGLDesktopVertexShader = R"(
#version 150
out vec4 vColor;

void main() {
  int id = gl_VertexID;
  float x = id == 1 ? 3.0 : -1.0;
  float y = id == 2 ? 3.0 : -1.0;
  gl_Position = vec4(x, y, 0.0, 1.0);
  vColor = vec4(float(id == 0), float(id == 1), float(id == 2), 1.0);
}
)";

constexpr std::string_view kOpenGLDesktopFragmentShader = R"(
#version 150
in vec4 vColor;
out vec4 outColor;

void main() {
  outColor = vColor;
}
)";

constexpr std::string_view kOpenGLESVertexShader = R"(
#version 300 es
out highp vec4 vColor;

void main() {
  int id = gl_VertexID;
  highp float x = id == 1 ? 3.0 : -1.0;
  highp float y = id == 2 ? 3.0 : -1.0;
  gl_Position = vec4(x, y, 0.0, 1.0);
  vColor = vec4(float(id == 0), float(id == 1), float(id == 2), 1.0);
}
)";

constexpr std::string_view kOpenGLESFragmentShader = R"(
#version 300 es
precision highp float;
in vec4 vColor;
out vec4 outColor;

void main() {
  outColor = vColor;
}
)";

constexpr std::string_view kMetalLibrarySource = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
  float4 position [[position]];
  float4 color;
};

vertex VertexOut vertexMain(uint vertexID [[vertex_id]]) {
  VertexOut out;
  const float x = vertexID == 1 ? 3.0 : -1.0;
  const float y = vertexID == 2 ? 3.0 : -1.0;
  out.position = float4(x, y, 0.0, 1.0);
  out.color = float4(vertexID == 0, vertexID == 1, vertexID == 2, 1.0);
  return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]]) {
  return in.color;
}
)";

enum class Status : uint8_t {
  Passed,
  Skipped,
  Failed,
};

struct Options {
  std::string backend = "all";
  std::string deviceTier = "unknown";
  std::string metallibFragmentEntry = "fragmentMain";
  std::string metallibPath;
  std::string metallibVertexEntry = "vertexMain";
  size_t iterations = kDefaultIterations;
  bool selfTest = false;
  bool help = false;
};

struct Stats {
  double coldMs = 0.0;
  double warmMedianMs = 0.0;
  double warmP95Ms = 0.0;
  size_t warmSamples = 0;
};

struct BenchResult {
  std::string backend;
  std::string operation;
  std::string deviceTier;
  Status status = Status::Failed;
  Stats stats;
  std::string message;
};

[[nodiscard]] bool startsWith(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] std::string statusToString(Status status) {
  switch (status) {
  case Status::Passed:
    return "passed";
  case Status::Skipped:
    return "skipped";
  case Status::Failed:
  default:
    return "failed";
  }
}

[[nodiscard]] double medianOfSorted(const std::vector<double>& sorted) {
  if (sorted.empty()) {
    return 0.0;
  }
  const size_t mid = sorted.size() / 2;
  if (sorted.size() % 2 == 0) {
    return (sorted[mid - 1] + sorted[mid]) * 0.5;
  }
  return sorted[mid];
}

[[nodiscard]] double percentileNearestRankOfSorted(const std::vector<double>& sorted,
                                                   double percentile) {
  if (sorted.empty()) {
    return 0.0;
  }
  const auto rank = static_cast<size_t>(std::ceil(percentile * static_cast<double>(sorted.size())));
  const size_t index = std::min(sorted.size() - 1, rank == 0 ? 0 : rank - 1);
  return sorted[index];
}

[[nodiscard]] Stats computeStats(const std::vector<double>& samplesMs) {
  Stats stats;
  if (samplesMs.empty()) {
    return stats;
  }

  stats.coldMs = samplesMs.front();

  std::vector<double> warmSamples(samplesMs.begin() + 1, samplesMs.end());
  if (warmSamples.empty()) {
    warmSamples = samplesMs;
  }
  std::sort(warmSamples.begin(), warmSamples.end());
  stats.warmSamples = warmSamples.size();
  stats.warmMedianMs = medianOfSorted(warmSamples);
  stats.warmP95Ms = percentileNearestRankOfSorted(warmSamples, 0.95);
  return stats;
}

[[nodiscard]] bool parseSizeT(std::string_view text, size_t& value) {
  if (text.empty()) {
    return false;
  }
  size_t parsed = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto [ptr, error] = std::from_chars(begin, end, parsed);
  if (error != std::errc{} || ptr != end) {
    return false;
  }
  value = parsed;
  return true;
}

[[nodiscard]] bool parseOptions(int argc, char** argv, Options& options, std::string& error) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--help" || arg == "-h") {
      options.help = true;
    } else if (arg == "--self-test") {
      options.selfTest = true;
    } else if (startsWith(arg, "--backend=")) {
      options.backend = std::string(arg.substr(std::string_view("--backend=").size()));
    } else if (startsWith(arg, "--iterations=")) {
      if (!parseSizeT(arg.substr(std::string_view("--iterations=").size()), options.iterations)) {
        error = "invalid --iterations value";
        return false;
      }
    } else if (startsWith(arg, "--device-tier=")) {
      options.deviceTier = std::string(arg.substr(std::string_view("--device-tier=").size()));
    } else if (startsWith(arg, "--metallib=")) {
      options.metallibPath = std::string(arg.substr(std::string_view("--metallib=").size()));
    } else if (startsWith(arg, "--metal-vertex-entry=")) {
      options.metallibVertexEntry =
          std::string(arg.substr(std::string_view("--metal-vertex-entry=").size()));
    } else if (startsWith(arg, "--metal-fragment-entry=")) {
      options.metallibFragmentEntry =
          std::string(arg.substr(std::string_view("--metal-fragment-entry=").size()));
    } else {
      error = "unknown argument: " + std::string(arg);
      return false;
    }
  }

  if (options.iterations < 2) {
    error = "--iterations must be at least 2 so cold and warm samples are both reported";
    return false;
  }
  return true;
}

void printUsage(std::ostream& os) {
  os << "Usage: IGLShaderCompileLoadBench [options]\n"
     << "  --backend=all|vulkan|opengl|metal|metal-msl|metal-metallib\n"
     << "  --iterations=N              total samples; sample 0 is cold, remaining samples are "
        "warm\n"
     << "  --device-tier=top|mid|low   metadata emitted with each row\n"
     << "  --metallib=/path/file       required for metal-metallib load measurements\n"
     << "  --metal-vertex-entry=name   vertex entry point in --metallib; default vertexMain\n"
     << "  --metal-fragment-entry=name fragment entry point in --metallib; default fragmentMain\n"
     << "  --self-test                 validate argument parsing/statistics without GPU work\n";
}

[[nodiscard]] bool wantsBackend(const Options& options, std::string_view backend) {
  if (options.backend == "all") {
    return true;
  }
  if (backend == "metal-msl" || backend == "metal-metallib") {
    return options.backend == backend || options.backend == "metal";
  }
  return options.backend == backend;
}

template<typename SampleFunc>
[[nodiscard]] BenchResult measureOperation(std::string backend,
                                           std::string operation,
                                           const Options& options,
                                           SampleFunc&& sampleFunc) {
  BenchResult result{
      .backend = std::move(backend),
      .operation = std::move(operation),
      .deviceTier = options.deviceTier,
  };

  std::vector<double> samplesMs;
  samplesMs.reserve(options.iterations);

  for (size_t i = 0; i < options.iterations; ++i) {
    std::string sampleError;
    const auto start = std::chrono::steady_clock::now();
    const bool ok = sampleFunc(i, sampleError);
    const auto end = std::chrono::steady_clock::now();
    if (!ok) {
      result.status = Status::Failed;
      result.message = "iteration " + std::to_string(i) + " failed";
      if (!sampleError.empty()) {
        result.message += ": " + sampleError;
      }
      return result;
    }
    const std::chrono::duration<double, std::milli> elapsed = end - start;
    samplesMs.push_back(elapsed.count());
  }

  result.status = Status::Passed;
  result.stats = computeStats(samplesMs);
  return result;
}

[[nodiscard]] BenchResult skippedResult(std::string backend,
                                        std::string operation,
                                        const Options& options,
                                        std::string message) {
  return BenchResult{
      .backend = std::move(backend),
      .operation = std::move(operation),
      .deviceTier = options.deviceTier,
      .status = Status::Skipped,
      .message = std::move(message),
  };
}

[[nodiscard]] std::string resultMessage(const igl::Result& result) {
  if (!result.message.empty()) {
    return result.message;
  }
  std::ostringstream os;
  os << "IGL Result code " << static_cast<int>(result.code);
  return os.str();
}

[[nodiscard]] std::string warmSemanticsForBackend(std::string_view backend) {
  if (backend == "vulkan") {
    return "repeated SPIR-V module and compute-pipeline creation on the same device";
  }
  if (backend == "metal-metallib") {
    return "repeated precompiled library data load on the same device";
  }
  if (backend == "opengl") {
    return "repeated GLSL shader compile/link on the same context";
  }
  if (backend == "metal-msl") {
    return "repeated MSL source compile on the same device";
  }
  return "samples after the first cold sample";
}

[[nodiscard]] bool readBinaryFile(const std::string& path,
                                  std::vector<uint8_t>& data,
                                  std::string& error) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    error = "failed to open " + path;
    return false;
  }

  const std::ifstream::pos_type size = file.tellg();
  if (size <= 0) {
    error = "empty file " + path;
    return false;
  }

  data.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
    error = "failed to read " + path;
    return false;
  }
  return true;
}

[[nodiscard]] bool compileVulkanComputeSpirv(std::vector<uint32_t>& spirv, std::string& error) {
  glslang_resource_t resource{};
  glslangGetDefaultResource(&resource);

  igl::glslang::initializeCompiler();
  const igl::ShaderCompilerOptions options{
      .fastMathEnabled = true,
      .optimization = igl::ShaderOptimization::Performance,
  };
  const igl::Result result = igl::glslang::compileShader(
      igl::ShaderStage::Compute, kVulkanComputeShader.data(), spirv, &resource, options);
  igl::glslang::finalizeCompiler();

  if (!result.isOk()) {
    error = resultMessage(result);
    return false;
  }
  if (spirv.empty()) {
    error = "glslang produced empty SPIR-V";
    return false;
  }
  return true;
}

[[nodiscard]] bool isVulkanLoaderLikelyAvailable(std::string& error) {
#if defined(IGL_USE_STATIC_KOSMICKRISP) || \
    (defined(FORCE_USE_STATIC_VULKAN_LOADER) && !defined(FORCE_USE_STATIC_VULKAN_LOADER_DISABLED))
  return true;
#elif defined(__APPLE__) || defined(__linux__)
#if defined(__APPLE__)
  const char* loaderNames[] = {
      "libvulkan.dylib",
      "libvulkan.1.dylib",
      "libMoltenVK.dylib",
  };
#else
  const char* loaderNames[] = {
      "libvulkan.so.1",
      "libvulkan.so",
  };
#endif
  std::ostringstream tried;
  for (const char* loaderName : loaderNames) {
    void* handle = dlopen(loaderName, RTLD_LAZY | RTLD_LOCAL);
    if (handle != nullptr) {
      dlclose(handle);
      return true;
    }
    if (tried.tellp() > 0) {
      tried << ", ";
    }
    tried << loaderName;
  }
  error = "Vulkan loader unavailable (tried " + tried.str() + ")";
  return false;
#else
  return true;
#endif
}

[[nodiscard]] BenchResult runVulkanBench(const Options& options) {
  std::string loaderError;
  if (!isVulkanLoaderLikelyAvailable(loaderError)) {
    return skippedResult("vulkan", "VkShaderModule+vkCreateComputePipelines", options, loaderError);
  }

  igl::tests::util::device::TestDeviceConfig config{
      .enableVulkanValidationLayers = false,
  };
  std::unique_ptr<igl::IDevice> device =
      igl::tests::util::device::createTestDevice(igl::BackendType::Vulkan, config);
  if (!device) {
    return skippedResult(
        "vulkan", "VkShaderModule+vkCreateComputePipelines", options, "Vulkan device unavailable");
  }

  std::vector<uint32_t> spirv;
  std::string compileError;
  if (!compileVulkanComputeSpirv(spirv, compileError)) {
    BenchResult result =
        skippedResult("vulkan", "VkShaderModule+vkCreateComputePipelines", options, compileError);
    result.status = Status::Failed;
    return result;
  }

  std::vector<std::shared_ptr<igl::IShaderStages>> retainedStages;
  std::vector<std::shared_ptr<igl::IComputePipelineState>> retainedPipelines;
  retainedStages.reserve(options.iterations);
  retainedPipelines.reserve(options.iterations);

  return measureOperation(
      "vulkan",
      "VkShaderModule+vkCreateComputePipelines",
      options,
      [&](size_t iteration, std::string& error) {
        igl::Result result;
        std::unique_ptr<igl::IShaderStages> stages =
            igl::ShaderStagesCreator::fromModuleBinaryInput(*device,
                                                            spirv.data(),
                                                            spirv.size() * sizeof(uint32_t),
                                                            "main",
                                                            "ShaderCompileLoadBenchCompute",
                                                            &result);
        if (!result.isOk() || !stages) {
          error = resultMessage(result);
          return false;
        }

        std::shared_ptr<igl::IShaderStages> sharedStages = std::move(stages);
        const igl::ComputePipelineDesc pipelineDesc{
            .shaderStages = sharedStages,
            .debugName = "ShaderCompileLoadBenchVulkan" + std::to_string(iteration),
        };
        std::shared_ptr<igl::IComputePipelineState> pipeline =
            device->createComputePipeline(pipelineDesc, &result);
        if (!result.isOk() || !pipeline) {
          error = resultMessage(result);
          return false;
        }

        auto* vulkanPipeline = static_cast<igl::vulkan::ComputePipelineState*>(pipeline.get());
        if (vulkanPipeline->getVkPipeline() == VK_NULL_HANDLE) {
          error = "vkCreateComputePipelines returned VK_NULL_HANDLE";
          return false;
        }

        retainedStages.push_back(std::move(sharedStages));
        retainedPipelines.push_back(std::move(pipeline));
        return true;
      });
}

[[nodiscard]] std::pair<std::string_view, std::string_view> getOpenGLShaders(
    const igl::IDevice& device) {
  const igl::BackendVersion version = device.getBackendVersion();
  if (version.flavor == igl::BackendFlavor::OpenGL_ES) {
    return {kOpenGLESVertexShader, kOpenGLESFragmentShader};
  }
  return {kOpenGLDesktopVertexShader, kOpenGLDesktopFragmentShader};
}

[[nodiscard]] BenchResult runOpenGLBench(const Options& options) {
  std::unique_ptr<igl::IDevice> device =
      igl::tests::util::device::createTestDevice(igl::BackendType::OpenGL);
  if (!device) {
    return skippedResult(
        "opengl", "glCompileShader+glLinkProgram", options, "OpenGL device unavailable");
  }

  const auto [vertexShader, fragmentShader] = getOpenGLShaders(*device);

  std::vector<std::shared_ptr<igl::IShaderStages>> retainedStages;
  std::vector<std::shared_ptr<igl::IRenderPipelineState>> retainedPipelines;
  retainedStages.reserve(options.iterations);
  retainedPipelines.reserve(options.iterations);

  return measureOperation("opengl",
                          "glCompileShader+glLinkProgram",
                          options,
                          [&](size_t iteration, std::string& error) {
                            igl::Result result;
                            std::unique_ptr<igl::IShaderStages> stages =
                                igl::ShaderStagesCreator::fromModuleStringInput(
                                    *device,
                                    vertexShader.data(),
                                    "main",
                                    "ShaderCompileLoadBenchVertex",
                                    fragmentShader.data(),
                                    "main",
                                    "ShaderCompileLoadBenchFragment",
                                    &result);
                            if (!result.isOk() || !stages) {
                              error = resultMessage(result);
                              return false;
                            }

                            std::shared_ptr<igl::IShaderStages> sharedStages = std::move(stages);
                            const igl::RenderPipelineDesc pipelineDesc{
                                .shaderStages = sharedStages,
                                .targetDesc = {.colorAttachments = {{
                                                   .textureFormat = igl::TextureFormat::RGBA_UNorm8,
                                               }}},
                            };

                            std::shared_ptr<igl::IRenderPipelineState> pipeline =
                                device->createRenderPipeline(pipelineDesc, &result);
                            if (!result.isOk() || !pipeline) {
                              error = resultMessage(result);
                              return false;
                            }

                            (void)iteration;
                            retainedStages.push_back(std::move(sharedStages));
                            retainedPipelines.push_back(std::move(pipeline));
                            return true;
                          });
}

[[nodiscard]] BenchResult runMetalSourceBench(const Options& options) {
  std::unique_ptr<igl::IDevice> device =
      igl::tests::util::device::createTestDevice(igl::BackendType::Metal);
  if (!device) {
    return skippedResult("metal-msl", "newLibraryWithSource", options, "Metal device unavailable");
  }

  std::vector<std::unique_ptr<igl::IShaderLibrary>> retainedLibraries;
  retainedLibraries.reserve(options.iterations);

  return measureOperation(
      "metal-msl", "newLibraryWithSource", options, [&](size_t iteration, std::string& error) {
        igl::Result result;
        std::unique_ptr<igl::IShaderLibrary> library = igl::ShaderLibraryCreator::fromStringInput(
            *device,
            kMetalLibrarySource.data(),
            "vertexMain",
            "fragmentMain",
            "ShaderCompileLoadBenchMetalSource" + std::to_string(iteration),
            &result);
        if (!result.isOk() || !library) {
          error = resultMessage(result);
          return false;
        }
        retainedLibraries.push_back(std::move(library));
        return true;
      });
}

[[nodiscard]] BenchResult runMetalLibBench(const Options& options) {
  if (options.metallibPath.empty()) {
    return skippedResult("metal-metallib",
                         "newLibraryWithData",
                         options,
                         "provide --metallib=/path/to/precompiled.metallib");
  }

  std::vector<uint8_t> metallib;
  std::string readError;
  if (!readBinaryFile(options.metallibPath, metallib, readError)) {
    BenchResult result = skippedResult("metal-metallib", "newLibraryWithData", options, readError);
    result.status = Status::Failed;
    return result;
  }

  std::unique_ptr<igl::IDevice> device =
      igl::tests::util::device::createTestDevice(igl::BackendType::Metal);
  if (!device) {
    return skippedResult(
        "metal-metallib", "newLibraryWithData", options, "Metal device unavailable");
  }

  std::vector<std::unique_ptr<igl::IShaderLibrary>> retainedLibraries;
  retainedLibraries.reserve(options.iterations);

  return measureOperation(
      "metal-metallib", "newLibraryWithData", options, [&](size_t iteration, std::string& error) {
        igl::Result result;
        std::unique_ptr<igl::IShaderLibrary> library = igl::ShaderLibraryCreator::fromBinaryInput(
            *device,
            metallib.data(),
            metallib.size(),
            options.metallibVertexEntry,
            options.metallibFragmentEntry,
            "ShaderCompileLoadBenchMetalLib" + std::to_string(iteration),
            &result);
        if (!result.isOk() || !library) {
          error = resultMessage(result);
          return false;
        }
        retainedLibraries.push_back(std::move(library));
        return true;
      });
}

void printResult(const BenchResult& result) {
  std::cout << result.backend << " | " << result.operation << " | tier=" << result.deviceTier
            << " | " << statusToString(result.status);
  if (result.status == Status::Passed) {
    std::cout << std::fixed << std::setprecision(3) << " | cold_ms=" << result.stats.coldMs
              << " | warm_median_ms=" << result.stats.warmMedianMs
              << " | warm_p95_ms=" << result.stats.warmP95Ms
              << " | warm_n=" << result.stats.warmSamples
              << " | warm_semantics=" << warmSemanticsForBackend(result.backend);
  }
  if (!result.message.empty()) {
    std::cout << " | " << result.message;
  }
  std::cout << '\n';
}

[[nodiscard]] int runSelfTest() {
  const Stats stats = computeStats({5.0, 1.0, 4.0, 2.0, 3.0});
  if (stats.coldMs != 5.0 || stats.warmSamples != 4 || stats.warmMedianMs != 2.5 ||
      stats.warmP95Ms != 4.0) {
    std::cerr << "self-test failed: statistics mismatch\n";
    return 1;
  }

  Options options;
  std::string error;
  const char* argv[] = {
      "IGLShaderCompileLoadBench",
      "--backend=opengl",
      "--iterations=7",
      "--device-tier=mid",
  };
  if (!parseOptions(4, const_cast<char**>(argv), options, error) || options.backend != "opengl" ||
      options.iterations != 7 || options.deviceTier != "mid") {
    std::cerr << "self-test failed: option parsing mismatch\n";
    return 1;
  }

  Options invalidOptions;
  std::string invalidError;
  const char* invalidArgv[] = {
      "IGLShaderCompileLoadBench",
      "--iterations=-1",
  };
  if (parseOptions(2, const_cast<char**>(invalidArgv), invalidOptions, invalidError)) {
    std::cerr << "self-test failed: negative iteration count accepted\n";
    return 1;
  }

  std::cout << "self-test passed\n";
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  Options options;
  std::string error;
  if (!parseOptions(argc, argv, options, error)) {
    std::cerr << error << "\n\n";
    printUsage(std::cerr);
    return 2;
  }

  if (options.help) {
    printUsage(std::cout);
    return 0;
  }

  if (options.selfTest) {
    return runSelfTest();
  }

  igl::setDebugBreakEnabled(false);

  std::vector<BenchResult> results;
  if (wantsBackend(options, "vulkan")) {
    results.push_back(runVulkanBench(options));
  }
  if (wantsBackend(options, "opengl")) {
    results.push_back(runOpenGLBench(options));
  }
  if (wantsBackend(options, "metal-msl")) {
    results.push_back(runMetalSourceBench(options));
  }
  if (wantsBackend(options, "metal-metallib")) {
    results.push_back(runMetalLibBench(options));
  }

  if (results.empty()) {
    std::cerr << "No backends selected by --backend=" << options.backend << '\n';
    printUsage(std::cerr);
    return 2;
  }

  size_t passed = 0;
  size_t skipped = 0;
  size_t failed = 0;
  for (const BenchResult& result : results) {
    printResult(result);
    switch (result.status) {
    case Status::Passed:
      ++passed;
      break;
    case Status::Skipped:
      ++skipped;
      break;
    case Status::Failed:
      ++failed;
      break;
    default:
      ++failed;
      break;
    }
  }

  std::cout << "summary | passed=" << passed << " | skipped=" << skipped << " | failed=" << failed
            << '\n';
  return failed == 0 ? 0 : 1;
}
