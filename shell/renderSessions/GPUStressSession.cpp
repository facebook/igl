/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "GPUStressSession.h"
#include <IGLU/imgui/Session.h>
#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/fwd.hpp>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>

#include <memory>
#include <random>
#include <shell/shared/renderSession/QuadLayerParams.h>
#include <shell/shared/renderSession/ShellParams.h>

#if defined(_MSC_VER) || (defined(__clang__) && IGL_PLATFORM_LINUX)
static uint32_t arc4random(void) {
  return static_cast<uint32_t>(rand());
}
#endif // _MSC_VER

#if ANDROID

#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace igl::shell {

struct VertexPosUvw {
  glm::vec3 position;
  glm::vec4 uvw;
  glm::vec4 base_color;
};

namespace {

std::atomic<int> kCubeCount = 1; // number of cubes in the vertex buffer
// number of times to draw the vertex buffer (triangles = 12 * kDrawCount *
// kCubeCount)
std::atomic<int> kDrawCount = 50;
// turn this on and set kDrawCount to 1.  Cube count will be the number of
// layers you'll see
std::atomic<bool> kTestOverdraw = false;

std::atomic<bool> kEnableBlending = false; // turn this on to see the effects of alpha blending
// make this number little to make all the cubes tiny on screen so fill isn't a
// problem
std::atomic<bool> kUseMSAA = true;
const int kMsaaSamples = 4; // this is the max number possible
const float kScaleFill = 1.f;
// each light will add about 45 ish instructions to your pixel shader (tested
// using powerVR compiler so grain of salt)arc lint --engine LintCPP
std::atomic<int> kLightCount = 5;
// number of times to do a lof of math that does not calculate pi
std::atomic<int> kGoSlowOnCpu = 10000; // max10000000;
// cpu threads - these are really necessary to get our CPU usage up  to 100%
// (otherwise the framerate just throttles)
std::atomic<int> kThreadCount = 1;
std::atomic<bool> kThrashMemory = true;
std::atomic<size_t> kMemorySize = 64; // in MB
std::atomic<unsigned long> kMemoryReads = 10000; // max 1000000;
std::atomic<unsigned long> kMemoryWrites = 10000; // 100000;
std::vector<int> threadIds = {-1, -1, -1, -1, -1, -1, -1, -1};
std::atomic<int> kDropFrameX = 0;
std::atomic<int> kDropFrameCount = 2;
std::atomic<bool> kRotateCubes = true;

const float half = .5f;

// single cube - 8 verts and 36 indicies
const std::vector<VertexPosUvw> vertexData0 = {
    {{-half, half, -half}, {0.0, 1.0, 0.0, 1.0}, {1.0, 1.0, 1.0, 1.0}},
    {{half, half, -half}, {1.0, 1.0, 1.0, 1.0}, {1.0, 1.0, 1.0, 1.0}},
    {{-half, -half, -half}, {0.0, 0.0, 0.0, 0.0}, {1.0, 1.0, 1.0, 1.0}},
    {{half, -half, -half}, {1.0, 0.0, 1.0, 0.0}, {1.0, 1.0, 1.0, 1.0}},
    {{half, half, half}, {1.0, 1.0, 1.0, 1.0}, {1.0, 1.0, 1.0, 1.0}},
    {{-half, half, half}, {0.0, 1.0, 0.0, 1.0}, {1.0, 1.0, 1.0, 1.0}},
    {{half, -half, half}, {1.0, 0.0, 1.0, 0.0}, {1.0, 1.0, 1.0, 1.0}},
    {{-half, -half, half}, {0.0, 0.0, 0.0, 0.0}, {1.0, 1.0, 1.0, 1.0}},
};

std::vector<VertexPosUvw> vertexData;
const std::vector<uint16_t> indexData0 = {0, 1, 2, 1, 3, 2, 1, 4, 3, 4, 6, 3, 4, 5, 6, 5, 7, 6,
                                          5, 0, 7, 0, 2, 7, 5, 4, 0, 4, 1, 0, 2, 3, 7, 3, 6, 7};

std::vector<uint16_t> indexData = indexData0;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::string getLightingFunc(const char* matrixProj, const char* matrixMod) {
  std::string const var1 = matrixProj;
  std::string const var2 = matrixMod;
  auto func = std::string(
      R"(

      vec3 calcLighting(vec3 lightDir, vec3 lightPosition,  vec3 normal, float attenuation, vec3 color)
      {
        normal.xyz = ()" +
      var1 + "*" + var2 +
      R"(* vec4(normal, 0.f)).xyz;
        normal = normalize(normal);
        float angle = dot(normalize(lightDir), normal);
        float distance = length(lightPosition - screen_pos);
        float intensity = smoothstep(attenuation, 0.f, distance);
        intensity = clamp(intensity, 0.0, 1.0);
        return intensity * color * angle;
      }
      )");

  return func;
}
std::string getLightingCalc() {
  std::string params = "\nvec4 lightFactor = color;\n";
  if (kLightCount) {
    params = "\nvec4 lightFactor = vec4(0.2, 0.2, 0.2, 1.0);\n";
  }
  for (int i = 0; i < kLightCount; ++i) {
    char tmp[256];
    snprintf(tmp,
             sizeof(tmp),
             "const vec3 lightColor%d = vec3(%f, %f, %f);\n",
             i,
             i % 3 == 0 ? 1.0 : static_cast<float>(arc4random() % 32) / 32.f,
             i % 3 == 1 ? 1.0 : static_cast<float>(arc4random() % 32) / 32.f,
             i % 3 == 2 ? 1.0 : static_cast<float>(arc4random() % 32) / 32.f);
    params += tmp;
    snprintf(tmp,
             sizeof(tmp),
             "const vec3 lightPos%d = vec3(%f, %f, %f);\n",
             i,
             -1.f + static_cast<float>(arc4random() % 32) / 16.f,
             -1.f + static_cast<float>(arc4random() % 32) / 16.f,
             -1.f + static_cast<float>(arc4random() % 32) / 16.f);
    params += tmp;
    snprintf(
        tmp,
        sizeof(tmp),
        "lightFactor.xyz += calcLighting(-lightPos%d, lightPos%d, color.xyz, 1.0, lightColor%d);\n",
        i,
        i,
        i);
    params += tmp;
  }
  return params;
}

std::string getVulkanVertexShaderSource(bool bMultiView) {
  return std::string(bMultiView ? "\n#define MULTIVIEW 1\n" : "") + R"(
#ifdef MULTIVIEW
#extension GL_EXT_multiview : enable
#endif
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 uvw_in;
layout(location = 2) in vec4 base_color;

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 uv;
layout (location = 2) out vec3 screen_pos;

layout(push_constant) uniform PushConstants {
    mat4 projectionMatrix;
    mat4 modelViewMatrix;
} pc;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  #ifdef MULTIVIEW
    color = vec4(base_color.x, abs(float(gl_ViewIndex)-1.f) * base_color.y, base_color.z, base_color.w);
  #elif
    color = base_color;
  #endif

    uv = uvw_in;
    gl_Position = pc.projectionMatrix * pc.modelViewMatrix * vec4(position.xyz, 1.0);
    screen_pos = gl_Position.xyz/gl_Position.w;
})";
}

