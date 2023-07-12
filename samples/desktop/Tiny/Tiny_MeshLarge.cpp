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

#include <igl/FPSCounter.h>
#include <igl/IGL.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

// GLI should be included after GLM
#include <gli/save_ktx.hpp>
#include <gli/texture2d.hpp>
#include <gli/texture_cube.hpp>

#include <Compress.h>
#include <meshoptimizer.h>
#include <shared/Camera.h>
#include <shared/UtilsCubemap.h>
#include <stb/stb_image.h>
#include <stb/stb_image_resize.h>
#include <taskflow/taskflow.hpp>
#include <tiny_obj_loader.h>

#define USE_OPENGL_BACKEND 0

#if IGL_BACKEND_OPENGL && !IGL_BACKEND_VULKAN
// no IGL/Vulkan was compiled in, switch to IGL/OpenGL
#undef USE_OPENGL_BACKEND
#define USE_OPENGL_BACKEND 1
#endif

#if defined(__cpp_lib_format)
#include <format>
#define IGL_FORMAT std::format
#else
#include <fmt/core.h>
#define IGL_FORMAT fmt::format
#endif // __cpp_lib_format

#include <igl/IGL.h>

// clang-format off
#if USE_OPENGL_BACKEND
  #include <igl/RenderCommandEncoder.h>
  #include <igl/opengl/RenderCommandEncoder.h>
  #include <igl/opengl/RenderPipelineState.h>
  #if IGL_PLATFORM_WIN
    #include <igl/opengl/wgl/Context.h>
    #include <igl/opengl/wgl/Device.h>
    #include <igl/opengl/wgl/HWDevice.h>
    #include <igl/opengl/wgl/PlatformDevice.h>
  #elif IGL_PLATFORM_LINUX
    #include <igl/opengl/glx/Context.h>
    #include <igl/opengl/glx/Device.h>
    #include <igl/opengl/glx/HWDevice.h>
    #include <igl/opengl/glx/PlatformDevice.h>
  #endif
#else
  #include <igl/vulkan/Common.h>
  #include <igl/vulkan/Device.h>
  #include <igl/vulkan/HWDevice.h>
  #include <igl/vulkan/PlatformDevice.h>
  #include <igl/vulkan/Texture.h>
  #include <igl/vulkan/VulkanContext.h>
#endif
// clang-format on

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#elif __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#else
#error Unsupported OS
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// @fb-only

// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only

constexpr uint32_t kMeshCacheVersion = 0xC0DE0009;
constexpr uint32_t kMaxTextures = 512;
constexpr int kNumSamplesMSAA = 8;
#if USE_OPENGL_BACKEND
constexpr bool kEnableCompression = false;
#else
constexpr bool kEnableCompression = true;
constexpr bool kPreferIntegratedGPU = false;
#if defined(NDEBUG)
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif // NDEBUG
#endif // USE_OPENGL_BACKEND

std::string contentRootFolder;

#if IGL_WITH_IGLU
#include <IGLU/imgui/Session.h>

std::unique_ptr<iglu::imgui::Session> imguiSession_;

igl::shell::InputDispatcher inputDispatcher_;
#endif // IGL_WITH_IGLU

const char* kCodeComputeTest = R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#ifdef VULKAN
// kBinding_StorageImages in VulkanContext.cpp
layout (set = 0, binding = 6, rgba8) uniform readonly  image2D kTextures2Din[];
layout (set = 0, binding = 6, rgba8) uniform writeonly image2D kTextures2Dout[];
#else
layout (binding = 3, rgba8) uniform readonly  image2D kTextures2Din;
layout (binding = 3, rgba8) uniform writeonly image2D kTextures2Dout;
#endif

vec4 imageLoad2D(uint slotTexture, ivec2 uv) {
#ifdef VULKAN
  uint idxTex = bindings.slots[slotTexture].x;
  return imageLoad(kTextures2Din[idxTex], uv);
#else
  return imageLoad(kTextures2Din, uv);
#endif
}

void imageStore2D(uint slotTexture, ivec2 uv, vec4 data) {
#ifdef VULKAN
  uint idxTex = bindings.slots[slotTexture].x;
  imageStore(kTextures2Dout[idxTex], uv, data);
#else
  imageStore(kTextures2Dout, uv, data);
#endif
}

void main() {
   vec4 pixel = imageLoad2D(0, ivec2(gl_GlobalInvocationID.xy));
   float luminance = dot(pixel, vec4(0.299, 0.587, 0.114, 0.0)); // https://www.w3.org/TR/AERT/#color-contrast
   imageStore2D(0, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(luminance), 1.0));
}
)";

const char* kCodeFullscreenVS = R"(
layout (location=0) out vec2 uv;
void main() {
  // generate a triangle covering the entire screen
  uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
#ifdef VULKAN
  gl_Position = vec4(uv * vec2(2, -2) + vec2(-1, 1), 0.0, 1.0);
#else
  gl_Position = vec4(uv * vec2(2, 2) + vec2(-1, -1), 0.0, 1.0);
#endif
}
)";

const char* kCodeFullscreenFS = R"(
layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;
#ifndef VULKAN
uniform sampler2D texFullScreen;
#endif
void main() {
#ifdef VULKAN
  out_FragColor = textureSample2D(0, 0, uv);
#else
  out_FragColor = texture(texFullScreen, uv);
#endif
}
)";

const char* kCodeVS = R"(
layout (location=0) in vec3 pos;
layout (location=1) in vec3 normal;
layout (location=2) in vec2 uv;
#ifdef VULKAN
layout (location=3) in uint mtlIndex;
#else
layout (location=3) in float mtlIndex;
#endif

struct UniformsPerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  int bDrawNormals;
  int bDebugLines;
  vec2 padding;
};

struct UniformsPerObject {
  mat4 model;
};

struct Material {
   vec4 ambient;
   vec4 diffuse;
   int texAmbient;
   int texDiffuse;
   int texAlpha;
   int padding;
};

#ifdef VULKAN
layout(std430, buffer_reference) readonly buffer PerFrame {
  UniformsPerFrame perFrame;
};

layout(std430, buffer_reference) readonly buffer PerObject {
  UniformsPerObject perObject;
};

layout(std430, buffer_reference) readonly buffer Materials {
  Material mtl[];
};
#else
uniform MeshFrameUniforms {
  UniformsPerFrame meshPerFrame;
};
uniform MeshObjectUniforms{
  UniformsPerObject meshPerObject;
};
uniform MeshMaterials{
  Material materials[132];
};
#endif

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
#ifdef VULKAN
  mat4 proj = PerFrame(getBuffer(0)).perFrame.proj;
  mat4 view = PerFrame(getBuffer(0)).perFrame.view;
  mat4 model = PerObject(getBuffer(1)).perObject.model;
  mat4 light = PerFrame(getBuffer(0)).perFrame.light;
  mtl = Materials(getBuffer(2)).mtl[uint(mtlIndex)];
#else
  mat4 proj = meshPerFrame.proj;
  mat4 view = meshPerFrame.view;
  mat4 model = meshPerObject.model;
  mat4 light = meshPerFrame.light;
  mtl = materials[int(mtlIndex)];
#endif
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

struct UniformsPerFrame {
  mat4 proj;
  mat4 view;
};

struct UniformsPerObject {
  mat4 model;
};

#ifdef VULKAN
layout(std430, buffer_reference) readonly buffer PerFrame {
  UniformsPerFrame perFrame;
};
layout(std430, buffer_reference) readonly buffer PerObject {
  UniformsPerObject perObject;
};
#else
uniform MeshFrameUniforms {
  UniformsPerFrame meshPerFrame;
};
uniform MeshObjectUniforms{
  UniformsPerObject meshPerObject;
};
#endif

void main() {
#ifdef VULKAN
  mat4 proj = PerFrame(getBuffer(0)).perFrame.proj;
  mat4 view = PerFrame(getBuffer(0)).perFrame.view;
  mat4 model = PerObject(getBuffer(1)).perObject.model;
#else
  mat4 proj = meshPerFrame.proj;
  mat4 view = meshPerFrame.view;
  mat4 model = meshPerObject.model;
#endif
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
struct UniformsPerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  int bDrawNormals;
  int bDebugLines;
  vec2 padding;
};
#ifdef VULKAN
layout(std430, buffer_reference) readonly buffer PerFrame {
  UniformsPerFrame perFrame;
};
#else
uniform MeshFrameUniforms {
  UniformsPerFrame meshPerFrame;
};
#endif
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

layout (location=0) in PerVertex vtx;
layout (location=5) flat in Material mtl;

layout (location=0) out vec4 out_FragColor;

#ifdef VULKAN
vec4 textureBindless2D(uint textureid, vec2 uv) {
  return texture(sampler2D(kTextures2D[textureid],
                           kSamplers[bindings.slots[0].y]), uv);
}
#else
  layout(binding = 0) uniform sampler2D texShadow;
  layout(binding = 1) uniform sampler2D texAmbient;
  layout(binding = 2) uniform sampler2D texDiffuse;
  layout(binding = 3) uniform sampler2D texAlpha;
  layout(binding = 4) uniform samplerCube texSkyboxIrradiance;
#endif // VULKAN

float PCF3(vec3 uvw) {
#ifdef VULKAN
  float size = 1.0 / textureSize2D(0, 1).x;
#else
  float size = 1.0 / float( textureSize(texShadow, 0).x );
#endif
  float shadow = 0.0;
  for (int v=-1; v<=+1; v++)
    for (int u=-1; u<=+1; u++)
#ifdef VULKAN
      shadow += textureSample2DShadow(0, 1, uvw + size * vec3(u, v, 0));
#else
      shadow += (uvw.z <= texture(texShadow, uvw.xy + size * vec2(u, v) ).r) ? 1.0 : 0.0;
#endif
  return shadow / 9;
}

