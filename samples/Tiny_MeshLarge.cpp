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

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#else
#error Unsupported OS
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

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

std::string contentRootFolder;

#if IGL_WITH_IGLU
#include <IGLU/imgui/Session.h>

std::unique_ptr<iglu::imgui::Session> imguiSession_;

igl::shell::InputDispatcher inputDispatcher_;
#endif // IGL_WITH_IGLU

const char* kCodeComputeTest = R"(
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (set = 0, binding = 6, rgba8) uniform readonly  image2D kTextures2Din[];
layout (set = 0, binding = 6, rgba8) uniform writeonly image2D kTextures2Dout[];

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
  float size = 1.0 / textureBindlessSize2D(pc.perFrame.texShadow, pc.perFrame.samplerShadow0).x;
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

using namespace igl;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

GLFWwindow* window_ = nullptr;
int width_ = 0;
int height_ = 0;
FramesPerSecondCounter fps_;

constexpr uint32_t kNumBufferedFrames = 3;

std::unique_ptr<IDevice> device_;
igl::Framebuffer fbMain_; // swapchain
igl::Framebuffer fbOffscreen_;
igl::Framebuffer fbShadowMap_;
std::shared_ptr<IComputePipelineState> computePipelineState_Grayscale_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Mesh_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_MeshWireframe_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Shadow_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Skybox_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Fullscreen_;
std::shared_ptr<IBuffer> vb0_, ib0_; // buffers for vertices and indices
std::shared_ptr<IBuffer> sbMaterials_; // storage buffer for materials
std::vector<std::shared_ptr<IBuffer>> ubPerFrame_, ubPerFrameShadow_, ubPerObject_;
std::shared_ptr<ISamplerState> sampler_;
std::shared_ptr<ISamplerState> samplerShadow_;
std::shared_ptr<ITexture> textureDummyWhite_;
std::shared_ptr<ITexture> skyboxTextureReference_;
std::shared_ptr<ITexture> skyboxTextureIrradiance_;
igl::RenderPass renderPassOffscreen_;
igl::RenderPass renderPassMain_;
igl::RenderPass renderPassShadow_;
igl::DepthStencilState depthStencilState_;
igl::DepthStencilState depthStencilStateLEqual_;

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
  std::shared_ptr<ITexture> ambient;
  std::shared_ptr<ITexture> diffuse;
  std::shared_ptr<ITexture> alpha;
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

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  // render full screen without overlapping taskbar
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);

  int posX = 0;
  int posY = 0;
  int width = mode->width;
  int height = mode->height;

  glfwGetMonitorWorkarea(monitor, &posX, &posY, &width, &height);

  GLFWwindow* window = glfwCreateWindow(width, height, "Vulkan Mesh", nullptr, nullptr);

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
    const igl::vulkan::VulkanContextConfig cfg = {
        .maxTextures = kMaxTextures,
        .maxSamplers = 128,
        .terminateOnValidationError = true,
        .enableValidation = kEnableValidationLayers,
        .enableGPUAssistedValidation = true,
        .swapChainColorSpace = igl::ColorSpace::SRGB_LINEAR,
    };
#ifdef _WIN32
    auto ctx = vulkan::Device::createContext(cfg, (void*)glfwGetWin32Window(window_));
#elif defined(__linux__)
    auto ctx = vulkan::Device::createContext(
        cfg, (void*)glfwGetX11Window(window_), 0, nullptr, (void*)glfwGetX11Display());