std::string getVulkanFragmentShaderSource() {
  return R"(
layout(location = 0) out vec4 fColor;
layout(location = 0) in vec4 color;
layout(location = 1) in vec4 uv;
layout(location = 2) in vec3 screen_pos;

layout (set = 0, binding = 0) uniform sampler2D uTex;
layout (set = 0, binding = 1) uniform sampler2D uTex2;

layout(push_constant) uniform PushConstants {
    mat4 projectionMatrix;
    mat4 modelViewMatrix;
} pc;
)" + getLightingFunc("pc.projectionMatrix", "pc.modelViewMatrix") +
         R"(
                      void main() {)" +
         getLightingCalc() +
         R"(
  fColor = lightFactor * texture(uTex2, uv.xy) * texture(uTex, uv.zw);
})";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) noexcept {
  const bool bMultiView = device.hasFeature(DeviceFeatures::Multiview);
  switch (device.getBackendType()) {
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device,
        getVulkanVertexShaderSource(bMultiView).c_str(),
        "main",
        "",
        getVulkanFragmentShaderSource().c_str(),
        "main",
        "",
        nullptr);
  default:
    IGL_ASSERT_NOT_REACHED();
    return nullptr;
  }
}

void addNormalsToCube() {
  if (!kLightCount) {
    return;
  }

  const size_t faceCount = indexData.size() / 6;
  bool normalSet[36] = {false};
  for (size_t j = 0; j < faceCount; j++) {
    const size_t offset = j * 6;
    auto vec1 = vertexData0.at(indexData[offset + 1]).position -
                vertexData0.at(indexData[offset + 2]).position;
    auto vec2 = vertexData0.at(indexData[offset + 1]).position -
                vertexData0.at(indexData.at(offset + 0)).position;
    auto normal = glm::normalize(glm::cross(vec1, vec2));
    std::vector<int> indexremap;
    indexremap.resize(24, -1);

    for (size_t i = offset; i < offset + 6; i++) {
      const size_t oldIndex = indexData[i];
      if (indexremap.at(oldIndex) != -1) {
        indexData.at(i) = indexremap[oldIndex];
      } else if (!normalSet[oldIndex]) {
        vertexData.at(oldIndex).base_color = glm::vec4(normal, 1.0);
        normalSet[oldIndex] = true;
        indexremap.at(oldIndex) = oldIndex;
      } else {
        auto vertex = vertexData0.at(oldIndex);
        vertex.base_color = glm::vec4(normal, 1.0);
        vertexData.push_back(vertex);
        size_t const nextIndex = (vertexData.size() - 1);
        indexData.at(i) = nextIndex;
        normalSet[nextIndex] = true;
        indexremap.at(oldIndex) = nextIndex;
      }
    }
  }
}