float shadow(vec4 s) {
  s = s / s.w;
  if (s.z > -1.0 && s.z < 1.0) {
    float depthBias = -0.00005;
#ifdef VULKAN
    s.y = 1.0 - s.y;
#endif
    float shadowSample = PCF3(vec3(s.x, s.y, s.z + depthBias));
    return mix(0.3, 1.0, shadowSample);
  }
  return 1.0;
}

void main() {
#ifdef VULKAN
  vec4 alpha = textureBindless2D(mtl.texAlpha, vtx.uv);
  if (mtl.texAlpha > 0 && alpha.r < 0.5)
    discard;
  vec4 Ka = mtl.ambient * textureBindless2D(mtl.texAmbient, vtx.uv);
  vec4 Kd = mtl.diffuse * textureBindless2D(mtl.texDiffuse, vtx.uv);
  bool drawNormals = PerFrame(getBuffer(0)).perFrame.bDrawNormals > 0;
#else
  vec4 alpha = texture(texAlpha, vtx.uv);
  // check it is not a dummy 1x1 texture
  if (textureSize(texAlpha, 0).x > 1 && alpha.r < 0.5)
    discard;
  vec4 Ka = mtl.ambient * texture(texAmbient, vtx.uv);
  vec4 Kd = mtl.diffuse * texture(texDiffuse, vtx.uv);
  bool drawNormals = meshPerFrame.bDrawNormals > 0;
#endif
  if (Kd.a < 0.5)
    discard;
  vec3 n = normalize(vtx.normal);
  float NdotL1 = clamp(dot(n, normalize(vec3(-1, 1,+1))), 0.0, 1.0);
  float NdotL2 = clamp(dot(n, normalize(vec3(-1, 1,-1))), 0.0, 1.0);
  float NdotL = 0.5 * (NdotL1+NdotL2);
  // IBL diffuse
  const vec4 f0 = vec4(0.04);
#ifdef VULKAN
  vec4 diffuse = textureSampleCube(1, 0, n) * Kd * (vec4(1.0) - f0);
#else
  vec4 diffuse = texture(texSkyboxIrradiance, n) * Kd * (vec4(1.0) - f0);
#endif
  out_FragColor = drawNormals ?
    vec4(0.5 * (n+vec3(1.0)), 1.0) :
    Ka + diffuse * shadow(vtx.shadowCoords);
};
)";

const char* kShadowVS = R"(
layout (location=0) in vec3 pos;

struct UniformsPerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  int bDrawNormals;
  int bDebugLines;
  vec2 padding;
};

struct UniformsPerObject {
  mat4 model;
};

#ifdef VULKAN
layout(std430, buffer_reference) readonly buffer PerFrame {
  UniformsPerFrame perFrame;
};

layout(std430, buffer_reference) readonly buffer PerObject {
  UniformsPerObject perObject;
};
#else
uniform ShadowFrameUniforms {
   UniformsPerFrame perFrame;
};
uniform ShadowObjectUniforms {
  UniformsPerObject perObject;
};

#endif
void main() {
#ifdef VULKAN
  mat4 proj = PerFrame(getBuffer(0)).perFrame.proj;
  mat4 view = PerFrame(getBuffer(0)).perFrame.view;
  mat4 model = PerObject(getBuffer(1)).perObject.model;
#else
  mat4 proj = perFrame.proj;
  mat4 view = perFrame.view;
  mat4 model = perObject.model;
#endif
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

struct UniformsPerFrame {
  mat4 proj;
  mat4 view;
  mat4 light;
  int bDrawNormals;
  int bDebugLines;
  vec2 padding;
};

#ifdef VULKAN
layout(std430, buffer_reference) readonly buffer PerFrame {
  UniformsPerFrame perFrame;
};
#else
uniform SkyboxFrameUniforms {
UniformsPerFrame uParameters;
};

#endif
void main() {
#ifdef VULKAN
  mat4 proj = PerFrame(getBuffer(0)).perFrame.proj;
  mat4 view = PerFrame(getBuffer(0)).perFrame.view;
#else
  mat4 proj = uParameters.proj;
  mat4 view = uParameters.view;
#endif
  // discard translation
  view = mat4(view[0], view[1], view[2], vec4(0, 0, 0, 1));
  mat4 transform = proj * view;
  vec3 pos = positions[indices[gl_VertexIndex]];
  gl_Position = (transform * vec4(pos, 1.0)).xyww;

  // skybox
  textureCoords = pos;
#ifdef VULKAN
  // Draws the skybox edges. One color per edge
  const bool drawDebugLines = PerFrame(getBuffer(0)).perFrame.bDebugLines > 0;
  if (drawDebugLines) {
      const int[12][2] edgeIndices = {
          {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}
      };

      const vec4 edgeColors[12] = vec4[12](
        vec4(  1,   0,   0, 1), vec4(  1,   1,   0, 1), vec4(  0,   1,   0, 1), vec4(  0,   1, 1, 1),
        vec4(  1,   0,   1, 1), vec4(  0,   0,   1, 1), vec4(  1,   1,   1, 1), vec4(  0,   0, 0, 1),
        vec4(0.5, 0.7, 0.8, 1), vec4(0.4, 0.4, 0.4, 1), vec4(  1, 0.3, 0.6, 1), vec4(  1, 0.8, 0, 1)
      );

      uint index = gl_VertexIndex / 3;
      drawLine(positions[edgeIndices[index][0]],
                positions[edgeIndices[index][1]],
                edgeColors[index],
                edgeColors[index],
                transform);
  }
#endif
}

)";
const char* kSkyboxFS = R"(
layout (location=0) in vec3 textureCoords;
layout (location=0) out vec4 out_FragColor;

#ifndef VULKAN
uniform samplerCube texSkybox;
#endif
void main() {
#ifdef VULKAN
  out_FragColor = textureSampleCube(0, 0, textureCoords);
#else
  out_FragColor = texture(texSkybox, textureCoords);
#endif
}
)";

// @fb-only
// @fb-only
// @fb-only

using namespace igl;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

GLFWwindow* window_ = nullptr;
int width_ = 0;
int height_ = 0;
igl::FPSCounter fps_;

constexpr uint32_t kNumBufferedFrames = 3;

std::unique_ptr<IDevice> device_;
std::shared_ptr<ICommandQueue> commandQueue_;
RenderPassDesc renderPassOffscreen_;
RenderPassDesc renderPassMain_;
RenderPassDesc renderPassShadow_;
std::shared_ptr<IFramebuffer> fbMain_; // swapchain
std::shared_ptr<IFramebuffer> fbOffscreen_;
std::shared_ptr<IFramebuffer> fbShadowMap_;
std::shared_ptr<IComputePipelineState> computePipelineState_Grayscale_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Mesh_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_MeshWireframe_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Shadow_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Skybox_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Fullscreen_;
std::shared_ptr<IBuffer> vb0_, ib0_; // buffers for vertices and indices
std::shared_ptr<IBuffer> sbMaterials_; // storage buffer for materials
std::vector<std::shared_ptr<IBuffer>> ubPerFrame_, ubPerFrameShadow_, ubPerObject_;
std::shared_ptr<IVertexInputState> vertexInput0_;
std::shared_ptr<IVertexInputState> vertexInputShadows_;
std::shared_ptr<IDepthStencilState> depthStencilState_;
std::shared_ptr<IDepthStencilState> depthStencilStateLEqual_;
std::shared_ptr<ISamplerState> sampler_;
std::shared_ptr<ISamplerState> samplerShadow_;
std::shared_ptr<ITexture> textureDummyWhite_;
#if USE_OPENGL_BACKEND
std::shared_ptr<ITexture> textureDummyBlack_;
#endif // USE_OPENGL_BACKEND
std::shared_ptr<ITexture> skyboxTextureReference_;
std::shared_ptr<ITexture> skyboxTextureIrradiance_;

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
  int bDrawNormals = 0;
  int bDebugLines = 0;
  vec2 padding;
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
  std::shared_ptr<ITexture> ambient;
  std::shared_ptr<ITexture> diffuse;
  std::shared_ptr<ITexture> alpha;
};

std::vector<MaterialTextures> textures_; // same indexing as in materials_