#else
#error Unsupported OS
#endif
    const HWDeviceType hardwareType = kPreferIntegratedGPU ? HWDeviceType::IntegratedGpu
                                                           : HWDeviceType::DiscreteGpu;
    std::vector<HWDeviceDesc> devices =
        vulkan::Device::queryDevices(*ctx.get(), hardwareType, nullptr);
    if (devices.empty()) {
      const HWDeviceType hardwareType = !kPreferIntegratedGPU ? HWDeviceType::IntegratedGpu
                                                              : HWDeviceType::DiscreteGpu;
      devices = vulkan::Device::queryDevices(*ctx.get(), hardwareType, nullptr);
    }
    IGL_ASSERT_MSG(!devices.empty(), "GPU is not found");
    device_ =
        vulkan::Device::create(std::move(ctx), devices[0], (uint32_t)width_, (uint32_t)height_);
    IGL_ASSERT(device_.get());
  }

  {
    textureDummyWhite_ = device_->createTexture(
        {
            .type = igl::TextureType::TwoD,
            .format = igl::TextureFormat::RGBA_UNorm8,
            .width = 1,
            .height = 1,
            .usage = igl::TextureUsageBits_Sampled,
            .debugName = "dummy 1x1 (white)",
        },
        nullptr);
    const uint32_t pixel = 0xFFFFFFFF;
    const void* data[] = {&pixel};
    textureDummyWhite_->upload({.width = 1, .height = 1}, data);
  }

  // create an Uniform buffers to store uniforms for 2 objects
  for (uint32_t i = 0; i != kNumBufferedFrames; i++) {
    ubPerFrame_.push_back(device_->createBuffer({.usage = BufferUsageBits_Uniform,
                                                 .storage = StorageType_HostVisible,
                                                 .size = sizeof(UniformsPerFrame),
                                                 .debugName = "Buffer: uniforms (per frame)"},
                                                nullptr));
    ubPerFrameShadow_.push_back(
        device_->createBuffer({.usage = BufferUsageBits_Uniform,
                               .storage = StorageType_HostVisible,
                               .size = sizeof(UniformsPerFrame),
                               .debugName = "Buffer: uniforms (per frame shadow)"},
                              nullptr));
    ubPerObject_.push_back(device_->createBuffer({.usage = BufferUsageBits_Uniform,
                                                  .storage = StorageType_HostVisible,
                                                  .size = sizeof(UniformsPerObject),
                                                  .debugName = "Buffer: uniforms (per object)"},
                                                 nullptr));
  }

  depthStencilState_ = {.compareOp = igl::CompareOp_Less, .isDepthWriteEnabled = true};
  depthStencilStateLEqual_ = {.compareOp = igl::CompareOp_LessEqual, .isDepthWriteEnabled = true};

  sampler_ = device_->createSamplerState(
      {
          .mipMap = igl::SamplerMip_Linear,
          .wrapU = igl::SamplerWrap_Repeat,
          .wrapV = igl::SamplerWrap_Repeat,
          .debugName = "Sampler: linear",
      },
      nullptr);
  samplerShadow_ = device_->createSamplerState(
      {
          .wrapU = igl::SamplerWrap_Clamp,
          .wrapV = igl::SamplerWrap_Clamp,
          .depthCompareOp = igl::CompareOp_LessEqual,
          .depthCompareEnabled = true,
          .debugName = "Sampler: shadow",
      },
      nullptr);

  renderPassOffscreen_ = {
      .numColorAttachments = 1,
      .colorAttachments = {{
          .loadAction = LoadAction::Clear,
          .storeAction = kNumSamplesMSAA > 1 ? StoreAction::MsaaResolve : StoreAction::Store,
          .clearColor = {0.0f, 0.0f, 0.0f, 1.0f},
      }},
      .depthAttachment = {
          .loadAction = LoadAction::Clear,
          .storeAction = StoreAction::Store,
          .clearDepth = 1.0f,
      }};

  renderPassMain_ = {.numColorAttachments = 1,
                     .colorAttachments = {
                         {
                             .loadAction = LoadAction::Clear,
                             .storeAction = StoreAction::Store,
                             .clearColor = {0.0f, 0.0f, 0.0f, 1.0f},
                         },
                     }};
  renderPassShadow_ = {.depthAttachment = {
                           .loadAction = LoadAction::Clear,
                           .storeAction = StoreAction::Store,
                           .clearDepth = 1.0f,
                       }};
}

void normalizeName(std::string& name) {
#if defined(__linux__)
  std::replace(name.begin(), name.end(), '\\', '/');
#endif
}