bool isDeviceCompatible(IDevice& device) noexcept {
  const auto backendtype = device.getBackendType();
  if (backendtype == BackendType::OpenGL) {
    const auto shaderVersion = device.getShaderVersion();
    if (shaderVersion.majorVersion >= 3 || shaderVersion.minorVersion >= 30) {
      return true;
    }
  }

  if (backendtype == BackendType::Vulkan) {
    return true;
  }
  return false;
}

int setCurrentThreadAffinityMask(int mask) {
#if IGL_PLATFORM_ANDROID
  int err, syscallres;
  pid_t const pid = gettid();
  syscallres = syscall(__NR_sched_setaffinity, pid, sizeof(mask), &mask);
  if (syscallres) {
    err = errno;
    IGL_LOG_ERROR("Set thread affinity failed. with mask 0x%x and error 0x%x\n", mask, err);
    return err;
  }
#else
  IGL_LOG_ERROR("Set thread affinity not supported on this platorm");
  return -1;
#endif

  return 0;
}

double calcPi(int numberOfDivisions, int core) {
  double pi = 0.0;

  if (core >= 0) {
    setCurrentThreadAffinityMask((1 << core));
  }
  for (int i = 0; i <= numberOfDivisions; ++i) {
    double const numerator = 1.0;
    double const denominator = std::sqrt(1.0 + std::pow(-1.0, i));
    if (denominator > 0.f) {
      pi += numerator / denominator;
    }
  }
  return pi * 4.0;
}

double pi = 0.f;
void thrashCPU() noexcept {
  static std::vector<std::future<double>> futures;
  static unsigned int threadSpawnId = 0;
  if (kGoSlowOnCpu) {
    // don't fall off the array
    while (threadIds.size() < kThreadCount) {
      threadIds.push_back(-1);
    }
    if (!kThreadCount) {
      pi = calcPi(kGoSlowOnCpu, -1);
    }
    while (futures.size() < kThreadCount) {
      auto future = std::async(std::launch::async, [] {
        return calcPi(kGoSlowOnCpu, threadIds[threadSpawnId % kThreadCount]);
      });

      futures.push_back(std::move(future));
      threadSpawnId++;
    }

    for (int i = futures.size() - 1; i > -1; i--) {
      auto& future = futures.at(i);

      // Use wait_for() with zero milliseconds to check thread status.
      auto status = future.wait_for(std::chrono::milliseconds(0));

      if (status == std::future_status::ready) {
        pi += future.get();
        futures.erase(futures.begin() + i);
      }
    }
  }
}

float doReadWrite(std::vector<std::vector<std::vector<float>>>& memBlock,
                  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                  int numBlocks,
                  int numRows,
                  int numCols,
                  int threadId) {
  if (threadId != -1) {
    setCurrentThreadAffinityMask(1 << threadId);
  }
  std::random_device const rd;
  std::mt19937 gen(0);
  std::uniform_int_distribution<> randBlocks(0, numBlocks - 1);
  std::uniform_int_distribution<> randRows(0, numRows - 1);
  std::uniform_int_distribution<> randCols(0, numCols - 1);
  float sum = 0.f;
  for (int i = 0; i < kMemoryWrites; i++) {
    const int block = randBlocks(gen);
    const int row = randRows(gen);
    const int col = randCols(gen);
    memBlock[block].at(row)[col] = arc4random();
  }

  for (int i = 0; i < kMemoryReads; i++) {
    const int block = randBlocks(gen);
    const int row = randRows(gen);
    const int col = randCols(gen);
    sum += i % 1 ? -1.f : 1.f * memBlock.at(block)[row][col];
  }

  return sum;
}

std::vector<std::vector<std::vector<float>>> memBlock;
void allocateMemory() {
  if (kThrashMemory) {
    const static size_t blocks = kMemorySize;
    const static size_t rows = 1024;
    const static size_t cols = 1024;
    if (memBlock.empty()) {
      memBlock.resize((blocks));
      for (auto& block : memBlock) {
        block.resize(rows);
        for (auto& row : block) {
          row.resize(cols, 0);
          for (int i = 0; i < cols; i++) {
            row.at(i) = (i);
          }
        }
      }
    }
  }
}