struct LoadedImage {
  int w = 0;
  int h = 0;
  uint8_t* pixels = nullptr;
  int channels = 0;
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
std::unordered_map<std::string, LoadedImage> imagesCache_; // accessible only from the loader thread
                                                           // pool (multiple threads)
std::unordered_map<std::string, std::shared_ptr<ITexture>> texturesCache_; // accessible only from
                                                                           // the main thread
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
  const std::string compressedPathPrefix = contentRootFolder;

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

bool initWindow(GLFWwindow** outWindow) {
  if (!glfwInit())
    return false;

#if USE_OPENGL_BACKEND
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_VISIBLE, true);
  glfwWindowHint(GLFW_DOUBLEBUFFER, true);
  glfwWindowHint(GLFW_SRGB_CAPABLE, true);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  const char* title = "OpenGL Mesh";
#else
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  const char* title = "Vulkan Mesh";
#endif
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  // render full screen without overlapping taskbar
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);

  int posX = 0;
  int posY = 0;
  int width = mode->width;
  int height = mode->height;

  glfwGetMonitorWorkarea(monitor, &posX, &posY, &width, &height);

  GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);

  if (!window) {
    glfwTerminate();
    return false;
  }

  glfwSetWindowPos(window, posX, posY);

  glfwSetErrorCallback([](int error, const char* description) {
    printf("GLFW Error (%i): %s\n", error, description);
  });

  glfwSetCursorPosCallback(window, [](auto* window, double x, double y) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    mousePos_ = vec2(x / width, 1.0f - y / height);
#if IGL_WITH_IGLU
    inputDispatcher_.queueEvent(igl::shell::MouseMotionEvent(x, y, 0, 0));
#endif // IGL_WITH_IGLU
  });

  glfwSetMouseButtonCallback(window, [](auto* window, int button, int action, int mods) {
#if IGL_WITH_IGLU
    if (!ImGui::GetIO().WantCaptureMouse) {
#endif // IGL_WITH_IGLU
      if (button == GLFW_MOUSE_BUTTON_LEFT) {
        mousePressed_ = (action == GLFW_PRESS);
      }
#if IGL_WITH_IGLU
    } else {
      // release the mouse
      mousePressed_ = false;
    }
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    using igl::shell::MouseButton;
    const MouseButton iglButton =
        (button == GLFW_MOUSE_BUTTON_LEFT)
            ? MouseButton::Left
            : (button == GLFW_MOUSE_BUTTON_RIGHT ? MouseButton::Right : MouseButton::Middle);
    inputDispatcher_.queueEvent(
        igl::shell::MouseButtonEvent(iglButton, action == GLFW_PRESS, (float)xpos, (float)ypos));
#endif // IGL_WITH_IGLU
  });

  glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int mods) {
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

  glfwGetWindowSize(window, &width_, &height_);

  if (outWindow) {
    *outWindow = window;
  }

  return true;
}

void initIGL() {
  // create a device
  {
    {
      Result result;
#if USE_OPENGL_BACKEND
#if IGL_PLATFORM_WIN
      auto ctx = std::make_unique<igl::opengl::wgl::Context>(GetDC(glfwGetWin32Window(window_)),
                                                             glfwGetWGLContext(window_));
      device_ = std::make_unique<igl::opengl::wgl::Device>(std::move(ctx));
#elif IGL_PLATFORM_LINUX
      auto ctx = std::make_unique<igl::opengl::glx::Context>(
          nullptr,
          glfwGetX11Display(),
          (igl::opengl::glx::GLXDrawable)glfwGetX11Window(window_),
          (igl::opengl::glx::GLXContext)glfwGetGLXContext(window_));
      device_ = std::make_unique<igl::opengl::glx::Device>(std::move(ctx));

#endif
#else
      const igl::vulkan::VulkanContextConfig cfg = {
          .maxTextures = kMaxTextures,
          .maxSamplers = 128,
          .terminateOnValidationError = true,
          .enhancedShaderDebugging = true,
          .enableValidation = kEnableValidationLayers,
          .swapChainColorSpace = igl::ColorSpace::SRGB_LINEAR,
      };
#ifdef _WIN32
      auto ctx = vulkan::HWDevice::createContext(cfg, (void*)glfwGetWin32Window(window_));

#elif __APPLE__
      auto ctx = vulkan::HWDevice::createContext(cfg, (void*)glfwGetCocoaWindow(window_));

#elif defined(__linux__)
      auto ctx = vulkan::HWDevice::createContext(
          cfg, (void*)glfwGetX11Window(window_), 0, nullptr, (void*)glfwGetX11Display());

#else
#error Unsupported OS
#endif
      const HWDeviceType hardwareType = kPreferIntegratedGPU ? HWDeviceType::IntegratedGpu
                                                             : HWDeviceType::DiscreteGpu;
      std::vector<HWDeviceDesc> devices =
          vulkan::HWDevice::queryDevices(*ctx.get(), HWDeviceQueryDesc(hardwareType), nullptr);
      if (devices.empty()) {
        const HWDeviceType hardwareType = !kPreferIntegratedGPU ? HWDeviceType::IntegratedGpu
                                                                : HWDeviceType::DiscreteGpu;
        devices =
            vulkan::HWDevice::queryDevices(*ctx.get(), HWDeviceQueryDesc(hardwareType), nullptr);
      }
      IGL_ASSERT_MSG(!devices.empty(), "GPU is not found");
      device_ =
          vulkan::HWDevice::create(std::move(ctx), devices[0], (uint32_t)width_, (uint32_t)height_);
#endif
      IGL_ASSERT(device_);
    }
  }
// @fb-only
  // @fb-only
      // @fb-only
// @fb-only

  {
    const TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                                1,
                                                1,
                                                TextureDesc::TextureUsageBits::Sampled,
                                                "dummy 1x1 (white)");
    textureDummyWhite_ = device_->createTexture(desc, nullptr);
    const uint32_t pixel = 0xFFFFFFFF;
    textureDummyWhite_->upload(TextureRangeDesc::new2D(0, 0, 1, 1), &pixel);
  }

#if USE_OPENGL_BACKEND
  {
    const TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                                1,
                                                1,
                                                TextureDesc::TextureUsageBits::Sampled,
                                                "dummy 1x1 (black)");
    textureDummyBlack_ = device_->createTexture(desc, nullptr);
    const uint32_t pixel = 0xFF000000;
    textureDummyBlack_->upload(TextureRangeDesc::new2D(0, 0, 1, 1), &pixel);
  }

  const auto bufType = BufferDesc::BufferTypeBits::Uniform;
  const auto hint = BufferDesc::BufferAPIHintBits::UniformBlock;
#else
  const auto bufType = BufferDesc::BufferTypeBits::Uniform;
  const auto hint = 0;
#endif
  // create an Uniform buffers to store uniforms for 2 objects
  for (uint32_t i = 0; i != kNumBufferedFrames; i++) {
    ubPerFrame_.push_back(device_->createBuffer(BufferDesc(bufType,
                                                           nullptr,
                                                           sizeof(UniformsPerFrame),
                                                           ResourceStorage::Shared,
                                                           hint,
                                                           "Buffer: uniforms (per frame)"),
                                                nullptr));
    ubPerFrameShadow_.push_back(
        device_->createBuffer(BufferDesc(bufType,
                                         nullptr,
                                         sizeof(UniformsPerFrame),
                                         ResourceStorage::Shared,
                                         hint,
                                         "Buffer: uniforms (per frame shadow)"),
                              nullptr));
    ubPerObject_.push_back(device_->createBuffer(BufferDesc(bufType,
                                                            nullptr,
                                                            sizeof(UniformsPerObject),
                                                            ResourceStorage::Shared,
                                                            hint,
                                                            "Buffer: uniforms (per object)"),
                                                 nullptr));
  }

  {
    VertexInputStateDesc desc;
    desc.numAttributes = 4;
    desc.attributes[0].format = VertexAttributeFormat::Float3;
    desc.attributes[0].offset = offsetof(VertexData, position);
    desc.attributes[0].bufferIndex = 0;
    desc.attributes[0].location = 0;
    desc.attributes[0].name = "pos";
    desc.attributes[1].format = VertexAttributeFormat::Int_2_10_10_10_REV;
    desc.attributes[1].offset = offsetof(VertexData, normal);
    desc.attributes[1].bufferIndex = 0;
    desc.attributes[1].location = 1;
    desc.attributes[1].name = "normal";
    desc.attributes[2].format = VertexAttributeFormat::HalfFloat2;
    desc.attributes[2].offset = offsetof(VertexData, uv);
    desc.attributes[2].bufferIndex = 0;
    desc.attributes[2].location = 2;
    desc.attributes[2].name = "uv";
    desc.attributes[3].format = VertexAttributeFormat::UInt1;
    desc.attributes[3].offset = offsetof(VertexData, mtlIndex);
    desc.attributes[3].bufferIndex = 0;
    desc.attributes[3].location = 3;
    desc.attributes[3].name = "mtlIndex";
    desc.numInputBindings = 1;
    desc.inputBindings[0].stride = sizeof(VertexData);
    vertexInput0_ = device_->createVertexInputState(desc, nullptr);
  }

  {
    VertexInputStateDesc desc;
    desc.numAttributes = 1;
    desc.attributes[0].format = VertexAttributeFormat::Float3;
    desc.attributes[0].offset = offsetof(VertexData, position);
    desc.attributes[0].bufferIndex = 0;
    desc.attributes[0].location = 0;
    desc.attributes[0].name = "pos";
    desc.numInputBindings = 1;
    desc.inputBindings[0].stride = sizeof(VertexData);
    vertexInputShadows_ = device_->createVertexInputState(desc, nullptr);
  }

  {
    DepthStencilStateDesc desc;
    desc.isDepthWriteEnabled = true;
    desc.compareFunction = igl::CompareFunction::Less;
    depthStencilState_ = device_->createDepthStencilState(desc, nullptr);

    desc.compareFunction = igl::CompareFunction::LessEqual;
    depthStencilStateLEqual_ = device_->createDepthStencilState(desc, nullptr);
  }

  {
    igl::SamplerStateDesc desc = igl::SamplerStateDesc::newLinear();
    desc.addressModeU = igl::SamplerAddressMode::Repeat;
    desc.addressModeV = igl::SamplerAddressMode::Repeat;
    desc.mipFilter = igl::SamplerMipFilter::Linear;
    desc.debugName = "Sampler: linear";
    sampler_ = device_->createSamplerState(desc, nullptr);

    desc.addressModeU = igl::SamplerAddressMode::Clamp;
    desc.addressModeV = igl::SamplerAddressMode::Clamp;
    desc.mipFilter = igl::SamplerMipFilter::Disabled;
    desc.debugName = "Sampler: shadow";
    desc.depthCompareEnabled = true;
    desc.depthCompareFunction = igl::CompareFunction::LessEqual;
    samplerShadow_ = device_->createSamplerState(desc, nullptr);
  }

  // Command queue: backed by different types of GPU HW queues
  CommandQueueDesc desc{CommandQueueType::Graphics};
  commandQueue_ = device_->createCommandQueue(desc, nullptr);

  renderPassOffscreen_.colorAttachments.push_back(igl::RenderPassDesc::ColorAttachmentDesc{});
  renderPassOffscreen_.colorAttachments.back().loadAction = LoadAction::Clear;
  renderPassOffscreen_.colorAttachments.back().storeAction =
      kNumSamplesMSAA > 1 ? StoreAction::MsaaResolve : StoreAction::Store;
  renderPassOffscreen_.colorAttachments.back().clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
  renderPassOffscreen_.depthAttachment.loadAction = LoadAction::Clear;
  renderPassOffscreen_.depthAttachment.storeAction = StoreAction::DontCare;
  renderPassOffscreen_.depthAttachment.clearDepth = 1.0f;

  renderPassMain_.colorAttachments.push_back(igl::RenderPassDesc::ColorAttachmentDesc{});
  renderPassMain_.colorAttachments.back().loadAction = LoadAction::Clear;
  renderPassMain_.colorAttachments.back().storeAction = StoreAction::Store;
  renderPassMain_.colorAttachments.back().clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
  renderPassMain_.depthAttachment.loadAction = LoadAction::Clear;
  renderPassMain_.depthAttachment.storeAction = StoreAction::DontCare;
  renderPassMain_.depthAttachment.clearDepth = 1.0f;

