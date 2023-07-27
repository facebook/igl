/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * A brief tutorial how to run this beast:
 *
 * 1) Run the script "deploy_deps.py" from the IGL root folder.
 * 2) Run the script "deploy_content.py" from the IGL root folder.
 * 3) Run this app.
 *
 */

#if !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif // _USE_MATH_DEFINES
#include <cassert>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <stdio.h>
#include <thread>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

// GLI should be included after GLM
#include <gli/save_ktx.hpp>
#include <gli/texture2d.hpp>
#include <gli/texture_cube.hpp>
#include <ldrutils/lutils/ScopeExit.h>

#include <Compress.h>
#include <meshoptimizer.h>
#include <shared/Camera.h>
#include <shared/UtilsCubemap.h>
#include <shared/UtilsFPS.h>
#include <stb/stb_image.h>
#include <stb/stb_image_resize.h>
#include <taskflow/taskflow.hpp>
#include <tiny_obj_loader.h>

#include <lvk/LVK.h>
#include <lvk/HelpersGLFW.h>
#include <lvk/HelpersImGui.h>

constexpr uint32_t kMeshCacheVersion = 0xC0DE0009;
constexpr uint32_t kMaxTextures = 512;
constexpr int kNumSamplesMSAA = 8;
constexpr bool kEnableCompression = true;
constexpr bool kPreferIntegratedGPU = false;
#if defined(NDEBUG)
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif // NDEBUG

std::string folderThirdParty;
std::string folderContentRoot;

std::unique_ptr<lvk::ImGuiRenderer> imgui_;

const char* kCodeComputeTest = R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (set = 0, binding = 2, rgba8) uniform readonly  image2D kTextures2Din[];
layout (set = 0, binding = 2, rgba8) uniform writeonly image2D kTextures2Dout[];

layout(push_constant) uniform constants
{
	uint tex;
} pc;

void main() {
   vec4 pixel = imageLoad(kTextures2Din[pc.tex], ivec2(gl_GlobalInvocationID.xy));
   float luminance = dot(pixel, vec4(0.299, 0.587, 0.114, 0.0)); // https://www.w3.org/TR/AERT/#color-contrast
   imageStore(kTextures2Dout[pc.tex], ivec2(gl_GlobalInvocationID.xy), vec4(vec3(luminance), 1.0));
}
)";

const char* kCodeFullscreenVS = R"(
layout (location=0) out vec2 uv;
void main() {
  // generate a triangle covering the entire screen
  uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(uv * vec2(2, -2) + vec2(-1, 1), 0.0, 1.0);
}
)";

const char* kCodeFullscreenFS = R"(
layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;

layout(push_constant) uniform constants
{
	uint tex;
} pc;

void main() {
  out_FragColor = textureBindless2D(pc.tex, 0, uv);
}
)";

const char* kCodeVS = R"(
layout (location=0) in vec3 pos;
layout (location=1) in vec3 normal;
layout (location=2) in vec2 uv;
layout (location=3) in uint mtlIndex;

struct Material {
   vec4 ambient;
   vec4 diffuse;
   int texAmbient;
   int texDiffuse;
   int texAlpha;
   int padding;
};

layout(std430, buffer_reference) readonly buffer PerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  uint texSkyboxRadiance;
  uint texSkyboxIrradiance;
  uint texShadow;
  uint sampler0;
  uint samplerShadow0;
  int bDrawNormals;
  int bDebugLines;
};

layout(std430, buffer_reference) readonly buffer PerObject {
  mat4 model;
};

layout(std430, buffer_reference) readonly buffer Materials {
  Material mtl[];
};

layout(push_constant) uniform constants
{
	PerFrame perFrame;
   PerObject perObject;
   Materials materials;
} pc;

// output
struct PerVertex {
  vec3 normal;
  vec2 uv;
  vec4 shadowCoords;
};
layout (location=0) out PerVertex vtx;
layout (location=5) flat out Material mtl;
//

void main() {
  mat4 proj = pc.perFrame.proj;
  mat4 view = pc.perFrame.view;
  mat4 model = pc.perObject.model;
  mat4 light = pc.perFrame.light;
  mtl = pc.materials.mtl[mtlIndex];
  gl_Position = proj * view * model * vec4(pos, 1.0);

  // Compute the normal in world-space
  mat3 norm_matrix = transpose(inverse(mat3(model)));
  vtx.normal = normalize(norm_matrix * normal);
  vtx.uv = uv;
  vtx.shadowCoords = light * model * vec4(pos, 1.0);
}
)";

const char* kCodeVS_Wireframe = R"(
layout (location=0) in vec3 pos;

layout(std430, buffer_reference) readonly buffer PerFrame {
  mat4 proj;
  mat4 view;
};

layout(std430, buffer_reference) readonly buffer PerObject {
  mat4 model;
};

layout(push_constant) uniform constants
{
	PerFrame perFrame;
   PerObject perObject;
} pc;

void main() {
  mat4 proj = pc.perFrame.proj;
  mat4 view = pc.perFrame.view;
  mat4 model = pc.perObject.model;
  gl_Position = proj * view * model * vec4(pos, 1.0);
}
)";

const char* kCodeFS_Wireframe = R"(
layout (location=0) out vec4 out_FragColor;

void main() {
  out_FragColor = vec4(1.0);
};
)";

const char* kCodeFS = R"(

layout(std430, buffer_reference) readonly buffer PerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  uint texSkyboxRadiance;
  uint texSkyboxIrradiance;
  uint texShadow;
  uint sampler0;
  uint samplerShadow0;
  int bDrawNormals;
  int bDebugLines;
};

struct Material {
  vec4 ambient;
  vec4 diffuse;
  int texAmbient;
  int texDiffuse;
  int texAlpha;
  int padding;
};

struct PerVertex {
  vec3 normal;
  vec2 uv;
  vec4 shadowCoords;
};

layout(push_constant) uniform constants
{
	PerFrame perFrame;
} pc;


layout (location=0) in PerVertex vtx;
layout (location=5) flat in Material mtl;

layout (location=0) out vec4 out_FragColor;

float PCF3(vec3 uvw) {
  float size = 1.0 / textureBindlessSize2D(pc.perFrame.texShadow).x;
  float shadow = 0.0;
  for (int v=-1; v<=+1; v++)
    for (int u=-1; u<=+1; u++)
      shadow += textureBindless2DShadow(pc.perFrame.texShadow, pc.perFrame.samplerShadow0, uvw + size * vec3(u, v, 0));
  return shadow / 9;
}

float shadow(vec4 s) {
  s = s / s.w;
  if (s.z > -1.0 && s.z < 1.0) {
    float depthBias = -0.00005;
    float shadowSample = PCF3(vec3(s.x, 1.0 - s.y, s.z + depthBias));
    return mix(0.3, 1.0, shadowSample);
  }
  return 1.0;
}

void main() {
  vec4 alpha = textureBindless2D(mtl.texAlpha, pc.perFrame.sampler0, vtx.uv);
  if (mtl.texAlpha > 0 && alpha.r < 0.5)
    discard;
  vec4 Ka = mtl.ambient * textureBindless2D(mtl.texAmbient, pc.perFrame.sampler0, vtx.uv);
  vec4 Kd = mtl.diffuse * textureBindless2D(mtl.texDiffuse, pc.perFrame.sampler0, vtx.uv);
  bool drawNormals = pc.perFrame.bDrawNormals > 0;
  if (Kd.a < 0.5)
    discard;
  vec3 n = normalize(vtx.normal);
  float NdotL1 = clamp(dot(n, normalize(vec3(-1, 1,+1))), 0.0, 1.0);
  float NdotL2 = clamp(dot(n, normalize(vec3(-1, 1,-1))), 0.0, 1.0);
  float NdotL = 0.5 * (NdotL1+NdotL2);
  // IBL diffuse
  const vec4 f0 = vec4(0.04);
  vec4 diffuse = textureBindlessCube(pc.perFrame.texSkyboxIrradiance, pc.perFrame.sampler0, n) * Kd * (vec4(1.0) - f0);
  out_FragColor = drawNormals ?
    vec4(0.5 * (n+vec3(1.0)), 1.0) :
    Ka + diffuse * shadow(vtx.shadowCoords);
};
)";

const char* kShadowVS = R"(
layout (location=0) in vec3 pos;

layout(std430, buffer_reference) readonly buffer PerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  uint texSkyboxRadiance;
  uint texSkyboxIrradiance;
  uint texShadow;
  uint sampler0;
  uint samplerShadow0;
  int bDrawNormals;
  int bDebugLines;
};

layout(std430, buffer_reference) readonly buffer PerObject {
  mat4 model;
};

layout(push_constant) uniform constants
{
	PerFrame perFrame;
	PerObject perObject;
} pc;