bool loadAndCache(const char* cacheFileName) {
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
  const std::string cacheFileName = contentRootFolder + "cache.data";

  if (!loadFromCache(cacheFileName.c_str())) {
    if (!IGL_VERIFY(loadAndCache(cacheFileName.c_str()))) {
      IGL_ASSERT_MSG(false, "Cannot load 3D model");
    }
  }

  const uint32_t id = (uint32_t)textureDummyWhite_->getTextureId();

  for (const auto& mtl : cachedMaterials_) {
    materials_.push_back(GPUMaterial{vec4(mtl.ambient, 1.0f), vec4(mtl.diffuse, 1.0f), id, id});
  }
  sbMaterials_ = device_->createBuffer({.usage = BufferUsageBits_Storage,
                                        .storage = StorageType_Device,
                                        .data = materials_.data(),
                                        .size = sizeof(GPUMaterial) * materials_.size(),
                                        .debugName = "Buffer: materials"},
                                       nullptr);

  vb0_ = device_->createBuffer({.usage = BufferUsageBits_Vertex,
                                .storage = StorageType_Device,
                                .data = vertexData_.data(),
                                .size = sizeof(VertexData) * vertexData_.size(),
                                .debugName = "Buffer: vertex"},
                               nullptr);
  ib0_ = device_->createBuffer({.usage = BufferUsageBits_Index,
                                .storage = StorageType_Device,
                                .data = indexData_.data(),
                                .size = sizeof(uint32_t) * indexData_.size(),
                                .debugName = "Buffer: index"},
                               nullptr);
}

void createComputePipeline() {
  if (computePipelineState_Grayscale_) {
    return;
  }

  computePipelineState_Grayscale_ =
      device_->createComputePipeline({.computeShaderModule = device_->createShaderModule(
                                          {
                                              kCodeComputeTest,
                                              Stage_Compute,
                                              "Shader Module: grayscale (comp)",
                                          },
                                          nullptr)},
                                     nullptr);
}

void createRenderPipelines() {
  if (renderPipelineState_Mesh_) {
    return;
  }

  VertexInputState vdesc;
  vdesc.numAttributes = 4;
  vdesc.attributes[0].format = VertexAttributeFormat::Float3;
  vdesc.attributes[0].offset = offsetof(VertexData, position);
  vdesc.attributes[0].bufferIndex = 0;
  vdesc.attributes[0].location = 0;
  vdesc.attributes[1].format = VertexAttributeFormat::Int_2_10_10_10_REV;
  vdesc.attributes[1].offset = offsetof(VertexData, normal);
  vdesc.attributes[1].bufferIndex = 0;
  vdesc.attributes[1].location = 1;
  vdesc.attributes[2].format = VertexAttributeFormat::HalfFloat2;
  vdesc.attributes[2].offset = offsetof(VertexData, uv);
  vdesc.attributes[2].bufferIndex = 0;
  vdesc.attributes[2].location = 2;
  vdesc.attributes[3].format = VertexAttributeFormat::UInt1;
  vdesc.attributes[3].offset = offsetof(VertexData, mtlIndex);
  vdesc.attributes[3].bufferIndex = 0;
  vdesc.attributes[3].location = 3;
  vdesc.numInputBindings = 1;
  vdesc.inputBindings[0].stride = sizeof(VertexData);

  // shadow
  VertexInputState vdescs;
  vdescs.numAttributes = 1;
  vdescs.attributes[0].format = VertexAttributeFormat::Float3;
  vdescs.attributes[0].offset = offsetof(VertexData, position);
  vdescs.attributes[0].bufferIndex = 0;
  vdescs.attributes[0].location = 0;
  vdescs.numInputBindings = 1;
  vdescs.inputBindings[0].stride = sizeof(VertexData);

  {
    RenderPipelineDesc desc = {
        .vertexInputState = vdesc,
        .shaderStages = device_->createShaderStages(
            kCodeVS, "Shader Module: main (vert)", kCodeFS, "Shader Module: main (frag)"),
        .numColorAttachments = 1,
        .colorAttachments = {{.textureFormat =
                                  fbOffscreen_.colorAttachments[0].texture->getFormat()}},
        .cullMode = igl::CullMode_Back,
        .frontFaceWinding = igl::WindingMode_CCW,
        .sampleCount = kNumSamplesMSAA,
        .debugName = "Pipeline: mesh",
    };

    if (fbOffscreen_.depthStencilAttachment.texture) {
      desc.depthAttachmentFormat = fbOffscreen_.depthStencilAttachment.texture->getFormat();
    }

    renderPipelineState_Mesh_ = device_->createRenderPipeline(desc, nullptr);

    desc.polygonMode = igl::PolygonMode_Line;
    desc.vertexInputState = vdescs; // positions-only
    desc.shaderStages = device_->createShaderStages(kCodeVS_Wireframe,
                                                    "Shader Module: main wireframe (vert)",
                                                    kCodeFS_Wireframe,
                                                    "Shader Module: main wireframe (frag)");
    desc.debugName = "Pipeline: mesh (wireframe)";
    renderPipelineState_MeshWireframe_ = device_->createRenderPipeline(desc, nullptr);
  }

  // shadow
  renderPipelineState_Shadow_ = device_->createRenderPipeline(
      {
          .vertexInputState = vdescs,
          .shaderStages = device_->createShaderStages(
              kShadowVS, "Shader Module: shadow (vert)", kShadowFS, "Shader Module: shadow (frag)"),
          .depthAttachmentFormat = fbShadowMap_.depthStencilAttachment.texture->getFormat(),
          .cullMode = igl::CullMode_None,
          .debugName = "Pipeline: shadow",
      },
      nullptr);

  // fullscreen
  {
    RenderPipelineDesc desc;
    desc.numColorAttachments = 1;
    desc.colorAttachments[0] = {.textureFormat =
                                    fbMain_.colorAttachments[0].texture->getFormat()};
    if (fbMain_.depthStencilAttachment.texture) {
      desc.depthAttachmentFormat = fbMain_.depthStencilAttachment.texture->getFormat();
    }
    desc.shaderStages = device_->createShaderStages(kCodeFullscreenVS,
                                                    "Shader Module: fullscreen (vert)",
                                                    kCodeFullscreenFS,
                                                    "Shader Module: fullscreen (frag)");
    desc.cullMode = igl::CullMode_None;
    desc.debugName = "Pipeline: fullscreen";
    renderPipelineState_Fullscreen_ = device_->createRenderPipeline(desc, nullptr);
  }
}

