/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Surface.h>

#include <optional>
#include <utility>
#include <igl/webgpu/Device.h>
#include <igl/webgpu/Texture.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

constexpr WGPUTextureUsage kSurfaceUsage =
    WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;

Handle<WGPUTexture> acquireSurfaceTexture(WGPUSurface surface) {
  WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
  wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
  Handle<WGPUTexture> texture(surfaceTexture.texture);
  if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
      surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
    IGL_LOG_ERROR("wgpuSurfaceGetCurrentTexture() failed (status %d)\n",
                  static_cast<int>(surfaceTexture.status));
    return {};
  }
  return texture;
}

} // namespace

Surface::Surface(Device& device, Handle<WGPUSurface> surface) :
  device_(device), surface_(std::move(surface)) {}

Surface::~Surface() {
  if (surface_ && format_ != TextureFormat::Invalid) {
    wgpuSurfaceUnconfigure(surface_.get());
  }
}

std::unique_ptr<Surface> Surface::create(Device& device,
                                         Handle<WGPUSurface> surface,
                                         Result* IGL_NULLABLE outResult) {
  if (!surface) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "WGPUSurface is null");
    return nullptr;
  }
  Result::setOk(outResult);
  return std::unique_ptr<Surface>(new Surface(device, std::move(surface)));
}

#if IGL_PLATFORM_APPLE && !IGL_PLATFORM_EMSCRIPTEN
std::unique_ptr<Surface> Surface::createFromMetalLayer(Device& device,
                                                       void* IGL_NONNULL layer,
                                                       Result* IGL_NULLABLE outResult) {
  WGPUSurfaceSourceMetalLayer metalLayer = WGPU_SURFACE_SOURCE_METAL_LAYER_INIT;
  metalLayer.layer = layer;
  WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
  desc.nextInChain = &metalLayer.chain;
  Handle<WGPUSurface> surface(wgpuInstanceCreateSurface(device.getContext().getInstance(), &desc));
  if (!surface) {
    Result::setResult(
        outResult, Result::Code::RuntimeError, "Cannot create a WebGPU surface for the layer");
    return nullptr;
  }
  return create(device, std::move(surface), outResult);
}
#endif

#if IGL_PLATFORM_EMSCRIPTEN
std::unique_ptr<Surface> Surface::createFromCanvas(Device& device,
                                                   const char* IGL_NONNULL selector,
                                                   Result* IGL_NULLABLE outResult) {
  WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas =
      WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
  canvas.selector = toWGPUStringView(selector);
  WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
  desc.nextInChain = &canvas.chain;
  Handle<WGPUSurface> surface(wgpuInstanceCreateSurface(device.getContext().getInstance(), &desc));
  if (!surface) {
    Result::setResult(
        outResult, Result::Code::ArgumentInvalid, "No WebGPU canvas matches the selector");
    return nullptr;
  }
  return create(device, std::move(surface), outResult);
}
#endif

Result Surface::configure(uint32_t width, uint32_t height, TextureFormat format) {
  if (width == 0 || height == 0) {
    return Result(Result::Code::ArgumentOutOfRange, "Surface size must not be zero");
  }
  const WebGPUContext& ctx = device_.getContext();
  std::optional<WGPUTextureFormat> wgpuFormat;
  if (format == TextureFormat::Invalid) {
    // Browsers report the preferred canvas format without an adapter.
    WGPUSurfaceCapabilities caps = WGPU_SURFACE_CAPABILITIES_INIT;
    if (wgpuSurfaceGetCapabilities(surface_.get(), ctx.getAdapter(), &caps) != WGPUStatus_Success ||
        caps.formatCount == 0) {
      return Result(Result::Code::Unsupported, "The surface's preferred format is unknown");
    }
    wgpuFormat = caps.formats[0];
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    format = wgpuTextureFormatToTextureFormat(*wgpuFormat);
  } else {
    wgpuFormat = textureFormatToWGPUTextureFormat(format);
  }
  if (!wgpuFormat || format == TextureFormat::Invalid) {
    return Result(Result::Code::Unsupported, "The surface format is not supported");
  }

  WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
  config.device = ctx.getDevice();
  config.format = *wgpuFormat;
  config.usage = kSurfaceUsage;
  config.width = width;
  config.height = height;
  config.presentMode = WGPUPresentMode_Fifo;
  config.alphaMode = WGPUCompositeAlphaMode_Opaque;
  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  wgpuSurfaceConfigure(surface_.get(), &config);
  Result result = ctx.popErrorScope();
  if (!result.isOk()) {
    return result;
  }
  format_ = format;
  width_ = width;
  height_ = height;
  return Result();
}

std::shared_ptr<ITexture> Surface::getCurrentTexture(Result* IGL_NULLABLE outResult) {
  if (format_ == TextureFormat::Invalid) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "The surface is not configured");
    return nullptr;
  }
  TextureDesc desc = TextureDesc::new2D(
      format_, width_, height_, TextureDesc::TextureUsageBits::Attachment, "igl.webgpu.surface");
  const Handle<WGPUSurface> surface = surface_;
  return Texture::createDeferred(
      device_.getContext(),
      device_.getDeviceFeatureSet(),
      desc,
      {
          .acquire = [surface] { return acquireSurfaceTexture(surface.get()); },
#if IGL_PLATFORM_EMSCRIPTEN
          .present = nullptr,
#else
          .present = [surface] { wgpuSurfacePresent(surface.get()); },
#endif
      },
      outResult);
}

} // namespace igl::webgpu