void main() {
  mat4 proj = pc.perFrame.proj;
  mat4 view = pc.perFrame.view;
  mat4 model = pc.perObject.model;
  gl_Position = proj * view * model * vec4(pos, 1.0);
}
)";

const char* kShadowFS = R"(
void main() {
};
)";

const char* kSkyboxVS = R"(
layout (location=0) out vec3 textureCoords;

const vec3 positions[8] = vec3[8](
	vec3(-1.0,-1.0, 1.0), vec3( 1.0,-1.0, 1.0), vec3( 1.0, 1.0, 1.0), vec3(-1.0, 1.0, 1.0),
	vec3(-1.0,-1.0,-1.0), vec3( 1.0,-1.0,-1.0), vec3( 1.0, 1.0,-1.0), vec3(-1.0, 1.0,-1.0)
);

const int indices[36] = int[36](
	0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7, 4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3
);

layout(std430, buffer_reference) readonly buffer PerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  uint texSkyboxRadiance;
  uint texSkyboxIrradiance;
  uint texShadow;
  uint sampler0;
  uint samplerShadow0;
  int bDrawNormals;
  int bDebugLines;
};

layout(push_constant) uniform constants
{
	PerFrame perFrame;
} pc;

void main() {
  mat4 proj = pc.perFrame.proj;
  mat4 view = pc.perFrame.view;
  // discard translation
  view = mat4(view[0], view[1], view[2], vec4(0, 0, 0, 1));
  mat4 transform = proj * view;
  vec3 pos = positions[indices[gl_VertexIndex]];
  gl_Position = (transform * vec4(pos, 1.0)).xyww;

  // skybox
  textureCoords = pos;
}

)";
const char* kSkyboxFS = R"(
layout (location=0) in vec3 textureCoords;
layout (location=0) out vec4 out_FragColor;

layout(std430, buffer_reference) readonly buffer PerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  uint texSkyboxRadiance;
  uint texSkyboxIrradiance;
  uint texShadow;
  uint sampler0;
  uint samplerShadow0;
  int bDrawNormals;
  int bDebugLines;
};

layout(push_constant) uniform constants
{
	PerFrame perFrame;
} pc;

void main() {
  out_FragColor = textureBindlessCube(pc.perFrame.texSkyboxRadiance, pc.perFrame.sampler0, textureCoords);
}
)";

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

GLFWwindow* window_ = nullptr;
int width_ = 0;
int height_ = 0;
FramesPerSecondCounter fps_;

constexpr uint32_t kNumBufferedFrames = 3;

std::unique_ptr<lvk::IDevice> device_;
lvk::Framebuffer fbMain_; // swapchain
lvk::Framebuffer fbOffscreen_;
lvk::Framebuffer fbShadowMap_;
lvk::Holder<lvk::ComputePipelineHandle> computePipelineState_Grayscale_;
lvk::Holder<lvk::RenderPipelineHandle> renderPipelineState_Mesh_;
lvk::Holder<lvk::RenderPipelineHandle> renderPipelineState_MeshWireframe_;
lvk::Holder<lvk::RenderPipelineHandle> renderPipelineState_Shadow_;
lvk::Holder<lvk::RenderPipelineHandle> renderPipelineState_Skybox_;
lvk::Holder<lvk::RenderPipelineHandle> renderPipelineState_Fullscreen_;
lvk::Holder<lvk::BufferHandle> vb0_, ib0_; // buffers for vertices and indices
lvk::Holder<lvk::BufferHandle> sbMaterials_; // storage buffer for materials
std::vector<lvk::Holder<lvk::BufferHandle>> ubPerFrame_, ubPerFrameShadow_, ubPerObject_;
lvk::Holder<lvk::SamplerHandle> sampler_;
lvk::Holder<lvk::SamplerHandle> samplerShadow_;
lvk::Holder<lvk::TextureHandle> textureDummyWhite_;
lvk::Holder<lvk::TextureHandle> skyboxTextureReference_;
lvk::Holder<lvk::TextureHandle> skyboxTextureIrradiance_;
lvk::RenderPass renderPassOffscreen_;
lvk::RenderPass renderPassMain_;
lvk::RenderPass renderPassShadow_;
lvk::DepthStencilState depthStencilState_;
lvk::DepthStencilState depthStencilStateLEqual_;

// scene navigation
CameraPositioner_FirstPerson positioner_(vec3(-100, 40, -47), vec3(0, 35, 0), vec3(0, 1, 0));
Camera camera_(positioner_);
glm::vec2 mousePos_ = glm::vec2(0.0f);
bool mousePressed_ = false;
bool enableComputePass_ = false;
bool enableWireframe_ = false;

bool isShadowMapDirty_ = true;

struct VertexData {
  vec3 position;
  uint32_t normal; // Int_2_10_10_10_REV
  uint32_t uv; // hvec2
  uint32_t mtlIndex;
};

std::vector<VertexData> vertexData_;
std::vector<uint32_t> indexData_;
std::vector<uint32_t> shapeVertexCnt_;

struct UniformsPerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  uint32_t texSkyboxRadiance = 0;
  uint32_t texSkyboxIrradiance = 0;
  uint32_t texShadow = 0;
  uint32_t sampler = 0;
  uint32_t samplerShadow = 0;
  int bDrawNormals = 0;
  int bDebugLines = 0;
  int padding = 0;
} perFrame_;

struct UniformsPerObject {
  mat4 model;
};
#define MAX_MATERIAL_NAME 128

struct CachedMaterial {
  char name[MAX_MATERIAL_NAME] = {};
  vec3 ambient = vec3(0.0f);
  vec3 diffuse = vec3(0.0f);
  char ambient_texname[MAX_MATERIAL_NAME] = {};
  char diffuse_texname[MAX_MATERIAL_NAME] = {};
  char alpha_texname[MAX_MATERIAL_NAME] = {};
};

// this goes into our GLSL shaders
struct GPUMaterial {
  vec4 ambient = vec4(0.0f);
  vec4 diffuse = vec4(0.0f);
  uint32_t texAmbient = 0;
  uint32_t texDiffuse = 0;
  uint32_t texAlpha = 0;
  uint32_t padding[1];
};

static_assert(sizeof(GPUMaterial) % 16 == 0);

std::vector<CachedMaterial> cachedMaterials_;
std::vector<GPUMaterial> materials_;

struct MaterialTextures {
  lvk::TextureHandle ambient;
  lvk::TextureHandle diffuse;
  lvk::TextureHandle alpha;
};

std::vector<MaterialTextures> textures_; // same indexing as in materials_

struct LoadedImage {
  uint32_t w = 0;
  uint32_t h = 0;
  uint32_t channels = 0;
  uint8_t* pixels = nullptr;
  std::string debugName;
  std::string compressedFileName;
};

struct LoadedMaterial {
  size_t idx = 0;
  LoadedImage ambient;
  LoadedImage diffuse;
  LoadedImage alpha;
};

// file name -> LoadedImage
std::mutex imagesCacheMutex_;
std::unordered_map<std::string, LoadedImage> imagesCache_; // accessible only from the loader pool (multiple threads)
std::unordered_map<std::string, lvk::Holder<lvk::TextureHandle>> texturesCache_; // accessible the main thread
std::vector<LoadedMaterial> loadedMaterials_;
std::mutex loadedMaterialsMutex_;
std::atomic<bool> loaderShouldExit_ = false;
std::atomic<uint32_t> remainingMaterialsToLoad_ = 0;
std::unique_ptr<tf::Executor> loaderPool_ =
    std::make_unique<tf::Executor>(std::max(2u, std::thread::hardware_concurrency() / 2));