void createRenderPipelineSkybox() {
  if (renderPipelineState_Skybox_) {
    return;
  }

  RenderPipelineDesc desc = {
      .shaderStages = device_->createShaderStages(
          kSkyboxVS, "Shader Module: skybox (vert)", kSkyboxFS, "Shader Module: skybox (frag)"),
      .numColorAttachments = 1,
      .colorAttachments = {{
          .textureFormat = fbOffscreen_.colorAttachments[0].texture->getFormat(),
      }},
      .cullMode = igl::CullMode_Front,
      .frontFaceWinding = igl::WindingMode_CCW,
      .sampleCount = kNumSamplesMSAA,
      .debugName = "Pipeline: skybox",
  };

  if (fbOffscreen_.depthStencilAttachment.texture) {
    desc.depthAttachmentFormat = fbOffscreen_.depthStencilAttachment.texture->getFormat();
  }

  renderPipelineState_Skybox_ = device_->createRenderPipeline(desc, nullptr);
}

void createShadowMap() {
  const uint32_t w = 4096;
  const uint32_t h = 4096;
  const TextureDesc desc = {
      .type = TextureType::TwoD,
      .format = igl::TextureFormat::Z_UNorm16,
      .width = w,
      .height = h,
      .usage = igl::TextureUsageBits_Attachment | igl::TextureUsageBits_Sampled,
      .numMipLevels = igl::calcNumMipLevels(w, h),
      .debugName = "Shadow map",
  };
  Result ret;
  fbShadowMap_ = {
      .depthStencilAttachment = {.texture = device_->createTexture(desc, &ret)},
  };
  IGL_ASSERT(ret.isOk());
}

