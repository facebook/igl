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
#include <shell/shared/imageLoader/mac/ImageLoaderMac.h>
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
  // Update viewport size but only in normal run since we call a 1024x768 window and on first frame
  // we get 1024x712
  if (!session_->appParams().screenshotTestsParams.isScreenshotTestsEnabled()) {
    shellParams_.viewportSize = glm::vec2(self.view.frame.size.width, self.view.frame.size.height);
  }
  shellParams_.viewportScale = self.view.window.backingScaleFactor;
  session_->setShellParams(shellParams_);
  // process user input
  shellPlatform_->getInputDispatcher().processEvents();

  // update retina scale
  session_->updateDisplayScale((float)self.view.window.backingScaleFactor);

  // draw
  igl::SurfaceTextures surfaceTextures =
      (session_->appParams().screenshotTestsParams.isScreenshotTestsEnabled())
          ? [self createOffScreenTextures]
          : igl::SurfaceTextures{[self createTextureFromNativeDrawable],
                                 [self createTextureFromNativeDepth]};
  IGL_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);
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
    metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;

    metalView.delegate = self;
    metalView.colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    metalView.colorPixelFormat =
        metal::Texture::textureFormatToMTLPixelFormat(shellParams_.defaultColorFramebufferFormat);
    metalView.framebufferOnly = NO;
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

    igl::vulkan::VulkanContextConfig vulkanContextConfig{
        128, // maxTextures
        80, // maxSamplers
        true, // terminateOnValidationError
        false // enhancedShaderDebugging
    };
    auto context =
        igl::vulkan::HWDevice::createContext(vulkanContextConfig, (__bridge void*)vulkanView);
    auto devices = igl::vulkan::HWDevice::queryDevices(
        *context.get(), igl::HWDeviceQueryDesc(igl::HWDeviceType::DiscreteGpu), nullptr);
    if (devices.empty()) {
      devices = igl::vulkan::HWDevice::queryDevices(
          *context.get(), igl::HWDeviceQueryDesc(igl::HWDeviceType::IntegratedGpu), nullptr);
    }
    auto device = igl::vulkan::HWDevice::create(
        std::move(context),
        devices[0],
        0,
        0,
        !igl::isTextureFormatsRGB(shellParams_.defaultColorFramebufferFormat));

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
  currentDrawable_ = view.currentDrawable;
  depthStencilTexture_ = view.depthStencilTexture;
  [self render];
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
}

- (std::shared_ptr<igl::ITexture>)createTextureFromNativeDrawable {
  switch (backendType_) {
#if IGL_BACKEND_METAL
  case igl::BackendType::Metal: {
    auto& device = shellPlatform_->getDevice();
    auto platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_ASSERT(platformDevice);
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

- (igl::SurfaceTextures)createOffScreenTextures {
  TextureDesc desc = {static_cast<size_t>(shellParams_.viewportSize.x),
                      static_cast<size_t>(shellParams_.viewportSize.y),
                      1,
                      1,
                      1,
                      TextureDesc::TextureUsageBits::Attachment,
                      0,
                      1,
                      TextureType::TwoD,
                      shellParams_.defaultColorFramebufferFormat,
                      ResourceStorage::Managed};
  auto color = shellPlatform_->getDevice().createTexture(desc, nullptr);
  desc.format = TextureFormat::Z_UNorm24;
  desc.storage = ResourceStorage::Private;
  auto depth = shellPlatform_->getDevice().createTexture(desc, nullptr);
  return igl::SurfaceTextures{std::move(color), std::move(depth)};
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

- (void)scrollWheel:(NSEvent*)event {
  shellPlatform_->getInputDispatcher().queueEvent(igl::shell::MouseWheelEvent(
      event.scrollingDeltaX * kMouseSpeed_, event.scrollingDeltaY * kMouseSpeed_));
}

- (CGRect)frame {
  return frame_;
}

@end