static bool endsWith(const std::string& str, const std::string& suffix) {
  return str.size() >= suffix.size() &&
         0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

static std::string convertFileName(std::string fileName) {
  // generate compressed filename
  const std::string compressedPathPrefix = folderContentRoot;

  if (fileName.find(compressedPathPrefix) == 0) {
    // remove leading path
    fileName = fileName.substr(compressedPathPrefix.length());
  }

  std::replace(fileName.begin(), fileName.end(), ':', '_');
  std::replace(fileName.begin(), fileName.end(), '.', '_');
  std::replace(fileName.begin(), fileName.end(), '/', '_');
  std::replace(fileName.begin(), fileName.end(), '\\', '_');

  // return absolute compressed filename
  return compressedPathPrefix + fileName + ".ktx";
}
static void stringReplaceAll(std::string& s,
                             const std::string& searchString,
                             const std::string& replaceString) {
  size_t pos = 0;
  while ((pos = s.find(searchString, pos)) != std::string::npos) {
    s.replace(pos, searchString.length(), replaceString);
  }
}

void initIGL() {
  device_ = lvk::createVulkanDeviceWithSwapchain(
      window_,
      width_,
      height_,
      {
          .maxTextures = kMaxTextures,
          .maxSamplers = 128,
          .enableValidation = kEnableValidationLayers,
      },
      kPreferIntegratedGPU ? lvk::HWDeviceType::IntegratedGpu : lvk::HWDeviceType::DiscreteGpu);
  {
    const uint32_t pixel = 0xFFFFFFFF;
    textureDummyWhite_ = device_->createTexture(
        {
            .type = lvk::TextureType_2D,
            .format = lvk::TextureFormat::RGBA_UN8,
            .dimensions = {1, 1},
            .usage = lvk::TextureUsageBits_Sampled,
            .debugName = "dummy 1x1 (white)",
            .initialData = &pixel,
        },
        nullptr);
  }

  // create an Uniform buffers to store uniforms for 2 objects
  for (uint32_t i = 0; i != kNumBufferedFrames; i++) {
    ubPerFrame_.push_back(device_->createBuffer({.usage = lvk::BufferUsageBits_Uniform,
                                                 .storage = lvk::StorageType_HostVisible,
                                                 .size = sizeof(UniformsPerFrame),
                                                 .debugName = "Buffer: uniforms (per frame)"},
                                                nullptr));
    ubPerFrameShadow_.push_back(
        device_->createBuffer({.usage = lvk::BufferUsageBits_Uniform,
                               .storage = lvk::StorageType_HostVisible,
                               .size = sizeof(UniformsPerFrame),
                               .debugName = "Buffer: uniforms (per frame shadow)"},
                              nullptr));
    ubPerObject_.push_back(device_->createBuffer({.usage = lvk::BufferUsageBits_Uniform,
                                                  .storage = lvk::StorageType_HostVisible,
                                                  .size = sizeof(UniformsPerObject),
                                                  .debugName = "Buffer: uniforms (per object)"},
                                                 nullptr));
  }

  depthStencilState_ = {.compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true};
  depthStencilStateLEqual_ = {.compareOp = lvk::CompareOp_LessEqual, .isDepthWriteEnabled = true};

  sampler_ = device_->createSampler(
      {
          .mipMap = lvk::SamplerMip_Linear,
          .wrapU = lvk::SamplerWrap_Repeat,
          .wrapV = lvk::SamplerWrap_Repeat,
          .debugName = "Sampler: linear",
      },
      nullptr);
  samplerShadow_ = device_->createSampler(
      {
          .wrapU = lvk::SamplerWrap_Clamp,
          .wrapV = lvk::SamplerWrap_Clamp,
          .depthCompareOp = lvk::CompareOp_LessEqual,
          .depthCompareEnabled = true,
          .debugName = "Sampler: shadow",
      },
      nullptr);

  renderPassOffscreen_ = {
      .colorAttachments = {{
          .loadOp = lvk::LoadOp_Clear,
          .storeOp = kNumSamplesMSAA > 1 ? lvk::StoreOp_MsaaResolve : lvk::StoreOp_Store,
          .clearColor = {0.0f, 0.0f, 0.0f, 1.0f},
      }},
      .depthAttachment = {
          .loadOp = lvk::LoadOp_Clear,
          .storeOp = lvk::StoreOp_Store,
          .clearDepth = 1.0f,
      }};

  renderPassMain_ = {
      .colorAttachments = {{.loadOp = lvk::LoadOp_Clear,
                            .storeOp = lvk::StoreOp_Store,
                            .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}}},
  };
  renderPassShadow_ = {
      .depthAttachment = {.loadOp = lvk::LoadOp_Clear,
                          .storeOp = lvk::StoreOp_Store,
                          .clearDepth = 1.0f},
  };
}

void normalizeName(std::string& name) {
#if defined(__linux__)
  std::replace(name.begin(), name.end(), '\\', '/');
#endif
}

bool loadAndCache(const char* cacheFileName) {
  IGL_PROFILER_FUNCTION();

  // load 3D model and cache it
  LLOGL("Loading `exterior.obj`... It can take a while in debug builds...\n");

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  const bool ret =
      tinyobj::LoadObj(&attrib,
                       &shapes,
                       &materials,
                       &warn,
                       &err,
                       (folderContentRoot + "src/bistro/Exterior/exterior.obj").c_str(),
                       (folderContentRoot + "src/bistro/Exterior/").c_str());

  if (!IGL_VERIFY(ret)) {
    IGL_ASSERT_MSG(ret, "Did you read the tutorial at the top of this file?");
    return false;
  }

  // loop over shapes as described in https://github.com/tinyobjloader/tinyobjloader
  std::vector<std::vector<VertexData>> resplitShapes;
  std::vector<VertexData> shapeData;
  resplitShapes.resize(materials.size());
  int prevIndex = shapes[0].mesh.material_ids[0];
  for (size_t s = 0; s < shapes.size(); s++) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      IGL_ASSERT(shapes[s].mesh.num_face_vertices[f] == 3);

      for (size_t v = 0; v < 3; v++) {
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

        const vec3 pos(attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                       attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                       attrib.vertices[3 * size_t(idx.vertex_index) + 2]);

        const bool hasNormal = (idx.normal_index >= 0);

        const vec3 normal = hasNormal ? vec3(attrib.normals[3 * size_t(idx.normal_index) + 0],
                                             attrib.normals[3 * size_t(idx.normal_index) + 1],
                                             attrib.normals[3 * size_t(idx.normal_index) + 2])
                                      : vec3(0, 0, 1);

        const bool hasUV = (idx.texcoord_index >= 0);

        const vec2 uv = hasUV ? vec2(attrib.texcoords[2 * size_t(idx.texcoord_index) + 0],
                                     attrib.texcoords[2 * size_t(idx.texcoord_index) + 1])
                              : vec2(0);

        const int mtlIndex = shapes[s].mesh.material_ids[f];

        IGL_ASSERT(mtlIndex >= 0 && mtlIndex < materials.size());

        if (prevIndex != mtlIndex) {
          resplitShapes[prevIndex].insert(
              resplitShapes[prevIndex].end(), shapeData.begin(), shapeData.end());
          shapeData.clear();
          prevIndex = mtlIndex;
        }
        vertexData_.push_back({pos,
                               glm::packSnorm3x10_1x2(vec4(normal, 0)),
                               glm::packHalf2x16(uv),
                               (uint32_t)mtlIndex});
        shapeData.push_back({pos,
                             glm::packSnorm3x10_1x2(vec4(normal, 0)),
                             glm::packHalf2x16(uv),
                             (uint32_t)mtlIndex});
      }
      index_offset += 3;
    }
  }
  resplitShapes[prevIndex].insert(
      resplitShapes[prevIndex].end(), shapeData.begin(), shapeData.end());
  shapeData.clear();
  for (auto shape : resplitShapes) {
    shapeData.insert(shapeData.end(), shape.begin(), shape.end());
    shapeVertexCnt_.emplace_back((uint32_t)shape.size());
  }

  // repack the mesh as described in https://github.com/zeux/meshoptimizer
  {
    // 1. Generate an index buffer
    const size_t indexCount = vertexData_.size();
    std::vector<uint32_t> remap(indexCount);
    const size_t vertexCount = meshopt_generateVertexRemap(
        remap.data(), nullptr, indexCount, vertexData_.data(), indexCount, sizeof(VertexData));
    // 2. Remap vertices
    std::vector<VertexData> remappedVertices;
    indexData_.resize(indexCount);
    remappedVertices.resize(vertexCount);
    meshopt_remapIndexBuffer(indexData_.data(), nullptr, indexCount, &remap[0]);
    meshopt_remapVertexBuffer(
        remappedVertices.data(), vertexData_.data(), indexCount, sizeof(VertexData), remap.data());
    vertexData_ = remappedVertices;
    // 3. Optimize for the GPU vertex cache reuse and overdraw
    meshopt_optimizeVertexCache(indexData_.data(), indexData_.data(), indexCount, vertexCount);
    meshopt_optimizeOverdraw(indexData_.data(),
                             indexData_.data(),
                             indexCount,
                             &vertexData_[0].position.x,
                             vertexCount,
                             sizeof(VertexData),
                             1.05f);
    meshopt_optimizeVertexFetch(vertexData_.data(),
                                indexData_.data(),
                                indexCount,
                                vertexData_.data(),
                                vertexCount,
                                sizeof(VertexData));
  }

  // loop over materials
  for (auto& m : materials) {
    CachedMaterial mtl;
    mtl.ambient = vec3(m.ambient[0], m.ambient[1], m.ambient[2]);
    mtl.diffuse = vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]);
    IGL_ASSERT(m.name.length() < MAX_MATERIAL_NAME);
    IGL_ASSERT(m.ambient_texname.length() < MAX_MATERIAL_NAME);
    IGL_ASSERT(m.diffuse_texname.length() < MAX_MATERIAL_NAME);
    IGL_ASSERT(m.alpha_texname.length() < MAX_MATERIAL_NAME);
    strcat(mtl.name, m.name.c_str());
    normalizeName(m.ambient_texname);
    normalizeName(m.diffuse_texname);
    normalizeName(m.alpha_texname);
    strcat(mtl.ambient_texname, m.ambient_texname.c_str());
    strcat(mtl.diffuse_texname, m.diffuse_texname.c_str());
    strcat(mtl.alpha_texname, m.alpha_texname.c_str());
    cachedMaterials_.push_back(mtl);
  }

  LLOGL("Caching mesh...\n");

  FILE* cacheFile = fopen(cacheFileName, "wb");
  if (!cacheFile) {
    return false;
  }
  const uint32_t numMaterials = (uint32_t)cachedMaterials_.size();
  const uint32_t numVertices = (uint32_t)vertexData_.size();
  const uint32_t numIndices = (uint32_t)indexData_.size();
  fwrite(&kMeshCacheVersion, sizeof(kMeshCacheVersion), 1, cacheFile);
  fwrite(&numMaterials, sizeof(numMaterials), 1, cacheFile);
  fwrite(&numVertices, sizeof(numVertices), 1, cacheFile);
  fwrite(&numIndices, sizeof(numIndices), 1, cacheFile);
  fwrite(cachedMaterials_.data(), sizeof(CachedMaterial), numMaterials, cacheFile);
  fwrite(vertexData_.data(), sizeof(VertexData), numVertices, cacheFile);
  fwrite(indexData_.data(), sizeof(uint32_t), numIndices, cacheFile);
  const uint32_t numShapes = (uint32_t)shapeData.size();
  fwrite(&numShapes, sizeof(numShapes), 1, cacheFile);
  fwrite(shapeData.data(), sizeof(VertexData), numShapes, cacheFile);
  const uint32_t numShapeVertices = (uint32_t)shapeVertexCnt_.size();
  fwrite(&numShapeVertices, sizeof(numShapeVertices), 1, cacheFile);
  fwrite(shapeVertexCnt_.data(), sizeof(uint32_t), numShapeVertices, cacheFile);
  return fclose(cacheFile) == 0;
}