#if USE_OPENGL_BACKEND
  renderPassShadow_.colorAttachments.push_back(igl::RenderPassDesc::ColorAttachmentDesc{});
  renderPassShadow_.colorAttachments.back().loadAction = LoadAction::Clear;
  renderPassShadow_.colorAttachments.back().storeAction = StoreAction::Store;
  renderPassShadow_.colorAttachments.back().clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
#endif
  renderPassShadow_.depthAttachment.loadAction = LoadAction::Clear;
  renderPassShadow_.depthAttachment.storeAction = StoreAction::Store;
  renderPassShadow_.depthAttachment.clearDepth = 1.0f;
}

namespace {
void normalizeName(std::string& name) {
#if defined(__linux__)
  std::replace(name.begin(), name.end(), '\\', '/');
#endif
}
} // namespace

bool loadAndCache(const char* cacheFileName) {
  // load 3D model and cache it
  IGL_LOG_INFO("Loading `exterior.obj`... It can take a while in debug builds...\n");

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
                       (contentRootFolder + "src/bistro/Exterior/exterior.obj").c_str(),
                       (contentRootFolder + "src/bistro/Exterior/").c_str());

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

  IGL_LOG_INFO("Caching mesh...\n");

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
#if USE_OPENGL_BACKEND
  vertexData_.clear();
  vertexData_.assign(shapeData.begin(), shapeData.end());
#endif
  return fclose(cacheFile) == 0;
}

bool loadFromCache(const char* cacheFileName) {
  FILE* cacheFile = fopen(cacheFileName, "rb");
  IGL_SCOPE_EXIT {
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
    IGL_LOG_INFO("Cache file has wrong version id\n");
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
#if !USE_OPENGL_BACKEND
  CHECK_READ(numVertices, fread(vertexData_.data(), sizeof(VertexData), numVertices, cacheFile));
  CHECK_READ(numIndices, fread(indexData_.data(), sizeof(uint32_t), numIndices, cacheFile));
#else
  fseek(cacheFile, sizeof(VertexData) * numVertices + sizeof(uint32_t) * numIndices, SEEK_CUR);
  CHECK_READ(1, fread(&numVertices, sizeof(numVertices), 1, cacheFile));
  vertexData_.resize(numVertices);
  CHECK_READ(numVertices, fread(vertexData_.data(), sizeof(VertexData), numVertices, cacheFile));
  uint32_t numShapeVertices = 0;
  CHECK_READ(1, fread(&numShapeVertices, sizeof(numShapeVertices), 1, cacheFile));
  shapeVertexCnt_.resize(numShapeVertices);
  CHECK_READ(numShapeVertices,
             fread(shapeVertexCnt_.data(), sizeof(uint32_t), numShapeVertices, cacheFile));
#endif
#undef CHECK_READ
  return true;
}

void initModel() {
  const std::string cacheFileName = contentRootFolder + "cache.data";

  if (!loadFromCache(cacheFileName.c_str())) {
    if (!IGL_VERIFY(loadAndCache(cacheFileName.c_str()))) {
      IGL_ASSERT_MSG(false, "Cannot load 3D model");
    }
  }

#if USE_OPENGL_BACKEND
  const uint32_t id = 0;
#else
  const uint32_t id = (uint32_t)textureDummyWhite_->getTextureId();
#endif

  for (const auto& mtl : cachedMaterials_) {
    materials_.push_back(GPUMaterial{vec4(mtl.ambient, 1.0f), vec4(mtl.diffuse, 1.0f), id, id});
  }
#if USE_OPENGL_BACKEND
  const auto bufType = BufferDesc::BufferTypeBits::Uniform;
  const auto hint = BufferDesc::BufferAPIHintBits::UniformBlock;
#else
  const auto bufType = BufferDesc::BufferTypeBits::Storage;
  const auto hint = 0;
#endif
  sbMaterials_ = device_->createBuffer(BufferDesc(bufType,
                                                  materials_.data(),
                                                  sizeof(GPUMaterial) * materials_.size(),
                                                  ResourceStorage::Private,
                                                  hint,
                                                  "Buffer: materials"),
                                       nullptr);

  vb0_ = device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Vertex,
                                          vertexData_.data(),
                                          sizeof(VertexData) * vertexData_.size(),
                                          ResourceStorage::Private,
                                          hint,
                                          "Buffer: vertex"),
                               nullptr);
  ib0_ = device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Index,
                                          indexData_.data(),
                                          sizeof(uint32_t) * indexData_.size(),
                                          ResourceStorage::Private,
                                          hint,
                                          "Buffer: index"),
                               nullptr);
}

void createComputePipeline() {
  if (computePipelineState_Grayscale_) {
    return;
  }

  ComputePipelineDesc desc;
#if USE_OPENGL_BACKEND
  std::string computeCode = std::string("#version 460") + kCodeComputeTest;
  kCodeComputeTest = computeCode.c_str();
#endif
  desc.shaderStages = ShaderStagesCreator::fromModuleStringInput(
      *device_, kCodeComputeTest, "main", "Shader Module: grayscale (comp)", nullptr);

  computePipelineState_Grayscale_ = device_->createComputePipeline(desc, nullptr);
}