void createOffscreenFramebuffer() {
  const uint32_t w = width_;
  const uint32_t h = height_;
  TextureDesc descDepth = {
      .type = TextureType::TwoD,
      .format = igl::TextureFormat::Z_UNorm24,
      .width = w,
      .height = h,
      .usage = igl::TextureUsageBits_Attachment | igl::TextureUsageBits_Sampled,
      .numMipLevels = igl::calcNumMipLevels(w, h),
      .debugName = "Offscreen framebuffer (d)",
  };
  if (kNumSamplesMSAA > 1) {
    descDepth.usage = TextureUsageBits_Attachment;
    descDepth.numSamples = kNumSamplesMSAA;
    descDepth.numMipLevels = 1;
  }
  Result ret;
  std::shared_ptr<ITexture> texDepth = device_->createTexture(descDepth, &ret);
  IGL_ASSERT(ret.isOk());

  const uint8_t usage =
      TextureUsageBits_Attachment | TextureUsageBits_Sampled | TextureUsageBits_Storage;
  const TextureFormat format = igl::TextureFormat::RGBA_UNorm8;

  TextureDesc descColor = {
      .type = TextureType::TwoD,
      .format = format,
      .width = w,
      .height = h,
      .usage = usage,
      .numMipLevels = igl::calcNumMipLevels(w, h),
      .debugName = "Offscreen framebuffer (c)",
  };
  if (kNumSamplesMSAA > 1) {
    descColor.usage = igl::TextureUsageBits_Attachment;
    descColor.numSamples = kNumSamplesMSAA;
    descColor.numMipLevels = 1;
  }
  std::shared_ptr<ITexture> texColor = device_->createTexture(descColor, &ret);
  IGL_ASSERT(ret.isOk());

  Framebuffer fb = {
      .numColorAttachments = 1,
      .colorAttachments = {{.texture = texColor}},
      .depthStencilAttachment = {.texture = texDepth},
  };

  if (kNumSamplesMSAA > 1) {
    fb.colorAttachments[0].resolveTexture = device_->createTexture(
        {
            .type = TextureType::TwoD,
            .format = format,
            .width = w,
            .height = h,
            .usage = usage,
            .debugName = "Offscreen framebuffer (c - resolve)",
        },
        &ret);
    IGL_ASSERT(ret.isOk());
  }

  fbOffscreen_ = fb;
}