bool loadFromCache(const char* cacheFileName) {
  FILE* cacheFile = fopen(cacheFileName, "rb");
  SCOPE_EXIT {
    if (cacheFile) {
      fclose(cacheFile);
    }
  };
  if (!cacheFile) {
    return false;
  }
#define CHECK_READ(expected, read) \
  if ((read) != (expected)) {      \
    return false;                  \
  }
  uint32_t versionProbe = 0;
  CHECK_READ(1, fread(&versionProbe, sizeof(versionProbe), 1, cacheFile));
  if (versionProbe != kMeshCacheVersion) {
    LLOGL("Cache file has wrong version id\n");
    return false;
  }
  uint32_t numMaterials = 0;
  uint32_t numVertices = 0;
  uint32_t numIndices = 0;
  CHECK_READ(1, fread(&numMaterials, sizeof(numMaterials), 1, cacheFile));
  CHECK_READ(1, fread(&numVertices, sizeof(numVertices), 1, cacheFile));
  CHECK_READ(1, fread(&numIndices, sizeof(numIndices), 1, cacheFile));
  cachedMaterials_.resize(numMaterials);
  vertexData_.resize(numVertices);
  indexData_.resize(numIndices);
  CHECK_READ(numMaterials,
             fread(cachedMaterials_.data(), sizeof(CachedMaterial), numMaterials, cacheFile));
  CHECK_READ(numVertices, fread(vertexData_.data(), sizeof(VertexData), numVertices, cacheFile));
  CHECK_READ(numIndices, fread(indexData_.data(), sizeof(uint32_t), numIndices, cacheFile));
#undef CHECK_READ
  return true;
}

void initModel() {
  const std::string cacheFileName = folderContentRoot + "cache.data";

  if (!loadFromCache(cacheFileName.c_str())) {
    if (!IGL_VERIFY(loadAndCache(cacheFileName.c_str()))) {
      IGL_ASSERT_MSG(false, "Cannot load 3D model");
    }
  }

  for (const auto& mtl : cachedMaterials_) {
    materials_.push_back(GPUMaterial{vec4(mtl.ambient, 1.0f),
                                     vec4(mtl.diffuse, 1.0f),
                                     textureDummyWhite_.index(),
                                     textureDummyWhite_.index()});
  }
  sbMaterials_ = device_->createBuffer({.usage = lvk::BufferUsageBits_Storage,
                                        .storage = lvk::StorageType_Device,
                                        .data = materials_.data(),
                                        .size = sizeof(GPUMaterial) * materials_.size(),
                                        .debugName = "Buffer: materials"},
                                       nullptr);

  vb0_ = device_->createBuffer({.usage = lvk::BufferUsageBits_Vertex,
                                .storage = lvk::StorageType_Device,
                                .data = vertexData_.data(),
                                .size = sizeof(VertexData) * vertexData_.size(),
                                .debugName = "Buffer: vertex"},
                               nullptr);
  ib0_ = device_->createBuffer({.usage = lvk::BufferUsageBits_Index,
                                .storage = lvk::StorageType_Device,
                                .data = indexData_.data(),
                                .size = sizeof(uint32_t) * indexData_.size(),
                                .debugName = "Buffer: index"},
                               nullptr);
}

void createComputePipeline() {
  if (computePipelineState_Grayscale_.valid()) {
    return;
  }

  computePipelineState_Grayscale_ =
      device_->createComputePipeline({.shaderStages = device_->createShaderStages(
                                          kCodeComputeTest, "Shader Module: grayscale (comp)")},
                                     nullptr);
}

void createRenderPipelines() {
  if (renderPipelineState_Mesh_.valid()) {
    return;
  }

  const lvk::VertexInput vdesc = {
      .attributes =
          {
              {.location = 0,
               .format = lvk::VertexFormat::Float3,
               .offset = offsetof(VertexData, position)},
              {.location = 1,
               .format = lvk::VertexFormat::Int_2_10_10_10_REV,
               .offset = offsetof(VertexData, normal)},
              {.location = 2,
               .format = lvk::VertexFormat::HalfFloat2,
               .offset = offsetof(VertexData, uv)},
              {.location = 3,
               .format = lvk::VertexFormat::UInt1,
               .offset = offsetof(VertexData, mtlIndex)},
          },
      .inputBindings = {{.stride = sizeof(VertexData)}},
  };

  // shadow
  const lvk::VertexInput vdescs = {
      .attributes = {{.format = lvk::VertexFormat::Float3,
                      .offset = offsetof(VertexData, position)}},
      .inputBindings = {{.stride = sizeof(VertexData)}},
  };

  {
    lvk::RenderPipelineDesc desc = {
        .vertexInput = vdesc,
        .shaderStages = device_->createShaderStages(
            kCodeVS, "Shader Module: main (vert)", kCodeFS, "Shader Module: main (frag)"),
        .colorAttachments = {{.format =
                                  device_->getFormat(fbOffscreen_.colorAttachments[0].texture)}},
        .depthAttachmentFormat = device_->getFormat(fbOffscreen_.depthStencilAttachment.texture),
        .cullMode = lvk::CullMode_Back,
        .frontFaceWinding = lvk::WindingMode_CCW,
        .samplesCount = kNumSamplesMSAA,
        .debugName = "Pipeline: mesh",
    };

    renderPipelineState_Mesh_ = device_->createRenderPipeline(desc, nullptr);

    desc.polygonMode = lvk::PolygonMode_Line;
    desc.vertexInput = vdescs; // positions-only
    desc.shaderStages = device_->createShaderStages(kCodeVS_Wireframe,
                                                    "Shader Module: main wireframe (vert)",
                                                    kCodeFS_Wireframe,
                                                    "Shader Module: main wireframe (frag)");
    desc.debugName = "Pipeline: mesh (wireframe)";
    renderPipelineState_MeshWireframe_ = device_->createRenderPipeline(desc, nullptr);
  }

  // shadow
  renderPipelineState_Shadow_ = device_->createRenderPipeline(
      lvk::RenderPipelineDesc{
          .vertexInput = vdescs,
          .shaderStages = device_->createShaderStages(
              kShadowVS, "Shader Module: shadow (vert)", kShadowFS, "Shader Module: shadow (frag)"),
          .depthAttachmentFormat = device_->getFormat(fbShadowMap_.depthStencilAttachment.texture),
          .cullMode = lvk::CullMode_None,
          .debugName = "Pipeline: shadow",
      },
      nullptr);

  // fullscreen
  {
    lvk::RenderPipelineDesc desc = {
        .colorAttachments = {{.format = device_->getFormat(fbMain_.colorAttachments[0].texture)}},
    };
    if (fbMain_.depthStencilAttachment.texture) {
      desc.depthAttachmentFormat = device_->getFormat(fbMain_.depthStencilAttachment.texture);
    }
    desc.shaderStages = device_->createShaderStages(kCodeFullscreenVS,
                                                    "Shader Module: fullscreen (vert)",
                                                    kCodeFullscreenFS,
                                                    "Shader Module: fullscreen (frag)");
    desc.cullMode = lvk::CullMode_None;
    desc.debugName = "Pipeline: fullscreen";
    renderPipelineState_Fullscreen_ = device_->createRenderPipeline(desc, nullptr);
  }
}