void createRenderPipelines() {
  if (renderPipelineState_Mesh_) {
    return;
  }

  IGL_ASSERT(fbMain_);

  {
    RenderPipelineDesc desc;

    desc.targetDesc.colorAttachments.resize(1);
    desc.targetDesc.colorAttachments[0].textureFormat = fbMain_->getColorAttachment(0)->getFormat();

    if (fbMain_->getDepthAttachment()) {
      desc.targetDesc.depthAttachmentFormat = fbMain_->getDepthAttachment()->getFormat();
    }

    desc.vertexInputState = vertexInput0_;

// @fb-only
    // @fb-only
      // @fb-only
          // @fb-only
           // @fb-only
           // @fb-only
           // @fb-only
      // @fb-only
      // @fb-only
          // @fb-only
          // @fb-only
          // @fb-only
          // @fb-only
      // @fb-only
      // @fb-only
          // @fb-only
           // @fb-only
           // @fb-only
           // @fb-only
      // @fb-only
          // @fb-only
          // @fb-only
          // @fb-only
          // @fb-only
      // @fb-only
      // @fb-only
          // @fb-only
              // @fb-only
              // @fb-only
              // @fb-only
              // @fb-only
              // @fb-only
              // @fb-only
              // @fb-only
              // @fb-only
              // @fb-only
              // @fb-only
    // @fb-only
// @fb-only
#if USE_OPENGL_BACKEND
    std::string vsCode = std::string("#version 460") + kCodeVS;
    kCodeVS = vsCode.c_str();
    std::string fsCode = std::string("#version 460") + kCodeFS;
    kCodeFS = fsCode.c_str();
#endif
    desc.shaderStages = ShaderStagesCreator::fromModuleStringInput(*device_,
                                                                   kCodeVS,
                                                                   "main",
                                                                   "Shader Module: main (vert)",
                                                                   kCodeFS,
                                                                   "main",
                                                                   "Shader Module: main (frag)",
                                                                   nullptr);
// @fb-only
#if USE_OPENGL_BACKEND
    desc.uniformBlockBindingMap.emplace(
        0, std::make_pair(IGL_NAMEHANDLE("MeshFrameUniforms"), igl::NameHandle{}));
    desc.uniformBlockBindingMap.emplace(
        1, std::make_pair(IGL_NAMEHANDLE("MeshObjectUniforms"), igl::NameHandle{}));
    desc.uniformBlockBindingMap.emplace(
        2, std::make_pair(IGL_NAMEHANDLE("MeshMaterials"), igl::NameHandle{}));
#endif
    desc.cullMode = igl::CullMode::Back;
    desc.frontFaceWinding = igl::WindingMode::CounterClockwise;
    desc.sampleCount = kNumSamplesMSAA;
    desc.debugName = IGL_NAMEHANDLE("Pipeline: mesh");
#if USE_OPENGL_BACKEND
    desc.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("texShadow");
    desc.fragmentUnitSamplerMap[1] = IGL_NAMEHANDLE("texAmbient");
    desc.fragmentUnitSamplerMap[2] = IGL_NAMEHANDLE("texDiffuse");
    desc.fragmentUnitSamplerMap[3] = IGL_NAMEHANDLE("texAlpha");
    desc.fragmentUnitSamplerMap[4] = IGL_NAMEHANDLE("texSkyboxIrradiance");
#endif
    renderPipelineState_Mesh_ = device_->createRenderPipeline(desc, nullptr);
    desc.polygonFillMode = igl::PolygonFillMode::Line;
    desc.vertexInputState = vertexInputShadows_; // positions-only
#if USE_OPENGL_BACKEND
    const std::string vsCodeWireframe = std::string("#version 460") + kCodeVS_Wireframe;
    kCodeVS_Wireframe = vsCodeWireframe.c_str();
    const std::string fsCodeWireframe = std::string("#version 460") + kCodeFS_Wireframe;
    kCodeFS_Wireframe = fsCodeWireframe.c_str();
    desc.fragmentUnitSamplerMap.clear();
    desc.uniformBlockBindingMap.clear();
    desc.uniformBlockBindingMap.emplace(
        0, std::make_pair(IGL_NAMEHANDLE("MeshFrameUniforms"), igl::NameHandle{}));
    desc.uniformBlockBindingMap.emplace(
        1, std::make_pair(IGL_NAMEHANDLE("MeshObjectUniforms"), igl::NameHandle{}));
#endif
    desc.shaderStages =
        ShaderStagesCreator::fromModuleStringInput(*device_,
                                                   kCodeVS_Wireframe,
                                                   "main",
                                                   "Shader Module: main wireframe (vert)",
                                                   kCodeFS_Wireframe,
                                                   "main",
                                                   "Shader Module: main wireframe (frag)",
                                                   nullptr);
    renderPipelineState_MeshWireframe_ = device_->createRenderPipeline(desc, nullptr);
  }

  // shadow
  {
    RenderPipelineDesc desc;
    desc.targetDesc.colorAttachments.clear();
    desc.targetDesc.depthAttachmentFormat = fbShadowMap_->getDepthAttachment()->getFormat();
    desc.vertexInputState = vertexInputShadows_;
#if USE_OPENGL_BACKEND
    std::string vsCode = std::string("#version 460") + kShadowVS;
    kShadowVS = vsCode.c_str();
    std::string fsCode = std::string("#version 460") + kShadowFS;
    kShadowFS = fsCode.c_str();
#endif
    desc.shaderStages = ShaderStagesCreator::fromModuleStringInput(*device_,
                                                                   kShadowVS,
                                                                   "main",
                                                                   "Shader Module: shadow (vert)",
                                                                   kShadowFS,
                                                                   "main",
                                                                   "Shader Module: shadow (frag)",
                                                                   nullptr);
#if USE_OPENGL_BACKEND
    size_t bindingPoint = 0;
    desc.uniformBlockBindingMap.emplace(
        bindingPoint++, std::make_pair(IGL_NAMEHANDLE("ShadowFrameUniforms"), igl::NameHandle{}));
    desc.uniformBlockBindingMap.emplace(
        bindingPoint++, std::make_pair(IGL_NAMEHANDLE("ShadowObjectUniforms"), igl::NameHandle{}));
#endif
    desc.cullMode = igl::CullMode::Disabled;
    desc.debugName = IGL_NAMEHANDLE("Pipeline: shadow");
    renderPipelineState_Shadow_ = device_->createRenderPipeline(desc, nullptr);
  }

  // fullscreen
  {
    RenderPipelineDesc desc;
    desc.targetDesc.colorAttachments.resize(1);
    desc.targetDesc.colorAttachments[0].textureFormat = fbMain_->getColorAttachment(0)->getFormat();
    if (fbMain_->getDepthAttachment()) {
      desc.targetDesc.depthAttachmentFormat = fbMain_->getDepthAttachment()->getFormat();
    }
#if USE_OPENGL_BACKEND
    std::string vsCode = std::string("#version 460") + kCodeFullscreenVS;
    stringReplaceAll(vsCode, "gl_VertexIndex", "gl_VertexID");
    kCodeFullscreenVS = vsCode.c_str();
    std::string fsCode = std::string("#version 460") + kCodeFullscreenFS;
    kCodeFullscreenFS = fsCode.c_str();
#endif
    desc.shaderStages =
        ShaderStagesCreator::fromModuleStringInput(*device_,
                                                   kCodeFullscreenVS,
                                                   "main",
                                                   "Shader Module: fullscreen (vert)",
                                                   kCodeFullscreenFS,
                                                   "main",
                                                   "Shader Module: fullscreen (frag)",
                                                   nullptr);
    desc.cullMode = igl::CullMode::Disabled;
    desc.debugName = IGL_NAMEHANDLE("Pipeline: fullscreen");
    desc.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("texFullScreen");
    renderPipelineState_Fullscreen_ = device_->createRenderPipeline(desc, nullptr);
  }
}

void createRenderPipelineSkybox() {
  if (renderPipelineState_Skybox_) {
    return;
  }

  IGL_ASSERT(fbMain_);

  RenderPipelineDesc desc;
  desc.targetDesc.colorAttachments.resize(1);
  desc.targetDesc.colorAttachments[0].textureFormat = fbMain_->getColorAttachment(0)->getFormat();

  if (fbMain_->getDepthAttachment()) {
    desc.targetDesc.depthAttachmentFormat = fbMain_->getDepthAttachment()->getFormat();
  }

// @fb-only
  // @fb-only
    // @fb-only
        // @fb-only
         // @fb-only
         // @fb-only
    // @fb-only
    // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
    // @fb-only
    // @fb-only
        // @fb-only
            // @fb-only
             // @fb-only
    // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
    // @fb-only
    // @fb-only
        // @fb-only
            // @fb-only
            // @fb-only
            // @fb-only
            // @fb-only
            // @fb-only
            // @fb-only
            // @fb-only
            // @fb-only
            // @fb-only
            // @fb-only
  // @fb-only
// @fb-only
#if USE_OPENGL_BACKEND
  std::string vsCode = std::string("#version 460") + kSkyboxVS;
  stringReplaceAll(vsCode, "gl_VertexIndex", "gl_VertexID");
  kSkyboxVS = vsCode.c_str();
  std::string fsCode = std::string("#version 460") + kSkyboxFS;
  kSkyboxFS = fsCode.c_str();
#endif
  desc.shaderStages = ShaderStagesCreator::fromModuleStringInput(*device_,
                                                                 kSkyboxVS,
                                                                 "main",
                                                                 "Shader Module: skybox (vert)",
                                                                 kSkyboxFS,
                                                                 "main",
                                                                 "Shader Module: skybox (frag)",
                                                                 nullptr);
// @fb-only
#if USE_OPENGL_BACKEND
  size_t bindingPoint = 0;
  desc.uniformBlockBindingMap.emplace(
      bindingPoint++, std::make_pair(IGL_NAMEHANDLE("SkyboxFrameUniforms"), igl::NameHandle{}));
#endif
  desc.cullMode = igl::CullMode::Front;
  desc.frontFaceWinding = igl::WindingMode::CounterClockwise;
  desc.sampleCount = kNumSamplesMSAA;
  desc.debugName = IGL_NAMEHANDLE("Pipeline: skybox");
#if USE_OPENGL_BACKEND
  desc.fragmentUnitSamplerMap[1] = IGL_NAMEHANDLE("texSkybox");
#endif
  renderPipelineState_Skybox_ = device_->createRenderPipeline(desc, nullptr);
}

std::shared_ptr<ITexture> getNativeDrawable() {
  IGL_PROFILER_FUNCTION();
  Result ret;
  std::shared_ptr<ITexture> drawable;
#if USE_OPENGL_BACKEND
#if IGL_PLATFORM_WIN
  const auto& platformDevice = device_->getPlatformDevice<opengl::wgl::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDrawable(&ret);
#elif IGL_PLATFORM_LINUX
  const auto& platformDevice = device_->getPlatformDevice<opengl::glx::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDrawable(width_, height_, &ret);
#endif
#else
  const auto& platformDevice = device_->getPlatformDevice<igl::vulkan::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDrawable(&ret);
#endif
  IGL_ASSERT_MSG(ret.isOk(), ret.message.c_str());
  IGL_ASSERT(drawable != nullptr);
  return drawable;
}

std::shared_ptr<ITexture> getNativeDepthDrawable() {
  IGL_PROFILER_FUNCTION();
  Result ret;
  std::shared_ptr<ITexture> drawable;
#if USE_OPENGL_BACKEND
#if IGL_PLATFORM_WIN
  const auto& platformDevice = device_->getPlatformDevice<opengl::wgl::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDepth(width_, height_, &ret);
#elif IGL_PLATFORM_LINUX
  const auto& platformDevice = device_->getPlatformDevice<opengl::glx::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDepth(width_, height_, &ret);
#endif
#else
  const auto& platformDevice = device_->getPlatformDevice<igl::vulkan::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDepth(width_, height_, &ret);
#endif
  IGL_ASSERT_MSG(ret.isOk(), ret.message.c_str());
  IGL_ASSERT(drawable != nullptr);
  return drawable;
}

void createFramebuffer(const std::shared_ptr<ITexture>& nativeDrawable) {
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = nativeDrawable;
  framebufferDesc.depthAttachment.texture = getNativeDepthDrawable();
  fbMain_ = device_->createFramebuffer(framebufferDesc, nullptr);
  IGL_ASSERT(fbMain_);
}