void render(const std::shared_ptr<ITexture>& nativeDrawable, uint32_t frameIndex) {
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
      .texSkyboxRadiance = skyboxTextureReference_->getTextureId(),
      .texSkyboxIrradiance = skyboxTextureIrradiance_->getTextureId(),
      .texShadow = fbShadowMap_.depthStencilAttachment.texture->getTextureId(),
      .sampler = sampler_->getSamplerId(),
      .samplerShadow = samplerShadow_->getSamplerId(),
      .bDrawNormals = perFrame_.bDrawNormals,
      .bDebugLines = perFrame_.bDebugLines,
  };

  ubPerFrame_[frameIndex]->upload(&perFrame_, sizeof(perFrame_));

  {
    const UniformsPerFrame perFrameShadow{
        .proj = shadowProj,
        .view = shadowView,
    };
    ubPerFrameShadow_[frameIndex]->upload(&perFrameShadow, sizeof(perFrameShadow));
  }

  UniformsPerObject perObject;

  perObject.model = glm::scale(mat4(1.0f), vec3(0.05f));

  ubPerObject_[frameIndex]->upload(&perObject, sizeof(perObject));

  // Command buffers (1-N per thread): create, submit and forget

  // Pass 1: shadows
  if (isShadowMapDirty_) {
    std::shared_ptr<ICommandBuffer> buffer = device_->createCommandBuffer();

    buffer->cmdBeginRendering(renderPassShadow_, fbShadowMap_);
    {
      buffer->cmdBindRenderPipelineState(renderPipelineState_Shadow_);
      buffer->cmdPushDebugGroupLabel("Render Shadows", igl::Color(1, 0, 0));
      buffer->cmdBindDepthStencilState(depthStencilState_);
      buffer->cmdBindVertexBuffer(0, vb0_, 0);
      struct {
        uint64_t perFrame;
        uint64_t perObject;
      } bindings = {
          .perFrame = ubPerFrameShadow_[frameIndex]->gpuAddress(),
          .perObject = ubPerObject_[frameIndex]->gpuAddress(),
      };
      buffer->cmdPushConstants(0, &bindings, sizeof(bindings));

      buffer->cmdDrawIndexed(
          PrimitiveType::Triangle, indexData_.size(), igl::IndexFormat::UInt32, *ib0_.get(), 0);
      buffer->cmdPopDebugGroupLabel();
    }
    buffer->cmdEndRendering();
    buffer->present(fbShadowMap_.depthStencilAttachment.texture);
    device_->submit(*buffer, igl::CommandQueueType::Graphics);

    fbShadowMap_.depthStencilAttachment.texture->generateMipmap();

    isShadowMapDirty_ = false;
  }

  // Pass 2: mesh
  {
    std::shared_ptr<ICommandBuffer> buffer = device_->createCommandBuffer();

    // This will clear the framebuffer
    buffer->cmdBeginRendering(renderPassOffscreen_, fbOffscreen_);
    {
      // Scene
      buffer->cmdBindRenderPipelineState(renderPipelineState_Mesh_);
      buffer->cmdPushDebugGroupLabel("Render Mesh", igl::Color(1, 0, 0));
      buffer->cmdBindDepthStencilState(depthStencilState_);
      buffer->cmdBindVertexBuffer(0, vb0_, 0);

      struct {
        uint64_t perFrame;
        uint64_t perObject;
        uint64_t materials;
      } bindings = {
          .perFrame = ubPerFrame_[frameIndex]->gpuAddress(),
          .perObject = ubPerObject_[frameIndex]->gpuAddress(),
          .materials = sbMaterials_->gpuAddress(),
      };
      buffer->cmdPushConstants(0, &bindings, sizeof(bindings));
      buffer->cmdDrawIndexed(
          PrimitiveType::Triangle, indexData_.size(), igl::IndexFormat::UInt32, *ib0_.get(), 0);
      if (enableWireframe_) {
        buffer->cmdBindRenderPipelineState(renderPipelineState_MeshWireframe_);
        buffer->cmdDrawIndexed(
            PrimitiveType::Triangle, indexData_.size(), igl::IndexFormat::UInt32, *ib0_.get(), 0);
      }
      buffer->cmdPopDebugGroupLabel();

      // Skybox
      buffer->cmdBindRenderPipelineState(renderPipelineState_Skybox_);
      buffer->cmdPushDebugGroupLabel("Render Skybox", igl::Color(0, 1, 0));
      buffer->cmdBindDepthStencilState(depthStencilStateLEqual_);
      buffer->cmdDraw(PrimitiveType::Triangle, 0, 3 * 6 * 2);
      buffer->cmdPopDebugGroupLabel();
    }
    buffer->cmdEndRendering();
    buffer->present(fbOffscreen_.colorAttachments[0].texture);
    device_->submit(*buffer, CommandQueueType::Graphics);
  }

  // Pass 3: compute shader post-processing
  if (enableComputePass_) {
    std::shared_ptr<ITexture> tex = kNumSamplesMSAA > 1
                                        ? fbOffscreen_.colorAttachments[0].resolveTexture
                                        : fbOffscreen_.colorAttachments[0].texture;
    std::shared_ptr<ICommandBuffer> buffer = device_->createCommandBuffer();

    buffer->useComputeTexture(tex);
    buffer->cmdBindComputePipelineState(computePipelineState_Grayscale_);

    struct {
      uint32_t texture;
    } bindings = {
        .texture = tex->getTextureId(),
    };
    buffer->cmdPushConstants(0, &bindings, sizeof(bindings));
    buffer->cmdDispatchThreadGroups({
        .width = (uint32_t)width_,
        .height = (uint32_t)height_,
        .depth = 1u,
    });

    device_->submit(*buffer, CommandQueueType::Compute);
  }

  // Pass 4: render into the swapchain image
  {
    std::shared_ptr<ICommandBuffer> buffer = device_->createCommandBuffer();

    // This will clear the framebuffer
    buffer->cmdBeginRendering(renderPassMain_, fbMain_);
    {
      buffer->cmdBindRenderPipelineState(renderPipelineState_Fullscreen_);
      buffer->cmdPushDebugGroupLabel("Swapchain Output", igl::Color(1, 0, 0));
      struct {
        uint32_t texture;
      } bindings = {
          .texture = kNumSamplesMSAA > 1
                         ? fbOffscreen_.colorAttachments[0].resolveTexture->getTextureId()
                         : fbOffscreen_.colorAttachments[0].texture->getTextureId(),
      };
      buffer->cmdPushConstants(0, &bindings, sizeof(bindings));
      buffer->cmdDraw(PrimitiveType::Triangle, 0, 3);
      buffer->cmdPopDebugGroupLabel();

#if IGL_WITH_IGLU
      imguiSession_->endFrame(*device_.get(), *buffer);
#endif // IGL_WITH_IGLU
    }
    buffer->cmdEndRendering();

    buffer->present(fbMain_.colorAttachments[0].texture);

    device_->submit(*buffer, CommandQueueType::Graphics, true);
  }
}