void createRenderPipelineSkybox() {
  if (renderPipelineState_Skybox_.valid()) {
    return;
  }

  const lvk::RenderPipelineDesc desc = {
      .shaderStages = device_->createShaderStages(
          kSkyboxVS, "Shader Module: skybox (vert)", kSkyboxFS, "Shader Module: skybox (frag)"),
      .colorAttachments = {{
          .format = device_->getFormat(fbOffscreen_.colorAttachments[0].texture),
      }},
      .depthAttachmentFormat = device_->getFormat(fbOffscreen_.depthStencilAttachment.texture),
      .cullMode = lvk::CullMode_Front,
      .frontFaceWinding = lvk::WindingMode_CCW,
      .samplesCount = kNumSamplesMSAA,
      .debugName = "Pipeline: skybox",
  };

  renderPipelineState_Skybox_ = device_->createRenderPipeline(desc, nullptr);
}

void createShadowMap() {
  const uint32_t w = 4096;
  const uint32_t h = 4096;
  const lvk::TextureDesc desc = {
      .type = lvk::TextureType_2D,
      .format = lvk::TextureFormat::Z_UN16,
      .dimensions = {w, h},
      .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
      .numMipLevels = lvk::calcNumMipLevels(w, h),
      .debugName = "Shadow map",
  };
  fbShadowMap_ = {
      .depthStencilAttachment = {.texture = device_->createTexture(desc).release()},
  };
}

void createOffscreenFramebuffer() {
  const uint32_t w = width_;
  const uint32_t h = height_;
  lvk::TextureDesc descDepth = {
      .type = lvk::TextureType_2D,
      .format = lvk::TextureFormat::Z_UN24,
      .dimensions = {w, h},
      .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
      .numMipLevels = lvk::calcNumMipLevels(w, h),
      .debugName = "Offscreen framebuffer (d)",
  };
  if (kNumSamplesMSAA > 1) {
    descDepth.usage = lvk::TextureUsageBits_Attachment;
    descDepth.numSamples = kNumSamplesMSAA;
    descDepth.numMipLevels = 1;
  }

  const uint8_t usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled |
                        lvk::TextureUsageBits_Storage;
  const lvk::TextureFormat format = lvk::TextureFormat::RGBA_UN8;

  lvk::TextureDesc descColor = {
      .type = lvk::TextureType_2D,
      .format = format,
      .dimensions = {w, h},
      .usage = usage,
      .numMipLevels = lvk::calcNumMipLevels(w, h),
      .debugName = "Offscreen framebuffer (color)",
  };
  if (kNumSamplesMSAA > 1) {
    descColor.usage = lvk::TextureUsageBits_Attachment;
    descColor.numSamples = kNumSamplesMSAA;
    descColor.numMipLevels = 1;
  }

  lvk::Framebuffer fb = {
      .colorAttachments = {{.texture = device_->createTexture(descColor).release()}},
      .depthStencilAttachment = {.texture = device_->createTexture(descDepth).release()},
  };

  if (kNumSamplesMSAA > 1) {
    fb.colorAttachments[0].resolveTexture =
        device_
            ->createTexture({
                .type = lvk::TextureType_2D,
                .format = format,
                .dimensions = {w, h},
                .usage = usage,
                .debugName = "Offscreen framebuffer (color resolve)",
            })
            .release();
  }

  fbOffscreen_ = fb;
}

void render(lvk::TextureHandle nativeDrawable, uint32_t frameIndex) {
  IGL_PROFILER_FUNCTION();

  fbMain_.colorAttachments[0].texture = nativeDrawable;

  const float fov = float(45.0f * (M_PI / 180.0f));
  const float aspectRatio = (float)width_ / (float)height_;

  const mat4 shadowProj = glm::perspective(float(60.0f * (M_PI / 180.0f)), 1.0f, 10.0f, 4000.0f);
  const mat4 shadowView = mat4(vec4(0.772608519f, 0.532385886f, -0.345892131f, 0),
                               vec4(0, 0.544812560f, 0.838557839f, 0),
                               vec4(0.634882748f, -0.647876859f, 0.420926809f, 0),
                               vec4(-58.9244843f, -30.4530792f, -508.410126f, 1.0f));
  const mat4 scaleBias =
      mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.0, 1.0);

  perFrame_ = UniformsPerFrame{
      .proj = glm::perspective(fov, aspectRatio, 0.5f, 500.0f),
      .view = camera_.getViewMatrix(),
      .light = scaleBias * shadowProj * shadowView,
      .texSkyboxRadiance = skyboxTextureReference_.index(),
      .texSkyboxIrradiance = skyboxTextureIrradiance_.index(),
      .texShadow = fbShadowMap_.depthStencilAttachment.texture.index(),
      .sampler = sampler_.index(),
      .samplerShadow = samplerShadow_.index(),
      .bDrawNormals = perFrame_.bDrawNormals,
      .bDebugLines = perFrame_.bDebugLines,
  };
  device_->upload(ubPerFrame_[frameIndex], &perFrame_, sizeof(perFrame_));

  {
    const UniformsPerFrame perFrameShadow{
        .proj = shadowProj,
        .view = shadowView,
    };
    device_->upload(ubPerFrameShadow_[frameIndex], &perFrameShadow, sizeof(perFrameShadow));
  }

  UniformsPerObject perObject;

  perObject.model = glm::scale(mat4(1.0f), vec3(0.05f));

  device_->upload(ubPerObject_[frameIndex], &perObject, sizeof(perObject));

  // Command buffers (1-N per thread): create, submit and forget

  // Pass 1: shadows
  if (isShadowMapDirty_) {
    lvk::ICommandBuffer& buffer = device_->acquireCommandBuffer();

    buffer.cmdBeginRendering(renderPassShadow_, fbShadowMap_);
    {
      buffer.cmdBindRenderPipeline(renderPipelineState_Shadow_);
      buffer.cmdPushDebugGroupLabel("Render Shadows", lvk::Color(1, 0, 0));
      buffer.cmdBindDepthStencilState(depthStencilState_);
      buffer.cmdBindVertexBuffer(0, vb0_, 0);
      struct {
        uint64_t perFrame;
        uint64_t perObject;
      } bindings = {
          .perFrame = device_->gpuAddress(ubPerFrameShadow_[frameIndex]),
          .perObject = device_->gpuAddress(ubPerObject_[frameIndex]),
      };
      buffer.cmdPushConstants(bindings);
      buffer.cmdDrawIndexed(
          lvk::Primitive_Triangle, indexData_.size(), lvk::IndexFormat_UI32, ib0_, 0);
      buffer.cmdPopDebugGroupLabel();
    }
    buffer.cmdEndRendering();
    buffer.transitionToShaderReadOnly(fbShadowMap_.depthStencilAttachment.texture);
    device_->submit(buffer, lvk::QueueType_Graphics);
    device_->generateMipmap(fbShadowMap_.depthStencilAttachment.texture);

    isShadowMapDirty_ = false;
  }

  // Pass 2: mesh
  {
    lvk::ICommandBuffer& buffer = device_->acquireCommandBuffer();

    // This will clear the framebuffer
    buffer.cmdBeginRendering(renderPassOffscreen_, fbOffscreen_);
    {
      // Scene
      buffer.cmdBindRenderPipeline(renderPipelineState_Mesh_);
      buffer.cmdPushDebugGroupLabel("Render Mesh", lvk::Color(1, 0, 0));
      buffer.cmdBindDepthStencilState(depthStencilState_);
      buffer.cmdBindVertexBuffer(0, vb0_, 0);

      struct {
        uint64_t perFrame;
        uint64_t perObject;
        uint64_t materials;
      } bindings = {
          .perFrame = device_->gpuAddress(ubPerFrame_[frameIndex]),
          .perObject = device_->gpuAddress(ubPerObject_[frameIndex]),
          .materials = device_->gpuAddress(sbMaterials_),
      };
      buffer.cmdPushConstants(bindings);
      buffer.cmdDrawIndexed(
          lvk::Primitive_Triangle, indexData_.size(), lvk::IndexFormat_UI32, ib0_, 0);
      if (enableWireframe_) {
        buffer.cmdBindRenderPipeline(renderPipelineState_MeshWireframe_);
        buffer.cmdDrawIndexed(
            lvk::Primitive_Triangle, indexData_.size(), lvk::IndexFormat_UI32, ib0_, 0);
      }
      buffer.cmdPopDebugGroupLabel();

      // Skybox
      buffer.cmdBindRenderPipeline(renderPipelineState_Skybox_);
      buffer.cmdPushDebugGroupLabel("Render Skybox", lvk::Color(0, 1, 0));
      buffer.cmdBindDepthStencilState(depthStencilStateLEqual_);
      buffer.cmdDraw(lvk::Primitive_Triangle, 0, 3 * 6 * 2);
      buffer.cmdPopDebugGroupLabel();
    }
    buffer.cmdEndRendering();
    buffer.transitionToShaderReadOnly(fbOffscreen_.colorAttachments[0].texture);
    device_->submit(buffer, lvk::QueueType_Graphics);
  }

  // Pass 3: compute shader post-processing
  if (enableComputePass_) {
    lvk::TextureHandle tex = kNumSamplesMSAA > 1 ? fbOffscreen_.colorAttachments[0].resolveTexture
                                                 : fbOffscreen_.colorAttachments[0].texture;
    lvk::ICommandBuffer& buffer = device_->acquireCommandBuffer();

    buffer.cmdBindComputePipeline(computePipelineState_Grayscale_);

    struct {
      uint32_t texture;
    } bindings = {
        .texture = tex.index(),
    };
    buffer.cmdPushConstants(bindings);
    buffer.cmdDispatchThreadGroups(
        {
            .width = (uint32_t)width_,
            .height = (uint32_t)height_,
            .depth = 1u,
        },
        {
            .textures = {tex},
        });

    device_->submit(buffer, lvk::QueueType_Compute);
  }

  // Pass 4: render into the swapchain image
  {
    lvk::ICommandBuffer& buffer = device_->acquireCommandBuffer();

    // This will clear the framebuffer
    buffer.cmdBeginRendering(renderPassMain_, fbMain_);
    {
      buffer.cmdBindRenderPipeline(renderPipelineState_Fullscreen_);
      buffer.cmdPushDebugGroupLabel("Swapchain Output", lvk::Color(1, 0, 0));
      struct {
        uint32_t texture;
      } bindings = {
          .texture = kNumSamplesMSAA > 1 ? fbOffscreen_.colorAttachments[0].resolveTexture.index()
                                         : fbOffscreen_.colorAttachments[0].texture.index(),
      };
      buffer.cmdPushConstants(bindings);
      buffer.cmdDraw(lvk::Primitive_Triangle, 0, 3);
      buffer.cmdPopDebugGroupLabel();

      imgui_->endFrame(*device_.get(), buffer);
    }
    buffer.cmdEndRendering();

    device_->submit(buffer, lvk::QueueType_Graphics, fbMain_.colorAttachments[0].texture);
  }
}