std::atomic<float> memoryVal;
void thrashMemory() noexcept {
  if (!kThrashMemory) {
    return;
  }

  const static size_t blocks = kMemorySize;
  const static size_t rows = 1024;
  const static size_t cols = 1024;

  if (!kThreadCount) {
    memoryVal.store(doReadWrite(memBlock, blocks, rows, cols, -1));
  } else {
    static std::vector<std::future<float>> futures;
    static int memoryThreadId = 0;

    while (futures.size() < kThreadCount) {
      auto future = std::async(std::launch::async, [] {
        return doReadWrite(memBlock, blocks, rows, cols, threadIds[memoryThreadId % kThreadCount]);
      });

      futures.push_back(std::move(future));
      memoryThreadId++;
    }

    for (int i = futures.size() - 1; i > -1; i--) {
      auto& future = futures.at(i);

      // Use wait_for() with zero milliseconds to check thread status.
      auto status = future.wait_for(std::chrono::milliseconds(0));

      if (status == std::future_status::ready) {
        memoryVal.store(future.get());
        futures.erase(futures.begin() + i);
      }
    }
  }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void getOffset(int counter, float& x, float& y, float& z) {
  if (kTestOverdraw) {
    x = 0.f;
    y = 0.f;
    z = counter % 2 ? -half / static_cast<float>(kCubeCount)
                    : half / static_cast<float>(kCubeCount);
    z *= counter / 2.f;
    return;
  }
  const float grid = std::ceil(std::pow(kCubeCount, 1.0f / 3.0f));
  const int igrid = (int)grid;
  // const float fgrid = static_cast<float>(igrid);
  x = static_cast<float>((counter % igrid) - grid / 2);
  z = (static_cast<float>(counter / (igrid * igrid)) - grid / 2.f);
  y = (static_cast<float>((counter % (igrid * igrid)) / igrid) - grid / 2.f);
}

glm::vec3 animateCube(int counter, float x, float y, float scale, int frameCount) {
  struct animationInfo {
    glm::vec3 velocity;
    glm::vec3 lastPos;
  };

  static std::vector<animationInfo> animations;
  if (animations.size() < counter) {
    animationInfo info;
    info.velocity = glm::vec3(1.f * (counter % 2 ? 1.0 : -1.0), 1.f - (float)(counter % 3), 0.f);
    info.lastPos = glm::vec3(x, y, 0);
    animations.push_back(info);
  }

  float velocityScale = 1.f;
  if (kDropFrameX && (frameCount % kDropFrameX) < kDropFrameCount) {
    velocityScale = 0.f;
  } else if (kDropFrameX && (frameCount % kDropFrameX) == kDropFrameCount) {
    velocityScale = 1.f + (float)kDropFrameCount;
  }
  glm::vec3 const pos =
      animations[counter].lastPos + animations[counter].velocity * velocityScale * scale * .005f;
  // check for collisons;
  float const radius = .75 * scale;
  if (pos.x + radius > 1.f) {
    animations[counter].velocity.x = -1.f;
  }
  if (pos.x - radius < -1.f) {
    animations[counter].velocity.x = 1.f;
  }

  if (pos.y + radius > 1.f) {
    animations[counter].velocity.y = -1.f;
  }
  if (pos.y - radius < -1.f) {
    animations[counter].velocity.y = 1.f;
  }

  animations[counter].lastPos = pos;
  return pos;
}

} // namespace

void GPUStressSession::createSamplerAndTextures(const igl::IDevice& device) {
  // Sampler & Texture
  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  samplerDesc.addressModeU = SamplerAddressMode::MirrorRepeat;
  samplerDesc.addressModeV = SamplerAddressMode::MirrorRepeat;
  samplerDesc.addressModeW = SamplerAddressMode::MirrorRepeat;
  samp0_ = device.createSamplerState(samplerDesc, nullptr);
  samp1_ = device.createSamplerState(samplerDesc, nullptr);

  tex0_ = getPlatform().loadTexture("macbeth.png");
  tex1_ = getPlatform().loadTexture("igl.png");
}