void generateCompressedTexture(LoadedImage img) {
  if (loaderShouldExit_.load(std::memory_order_acquire)) {
    return;
  }

  printf("...compressing texture to %s\n", img.compressedFileName.c_str());

  const auto mipmapLevelCount = igl::calcNumMipLevels(img.w, img.h);

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
  IGL_ASSERT_MSG(false, "Code should NOT be reached");
  return igl::TextureFormat::RGBA_UNorm8;
}

LoadedImage loadImage(const char* fileName, int channels) {
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
  static const std::string pathPrefix = contentRootFolder + "src/bistro/Exterior/";

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

void loadCubemapTexture(const std::string& fileNameKTX, std::shared_ptr<ITexture>& tex) {
  auto texRef = gli::load_ktx(fileNameKTX);

  if (!IGL_VERIFY(texRef.format() == gli::FORMAT_RGBA32_SFLOAT_PACK32)) {
    IGL_ASSERT_MSG(false, "Texture format not supported");
    return;
  }

  const uint32_t width = (uint32_t)texRef.extent().x;
  const uint32_t height = (uint32_t)texRef.extent().y;

  if (!tex) {
    tex = device_->createTexture(
        {
            .type = TextureType::Cube,
            .format = gli2iglTextureFormat(texRef.format()),
            .width = width,
            .height = height,
            .usage = igl::TextureUsageBits_Sampled,
            .numMipLevels = igl::calcNumMipLevels(texRef.extent().x, texRef.extent().y),
            .debugName = fileNameKTX.c_str(),
        },
        nullptr);
    IGL_ASSERT(tex.get());
  }

  const void* data[6];

  for (uint8_t face = 0; face < 6; ++face) {
    data[face] = texRef.data(0, face, 0);
  }

  const TextureRangeDesc texRefRange = {
      .width = width,
      .height = height,
      .numLayers = 6,
      // if compression is enabled, upload all mip-levels
      .numMipLevels = kEnableCompression ? igl::calcNumMipLevels(width, height) : 1u,
  };
  tex->upload(texRefRange, data);

  if (!kEnableCompression) {
    tex->generateMipmap();
  }
}

gli::texture_cube gliToCube(Bitmap& bmp) {
  IGL_ASSERT(bmp.comp_ == 3); // RGB
  IGL_ASSERT(bmp.type_ == eBitmapType_Cube);
  IGL_ASSERT(bmp.fmt_ == eBitmapFormat_Float);

  const int w = bmp.w_;
  const int h = bmp.h_;

  const gli::texture_cube::extent_type extents{w, h};

  const auto miplevels = igl::calcNumMipLevels(w, h);

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
  static const std::string skyboxFileName{"immenstadter_horn_2k"};
  static const std::string skyboxSubdir{"src/skybox_hdr/"};

  static const std::string fileNameRefKTX =
      contentRootFolder + skyboxFileName + "_ReferenceMap.ktx";
  static const std::string fileNameIrrKTX =
      contentRootFolder + skyboxFileName + "_IrradianceMap.ktx";

  if (!std::filesystem::exists(fileNameRefKTX) || !std::filesystem::exists(fileNameIrrKTX)) {
    LLOGL("Cubemap in KTX format not found. Extracting from HDR file...\n");
    static const std::string inFilename =
        contentRootFolder + skyboxSubdir + skyboxFileName + ".hdr";

    processCubemap(inFilename, fileNameRefKTX, fileNameIrrKTX);
  }

  loadCubemapTexture(fileNameRefKTX, skyboxTextureReference_);
  loadCubemapTexture(fileNameIrrKTX, skyboxTextureIrradiance_);
}

igl::TextureFormat formatFromChannels(uint32_t channels) {
  if (channels == 1) {
    return igl::TextureFormat::R_UNorm8;
  }

  if (channels == 4) {
    return kEnableCompression ? igl::TextureFormat::RGBA_BC7_UNORM_4x4
                              : igl::TextureFormat::RGBA_UNorm8;
  }

  return igl::TextureFormat::Invalid;
}

std::shared_ptr<ITexture> createTexture(const LoadedImage& img) {
  if (!img.pixels) {
    return nullptr;
  }

  const auto it = texturesCache_.find(img.debugName);

  if (it != texturesCache_.end()) {
    return it->second;
  }

  const TextureDesc desc = {
      .type = TextureType::TwoD,
      .format = formatFromChannels(img.channels),
      .width = img.w,
      .height = img.h,
      .usage = igl::TextureUsageBits_Sampled,
      .numMipLevels = igl::calcNumMipLevels(img.w, img.h),
      .debugName = img.debugName.c_str(),
  };
  auto tex = device_->createTexture(desc, nullptr);

  if (kEnableCompression && img.channels == 4 &&
      std::filesystem::exists(img.compressedFileName.c_str())) {
    // Uploading the texture
    const TextureRangeDesc rangeDesc = {
        .width = img.w,
        .height = img.h,
        .numMipLevels = desc.numMipLevels,
    };
    auto gliTex2d = gli::load_ktx(img.compressedFileName.c_str());
    if (IGL_UNEXPECTED(gliTex2d.empty())) {
      printf("Failed to load %s\n", img.compressedFileName.c_str());
    }
    const void* data[] = {gliTex2d.data()};
    tex->upload(rangeDesc, data);
  } else {
    const void* data[] = {img.pixels};
    tex->upload({.width = img.w, .height = img.h}, data);
    tex->generateMipmap();
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
  materials_[mtl.idx].texAmbient = tex.ambient ? (uint32_t)tex.ambient->getTextureId() : 0;
  materials_[mtl.idx].texDiffuse = tex.diffuse ? (uint32_t)tex.diffuse->getTextureId() : 0;
  materials_[mtl.idx].texAlpha = tex.alpha ? (uint32_t)tex.alpha->getTextureId() : 0;
  IGL_ASSERT(materials_[mtl.idx].texAmbient >= 0 && materials_[mtl.idx].texAmbient < kMaxTextures);
  IGL_ASSERT(materials_[mtl.idx].texDiffuse >= 0 && materials_[mtl.idx].texDiffuse < kMaxTextures);
  IGL_ASSERT(materials_[mtl.idx].texAlpha >= 0 && materials_[mtl.idx].texAlpha < kMaxTextures);
  sbMaterials_->upload(materials_.data(), sizeof(GPUMaterial) * materials_.size());
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

  fbMain_ = {
      .numColorAttachments = 1,
      .colorAttachments = {{.texture = device_->getCurrentSwapchainTexture()}},
  };
  createShadowMap();
  createOffscreenFramebuffer();
  createRenderPipelines();
  createRenderPipelineSkybox();
  createComputePipeline();

#if IGL_WITH_IGLU
  imguiSession_ = std::make_unique<iglu::imgui::Session>(*device_.get(), inputDispatcher_);
#endif IGL_WITH_IGLU

  double prevTime = glfwGetTime();

  uint32_t frameIndex = 0;

  // Main loop
  while (!glfwWindowShouldClose(window_)) {
    {
#if IGL_WITH_IGLU
      imguiSession_->beginFrame(fbMain_);
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
          ImGui::Text("FPS : %i", (int)fps_.getFPS());
          ImGui::Text("Ms  : %.1f", 1000.0 / fps_.getFPS());
        }
        ImGui::End();
      }
#endif // IGL_WITH_IGLU
    }

    processLoadedMaterials();
    const double newTime = glfwGetTime();
    const double delta = newTime - prevTime;
    fps_.tick(delta);
    positioner_.update(delta, mousePos_, mousePressed_);
    prevTime = newTime;
#if IGL_WITH_IGLU
    inputDispatcher_.processEvents();
#endif // IGL_WITH_IGLU
    render(device_->getCurrentSwapchainTexture(), frameIndex);
    glfwPollEvents();
    frameIndex = (frameIndex + 1) % kNumBufferedFrames;
  }

  loaderShouldExit_.store(true, std::memory_order_release);

#if IGL_WITH_IGLU
  imguiSession_ = nullptr;
#endif IGL_WITH_IGLU
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
  fbMain_ = {};
  fbShadowMap_ = {};
  fbOffscreen_ = {};
  device_.reset(nullptr);

  glfwDestroyWindow(window_);
  glfwTerminate();

  printf("Waiting for the loader thread to exit...\n");

  loaderPool_ = nullptr;

  return 0;
}