void generateCompressedTexture(LoadedImage img) {
  IGL_PROFILER_FUNCTION();

  if (loaderShouldExit_.load(std::memory_order_acquire)) {
    return;
  }

  printf("...compressing texture to %s\n", img.compressedFileName.c_str());

  const auto mipmapLevelCount = lvk::calcNumMipLevels(img.w, img.h);

  // Go over all generated mipmap and create a compressed texture
  gli::texture2d::extent_type extents;
  extents.x = img.w;
  extents.y = img.h;

  // Create gli texture
  // hard coded and support only BC7 format
  gli::texture2d gliTex2d =
      gli::texture2d(gli::FORMAT_RGBA_BP_UNORM_BLOCK16, extents, mipmapLevelCount);

  uint32_t w = img.w;
  uint32_t h = img.h;
  size_t imgsize = 0;
  for (uint32_t i = 0; i < mipmapLevelCount; ++i) {
    // mipi level
    gli::image gliImage = gliTex2d[i];

    std::vector<uint8_t> destPixels(w * h * img.channels);

    // resize
    stbir_resize_uint8((const unsigned char*)img.pixels,
                       (int)img.w,
                       (int)img.h,
                       0,
                       (unsigned char*)destPixels.data(),
                       w,
                       h,
                       0,
                       (int)img.channels);
    // compress
    auto packedImage16 = Compress::getCompressedImage(
        destPixels.data(), w, h, img.channels, false, &loaderShouldExit_);
    memcpy(gliImage.data(), packedImage16.data(), sizeof(block16) * packedImage16.size());
    imgsize += gliTex2d.size(i);
    h = h > 1 ? h >> 1 : 1;
    w = w > 1 ? w >> 1 : 1;

    if (loaderShouldExit_.load(std::memory_order_acquire)) {
      return;
    }
  }

  gli::save_ktx(gliTex2d, img.compressedFileName.c_str());
}

lvk::TextureFormat gli2iglTextureFormat(gli::texture2d::format_type format) {
  switch (format) {
  case gli::FORMAT_RGBA32_SFLOAT_PACK32:
    return lvk::TextureFormat::RGBA_F32;
  case gli::FORMAT_RG16_SFLOAT_PACK16:
    return lvk::TextureFormat::RG_F16;
  }
  IGL_ASSERT_MSG(false, "Code should NOT be reached");
  return lvk::TextureFormat::RGBA_UN8;
}

LoadedImage loadImage(const char* fileName, int channels) {
  IGL_PROFILER_FUNCTION();

  if (!fileName || !*fileName) {
    return LoadedImage();
  }

  char debugStr[512] = { 0 };

  snprintf(debugStr, sizeof(debugStr)-1, "%s (%i)", fileName, channels);

  const std::string debugName(debugStr);

  {
    std::lock_guard lock(imagesCacheMutex_);

    const auto it = imagesCache_.find(debugName);

    if (it != imagesCache_.end()) {
      IGL_ASSERT(channels == it->second.channels);
      return it->second;
    }
  }

  int w, h;
  uint8_t* pixels = stbi_load(fileName, &w, &h, nullptr, channels);

  const LoadedImage img = {
      .w = (uint32_t)w,
      .h = (uint32_t)h,
      .channels = (uint32_t)channels,
      .pixels = pixels,
      .debugName = debugName,
      .compressedFileName = convertFileName(fileName),
  };

  if (img.pixels && kEnableCompression && (channels != 1) &&
      !std::filesystem::exists(img.compressedFileName.c_str())) {
    generateCompressedTexture(img);
  }

  std::lock_guard lock(imagesCacheMutex_);

  imagesCache_[fileName] = img;

  return img;
}

void loadMaterial(size_t i) {
  IGL_PROFILER_FUNCTION();

  static const std::string pathPrefix = folderContentRoot + "src/bistro/Exterior/";

  SCOPE_EXIT {
    remainingMaterialsToLoad_.fetch_sub(1u, std::memory_order_release);
  };

#define LOAD_TEX(result, tex, channels)                                          \
  const LoadedImage result =                                                     \
      std::string(cachedMaterials_[i].tex).empty()                               \
          ? LoadedImage()                                                        \
          : loadImage((pathPrefix + cachedMaterials_[i].tex).c_str(), channels); \
  if (loaderShouldExit_.load(std::memory_order_acquire)) {                       \
    return;                                                                      \
  }

  LOAD_TEX(ambient, ambient_texname, 4);
  LOAD_TEX(diffuse, diffuse_texname, 4);
  LOAD_TEX(alpha, alpha_texname, 1);

#undef LOAD_TEX

  const LoadedMaterial mtl{i, ambient, diffuse, alpha};

  if (!mtl.ambient.pixels && !mtl.diffuse.pixels) {
    // skip missing textures
    materials_[i].texDiffuse = 0;
  } else {
    std::lock_guard guard(loadedMaterialsMutex_);
    loadedMaterials_.push_back(mtl);
    remainingMaterialsToLoad_.fetch_add(1u, std::memory_order_release);
  }
}

void loadMaterials() {
  stbi_set_flip_vertically_on_load(1);

  remainingMaterialsToLoad_ = (uint32_t)cachedMaterials_.size();

  textures_.resize(cachedMaterials_.size());
  for (size_t i = 0; i != cachedMaterials_.size(); i++) {
    loaderPool_->silent_async([i]() { loadMaterial(i); });
  }
}

