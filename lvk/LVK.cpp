/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "LVK.h"

#include <assert.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#if LVK_WITH_GLFW
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// clang-format off
#ifdef _WIN32
#  define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#elif __APPLE__
#  define GLFW_EXPOSE_NATIVE_COCOA
#else
#  error Unsupported OS
#endif
// clang-format on

#include <GLFW/glfw3native.h>
#endif // LVK_WITH_GLFW

#include <lvk/vulkan/VulkanClasses.h>

namespace {

struct TextureFormatProperties {
  const lvk::Format format = lvk::Format_Invalid;
  const uint8_t bytesPerBlock : 5 = 1;
  const uint8_t blockWidth : 4 = 1;
  const uint8_t blockHeight : 4 = 1;
  const uint8_t minBlocksX : 2 = 1;
  const uint8_t minBlocksY : 2 = 1;
  const bool depth : 1 = false;
  const bool stencil : 1 = false;
  const bool compressed : 1 = false;
};

#define PROPS(fmt, bpb, ...) \
  TextureFormatProperties { .format = lvk::Format_##fmt, .bytesPerBlock = bpb, ##__VA_ARGS__ }

static constexpr TextureFormatProperties properties[] = {
    PROPS(Invalid, 1),
    PROPS(R_UN8, 1),
    PROPS(R_UI16, 2),
    PROPS(R_UN16, 2),
    PROPS(R_F16, 2),
    PROPS(R_F32, 4),
    PROPS(RG_UN8, 2),
    PROPS(RG_UI16, 4),
    PROPS(RG_UN16, 4),
    PROPS(RG_F16, 4),
    PROPS(RG_F32, 8),
    PROPS(RGBA_UN8, 4),
    PROPS(RGBA_UI32, 16),
    PROPS(RGBA_F16, 8),
    PROPS(RGBA_F32, 16),
    PROPS(RGBA_SRGB8, 4),
    PROPS(BGRA_UN8, 4),
    PROPS(BGRA_SRGB8, 4),
    PROPS(ETC2_RGB8, 8, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(ETC2_SRGB8, 8, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(BC7_RGBA, 16, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(Z_UN16, 2, .depth = true),
    PROPS(Z_UN24, 3, .depth = true),
    PROPS(Z_F32, 4, .depth = true),
    PROPS(Z_UN24_S_UI8, 4, .depth = true, .stencil = true),
    PROPS(Z_F32_S_UI8, 5, .depth = true, .stencil = true),
};

} // namespace

#if __APPLE__ && LVK_WITH_GLFW
void* createCocoaWindowView(GLFWwindow* window);
#endif

static_assert(sizeof(TextureFormatProperties) <= sizeof(uint32_t));
static_assert(LVK_ARRAY_NUM_ELEMENTS(properties) == lvk::Format_Z_F32_S_UI8 + 1);

bool lvk::isDepthOrStencilFormat(lvk::Format format) {
  return properties[format].depth || properties[format].stencil;
}

uint32_t lvk::getTextureBytesPerLayer(uint32_t width,
                                      uint32_t height,
                                      lvk::Format format,
                                      uint32_t level) {
  const uint32_t levelWidth = std::max(width >> level, 1u);
  const uint32_t levelHeight = std::max(height >> level, 1u);

  const auto props = properties[format];

  if (!props.compressed) {
    return props.bytesPerBlock * levelWidth * levelHeight;
  }

  const uint32_t blockWidth = std::max((uint32_t)props.blockWidth, 1u);
  const uint32_t blockHeight = std::max((uint32_t)props.blockHeight, 1u);
  const uint32_t widthInBlocks = (levelWidth + props.blockWidth - 1) / props.blockWidth;
  const uint32_t heightInBlocks = (levelHeight + props.blockHeight - 1) / props.blockHeight;
  return widthInBlocks * heightInBlocks * props.bytesPerBlock;
}

uint32_t lvk::calcNumMipLevels(uint32_t width, uint32_t height) {
  assert(width > 0);
  assert(height > 0);

  uint32_t levels = 1;

  while ((width | height) >> levels) {
    levels++;
  }

  return levels;
}

bool lvk::Assert(bool cond, const char* file, int line, const char* format, ...) {
  if (!cond) {
    va_list ap;
    va_start(ap, format);
    LLOGW("[LVK] Assertion failed in %s:%d: ", file, line);
    LLOGW(format, ap);
    LLOGW("\n");
    va_end(ap);
    assert(false);
  }
  return cond;
}

void lvk::destroy(lvk::IContext* ctx, lvk::ComputePipelineHandle handle) {
  if (ctx) {
    ctx->destroy(handle);
  }
}

void lvk::destroy(lvk::IContext* ctx, lvk::RenderPipelineHandle handle) {
  if (ctx) {
    ctx->destroy(handle);
  }
}

void lvk::destroy(lvk::IContext* ctx, lvk::ShaderModuleHandle handle) {
  if (ctx) {
    ctx->destroy(handle);
  }
}

void lvk::destroy(lvk::IContext* ctx, lvk::SamplerHandle handle) {
  if (ctx) {
    ctx->destroy(handle);
  }
}

void lvk::destroy(lvk::IContext* ctx, lvk::BufferHandle handle) {
  if (ctx) {
    ctx->destroy(handle);
  }
}

void lvk::destroy(lvk::IContext* ctx, lvk::TextureHandle handle) {
  if (ctx) {
    ctx->destroy(handle);
  }
}

// Logs GLSL shaders with line numbers annotation
void lvk::logShaderSource(const char* text) {
  uint32_t line = 1;

  LLOGL("\n(%3u) ", line);

  while (text && *text) {
    if (*text == '\n') {
      LLOGL("\n(%3u) ", ++line);
    } else if (*text == '\r') {
      // skip it to support Windows/UNIX EOLs
    } else {
      LLOGL("%c", *text);
    }
    text++;
  }
  LLOGL("\n");
}

#if LVK_WITH_GLFW
GLFWwindow* lvk::initWindow(const char* windowTitle, int& outWidth, int& outHeight, bool resizable) {
  if (!glfwInit()) {
    return nullptr;
  }

  const bool wantsWholeArea = outWidth <= 0 || outHeight <= 0;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, wantsWholeArea || !resizable ? GLFW_FALSE : GLFW_TRUE);

  // render full screen without overlapping taskbar
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();

  int x = 0;
  int y = 0;
  int w = outWidth;
  int h = outHeight;

  if (wantsWholeArea) {
    int areaW = 0;
    int areaH = 0;
    glfwGetMonitorWorkarea(monitor, &x, &y, &areaW, &areaH);
    auto getPercent = [](int value, int percent) {
      assert(percent > 0 && percent <= 100);
      return static_cast<int>(static_cast<float>(value) * static_cast<float>(percent) / 100.0f);
    };
    if (outWidth < 0) {
      w = getPercent(areaW, -outWidth);
      x = (areaW - w) / 2;
    } else {
      w = areaW;
    }
    if (outHeight < 0) {
      h = getPercent(areaH, -outHeight);
      y = (areaH - h) / 2;
    } else {
      h = areaH;
    }
  }

  GLFWwindow* window = glfwCreateWindow(w, h, windowTitle, nullptr, nullptr);

  if (!window) {
    glfwTerminate();
    return nullptr;
  }

  if (wantsWholeArea) {
    glfwSetWindowPos(window, x, y);
  }

  glfwGetWindowSize(window, &outWidth, &outHeight);

  glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  });

  glfwSetErrorCallback([](int error, const char* description) { printf("GLFW Error (%i): %s\n", error, description); });

  return window;
}

std::unique_ptr<lvk::IContext> lvk::createVulkanContextWithSwapchain(GLFWwindow* window,
                                                                     uint32_t width,
                                                                     uint32_t height,
                                                                     const lvk::ContextConfig& cfg,
                                                                     lvk::HWDeviceType preferredDeviceType) {
  using namespace lvk;

  std::unique_ptr<VulkanContext> ctx;

#if defined(_WIN32)
  ctx = std::make_unique<VulkanContext>(cfg, (void*)glfwGetWin32Window(window));
#elif defined(__linux__)
  ctx = std::make_unique<VulkanContext>(cfg, (void*)glfwGetX11Window(window), (void*)glfwGetX11Display());
#elif defined(__APPLE__)
  ctx = std::make_unique<VulkanContext>(cfg, createCocoaWindowView(window));
#else
#error Unsupported OS
#endif

  HWDeviceDesc device;
  uint32_t numDevices = ctx->queryDevices(preferredDeviceType, &device, 1);

  if (!numDevices) {
    if (preferredDeviceType == HWDeviceType_Discrete) {
      numDevices = ctx->queryDevices(HWDeviceType_Integrated, &device);
    }
    else if (preferredDeviceType == HWDeviceType_Integrated) {
      numDevices = ctx->queryDevices(HWDeviceType_Discrete, &device);
    }
  }

  if (!numDevices) {
    LVK_ASSERT_MSG(false, "GPU is not found");
    return nullptr;
  }

  Result res = ctx->initContext(device);

  if (!res.isOk()) {
    LVK_ASSERT_MSG(false, "Failed initContext()");
    return nullptr;
  }

  if (width > 0 && height > 0) {
    res = ctx->initSwapchain(width, height);
    if (!res.isOk()) {
      LVK_ASSERT_MSG(false, "Failed to create swapchain");
      return nullptr;
    }
  }

  return std::move(ctx);
}
#endif // LVK_WITH_GLFW
