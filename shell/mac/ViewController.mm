/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import "ViewController.h"

#import "GLView.h"
#import "MetalView.h"
#import "VulkanView.h"
// @fb-only

#import <igl/Common.h>
#import <igl/IGL.h>
#if IGL_BACKEND_METAL
#import <igl/metal/HWDevice.h>
#import <igl/metal/Texture.h>
#import <igl/metal/macos/Device.h>
#endif
#if IGL_BACKEND_OPENGL
#import <igl/opengl/macos/Context.h>
#import <igl/opengl/macos/Device.h>
#import <igl/opengl/macos/HWDevice.h>
#endif
#include <shell/shared/platform/mac/PlatformMac.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/ShellParams.h>
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
#if IGL_BACKEND_VULKAN
#import <igl/vulkan/Device.h>
#import <igl/vulkan/HWDevice.h>
#import <igl/vulkan/VulkanContext.h>
#endif
#import <math.h>
#import <simd/simd.h>

using namespace igl;

@interface ViewController () {
  igl::BackendType backendType_;
  igl::shell::ShellParams shellParams_;
  CGRect frame_;
  CVDisplayLinkRef displayLink_; // For OpenGL only
  id<CAMetalDrawable> currentDrawable_;
  id<MTLTexture> depthStencilTexture_;
  std::shared_ptr<igl::shell::Platform> shellPlatform_;
  std::unique_ptr<igl::shell::RenderSession> session_;
  bool preferLatestVersion_;
  int majorVersion_;
  int minorVersion_;
  float kMouseSpeed_;
}
@end

@implementation ViewController

///--------------------------------------
/// MARK: - Init
///--------------------------------------

- (instancetype)initWithFrame:(CGRect)frame
                  backendType:(igl::BackendType)backendType
          preferLatestVersion:(bool)preferLatestVersion {
  self = [super initWithNibName:nil bundle:nil];
  if (!self)
    return self;

  backendType_ = backendType;
  shellParams_ = igl::shell::ShellParams();
  frame.size.width = shellParams_.viewportSize.x;
  frame.size.height = shellParams_.viewportSize.y;
  frame_ = frame;
  kMouseSpeed_ = 0.0005;
  currentDrawable_ = nil;
  depthStencilTexture_ = nil;
  preferLatestVersion_ = preferLatestVersion;

  return self;
}

- (instancetype)initWithFrame:(CGRect)frame
                  backendType:(igl::BackendType)backendType
                 majorVersion:(int)majorVersion
                 minorVersion:(int)minorVersion {
  majorVersion_ = majorVersion;
  minorVersion_ = minorVersion;
  return [self initWithFrame:frame backendType:backendType preferLatestVersion:false];
}

- (void)initModule {
}

- (void)teardown {
  session_ = nullptr;
  shellPlatform_ = nullptr;
}

- (void)render {
  shellParams_.viewportSize = glm::vec2(self.view.frame.size.width, self.view.frame.size.height);
  shellParams_.viewportScale = self.view.window.backingScaleFactor;
  session_->setShellParams(shellParams_);
  // process user input
  shellPlatform_->getInputDispatcher().processEvents();

  // surface textures
  igl::SurfaceTextures surfaceTextures = igl::SurfaceTextures{
      [self createTextureFromNativeDrawable], [self createTextureFromNativeDepth]};
  IGL_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);
  const auto& dims = surfaceTextures.color->getDimensions();
  shellParams_.nativeSurfaceDimensions = glm::ivec2{dims.width, dims.height};

  // update retina scale
  float pixelsPerPoint = shellParams_.nativeSurfaceDimensions.x / shellParams_.viewportSize.x;
  session_->setPixelsPerPoint(pixelsPerPoint);
// @fb-only
  // @fb-only
  IGL_ASSERT(fabs(shellParams_.nativeSurfaceDimensions.y / shellParams_.viewportSize.y -
                  pixelsPerPoint) < FLT_EPSILON);
// @fb-only
  // draw
  session_->update(std::move(surfaceTextures));
  if (session_->appParams().exitRequested)
    [[NSApplication sharedApplication] terminate:nil];
}