void loadCubemapTexture(const std::string& fileNameKTX, lvk::Holder<lvk::TextureHandle>& tex) {
  IGL_PROFILER_FUNCTION();

  auto texRef = gli::load_ktx(fileNameKTX);

  if (!IGL_VERIFY(texRef.format() == gli::FORMAT_RGBA32_SFLOAT_PACK32)) {
    IGL_ASSERT_MSG(false, "Texture format not supported");
    return;
  }

  const uint32_t width = (uint32_t)texRef.extent().x;
  const uint32_t height = (uint32_t)texRef.extent().y;

  if (tex.empty()) {
    tex = device_->createTexture(
        {
            .type = lvk::TextureType_Cube,
            .format = gli2iglTextureFormat(texRef.format()),
            .dimensions = {width, height},
            .usage = lvk::TextureUsageBits_Sampled,
            .numMipLevels = lvk::calcNumMipLevels(texRef.extent().x, texRef.extent().y),
            .debugName = fileNameKTX.c_str(),
        },
        nullptr);
  }

  const void* data[6] = {
      texRef.data(0, 0, 0),
      texRef.data(0, 1, 0),
      texRef.data(0, 2, 0),
      texRef.data(0, 3, 0),
      texRef.data(0, 4, 0),
      texRef.data(0, 5, 0),
  };

  const lvk::TextureRangeDesc texRefRange = {
      .dimensions = {width, height},
      .numLayers = 6,
      // if compression is enabled, upload all mip-levels
      .numMipLevels = kEnableCompression ? lvk::calcNumMipLevels(width, height) : 1u,
  };
  device_->upload(tex, texRefRange, data);

  if (!kEnableCompression) {
     device_->generateMipmap(tex);
  }
}

gli::texture_cube gliToCube(Bitmap& bmp) {
  IGL_ASSERT(bmp.comp_ == 3); // RGB
  IGL_ASSERT(bmp.type_ == eBitmapType_Cube);
  IGL_ASSERT(bmp.fmt_ == eBitmapFormat_Float);

  const int w = bmp.w_;
  const int h = bmp.h_;

  const gli::texture_cube::extent_type extents{w, h};

  const uint32_t miplevels = lvk::calcNumMipLevels(w, h);

  gli::texture_cube gliTexCube =
      gli::texture_cube(gli::FORMAT_RGBA32_SFLOAT_PACK32, extents, miplevels);

  const int numFacePixels = w * h;

  for (size_t face = 0; face != 6; face++) {
    const vec3* src = reinterpret_cast<vec3*>(bmp.data_.data()) + face * numFacePixels;
    float* dst = (float*)gliTexCube[face].data();
    for (int y = 0; y != h; y++) {
      for (int x = 0; x != w; x++) {
        const vec3& rgb = src[x + y * w];
        gliTexCube[face].store(gli::texture2d::extent_type{x, y}, 0, vec4(rgb, 0.0f));
      }
    }
  }

  return gliTexCube;
}

void generateMipmaps(const std::string& outFilename, gli::texture_cube& cubemap) {
  IGL_PROFILER_FUNCTION();

  LLOGL("Generating mipmaps");

  auto prevWidth = cubemap.extent().x;
  auto prevHeight = cubemap.extent().y;
  for (size_t face = 0; face < 6; ++face) {
    LLOGL(".");
    for (size_t miplevel = 1; miplevel <= cubemap.max_level(); ++miplevel) {
      LLOGL(":");
      const auto width = prevWidth > 1 ? prevWidth >> 1 : 1;
      const auto height = prevHeight > 1 ? prevWidth >> 1 : 1;

      stbir_resize_float((const float*)cubemap.data(0, face, miplevel - 1),
                         prevWidth,
                         prevHeight,
                         0,
                         (float*)cubemap.data(0, face, miplevel),
                         width,
                         height,
                         0,
                         4);

      prevWidth = width;
      prevHeight = height;
    }
    prevWidth = cubemap.extent().x;
    prevHeight = cubemap.extent().y;
  }

  LLOGL("\n");
  gli::save_ktx(cubemap, outFilename);
}

void processCubemap(const std::string& inFilename,
                    const std::string& outFilenameEnv,
                    const std::string& outFilenameIrr) {
  IGL_PROFILER_FUNCTION();

  int sourceWidth, sourceHeight;
  float* pxs = stbi_loadf(inFilename.c_str(), &sourceWidth, &sourceHeight, nullptr, 3);
  SCOPE_EXIT {
    if (pxs) {
      stbi_image_free(pxs);
    }
  };

  if (!IGL_VERIFY(pxs != nullptr)) {
    IGL_ASSERT_MSG(false, "Did you read the tutorial at the top of Tiny_MeshLarge.cpp?");
    return;
  }

  // Environment map
  {
    Bitmap bmp = convertEquirectangularMapToCubeMapFaces(
        Bitmap(sourceWidth, sourceHeight, 3, eBitmapFormat_Float, pxs));
    gli::texture_cube cube = gliToCube(bmp);
    generateMipmaps(outFilenameEnv, cube);
  }

  // Irradiance map
  {
    constexpr int dstW = 256;
    constexpr int dstH = 128;

    std::vector<vec3> out(dstW * dstH);
    convolveDiffuse((vec3*)pxs, sourceWidth, sourceHeight, dstW, dstH, out.data(), 1024);

    Bitmap bmp = convertEquirectangularMapToCubeMapFaces(
        Bitmap(dstW, dstH, 3, eBitmapFormat_Float, out.data()));
    gli::texture_cube cube = gliToCube(bmp);
    generateMipmaps(outFilenameIrr, cube);
  }
}

void loadSkyboxTexture() {
  IGL_PROFILER_FUNCTION();

  const std::string skyboxFileName{"immenstadter_horn_2k"};
  const std::string skyboxSubdir{"src/skybox_hdr/"};

  const std::string fileNameRefKTX = folderContentRoot + skyboxFileName + "_ReferenceMap.ktx";
  const std::string fileNameIrrKTX = folderContentRoot + skyboxFileName + "_IrradianceMap.ktx";

  if (!std::filesystem::exists(fileNameRefKTX) || !std::filesystem::exists(fileNameIrrKTX)) {
    const std::string inFilename = folderContentRoot + skyboxSubdir + skyboxFileName + ".hdr";
    LLOGL("Cubemap in KTX format not found. Extracting from HDR file `%s`...\n", inFilename.c_str());

    processCubemap(inFilename, fileNameRefKTX, fileNameIrrKTX);
  }

  loadCubemapTexture(fileNameRefKTX, skyboxTextureReference_);
  loadCubemapTexture(fileNameIrrKTX, skyboxTextureIrradiance_);
}

lvk::TextureFormat formatFromChannels(uint32_t channels) {
  if (channels == 1) {
    return lvk::TextureFormat::R_UN8;
  }

  if (channels == 4) {
    return kEnableCompression ? lvk::TextureFormat::BC7_RGBA : lvk::TextureFormat::RGBA_UN8;
  }

  return lvk::TextureFormat::Invalid;
}

lvk::TextureHandle createTexture(const LoadedImage& img) {
  if (!img.pixels) {
    return {};
  }

  const auto it = texturesCache_.find(img.debugName);

  if (it != texturesCache_.end()) {
    return it->second;
  }

  const lvk::TextureDesc desc = {
      .type = lvk::TextureType_2D,
      .format = formatFromChannels(img.channels),
      .dimensions = {img.w, img.h},
      .usage = lvk::TextureUsageBits_Sampled,
      .numMipLevels = lvk::calcNumMipLevels(img.w, img.h),
      .debugName = img.debugName.c_str(),
  };

  lvk::Holder<lvk::TextureHandle> tex = device_->createTexture(desc, nullptr);

  if (kEnableCompression && img.channels == 4 &&
      std::filesystem::exists(img.compressedFileName.c_str())) {
    // uploading the texture
    auto gliTex2d = gli::load_ktx(img.compressedFileName.c_str());
    if (gliTex2d.empty()) {
      printf("Failed to load %s\n", img.compressedFileName.c_str());
      assert(0);
    }
    const void* data[] = {gliTex2d.data()};
    device_->upload(tex, {.dimensions = {img.w, img.h}, .numMipLevels = desc.numMipLevels}, data);
  } else {
    const void* data[] = {img.pixels};
    device_->upload(tex, {.dimensions = {img.w, img.h}}, data);
    device_->generateMipmap(tex);
  }

  lvk::TextureHandle handle = tex;

  texturesCache_[img.debugName] = std::move(tex);

  return handle;
}