void createShadowMap() {
  const uint32_t w = 4096;
  const uint32_t h = 4096;
  auto desc = TextureDesc::new2D(igl::TextureFormat::Z_UNorm16,
                                 w,
                                 h,
                                 TextureDesc::TextureUsageBits::Attachment |
                                     TextureDesc::TextureUsageBits::Sampled,
                                 "Shadow map");
  desc.numMipLevels = TextureDesc::calcNumMipLevels(w, h);
  Result ret;
  std::shared_ptr<ITexture> shadowMap = device_->createTexture(desc, &ret);
  IGL_ASSERT(ret.isOk());

  FramebufferDesc framebufferDesc;

  framebufferDesc.depthAttachment.texture = shadowMap;
#if USE_OPENGL_BACKEND
  // OpenGL backend requires both color and depth attchments.
  auto descColor = TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                      w,
                                      h,
                                      TextureDesc::TextureUsageBits::Sampled |
                                          TextureDesc::TextureUsageBits::Attachment,
                                      "shadow color framebuffer");
  descColor.numMipLevels = TextureDesc::calcNumMipLevels(w, h);
  std::shared_ptr<ITexture> texColor = device_->createTexture(descColor, &ret);
  IGL_ASSERT(ret.isOk());
  framebufferDesc.colorAttachments[0].texture = texColor;
#endif
  fbShadowMap_ = device_->createFramebuffer(framebufferDesc, nullptr);
  IGL_ASSERT(fbShadowMap_);
}

void createOffscreenFramebuffer() {
  const uint32_t w = width_;
  const uint32_t h = height_;
  Result ret;
  auto descDepth = TextureDesc::new2D(igl::TextureFormat::Z_UNorm24,
                                      w,
                                      h,
                                      TextureDesc::TextureUsageBits::Attachment |
                                          TextureDesc::TextureUsageBits::Sampled,
                                      "Offscreen framebuffer (d)");
  descDepth.numMipLevels = TextureDesc::calcNumMipLevels(w, h);
  if (kNumSamplesMSAA > 1) {
    descDepth.usage = TextureDesc::TextureUsageBits::Attachment;
    descDepth.numSamples = kNumSamplesMSAA;
    descDepth.numMipLevels = 1;
  }
  std::shared_ptr<ITexture> texDepth = device_->createTexture(descDepth, &ret);
  IGL_ASSERT(ret.isOk());

  TextureDesc::TextureUsage usage =
      TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled;
  const TextureFormat format = igl::TextureFormat::RGBA_UNorm8;
#if !USE_OPENGL_BACKEND
  usage |= TextureDesc::TextureUsageBits::Storage; // compute shader postprocessing
#endif

  auto descColor = TextureDesc::new2D(format, w, h, usage, "Offscreen framebuffer (c)");
  descColor.numMipLevels = TextureDesc::calcNumMipLevels(w, h);
  if (kNumSamplesMSAA > 1) {
    descColor.usage = TextureDesc::TextureUsageBits::Attachment;
    descColor.numSamples = kNumSamplesMSAA;
    descColor.numMipLevels = 1;
  }
  std::shared_ptr<ITexture> texColor = device_->createTexture(descColor, &ret);
  IGL_ASSERT(ret.isOk());

  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = texColor;
  framebufferDesc.depthAttachment.texture = texDepth;
  if (kNumSamplesMSAA > 1) {
    auto descColorResolve =
        TextureDesc::new2D(format, w, h, usage, "Offscreen framebuffer (c - resolve)");
    descColorResolve.usage = usage;
    std::shared_ptr<ITexture> texResolveColor = device_->createTexture(descColorResolve, &ret);
    IGL_ASSERT(ret.isOk());
    framebufferDesc.colorAttachments[0].resolveTexture = texResolveColor;
  }
  fbOffscreen_ = device_->createFramebuffer(framebufferDesc, nullptr);
  IGL_ASSERT(fbOffscreen_);
}

void render(const std::shared_ptr<ITexture>& nativeDrawable, uint32_t frameIndex) {
  IGL_PROFILER_FUNCTION();

  fbMain_->updateDrawable(nativeDrawable);

  // from igl/shell/renderSessions/Textured3DCubeSession.cpp
  const float fov = float(45.0f * (M_PI / 180.0f));
  const float aspectRatio = (float)width_ / (float)height_;

  const mat4 shadowProj = glm::perspective(float(60.0f * (M_PI / 180.0f)), 1.0f, 10.0f, 4000.0f);
  const mat4 shadowView = mat4(vec4(0.772608519f, 0.532385886f, -0.345892131f, 0),
                               vec4(0, 0.544812560f, 0.838557839f, 0),
                               vec4(0.634882748f, -0.647876859f, 0.420926809f, 0),
                               vec4(-58.9244843f, -30.4530792f, -508.410126f, 1.0f));
#if USE_OPENGL_BACKEND
  const mat4 scaleBias =
      mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.5, 0.5, 0.5, 1.0);
#else
  const mat4 scaleBias =
      mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.0, 1.0);