- (void)loadView {
  // We don't care about the device type, just
  // return something that works
  HWDeviceQueryDesc queryDesc(HWDeviceType::Unknown);

  switch (backendType_) {
#if IGL_BACKEND_METAL
  case igl::BackendType::Metal: {
    auto hwDevices = metal::HWDevice().queryDevices(queryDesc, nullptr);
    auto device = metal::HWDevice().create(hwDevices[0], nullptr);

    auto d = static_cast<igl::metal::macos::Device*>(device.get())->get();

    auto metalView = [[MetalView alloc] initWithFrame:frame_ device:d];
    metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    metalView.delegate = self;

    metalView.colorPixelFormat =
        metal::Texture::textureFormatToMTLPixelFormat(shellParams_.defaultColorFramebufferFormat);
    // !!!WARNING must be called after setting the colorPixelFormat WARNING!!!
    //
    // Disables OS Level Color Management to achieve parity with OpenGL
    // Without this, the OS will try to "color convert" the resulting framebuffer
    // to the monitor's color profile which is fine but doesn't seem to be
    // supported under OpenGL which results in discrepancies between Metal
    // and OpenGL. This feature is equivalent to using MoltenVK colorSpace
    // VK_COLOR_SPACE_PASS_THROUGH_EXT
    // Must be called after set colorPixelFormat since it resets the colorspace
    metalView.colorspace = nil;

    metalView.framebufferOnly = NO;
    [metalView setViewController:self];
    self.view = metalView;
    shellPlatform_ = std::make_shared<igl::shell::PlatformMac>(std::move(device));
    break;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendType::OpenGL: {
    NSOpenGLPixelFormat* pixelFormat;
    if (preferLatestVersion_) {
      static NSOpenGLPixelFormatAttribute attributes[] = {
          NSOpenGLPFADoubleBuffer,
          NSOpenGLPFAAllowOfflineRenderers,
          NSOpenGLPFAMultisample,
          1,
          NSOpenGLPFASampleBuffers,
          1,
          NSOpenGLPFASamples,
          4,
          NSOpenGLPFAColorSize,
          32,
          NSOpenGLPFADepthSize,
          24,
          NSOpenGLPFAOpenGLProfile,
          NSOpenGLProfileVersion4_1Core,
          0,
      };
      pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      IGL_ASSERT_MSG(pixelFormat, "Requested attributes not supported");
    } else {
      if (majorVersion_ >= 4 && minorVersion_ >= 1) {
        static NSOpenGLPixelFormatAttribute attributes[] = {
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAllowOfflineRenderers,
            NSOpenGLPFAMultisample,
            1,
            NSOpenGLPFASampleBuffers,
            1,
            NSOpenGLPFASamples,
            4,
            NSOpenGLPFAColorSize,
            32,
            NSOpenGLPFADepthSize,
            24,
            NSOpenGLPFAOpenGLProfile,
            NSOpenGLProfileVersion4_1Core,
            0,
        };
        pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      } else if (majorVersion_ >= 3 && minorVersion_ >= 2) {
        static NSOpenGLPixelFormatAttribute attributes[] = {
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAllowOfflineRenderers,
            NSOpenGLPFAMultisample,
            1,
            NSOpenGLPFASampleBuffers,
            1,
            NSOpenGLPFASamples,
            4,
            NSOpenGLPFAColorSize,
            32,
            NSOpenGLPFADepthSize,
            24,
            NSOpenGLPFAOpenGLProfile,
            NSOpenGLProfileVersion3_2Core,
            0,
        };
        pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      } else {
        static NSOpenGLPixelFormatAttribute attributes[] = {
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAllowOfflineRenderers,
            NSOpenGLPFAMultisample,
            1,
            NSOpenGLPFASampleBuffers,
            1,
            NSOpenGLPFASamples,
            4,
            NSOpenGLPFAColorSize,
            32,
            NSOpenGLPFADepthSize,
            24,
            NSOpenGLPFAOpenGLProfile,
            NSOpenGLProfileVersionLegacy,
            0,
        };
        pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      }
    }
    auto openGLView = [[GLView alloc] initWithFrame:frame_ pixelFormat:pixelFormat];
    igl::Result result;
    auto context = igl::opengl::macos::Context::createContext(openGLView.openGLContext, &result);
    IGL_ASSERT(result.isOk());
    shellPlatform_ = std::make_shared<igl::shell::PlatformMac>(
        opengl::macos::HWDevice().createWithContext(std::move(context), nullptr));
    self.view = openGLView;
    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendType::Vulkan: {
    auto vulkanView = [[VulkanView alloc] initWithFrame:frame_];

    self.view = vulkanView;

    // vulkanView.wantsLayer =
    // YES; // Back the view with a layer created by the makeBackingLayer method.

    const auto properties =
        igl::TextureFormatProperties::fromTextureFormat(shellParams_.defaultColorFramebufferFormat);

    igl::vulkan::VulkanContextConfig vulkanContextConfig;
    vulkanContextConfig.terminateOnValidationError = true;
    vulkanContextConfig.enhancedShaderDebugging = false;
    vulkanContextConfig.swapChainColorSpace = properties.isSRGB() ? igl::ColorSpace::SRGB_NONLINEAR
                                                                  : igl::ColorSpace::SRGB_LINEAR;

    auto context =
        igl::vulkan::HWDevice::createContext(vulkanContextConfig, (__bridge void*)vulkanView);
    auto devices = igl::vulkan::HWDevice::queryDevices(
        *context.get(), igl::HWDeviceQueryDesc(igl::HWDeviceType::DiscreteGpu), nullptr);
    if (devices.empty()) {
      devices = igl::vulkan::HWDevice::queryDevices(
          *context.get(), igl::HWDeviceQueryDesc(igl::HWDeviceType::IntegratedGpu), nullptr);
    }
    auto device = igl::vulkan::HWDevice::create(std::move(context), devices[0], 0, 0);

    shellPlatform_ = std::make_shared<igl::shell::PlatformMac>(std::move(device));
    [vulkanView prepareVulkan:shellPlatform_];
    break;
  }
#endif

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

  default: {
    IGL_ASSERT_NOT_IMPLEMENTED();
    break;
  }
  }

  session_ = igl::shell::createDefaultRenderSession(shellPlatform_);
  IGL_ASSERT_MSG(session_, "createDefaultRenderSession() must return a valid session");
  // Get initial native surface dimensions
  shellParams_.nativeSurfaceDimensions = glm::ivec2(2048, 1536);
  session_->initialize();
}

- (void)viewDidAppear {
  if ([self.view isKindOfClass:[MetalView class]]) {
    MetalView* v = (MetalView*)self.view;
    v.paused = NO;
  } else if ([self.view isKindOfClass:[GLView class]]) {
    GLView* v = (GLView*)self.view;
    [v startTimer];
  }
}

- (void)viewWillDisappear {
  if ([self.view isKindOfClass:[MetalView class]]) {
    MetalView* v = (MetalView*)self.view;
    v.paused = YES;
  } else if ([self.view isKindOfClass:[GLView class]]) {
    GLView* v = (GLView*)self.view;
    [v stopTimer];
  }
}

- (void)drawInMTKView:(nonnull MTKView*)view {
  @autoreleasepool {
    currentDrawable_ = view.currentDrawable;
    depthStencilTexture_ = view.depthStencilTexture;
    [self render];
    currentDrawable_ = nil;
  }
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
  frame_.size = size;
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeDidChange:(CGSize)size {
}

- (std::shared_ptr<igl::ITexture>)createTextureFromNativeDrawable {
  switch (backendType_) {
#if IGL_BACKEND_METAL
  case igl::BackendType::Metal: {
    auto& device = shellPlatform_->getDevice();
    auto platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_ASSERT(platformDevice);
    IGL_ASSERT(currentDrawable_ != nil);
    auto texture = platformDevice->createTextureFromNativeDrawable(currentDrawable_, nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendType::OpenGL: {
    auto& device = shellPlatform_->getDevice();
    auto platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    IGL_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDrawable(nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendType::Vulkan: {
    auto& device = shellPlatform_->getDevice();
    auto platformDevice = device.getPlatformDevice<igl::vulkan::PlatformDevice>();
    IGL_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDrawable(nullptr);
    return texture;
  }
#endif

// @fb-only
  // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
  // @fb-only
// @fb-only

  default: {
    IGL_ASSERT_NOT_IMPLEMENTED();
    return nullptr;
  }
  }
}

- (std::shared_ptr<igl::ITexture>)createTextureFromNativeDepth {
  switch (backendType_) {
#if IGL_BACKEND_METAL
  case igl::BackendType::Metal: {
    auto& device = shellPlatform_->getDevice();
    auto platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDepth(depthStencilTexture_, nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendType::OpenGL: {
    auto& device = shellPlatform_->getDevice();
    auto platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    IGL_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDepth(nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendType::Vulkan: {
    auto& device = static_cast<igl::vulkan::Device&>(shellPlatform_->getDevice());
    auto extents = device.getVulkanContext().getSwapchainExtent();
    auto platformDevice =
        shellPlatform_->getDevice().getPlatformDevice<igl::vulkan::PlatformDevice>();

    IGL_ASSERT(platformDevice);
    auto texture =
        platformDevice->createTextureFromNativeDepth(extents.width, extents.height, nullptr);
    return texture;
  }
#endif

// @fb-only
  // @fb-only
    // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
  // @fb-only
// @fb-only

  default: {
    IGL_ASSERT_NOT_IMPLEMENTED();
    return nullptr;
  }
  }
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (NSPoint)locationForEvent:(NSEvent*)event {
  NSRect contentRect = self.view.frame;
  NSPoint pos = [event locationInWindow];
  return NSMakePoint(pos.x, contentRect.size.height - pos.y);
}

- (void)keyUp:(NSEvent*)event {
  shellPlatform_->getInputDispatcher().queueEvent(igl::shell::KeyEvent(false, event.keyCode));
}

- (void)keyDown:(NSEvent*)event {
  shellPlatform_->getInputDispatcher().queueEvent(igl::shell::KeyEvent(true, event.keyCode));
}

- (void)mouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Left, true, curPoint.x, curPoint.y));
}

- (void)rightMouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Right, true, curPoint.x, curPoint.y));
}

- (void)otherMouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Middle, true, curPoint.x, curPoint.y));
}

- (void)mouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Left, false, curPoint.x, curPoint.y));
}

- (void)rightMouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Right, false, curPoint.x, curPoint.y));
}

- (void)otherMouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Middle, false, curPoint.x, curPoint.y));
}

- (void)mouseMoved:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)mouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)rightMouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)otherMouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)scrollWheel:(NSEvent*)event {
  shellPlatform_->getInputDispatcher().queueEvent(igl::shell::MouseWheelEvent(
      event.scrollingDeltaX * kMouseSpeed_, event.scrollingDeltaY * kMouseSpeed_));
}

- (CGRect)frame {
  return frame_;
}

@end