void GPUStressSession::createCubes() {
  // only reset once - on mac we hit this path multiple times for different
  // devices
  vertexData = vertexData0;
  indexData = indexData0;

  addNormalsToCube(); // setup for lighting if appropriate

  const float grid = std::ceil(std::pow(kCubeCount, 1.0f / 3.0f));

  const int vertexCount = vertexData.size();
  const int indexCount = indexData.size();

  std::random_device const rd;
  std::mt19937 gen(0);
  std::uniform_real_distribution<> dis(0, 1.f);
  float const scale = 1.f / grid;

  int const uvScale = 1.f / grid;
  glm::vec2 offset = glm::vec2(0.f, 0.f);

  // Vertex buffer, Index buffer and Vertex Input
  for (int i = 1; i < kCubeCount; i++) {
    float x, y, z;
    getOffset(i, x, y, z);
    glm::vec4 color(1.0, 1.0, 1.0, 1.f);
    color[0] = (dis(gen));
    color[1] = (dis(gen));
    color[2] = (dis(gen));

    for (int j = 0; j < vertexCount; j++) {
      VertexPosUvw newPoint = vertexData.at(j);
      newPoint.position += (glm::vec3(x, y, z));
      newPoint.uvw *= glm::vec4(uvScale, uvScale, 1.f, 1.f);
      newPoint.uvw += glm::vec4(offset.x, offset.y, 0.f, 0.f);
      if (!kLightCount) {
        newPoint.base_color = color;
      }
      vertexData.push_back(newPoint);
    }
    for (int j = 0; j < indexCount; j++) {
      indexData.push_back(static_cast<uint16_t>(indexData.at(j) + i * (vertexCount)));
    }

    offset.x += 1.f / grid;
    if (offset.x > 1.f) {
      offset.x = 0.f;
      offset.y += 1.f / grid;
    }
  }

  if (!kTestOverdraw) // we want to fill up the screen here
  {
    for (auto& i : vertexData) {
      i.position.x *= scale;
      i.position.y *= scale;
      i.position.z *= scale;
    }
  }

  auto& device = getPlatform().getDevice();
  const BufferDesc vb0Desc = BufferDesc(BufferDesc::BufferTypeBits::Vertex,
                                        vertexData.data(),
                                        sizeof(VertexPosUvw) * vertexData.size());
  vb0_ = device.createBuffer(vb0Desc, nullptr);
  const BufferDesc ibDesc = BufferDesc(
      BufferDesc::BufferTypeBits::Index, indexData.data(), sizeof(uint16_t) * indexData.size());
  ib0_ = device.createBuffer(ibDesc, nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 3;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[0].offset = offsetof(VertexPosUvw, position);
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].name = "position";
  inputDesc.attributes[0].location = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float4;
  inputDesc.attributes[1].offset = offsetof(VertexPosUvw, uvw);
  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].name = "uvw_in";
  inputDesc.attributes[1].location = 1;
  inputDesc.numInputBindings = 1;
  inputDesc.attributes[2].format = VertexAttributeFormat::Float4;
  inputDesc.attributes[2].offset = offsetof(VertexPosUvw, base_color);
  inputDesc.attributes[2].bufferIndex = 0;
  inputDesc.attributes[2].name = "base_color";
  inputDesc.attributes[2].location = 2;
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(VertexPosUvw);
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);
}

void GPUStressSession::initialize() noexcept {
  pipelineState_ = nullptr;
  vertexInput0_ = nullptr;
  vb0_ = nullptr;
  ib0_ = nullptr; // Buffers for vertices and indices (or constants)
  samp0_ = nullptr;
  samp1_ = nullptr;
  framebuffer_ = nullptr;
  vertexData.resize(0); // recalc verts
  indexData.resize(36); // keep the first 36 indices

  //  this is sets the size of our 'app window' so we can shrink the number of
  //  changed pixels we send to the delphi.
  appParamsRef().sizeX = .5f;
  appParamsRef().sizeY = .5f;
  lastTime_ = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::high_resolution_clock::now().time_since_epoch())
                  .count();
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  createCubes();
  if (!imguiSession_) {
    imguiSession_ = std::make_unique<iglu::imgui::Session>(getPlatform().getDevice(),
                                                           getPlatform().getInputDispatcher());
  }

  createSamplerAndTextures(device);
  shaderStages_ = getShaderStagesForBackend(device);

  // Command queue: backed by different types of GPU HW queues
  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = device.createCommandQueue(desc, nullptr);

  tex0_->generateMipmap(*commandQueue_);
  tex1_->generateMipmap(*commandQueue_);

  // Set up vertex uniform data
  vertexParameters_.scaleZ = 1.0f;

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0f, 0.0f};
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;

  if (kUseMSAA) {
    renderPass_.colorAttachments[0].storeAction = igl::StoreAction::MsaaResolve;
  }

  DepthStencilStateDesc depthDesc;
  depthDesc.isDepthWriteEnabled = true;
  depthDesc.compareFunction = igl::CompareFunction::Less;
  depthStencilState_ = device.createDepthStencilState(depthDesc, nullptr);
}