#endif

  perFrame_.proj = glm::perspective(fov, aspectRatio, 0.5f, 500.0f);
  perFrame_.view = camera_.getViewMatrix();
  perFrame_.light = scaleBias * shadowProj * shadowView;

  ubPerFrame_[frameIndex]->upload(&perFrame_, igl::BufferRange(sizeof(perFrame_), 0));

  {
    UniformsPerFrame perFrameShadow;
    perFrameShadow.proj = shadowProj;
    perFrameShadow.view = shadowView;
    ubPerFrameShadow_[frameIndex]->upload(&perFrameShadow,
                                          igl::BufferRange(sizeof(perFrameShadow), 0));
  }

  UniformsPerObject perObject;

  perObject.model = glm::scale(mat4(1.0f), vec3(0.05f));

  ubPerObject_[frameIndex]->upload(&perObject, igl::BufferRange(sizeof(perObject), 0));

  // Command buffers (1-N per thread): create, submit and forget

  // Pass 1: shadows
  if (isShadowMapDirty_) {
    std::shared_ptr<ICommandBuffer> buffer =
        commandQueue_->createCommandBuffer(CommandBufferDesc(), nullptr);

    auto commands = buffer->createRenderCommandEncoder(renderPassShadow_, fbShadowMap_);

    commands->bindRenderPipelineState(renderPipelineState_Shadow_);
    commands->pushDebugGroupLabel("Render Shadows", igl::Color(1, 0, 0));
    commands->bindDepthStencilState(depthStencilState_);
    commands->bindBuffer(0, BindTarget::kVertex, vb0_, 0);
#if USE_OPENGL_BACKEND
    const auto& glPipelineState =
        static_cast<const igl::opengl::RenderPipelineState*>(renderPipelineState_Shadow_.get());
    const int ubPerFrameShadowIdx =
        glPipelineState->getUniformBlockBindingPoint(IGL_NAMEHANDLE(("ShadowFrameUniforms")));

    const int ubPerObjectIdx =
        glPipelineState->getUniformBlockBindingPoint(IGL_NAMEHANDLE(("ShadowObjectUniforms")));
#else
    const int ubPerFrameShadowIdx = 0;
    const int ubPerObjectIdx = 1;
#endif
    commands->bindBuffer(
        ubPerFrameShadowIdx, BindTarget::kAllGraphics, ubPerFrameShadow_[frameIndex], 0);
    commands->bindBuffer(ubPerObjectIdx, BindTarget::kAllGraphics, ubPerObject_[frameIndex], 0);

#if USE_OPENGL_BACKEND
    int start = 0;
    for (auto numVertices : shapeVertexCnt_) {
      commands->draw(PrimitiveType::Triangle, start, numVertices);
      start += numVertices;
    }
#else
    commands->drawIndexed(
        PrimitiveType::Triangle, indexData_.size(), igl::IndexFormat::UInt32, *ib0_.get(), 0);
#endif
    commands->popDebugGroupLabel();
    commands->endEncoding();

    buffer->present(fbShadowMap_->getDepthAttachment());

    commandQueue_->submit(*buffer);

    fbShadowMap_->getDepthAttachment()->generateMipmap(*commandQueue_.get());

    isShadowMapDirty_ = false;
  }

  // Pass 2: mesh
  {
    std::shared_ptr<ICommandBuffer> buffer =
        commandQueue_->createCommandBuffer(CommandBufferDesc(), nullptr);

    // This will clear the framebuffer
    auto commands = buffer->createRenderCommandEncoder(renderPassOffscreen_, fbOffscreen_);
    // Scene
    commands->bindRenderPipelineState(renderPipelineState_Mesh_);
    commands->pushDebugGroupLabel("Render Mesh", igl::Color(1, 0, 0));
    commands->bindDepthStencilState(depthStencilState_);
    commands->bindBuffer(0, BindTarget::kVertex, vb0_, 0);

#if USE_OPENGL_BACKEND
    const auto& glPipelineState =
        static_cast<const igl::opengl::RenderPipelineState*>(renderPipelineState_Mesh_.get());
    const int ubPerFrameIdx =
        glPipelineState->getUniformBlockBindingPoint(IGL_NAMEHANDLE(("MeshFrameUniforms")));

    const int ubPerObjectIdx =
        glPipelineState->getUniformBlockBindingPoint(IGL_NAMEHANDLE(("MeshObjectUniforms")));

    const int sbIdx =
        glPipelineState->getUniformBlockBindingPoint(IGL_NAMEHANDLE(("MeshMaterials")));
#else
    const int ubPerFrameIdx = 0;
    const int ubPerObjectIdx = 1;
    const int sbIdx = 2;
#endif
    commands->bindBuffer(ubPerFrameIdx, BindTarget::kAllGraphics, ubPerFrame_[frameIndex], 0);
    commands->bindBuffer(ubPerObjectIdx, BindTarget::kAllGraphics, ubPerObject_[frameIndex], 0);
    commands->bindBuffer(sbIdx, BindTarget::kAllGraphics, sbMaterials_, 0);

#if USE_OPENGL_BACKEND
    commands->bindBuffer(0, BindTarget::kVertex, vb0_, 0);
    int shapeStart = 0;
    for (auto numVertices : shapeVertexCnt_) {
      const uint32_t imageIdx = (uint32_t)vertexData_[shapeStart].mtlIndex;
      const auto ambientTextureReference =
          strstr(cachedMaterials_[imageIdx].name, "MASTER_Glass_") ? textureDummyWhite_
          : textures_[imageIdx].ambient                            ? textures_[imageIdx].ambient
                                                                   : textureDummyBlack_;
      const auto diffuseTextureReference =
          strstr(cachedMaterials_[imageIdx].name, "MASTER_Glass_Clean") ? textureDummyWhite_
          : textures_[imageIdx].diffuse ? textures_[imageIdx].diffuse
                                        : textureDummyBlack_;
      const auto alphaTextureReference = strstr(cachedMaterials_[imageIdx].name, "MASTER_Glass_")
                                             ? textureDummyWhite_
                                         : textures_[imageIdx].alpha ? textures_[imageIdx].alpha
                                                                     : textureDummyBlack_;

      commands->bindTexture(0, igl::BindTarget::kFragment, fbShadowMap_->getDepthAttachment());
      commands->bindTexture(1, igl::BindTarget::kFragment, ambientTextureReference);
      commands->bindTexture(2, igl::BindTarget::kFragment, diffuseTextureReference);
      commands->bindTexture(3, igl::BindTarget::kFragment, alphaTextureReference);
      commands->bindTexture(4, igl::BindTarget::kFragment, skyboxTextureIrradiance_);
      commands->bindSamplerState(0, igl::BindTarget::kFragment, samplerShadow_);
      commands->bindSamplerState(1, igl::BindTarget::kFragment, sampler_);
      commands->bindSamplerState(2, igl::BindTarget::kFragment, sampler_);
      commands->bindSamplerState(3, igl::BindTarget::kFragment, sampler_);
      commands->bindSamplerState(4, igl::BindTarget::kFragment, sampler_);
      commands->draw(PrimitiveType::Triangle, shapeStart, numVertices);
      if (enableWireframe_) {
        commands->bindRenderPipelineState(renderPipelineState_MeshWireframe_);
        commands->bindBuffer(0, BindTarget::kVertex, vb0_, 0);
        commands->draw(PrimitiveType::Triangle, shapeStart, numVertices);

        // Bind the non-wireframe pipeline and the vertex buffer
        commands->bindRenderPipelineState(renderPipelineState_Mesh_);
        commands->bindBuffer(0, BindTarget::kVertex, vb0_, 0);
      }

      shapeStart += numVertices;
    }
#else
    commands->bindTexture(0, igl::BindTarget::kFragment, fbShadowMap_->getDepthAttachment());
    commands->bindTexture(1, igl::BindTarget::kFragment, skyboxTextureIrradiance_);
    commands->bindSamplerState(0, igl::BindTarget::kFragment, sampler_);
    commands->bindSamplerState(1, igl::BindTarget::kFragment, samplerShadow_);
    commands->drawIndexed(
        PrimitiveType::Triangle, indexData_.size(), igl::IndexFormat::UInt32, *ib0_.get(), 0);
    if (enableWireframe_) {
      commands->bindRenderPipelineState(renderPipelineState_MeshWireframe_);
      commands->drawIndexed(
          PrimitiveType::Triangle, indexData_.size(), igl::IndexFormat::UInt32, *ib0_.get(), 0);
    }
#endif
    commands->popDebugGroupLabel();

    // Skybox
    commands->bindRenderPipelineState(renderPipelineState_Skybox_);
#if USE_OPENGL_BACKEND
    commands->bindTexture(1, igl::BindTarget::kFragment, skyboxTextureReference_);
    commands->bindSamplerState(1, igl::BindTarget::kFragment, sampler_);
#else
    commands->bindTexture(0, igl::BindTarget::kFragment, skyboxTextureReference_);
#endif
    commands->pushDebugGroupLabel("Render Skybox", igl::Color(0, 1, 0));
    commands->bindDepthStencilState(depthStencilStateLEqual_);
    commands->draw(PrimitiveType::Triangle, 0, 3 * 6 * 2);
    commands->popDebugGroupLabel();
    commands->endEncoding();

#if !USE_OPENGL_BACKEND
    buffer->present(fbOffscreen_->getColorAttachment(0));
#endif
    commandQueue_->submit(*buffer);
#if USE_OPENGL_BACKEND
    if (kNumSamplesMSAA == 1) {
      fbOffscreen_->getColorAttachment(0)->generateMipmap(*commandQueue_.get());
    }
#endif
  }

  // Pass 3: compute shader post-processing
  if (enableComputePass_) {
    std::shared_ptr<ICommandBuffer> buffer =
        commandQueue_->createCommandBuffer(CommandBufferDesc{"computeBuffer"}, nullptr);

    auto commands = buffer->createComputeCommandEncoder();
    commands->bindComputePipelineState(computePipelineState_Grayscale_);
    commands->bindTexture(0,
                          kNumSamplesMSAA > 1 ? fbOffscreen_->getResolveColorAttachment(0).get()
                                              : fbOffscreen_->getColorAttachment(0).get());
    commands->dispatchThreadGroups(igl::Dimensions(width_, height_, 1), igl::Dimensions());
    commands->endEncoding();

    commandQueue_->submit(*buffer);
  }

  // Pass 4: render into the swapchain image
  {
    std::shared_ptr<ICommandBuffer> buffer =
        commandQueue_->createCommandBuffer(CommandBufferDesc(), nullptr);

    // This will clear the framebuffer
    auto commands = buffer->createRenderCommandEncoder(renderPassMain_, fbMain_);
    commands->bindRenderPipelineState(renderPipelineState_Fullscreen_);
    commands->pushDebugGroupLabel("Swapchain Output", igl::Color(1, 0, 0));
    commands->bindTexture(0,
                          igl::BindTarget::kFragment,
                          kNumSamplesMSAA > 1 ? fbOffscreen_->getResolveColorAttachment(0)
                                              : fbOffscreen_->getColorAttachment(0));
#if USE_OPENGL_BACKEND
    commands->bindSamplerState(0, igl::BindTarget::kFragment, sampler_);
#endif
    commands->draw(PrimitiveType::Triangle, 0, 3);
    commands->popDebugGroupLabel();

#if IGL_WITH_IGLU
    imguiSession_->endFrame(*device_.get(), *commands);
#endif // IGL_WITH_IGLU

    commands->endEncoding();

    buffer->present(fbMain_->getColorAttachment(0));

    commandQueue_->submit(*buffer);
  }

#if !USE_OPENGL_BACKEND
  fbMain_->getDepthAttachment()->generateMipmap(*commandQueue_.get());
#endif
}