void processLoadedMaterials() {
  LoadedMaterial mtl;

  {
    std::lock_guard guard(loadedMaterialsMutex_);
    if (loadedMaterials_.empty()) {
      return;
    } else {
      mtl = loadedMaterials_.back();
      loadedMaterials_.pop_back();
      remainingMaterialsToLoad_.fetch_sub(1u, std::memory_order_release);
    }
  }

  {
    MaterialTextures tex;

    tex.ambient = createTexture(mtl.ambient);
    tex.diffuse = createTexture(mtl.diffuse);
    tex.alpha = createTexture(mtl.alpha);

    // update GPU materials
    materials_[mtl.idx].texAmbient = tex.ambient.index();
    materials_[mtl.idx].texDiffuse = tex.diffuse.index();
    materials_[mtl.idx].texAlpha = tex.alpha.index();
    textures_[mtl.idx] = std::move(tex);
  }
  IGL_ASSERT(materials_[mtl.idx].texAmbient >= 0 && materials_[mtl.idx].texAmbient < kMaxTextures);
  IGL_ASSERT(materials_[mtl.idx].texDiffuse >= 0 && materials_[mtl.idx].texDiffuse < kMaxTextures);
  IGL_ASSERT(materials_[mtl.idx].texAlpha >= 0 && materials_[mtl.idx].texAlpha < kMaxTextures);
  device_->upload(sbMaterials_, materials_.data(), sizeof(GPUMaterial) * materials_.size());
}

int main(int argc, char* argv[]) {
  minilog::initialize(nullptr, {.threadNames = false});
  // find the content folder
  {
    using namespace std::filesystem;
    path subdir("third-party/content/");
    path dir = current_path();
    // find the content somewhere above our current build directory
    while (dir != current_path().root_path() && !exists(dir / subdir)) {
      dir = dir.parent_path();
    }
    if (!exists(dir / subdir)) {
      printf("Cannot find the content directory. Run `deploy_content.py` before running this app.");
      IGL_ASSERT(false);
      return EXIT_FAILURE;
    }
    folderThirdParty = (dir / path("third-party/deps/src/")).string();
    folderContentRoot = (dir / subdir).string();
  }

  window_ = lvk::initWindow("Vulkan Bistro", width_, height_);
  initIGL();
  initModel();

  glfwSetCursorPosCallback(window_, [](auto* window, double x, double y) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    mousePos_ = vec2(x / width, 1.0f - y / height);
    ImGui::GetIO().MousePos = ImVec2(x, y);
  });

  glfwSetMouseButtonCallback(window_, [](auto* window, int button, int action, int mods) {
    if (!ImGui::GetIO().WantCaptureMouse) {
      if (button == GLFW_MOUSE_BUTTON_LEFT) {
        mousePressed_ = (action == GLFW_PRESS);
      }
    } else {
      // release the mouse
      mousePressed_ = false;
    }
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    const ImGuiMouseButton_ imguiButton =
        (button == GLFW_MOUSE_BUTTON_LEFT)
            ? ImGuiMouseButton_Left
            : (button == GLFW_MOUSE_BUTTON_RIGHT ? ImGuiMouseButton_Right
                                                 : ImGuiMouseButton_Middle);
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2((float)xpos, (float)ypos);
    io.MouseDown[imguiButton] = action == GLFW_PRESS;
  });

  glfwSetScrollCallback(window_, [](GLFWwindow* window, double dx, double dy) {
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheelH = (float)dx;
    io.MouseWheel = (float)dy;
  });

  glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int, int action, int mods) {
    const bool pressed = action != GLFW_RELEASE;
    if (key == GLFW_KEY_ESCAPE && pressed) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_N && pressed) {
      perFrame_.bDrawNormals = (perFrame_.bDrawNormals + 1) % 2;
    }
    if (key == GLFW_KEY_C && pressed) {
      enableComputePass_ = !enableComputePass_;
    }
    if (key == GLFW_KEY_T && pressed) {
      enableWireframe_ = !enableWireframe_;
    }
    if (key == GLFW_KEY_ESCAPE && pressed)
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_W) {
      positioner_.movement_.forward_ = pressed;
    }
    if (key == GLFW_KEY_S) {
      positioner_.movement_.backward_ = pressed;
    }
    if (key == GLFW_KEY_A) {
      positioner_.movement_.left_ = pressed;
    }
    if (key == GLFW_KEY_D) {
      positioner_.movement_.right_ = pressed;
    }
    if (key == GLFW_KEY_1) {
      positioner_.movement_.up_ = pressed;
    }
    if (key == GLFW_KEY_2) {
      positioner_.movement_.down_ = pressed;
    }
    if (mods & GLFW_MOD_SHIFT) {
      positioner_.movement_.fastSpeed_ = pressed;
    }
    if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) {
      positioner_.movement_.fastSpeed_ = pressed;
    }
    if (key == GLFW_KEY_SPACE) {
      positioner_.setUpVector(vec3(0.0f, 1.0f, 0.0f));
    }
    if (key == GLFW_KEY_L && pressed) {
      perFrame_.bDebugLines = (perFrame_.bDebugLines + 1) % 2;
    }
  });

  if (kEnableCompression) {
    printf(
        "Compressing textures... It can take a while in debug builds...(needs to be done once)\n");
  }

  loadSkyboxTexture();
  loadMaterials();

  fbMain_ = {
      .colorAttachments = {{.texture = device_->getCurrentSwapchainTexture()}},
  };
  createShadowMap();
  createOffscreenFramebuffer();
  createRenderPipelines();
  createRenderPipelineSkybox();
  createComputePipeline();

  imgui_ = std::make_unique<lvk::ImGuiRenderer>(
      *device_,
      (folderThirdParty + "3D-Graphics-Rendering-Cookbook/data/OpenSans-Light.ttf").c_str(),
     float(height_) / 70.0f );

  double prevTime = glfwGetTime();

  uint32_t frameIndex = 0;

  // Main loop
  while (!glfwWindowShouldClose(window_)) {
    {
      imgui_->beginFrame(fbMain_);
      ImGui::ShowDemoWindow();

      ImGui::Begin("Keyboard hints:", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::Text("W/S/A/D - camera movement");
      ImGui::Text("1/2 - camera up/down");
      ImGui::Text("Shift - fast movement");
      ImGui::Text("C - toggle compute shader postprocessing");
      ImGui::Text("N - toggle normals");
      ImGui::Text("T - toggle wireframe");
      ImGui::End();

      if (!textures_[1].diffuse.empty()) {
        ImGui::Begin("Texture Viewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Image(ImTextureID(textures_[1].diffuse.indexAsVoid()), ImVec2(256, 256));
        ImGui::End();
      }

      if (uint32_t num = remainingMaterialsToLoad_.load(std::memory_order_acquire)) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin(
            "Loading...", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
        ImGui::ProgressBar(1.0f - float(num) / cachedMaterials_.size(),
                           ImVec2(ImGui::GetIO().DisplaySize.x, 32));
        ImGui::End();
      }
      // a nice FPS counter
      {
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        const ImGuiViewport* v = ImGui::GetMainViewport();
        IGL_ASSERT(v);
        ImGui::SetNextWindowPos(
            {
                v->WorkPos.x + v->WorkSize.x - 15.0f,
                v->WorkPos.y + 15.0f,
            },
            ImGuiCond_Always,
            {1.0f, 0.0f});
        ImGui::SetNextWindowBgAlpha(0.30f);
        ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize("FPS : _______").x, 0));
        if (ImGui::Begin("##FPS", nullptr, flags)) {
          ImGui::Text("FPS : %i", (int)fps_.getFPS());
          ImGui::Text("Ms  : %.1f", 1000.0 / fps_.getFPS());
        }
        ImGui::End();
      }
    }

    processLoadedMaterials();
    const double newTime = glfwGetTime();
    const double delta = newTime - prevTime;
    fps_.tick(delta);
    positioner_.update(delta, mousePos_, mousePressed_);
    prevTime = newTime;
    render(device_->getCurrentSwapchainTexture(), frameIndex);
    glfwPollEvents();
    frameIndex = (frameIndex + 1) % kNumBufferedFrames;
  }

  loaderShouldExit_.store(true, std::memory_order_release);

  imgui_ = nullptr;
  // destroy all the Vulkan stuff before closing the window
  vb0_ = nullptr;
  ib0_ = nullptr;
  sbMaterials_ = nullptr;
  ubPerFrame_.clear();
  ubPerFrameShadow_.clear();
  ubPerObject_.clear();
  renderPipelineState_Mesh_ = nullptr;
  renderPipelineState_MeshWireframe_ = nullptr;
  renderPipelineState_Shadow_ = nullptr;
  renderPipelineState_Skybox_ = nullptr;
  renderPipelineState_Fullscreen_ = nullptr;
  computePipelineState_Grayscale_ = nullptr;
  textureDummyWhite_ = nullptr;
  skyboxTextureReference_ = nullptr;
  skyboxTextureIrradiance_ = nullptr;
  textures_.clear();
  texturesCache_.clear();
  sampler_ = nullptr;
  samplerShadow_ = nullptr;
  device_->destroy(fbMain_);
  device_->destroy(fbShadowMap_);
  device_->destroy(fbOffscreen_);
  device_ = nullptr;

  glfwDestroyWindow(window_);
  glfwTerminate();

  printf("Waiting for the loader thread to exit...\n");

  loaderPool_ = nullptr;

  return 0;
}