void GPUStressSession::setProjectionMatrix(float aspectRatio) {
  // perspective projection
  constexpr float fov = 45.0f * (M_PI / 180.0f);
  glm::mat4 projectionMat = glm::perspectiveLH(fov, aspectRatio, .1f, 2.1f);
  if (kTestOverdraw || !kRotateCubes) {
    projectionMat =
        glm::orthoLH_ZO(-half, half, -half / aspectRatio, half / aspectRatio, .1f, 2.1f);
  }
  vertexParameters_.projectionMatrix = projectionMat;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void GPUStressSession::setModelViewMatrix(float angle,
                                          float scaleZ,
                                          float offsetX,
                                          float offsetY,
                                          float offsetZ) {
  float divisor = std::ceil(std::sqrt(static_cast<float>(kDrawCount))) / (half * kScaleFill);

  if (kTestOverdraw) {
    divisor = 1.f;
    offsetX = 0.f;
    offsetY = 0.f;
  }

  float const cosAngle = std::cos(angle);
  float const sinAngle = std::sin(angle);
  glm::vec4 const v0(cosAngle / divisor, 0.f, -sinAngle / divisor, 0.f);
  glm::vec4 const v1(0.f, 1.f / divisor, 0.f, 0.f);
  glm::vec4 const v2(sinAngle / divisor, 0.f, cosAngle / divisor, 0.f);
  glm::vec4 const v3(offsetX, offsetY, 1.f + offsetZ, 1.f);
  glm::mat4 const test(v0, v1, v2, v3);

  vertexParameters_.modelViewMatrix = test;
  vertexParameters_.scaleZ = scaleZ;
}

void GPUStressSession::initState(const igl::SurfaceTextures& surfaceTextures) {
  igl::Result ret;

  // TODO: fix framebuffers so you can update the resolve texture
  if (framebuffer_ == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
    framebufferDesc.mode = surfaceTextures.color->getNumLayers() > 1 ? FramebufferMode::Stereo
                                                                     : FramebufferMode::Mono;

    if (kUseMSAA) {
      const auto dimensions = surfaceTextures.color->getDimensions();

      TextureDesc const fbTexDesc = {
          dimensions.width,
          dimensions.height,
          1,
          surfaceTextures.color->getNumLayers(),
          kMsaaSamples,
          TextureDesc::TextureUsageBits::Attachment,
          1,
          surfaceTextures.color->getNumLayers() > 1 ? TextureType::TwoDArray : TextureType::TwoD,
          surfaceTextures.color->getFormat(),
          igl::ResourceStorage::Private};

      framebufferDesc.colorAttachments[0].texture =
          getPlatform().getDevice().createTexture(fbTexDesc, nullptr);

      framebufferDesc.colorAttachments[0].resolveTexture = surfaceTextures.color;

      igl::TextureDesc const depthDesc = {
          dimensions.width,
          dimensions.height,
          1,
          surfaceTextures.depth->getNumLayers(),
          kMsaaSamples,
          TextureDesc::TextureUsageBits::Attachment,
          1,
          surfaceTextures.depth->getNumLayers() > 1 ? TextureType::TwoDArray : TextureType::TwoD,
          surfaceTextures.depth->getFormat(),
          igl::ResourceStorage::Private};

      framebufferDesc.depthAttachment.texture =
          getPlatform().getDevice().createTexture(depthDesc, nullptr);
    }

    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);

    IGL_ASSERT(ret.isOk());
    IGL_ASSERT(framebuffer_ != nullptr);
  }

  if (kUseMSAA) {
    framebuffer_->updateResolveAttachment(surfaceTextures.color);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  constexpr uint32_t textureUnit = 0;
  if (pipelineState_ == nullptr) {
    // Graphics pipeline: state batch that fully configures GPU for rendering

    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput0_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    graphicsDesc.sampleCount = kUseMSAA ? kMsaaSamples : 1;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    graphicsDesc.fragmentUnitSamplerMap[textureUnit] = IGL_NAMEHANDLE("inputImage");
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;
    graphicsDesc.targetDesc.colorAttachments[0].blendEnabled = kEnableBlending;
    graphicsDesc.targetDesc.colorAttachments[0].rgbBlendOp = BlendOp::Add;
    graphicsDesc.targetDesc.colorAttachments[0].alphaBlendOp = BlendOp::Add;
    graphicsDesc.targetDesc.colorAttachments[0].srcRGBBlendFactor = BlendFactor::SrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[0].srcAlphaBlendFactor = BlendFactor::SrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[0].dstRGBBlendFactor = BlendFactor::OneMinusSrcAlpha;
    graphicsDesc.targetDesc.colorAttachments[0].dstAlphaBlendFactor = BlendFactor::OneMinusSrcAlpha;

    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
  }
}

void GPUStressSession::drawCubes(const igl::SurfaceTextures& surfaceTextures,
                                 std::shared_ptr<igl::IRenderCommandEncoder> commands) {
  static float angle = 0.0f;
  static int frameCount = 0;
  frameCount++;

  angle += 0.005f;

  // rotating animation
  static float scaleZ = 1.0f, ss = 0.005f;
  scaleZ += ss;
  scaleZ = scaleZ < 0.0f ? 0.0f : scaleZ > 1.0 ? 1.0f : scaleZ;
  if (scaleZ <= 0.05f || scaleZ >= 1.0f) {
    ss *= -1.0f;
  }

  auto& device = getPlatform().getDevice();
  // cube animation
  constexpr uint32_t textureUnit = 0;
  constexpr uint32_t textureUnit1 = 1;
  const int grid = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(kDrawCount))));
  float const divisor = .5 / static_cast<float>(grid);
  const float scale = 1.f / std::ceil(std::pow(kCubeCount, 1.0f / 3.0f));

  int counter = 0;
  setProjectionMatrix(surfaceTextures.color->getAspectRatio());

  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);

  std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer = nullptr;
  for (int i = -grid / 2; i < grid / 2 + grid % 2; i++) {
    for (int j = -grid / 2; j < grid / 2 + grid % 2; j++) {
      if (counter >= kDrawCount) {
        break;
      }
      counter++;
      float x = static_cast<float>(j) * divisor;
      float y = static_cast<float>(i) * divisor;
      if (kDropFrameX) {
        auto offset = animateCube(counter, x, y, scale, frameCount);
        x = offset.x;
        y = offset.y;
      }

      setModelViewMatrix((kTestOverdraw || kRotateCubes == false) ? 0.f : angle, scaleZ, x, y, 0.f);

      // note that we are deliberately binding redundant state - the goal here
      // is to tax the driver.  The giant vertex buffer (kCubeCount) will stress
      // just the gpu
      commands->bindVertexBuffer(0, *vb0_);
      commands->bindTexture(textureUnit, BindTarget::kFragment, tex0_.get());
      commands->bindSamplerState(textureUnit, BindTarget::kFragment, samp0_.get());
      commands->bindTexture(textureUnit1, BindTarget::kFragment, tex1_.get());
      commands->bindSamplerState(textureUnit1, BindTarget::kFragment, samp1_.get());
      commands->bindRenderPipelineState(pipelineState_);
      commands->bindDepthStencilState(depthStencilState_);

      // Bind Vertex Uniform Data
      if (device.getBackendType() == BackendType::Vulkan) {
        commands->bindPushConstants(&vertexParameters_,
                                    sizeof(vertexParameters_) - sizeof(float)); // z isn't used
      } else {
        if (!vertUniformBuffer) {
          iglu::ManagedUniformBufferInfo info;
          info.index = 1;
          info.length = sizeof(VertexFormat);
          info.uniforms = std::vector<igl::UniformDesc>{
              igl::UniformDesc{"projectionMatrix",
                               -1,
                               igl::UniformType::Mat4x4,
                               1,
                               offsetof(VertexFormat, projectionMatrix),
                               0},
              igl::UniformDesc{"modelViewMatrix",
                               -1,
                               igl::UniformType::Mat4x4,
                               1,
                               offsetof(VertexFormat, modelViewMatrix),
                               0},
              igl::UniformDesc{
                  "scaleZ", -1, igl::UniformType::Float, 1, offsetof(VertexFormat, scaleZ), 0}};

          vertUniformBuffer = std::make_shared<iglu::ManagedUniformBuffer>(device, info);
          IGL_ASSERT(vertUniformBuffer->result.isOk());
        }
        *static_cast<VertexFormat*>(vertUniformBuffer->getData()) = vertexParameters_;
        vertUniformBuffer->bind(device, *pipelineState_, *commands);
      }

      commands->drawIndexed(indexData.size());
    }
  }
}

void GPUStressSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  const long long newTime = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::high_resolution_clock::now().time_since_epoch())
                                .count();

  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }
  if (forceReset_) {
    memBlock.resize(0);
    forceReset_ = false;
    initialize();
  }

  allocateMemory();
  thrashCPU();
  thrashMemory();

  const long long delta = (newTime - lastTime_);
  fps_.updateFPS((double)delta / 1000000.0);
  lastTime_ = newTime;

  initState(surfaceTextures);

  // Command buffers (1-N per thread): create, submit and forget
  auto buffer = commandQueue_->createCommandBuffer(CommandBufferDesc{}, nullptr);
  const std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  igl::FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = framebuffer_->getColorAttachment(0);
  framebufferDesc.depthAttachment.texture = framebuffer_->getDepthAttachment();

  // setup UI
  const ImGuiViewport* v = ImGui::GetMainViewport();
  imguiSession_->beginFrame(framebufferDesc, getPlatform().getDisplayContext().pixelsPerPoint);
  bool open;
  ImGui::SetNextWindowPos(
      {
          v->WorkPos.x + v->WorkSize.x - 60.0f,
          v->WorkPos.y + v->WorkSize.y * .25f + 15.0f,
      },
      ImGuiCond_Always,
      {1.0f, 0.0f});
  ImGui::Begin("GPU", &open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
  ImGui::SetWindowFontScale(2.f);

  // draw stuff
  drawCubes(surfaceTextures, commands);

  { // Draw using ImGui every frame

    ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f),
                       "FPS: (%f)   PI: (%lf)  Memory (%f)",
                       fps_.getAverageFPS(),
                       pi,
                       memoryVal.load());
    ImGui::End();
    imguiSession_->endFrame(getPlatform().getDevice(), *commands);
  }

  commands->endEncoding();

  buffer->present(kUseMSAA ? framebuffer_->getResolveColorAttachment(0)
                           : framebuffer_->getColorAttachment(0));

  commandQueue_->submit(*buffer); // Guarantees ordering between command buffers
}

void GPUStressSession::setNumThreads(int numThreads) {
  kThreadCount = numThreads;
}