void generateCompressedTexture(LoadedImage img) {
  if (loaderShouldExit_.load(std::memory_order_acquire)) {
    return;
  }

  printf("...compressing texture to %s\n", img.compressedFileName.c_str());

  const auto mipmapLevelCount = TextureDesc::calcNumMipLevels(img.w, img.h);

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

igl::TextureFormat gli2iglTextureFormat(gli::texture2d::format_type format) {
  switch (format) {
  case gli::FORMAT_RGBA32_SFLOAT_PACK32:
    return igl::TextureFormat::RGBA_F32;
  case gli::FORMAT_RG16_SFLOAT_PACK16:
    return igl::TextureFormat::RG_F16;
  }

  IGL_ASSERT_NOT_REACHED();
  return igl::TextureFormat::RGBA_UNorm8;
}

LoadedImage loadImage(const char* fileName, int channels) {
  if (!fileName || !*fileName) {
    return LoadedImage();
  }

  const std::string debugName = IGL_FORMAT("{} ({})", fileName, channels).c_str();

  {
    std::lock_guard lock(imagesCacheMutex_);

    const auto it = imagesCache_.find(debugName);

    if (it != imagesCache_.end()) {
      IGL_ASSERT(channels == it->second.channels);
      return it->second;
    }
  }

  LoadedImage img;
  img.compressedFileName = convertFileName(fileName);
  img.pixels = stbi_load(fileName, &img.w, &img.h, nullptr, channels);
  img.channels = channels;
  img.debugName = debugName;

  if (img.pixels && kEnableCompression && (channels != 1) &&
      !std::filesystem::exists(img.compressedFileName.c_str())) {
    generateCompressedTexture(img);
  }

  std::lock_guard lock(imagesCacheMutex_);

  imagesCache_[fileName] = img;

  return img;
}

void loadMaterial(size_t i) {
  static const std::string pathPrefix = contentRootFolder + "src/bistro/Exterior/";

  IGL_SCOPE_EXIT {
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

void loadCubemapTexture(const std::string& fileNameKTX, std::shared_ptr<ITexture>& tex) {
  auto texRef = gli::load_ktx(fileNameKTX);

  if (!IGL_VERIFY(texRef.format() == gli::FORMAT_RGBA32_SFLOAT_PACK32)) {
    IGL_ASSERT_MSG(false, "Texture format not supported");
    return;
  }

  auto texRefRange =
      TextureRangeDesc::new2D(0, 0, (size_t)texRef.extent().x, (size_t)texRef.extent().y);

  // If compression is enabled, upload all mip levels
  if (kEnableCompression) {
    texRefRange.numMipLevels = TextureDesc::calcNumMipLevels(texRefRange.width, texRefRange.height);
  }

  for (uint8_t face = 0; face < 6; ++face) {
    if (!tex) {
      auto desc = TextureDesc::newCube(gli2iglTextureFormat(texRef.format()),
                                       texRef.extent().x,
                                       texRef.extent().y,
                                       TextureDesc::TextureUsageBits::Sampled,
                                       fileNameKTX.c_str());
      desc.numMipLevels = TextureDesc::calcNumMipLevels(texRef.extent().x, texRef.extent().y);
      tex = device_->createTexture(desc, nullptr);
      IGL_ASSERT(tex);
    }

    tex->uploadCube(texRefRange, (igl::TextureCubeFace)face, texRef.data(0, face, 0));
  }

  if (!kEnableCompression) {
    tex->generateMipmap(*commandQueue_);
  }
}

gli::texture_cube gliToCube(Bitmap& bmp) {
  IGL_ASSERT(bmp.comp_ == 3); // RGB
  IGL_ASSERT(bmp.type_ == eBitmapType_Cube);
  IGL_ASSERT(bmp.fmt_ == eBitmapFormat_Float);

  const int w = bmp.w_;
  const int h = bmp.h_;

  const gli::texture_cube::extent_type extents{w, h};

  const auto miplevels = igl::TextureDesc::calcNumMipLevels(w, h);

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
  IGL_LOG_INFO("Generating mipmaps");

  auto prevWidth = cubemap.extent().x;
  auto prevHeight = cubemap.extent().y;
  for (size_t face = 0; face < 6; ++face) {
    IGL_LOG_INFO(".");
    for (size_t miplevel = 1; miplevel <= cubemap.max_level(); ++miplevel) {
      IGL_LOG_INFO(":");
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

  IGL_LOG_INFO("\n");
  gli::save_ktx(cubemap, outFilename);
}

void processCubemap(const std::string& inFilename,
                    const std::string& outFilenameEnv,
                    const std::string& outFilenameIrr) {
  int sourceWidth, sourceHeight;
  float* pxs = stbi_loadf(inFilename.c_str(), &sourceWidth, &sourceHeight, nullptr, 3);
  IGL_SCOPE_EXIT {
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
  static const std::string skyboxFileName{"immenstadter_horn_2k"};
  static const std::string skyboxSubdir{"src/skybox_hdr/"};

  static const std::string fileNameRefKTX =
      contentRootFolder + skyboxFileName + "_ReferenceMap.ktx";
  static const std::string fileNameIrrKTX =
      contentRootFolder + skyboxFileName + "_IrradianceMap.ktx";

  if (!std::filesystem::exists(fileNameRefKTX) || !std::filesystem::exists(fileNameIrrKTX)) {
    IGL_LOG_INFO("Cubemap in KTX format not found. Extracting from HDR file...\n");
    static const std::string inFilename =
        contentRootFolder + skyboxSubdir + skyboxFileName + ".hdr";

    processCubemap(inFilename, fileNameRefKTX, fileNameIrrKTX);
  }

  loadCubemapTexture(fileNameRefKTX, skyboxTextureReference_);
  loadCubemapTexture(fileNameIrrKTX, skyboxTextureIrradiance_);
}

std::shared_ptr<ITexture> createTexture(const LoadedImage& img) {
  if (!img.pixels) {
    return nullptr;
  }

  const auto it = texturesCache_.find(img.debugName);

  if (it != texturesCache_.end()) {
    return it->second;
  }

  igl::TextureFormat fmt = igl::TextureFormat::Invalid;
  if (img.channels == 1) {
    fmt = igl::TextureFormat::R_UNorm8;
  } else if (img.channels == 4) {
    fmt = kEnableCompression ? igl::TextureFormat::RGBA_BC7_UNORM_4x4
                             : igl::TextureFormat::RGBA_UNorm8;
  }

  TextureDesc desc = TextureDesc::new2D(
      fmt, img.w, img.h, TextureDesc::TextureUsageBits::Sampled, img.debugName.c_str());
  desc.numMipLevels = TextureDesc::calcNumMipLevels(img.w, img.h);
  auto tex = device_->createTexture(desc, nullptr);

  if (kEnableCompression && img.channels == 4 &&
      std::filesystem::exists(img.compressedFileName.c_str())) {
    // Uploading the texture
    auto rangeDesc = TextureRangeDesc::new2D(0, 0, img.w, img.h);
    rangeDesc.numMipLevels = desc.numMipLevels;
    auto gliTex2d = gli::load_ktx(img.compressedFileName.c_str());
    if (IGL_UNEXPECTED(gliTex2d.empty())) {
      printf("Failed to load %s\n", img.compressedFileName.c_str());
    }
    tex->upload(rangeDesc, gliTex2d.data());
  } else {
    tex->upload(TextureRangeDesc::new2D(0, 0, img.w, img.h), img.pixels);
    tex->generateMipmap(*commandQueue_.get());
  }
  texturesCache_[img.debugName] = tex;
  return tex;
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

  MaterialTextures tex;

  tex.ambient = createTexture(mtl.ambient);
  tex.diffuse = createTexture(mtl.diffuse);
  tex.alpha = createTexture(mtl.alpha);

  // update GPU materials
  textures_[mtl.idx] = tex;
#if !USE_OPENGL_BACKEND
  materials_[mtl.idx].texAmbient = tex.ambient ? (uint32_t)tex.ambient->getTextureId() : 0;
  materials_[mtl.idx].texDiffuse = tex.diffuse ? (uint32_t)tex.diffuse->getTextureId() : 0;
  materials_[mtl.idx].texAlpha = tex.alpha ? (uint32_t)tex.alpha->getTextureId() : 0;
  IGL_ASSERT(materials_[mtl.idx].texAmbient >= 0 && materials_[mtl.idx].texAmbient < kMaxTextures);
  IGL_ASSERT(materials_[mtl.idx].texDiffuse >= 0 && materials_[mtl.idx].texDiffuse < kMaxTextures);
  IGL_ASSERT(materials_[mtl.idx].texAlpha >= 0 && materials_[mtl.idx].texAlpha < kMaxTextures);
#endif
  sbMaterials_->upload(materials_.data(), BufferRange(sizeof(GPUMaterial) * materials_.size()));
}

int main(int argc, char* argv[]) {
  // find the content folder
  {
    using namespace std::filesystem;
    path subdir("third-party/content/");
    // @fb-only
    path dir = current_path();
    // find the content somewhere above our current build directory
    while (dir != current_path().root_path() && !exists(dir / subdir)) {
      dir = dir.parent_path();
    }
    if (!exists(dir / subdir)) {
      printf("Cannot find the content directory. Run `deploy_content.py` before running this app.");
      IGL_ASSERT_NOT_REACHED();
      return EXIT_FAILURE;
    }
    contentRootFolder = (dir / subdir).string();
  }

  initWindow(&window_);
  initIGL();
  initModel();

  if (kEnableCompression) {
    printf(
        "Compressing textures... It can take a while in debug builds...(needs to be done once)\n");
  }

  loadSkyboxTexture();
  loadMaterials();

  createFramebuffer(getNativeDrawable());
  createShadowMap();
  createOffscreenFramebuffer();
  createRenderPipelines();
  createRenderPipelineSkybox();
  createComputePipeline();

#if IGL_WITH_IGLU
  imguiSession_ = std::make_unique<iglu::imgui::Session>(*device_.get(), inputDispatcher_);
#endif // IGL_WITH_IGLU

  double prevTime = glfwGetTime();

  uint32_t frameIndex = 0;

  // Main loop
  while (!glfwWindowShouldClose(window_)) {
    {
      FramebufferDesc framebufferDesc;
      framebufferDesc.colorAttachments[0].texture = getNativeDrawable();
      framebufferDesc.depthAttachment.texture = getNativeDepthDrawable();
#if IGL_WITH_IGLU
      imguiSession_->beginFrame(framebufferDesc, 1.0f);
      ImGui::ShowDemoWindow();

      ImGui::Begin("Keyboard hints:", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::Text("W/S/A/D - camera movement");
      ImGui::Text("1/2 - camera up/down");
      ImGui::Text("Shift - fast movement");
      ImGui::Text("C - toggle compute shader postprocessing");
      ImGui::Text("N - toggle normals");
      ImGui::Text("T - toggle wireframe");
      ImGui::End();

      if (textures_[1].diffuse) {
        ImGui::Begin("Texture Viewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Image(ImTextureID(textures_[1].diffuse.get()), ImVec2(256, 256));
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
          ImGui::Text("FPS : %i", (int)fps_.getAverageFPS());
          ImGui::Text("Ms  : %.1f", 1000.0 / fps_.getAverageFPS());
        }
        ImGui::End();
      }

#endif // IGL_WITH_IGLU
    }

    processLoadedMaterials();
    const double newTime = glfwGetTime();
    const double delta = newTime - prevTime;
    fps_.updateFPS(delta);
    positioner_.update(delta, mousePos_, mousePressed_);
    prevTime = newTime;
#if IGL_WITH_IGLU
    inputDispatcher_.processEvents();
#endif // IGL_WITH_IGLU
    render(getNativeDrawable(), frameIndex);
    glfwPollEvents();
    frameIndex = (frameIndex + 1) % kNumBufferedFrames;
  }

  loaderShouldExit_.store(true, std::memory_order_release);

#if IGL_WITH_IGLU
  imguiSession_ = nullptr;
#endif // IGL_WITH_IGLU
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
#if USE_OPENGL_BACKEND
  textureDummyBlack_ = nullptr;
#endif // USE_OPENGL_BACKEND
  skyboxTextureReference_ = nullptr;
  skyboxTextureIrradiance_ = nullptr;
  textures_.clear();
  texturesCache_.clear();
  sampler_ = nullptr;
  samplerShadow_ = nullptr;
  fbMain_ = nullptr;
  fbShadowMap_ = nullptr;
  fbOffscreen_ = nullptr;
  device_.reset(nullptr);

  glfwDestroyWindow(window_);
  glfwTerminate();

  printf("Waiting for the loader thread to exit...\n");

  loaderPool_ = nullptr;

  return 0;
}