void GPUStressSession::setThrashMemory(bool thrashMemory) {
  kThrashMemory = thrashMemory;
}
void GPUStressSession::setMemorySize(size_t memorySize) {
  if (memorySize != kMemorySize) {
    kMemorySize = memorySize;
    forceReset_ = true;
  }
}
void GPUStressSession::setMemoryReads(size_t memoryReads) {
  kMemoryReads = memoryReads;
}
void GPUStressSession::setMemoryWrites(size_t memoryWrites) {
  kMemoryWrites = memoryWrites;
}
void GPUStressSession::setGoSlowOnCpu(int goSlowOnCpu) {
  kGoSlowOnCpu = goSlowOnCpu;
}
void GPUStressSession::setCubeCount(int count) {
  if (kCubeCount != count) {
    forceReset_ = true;
    kCubeCount = count;
  }
}
void GPUStressSession::setDrawCount(int count) {
  kDrawCount = count;
}
void GPUStressSession::setTestOverdraw(bool testOverdraw) {
  if (testOverdraw != kTestOverdraw) {
    kTestOverdraw = testOverdraw;
    forceReset_ = true;
  }
}
void GPUStressSession::setEnableBlending(bool enableBlending) {
  if (enableBlending != kEnableBlending) {
    kEnableBlending = enableBlending;
    forceReset_ = true;
  }
}
void GPUStressSession::setUseMSAA(bool useMSAA) {
  if (kUseMSAA != useMSAA) {
    kUseMSAA = useMSAA;
    forceReset_ = true;
  }
}
void GPUStressSession::setLightCount(int lightCount) {
  if (kLightCount != lightCount) {
    kLightCount = lightCount;
    forceReset_ = true;
  }
}

void GPUStressSession::setThreadCore(int thread, int core) {
  threadIds[thread % kThreadCount] = core;
}

int GPUStressSession::getNumThreads() const {
  return kThreadCount;
}
bool GPUStressSession::getThrashMemory() const {
  return kThrashMemory;
}
size_t GPUStressSession::getMemorySize() const {
  return kMemorySize;
}
size_t GPUStressSession::getMemoryReads() const {
  return kMemoryReads;
}
size_t GPUStressSession::getMemoryWrites() const {
  return kMemoryWrites;
}
bool GPUStressSession::getGoSlowOnCpu() const {
  return kGoSlowOnCpu != 0;
}
int GPUStressSession::getCubeCount() const {
  return kCubeCount;
}
int GPUStressSession::getDrawCount() const {
  return kDrawCount;
}
bool GPUStressSession::getTestOverdraw() const {
  return kTestOverdraw;
}
bool GPUStressSession::getEnableBlending() const {
  return kEnableBlending;
}
bool GPUStressSession::getUseMSAA() const {
  return kUseMSAA;
}
int GPUStressSession::getLightCount() const {
  return kLightCount;
}
std::vector<int> GPUStressSession::getThreadsCores() const {
  return threadIds;
}

void GPUStressSession::setDropFrameInterval(int numberOfFramesBetweenDrops) {
  kDropFrameX = numberOfFramesBetweenDrops;
}

int GPUStressSession::getDropFrameInterval() const {
  return kDropFrameX;
}

void GPUStressSession::setDropFrameCount(int numberOfFramesToDrop) {
  kDropFrameCount = numberOfFramesToDrop;
}

int GPUStressSession::getDropFrameCount() const {
  return kDropFrameCount;
}

void GPUStressSession::setRotateCubes(bool bRotate) {
  kRotateCubes = bRotate;
}

bool GPUStressSession::getRotateCubes() const {
  return kRotateCubes;
}

std::string GPUStressSession::getCurrentUsageString() const {
  char output[2048];

  snprintf(output,
           sizeof(output),
           "cubes: %d, draws: %d, lights: %d, threads: %d,  cpu load: %d, memory reads: %lu , "
           "memory writes: %lu, "
           "msaa %s , blending %s, framerate: %.2f,",
           kCubeCount.load(),
           kDrawCount.load(),
           kLightCount.load(),
           kThreadCount.load(),
           kGoSlowOnCpu.load(),
           kMemoryReads.load() * (kThrashMemory ? 1 : 0),
           kMemoryWrites.load() * (kThrashMemory ? 1 : 0),
           kUseMSAA ? "on" : "off",
           kEnableBlending ? "on" : "off ",
           fps_.getAverageFPS());

  return output;
}
void GPUStressSession::setNumLayers(size_t numLayers) {
#if !defined(IGL_PLATFORM_WIN)
  igl::shell::QuadLayerParams params;
  params.layerInfo.reserve(numLayers);
  for (int i = 0; i < numLayers; i++) {
    params.layerInfo.emplace_back({.position = {0.f, 0.f, 0.f},
                                   .size = {1.f, 1.f},
                                   .blendMode = igl::shell::LayerBlendMode::AlphaBlend});
  }

  appParamsRef().quadLayerParamsGetter = [params]() -> igl::shell::QuadLayerParams {
    return params;
  };
#endif
}
} // namespace igl::shell
