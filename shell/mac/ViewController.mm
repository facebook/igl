/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "ViewController.h"

#import "GLView.h"
#import "HeadlessView.h"
#import "MetalView.h"
// @fb-only

#import <AppKit/NSApplication.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSOpenGLView.h>
#import <AppKit/NSView.h>
#import <shell/shared/input/InputDispatcher.h>
#include <shell/shared/platform/Platform.h>
#import <igl/Common.h>
#import <igl/IGL.h>
#if IGL_BACKEND_METAL
#include <igl/metal/ColorSpace.h>
#include <igl/metal/HWDevice.h>
#include <igl/metal/Texture.h>
#include <igl/metal/macos/Device.h>
#endif
#if IGL_BACKEND_OPENGL
#include <igl/opengl/macos/Context.h>
#include <igl/opengl/macos/HWDevice.h>
#include <igl/opengl/macos/PlatformDevice.h>
#endif
#include <shell/shared/platform/mac/PlatformMac.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/ShellParams.h>
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
#if IGL_BACKEND_VULKAN
#import "VulkanView.h"

#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#endif
#import <cmath>
#import <simd/simd.h>

using namespace igl;

@interface ViewController () {
  igl::shell::IRenderSessionFactory* factory;
  igl::shell::RenderSessionConfig config;
  igl::shell::ShellParams shellParams;
  CGRect frame;
  CVDisplayLinkRef displayLink; // For OpenGL only
  id<CAMetalDrawable> currentDrawable;
  id<MTLTexture> depthStencilTexture;
  std::shared_ptr<igl::shell::Platform> shellPlatform;
  std::unique_ptr<igl::shell::RenderSession> session;
  float kMouseSpeed;
}
@end

@implementation ViewController

@synthesize iglView = iglView;

///--------------------------------------
/// MARK: - Init
///--------------------------------------

- (instancetype)initWithFrame:(CGRect)frame
                      factory:(igl::shell::IRenderSessionFactory&)factory
                       config:(igl::shell::RenderSessionConfig)config {
  self = [super initWithNibName:nil bundle:nil];
  if (!self) {
    return self;
  }

  self->config = std::move(config);
  self->factory = &factory;
  self->shellParams = igl::shell::ShellParams();
  self->shellParams.viewportSize.x = frame.size.width;
  self->shellParams.viewportSize.y = frame.size.height;
  self->frame = frame;
  self->kMouseSpeed = 0.05f;
  self->currentDrawable = nil;
  self->depthStencilTexture = nil;

  return self;
}

- (void)initModule {
}

- (void)teardown {
  if (session) {
    session->teardown();
  }
  session = nullptr;
  shellPlatform = nullptr;
}

- (void)render {
  if (session == nullptr) {
    return;
  }

  const NSRect contentRect = self.view.frame;

  shellParams.viewportSize = glm::vec2(contentRect.size.width, contentRect.size.height);
  shellParams.viewportScale = self.view.window.backingScaleFactor;
  session->setShellParams(shellParams);
  // process user input
  shellPlatform->getInputDispatcher().processEvents();

  igl::SurfaceTextures surfaceTextures;
  if (config.backendVersion.flavor != igl::BackendFlavor::Invalid &&
      shellPlatform->getDevicePtr() != nullptr) {
// @fb-only
    // @fb-only
    // @fb-only
      // @fb-only
          // @fb-only
      // @fb-only
    // @fb-only
// @fb-only

    // surface textures
    surfaceTextures = igl::SurfaceTextures{.color = [self createTextureFromNativeDrawable],
                                           .depth = [self createTextureFromNativeDepth]};
    IGL_DEBUG_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);
    const auto& dims = surfaceTextures.color->getDimensions();
    shellParams.nativeSurfaceDimensions = glm::ivec2{dims.width, dims.height};

    // update retina scale
    float pixelsPerPoint = shellParams.nativeSurfaceDimensions.x / shellParams.viewportSize.x;
    session->setPixelsPerPoint(pixelsPerPoint);
// @fb-only
    // @fb-only
    // @fb-only
        // @fb-only
    // @fb-only
// @fb-only
  }

  // Use runUpdate() which automatically handles benchmark timing, reporting, and expiration
  session->runUpdate(std::move(surfaceTextures));

  if (session->appParams().exitRequested) {
    [[NSApplication sharedApplication] terminate:nil];
  }
}

- (void)loadView {
  // We don't care about the device type, just
  // return something that works
  HWDeviceQueryDesc queryDesc(HWDeviceType::Unknown);

  switch (config.backendVersion.flavor) {
  case igl::BackendFlavor::Invalid: {
    auto headlessView = [[HeadlessView alloc] initWithFrame:frame];
    self.view = headlessView;

    // @fb-only
        // @fb-only

    // Headless platform does not run on a real device
    shellPlatform = std::make_shared<igl::shell::PlatformMac>(nullptr);

    [headlessView prepareHeadless];
    break;
  }

#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto hwDevices = metal::HWDevice().queryDevices(queryDesc, nullptr);
    auto device = metal::HWDevice().create(hwDevices[0], nullptr);

    auto d = static_cast<igl::metal::macos::Device*>(device.get())->get();

    auto metalView = [[MetalView alloc] initWithFrame:frame device:d];
    metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    metalView.delegate = self;

    metalView.colorPixelFormat =
        metal::Texture::textureFormatToMTLPixelFormat(config.swapchainColorTextureFormat);
    metalView.colorspace = metal::colorSpaceToCGColorSpace(config.swapchainColorSpace);

    metalView.framebufferOnly = NO;
    [metalView setViewController:self];
    self.view = metalView;
    shellPlatform = std::make_shared<igl::shell::PlatformMac>(std::move(device));
    break;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    const bool enableStencilBuffer =
        config.depthTextureFormat == igl::TextureFormat::S8_UInt_Z24_UNorm ||
        config.depthTextureFormat == igl::TextureFormat::S_UInt8;
    const NSOpenGLPixelFormatAttribute stencilSize = enableStencilBuffer ? 8 : 0;

    NSOpenGLPixelFormat* pixelFormat;
    if (config.backendVersion.majorVersion == 4 && config.backendVersion.minorVersion == 1) {
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
          NSOpenGLPFAStencilSize,
          stencilSize,
          NSOpenGLPFAOpenGLProfile,
          NSOpenGLProfileVersion4_1Core,
          0,
      };
      pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      IGL_DEBUG_ASSERT(pixelFormat, "Requested attributes not supported");
    } else if (config.backendVersion.majorVersion == 3 && config.backendVersion.minorVersion == 2) {
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
          NSOpenGLPFAStencilSize,
          stencilSize,
          NSOpenGLPFAOpenGLProfile,
          NSOpenGLProfileVersion3_2Core,
          0,
      };
      pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    } else if (config.backendVersion.majorVersion == 2 && config.backendVersion.minorVersion == 1) {
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
          NSOpenGLPFAStencilSize,
          stencilSize,
          NSOpenGLPFAOpenGLProfile,
          NSOpenGLProfileVersionLegacy,
          0,
      };
      pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    } else {
      IGL_DEBUG_ABORT("Unsupported OpenGL version: %u.%u\n",
                      config.backendVersion.majorVersion,
                      config.backendVersion.minorVersion);
    }
    auto openGLView = [[GLView alloc] initWithFrame:frame pixelFormat:pixelFormat];
    igl::Result result;
    auto context = igl::opengl::macos::Context::createContext(openGLView.openGLContext, &result);
    IGL_DEBUG_ASSERT(result.isOk());
    shellPlatform = std::make_shared<igl::shell::PlatformMac>(
        opengl::macos::HWDevice().createWithContext(std::move(context), nullptr));

    auto& device = shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    platformDevice->setNativeDrawableTextureFormat(config.swapchainColorTextureFormat, nullptr);
    self.view = openGLView;
    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto vulkanView = [[VulkanView alloc] initWithFrame:frame];

    self.view = vulkanView;

    // vulkanView.wantsLayer =
    // YES; // Back the view with a layer created by the makeBackingLayer method.

    igl::vulkan::VulkanContextConfig vulkanContextConfig;
    vulkanContextConfig.terminateOnValidationError = true;
    vulkanContextConfig.enhancedShaderDebugging = false;

    vulkanContextConfig.swapChainColorSpace = config.swapchainColorSpace;
    vulkanContextConfig.requestedSwapChainTextureFormat = config.swapchainColorTextureFormat;

    auto context =
        igl::vulkan::HWDevice::createContext(vulkanContextConfig, (__bridge void*)vulkanView);
    auto devices = igl::vulkan::HWDevice::queryDevices(
        *context, igl::HWDeviceQueryDesc(igl::HWDeviceType::DiscreteGpu), nullptr);
    if (devices.empty()) {
      devices = igl::vulkan::HWDevice::queryDevices(
          *context, igl::HWDeviceQueryDesc(igl::HWDeviceType::IntegratedGpu), nullptr);
    }
    if (devices.empty()) {
      devices = igl::vulkan::HWDevice::queryDevices(
          *context, igl::HWDeviceQueryDesc(igl::HWDeviceType::SoftwareGpu), nullptr);
    }
    auto device = igl::vulkan::HWDevice::create(
        std::move(context), devices[0], 0, 0, 0, nullptr, nullptr, "IGL Shell", nullptr);

    shellPlatform = std::make_shared<igl::shell::PlatformMac>(std::move(device));
    [vulkanView prepareVulkan:shellPlatform.get()];
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
  // @fb-only
// @fb-only

  default: {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    break;
  }
  }

  session = factory->createRenderSession(shellPlatform);
  IGL_DEBUG_ASSERT(session, "createDefaultRenderSession() must return a valid session");
  // Get initial native surface dimensions
  shellParams.nativeSurfaceDimensions = glm::ivec2(2048, 1536);
  auto args =
      shell::convertArgvToParams(igl::shell::Platform::argc(), igl::shell::Platform::argv());
  shell::parseShellParams(args, shellParams);

  session->setShellParams(shellParams);
  session->initialize();
}

- (void)viewDidAppear {
  if ([self.view isKindOfClass:[MetalView class]]) {
    MetalView* v = (MetalView*)self.view;
    v.paused = NO;
  } else if ([self.view isKindOfClass:[GLView class]]) {
    GLView* v = (GLView*)self.view;
    [v startTimer];
  }
  [self.view.window makeFirstResponder:self];
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
    currentDrawable = view.currentDrawable;
    depthStencilTexture = view.depthStencilTexture;
    [self render];
    currentDrawable = nil;
  }
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
  frame.size = size;
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeDidChange:(CGSize)size {
}

- (std::shared_ptr<igl::ITexture>)createTextureFromNativeDrawable {
  switch (config.backendVersion.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto& device = shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    IGL_DEBUG_ASSERT(currentDrawable != nil);
    auto texture = platformDevice->createTextureFromNativeDrawable(currentDrawable, nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    auto& device = shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDrawable(nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto& device = shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::vulkan::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
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
    // @fb-only
  // @fb-only
// @fb-only

  default: {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return nullptr;
  }
  }
}

- (std::shared_ptr<igl::ITexture>)createTextureFromNativeDepth {
  switch (config.backendVersion.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto& device = shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDepth(depthStencilTexture, nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    auto& device = shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDepth(nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto& device = static_cast<igl::vulkan::Device&>(shellPlatform->getDevice());
    auto extents = device.getVulkanContext().getSwapchainExtent();
    auto* platformDevice =
        shellPlatform->getDevice().getPlatformDevice<igl::vulkan::PlatformDevice>();

    IGL_DEBUG_ASSERT(platformDevice);
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
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
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

static uint32_t getModifiers(NSEvent* event) {
  uint32_t modifiers = igl::shell::kKeyEventModifierNone;
  const NSUInteger flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

  if (flags & NSEventModifierFlagShift) {
    modifiers |= igl::shell::kKeyEventModifierShift;
  }
  if (flags & NSEventModifierFlagCapsLock) {
    modifiers |= igl::shell::kKeyEventModifierCapsLock;
  }
  if (flags & NSEventModifierFlagControl) {
    modifiers |= igl::shell::kKeyEventModifierControl;
  }
  if (flags & NSEventModifierFlagOption) {
    modifiers |= igl::shell::kKeyEventModifierOption;
  }
  if (flags & NSEventModifierFlagCommand) {
    modifiers |= igl::shell::kKeyEventModifierCommand;
  }
  if (flags & NSEventModifierFlagNumericPad) {
    modifiers |= igl::shell::kKeyEventModifierNumLock;
  }
  return modifiers;
}

- (void)keyUp:(NSEvent*)event {
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::KeyEvent(false, event.keyCode, getModifiers(event)));
}

- (void)keyDown:(NSEvent*)event {
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::KeyEvent(true, event.keyCode, getModifiers(event)));
  std::string characters([event.characters UTF8String]);
  for (const auto& c : characters) {
    shellPlatform->getInputDispatcher().queueEvent(igl::shell::CharEvent{.character = c});
  }
}

- (void)mouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Left, true, curPoint.x, curPoint.y));
}

- (void)rightMouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Right, true, curPoint.x, curPoint.y));
}

- (void)otherMouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Middle, true, curPoint.x, curPoint.y));
}

- (void)mouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Left, false, curPoint.x, curPoint.y));
}

- (void)rightMouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Right, false, curPoint.x, curPoint.y));
}

- (void)otherMouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Middle, false, curPoint.x, curPoint.y));
}

- (void)mouseMoved:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)mouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)rightMouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)otherMouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)scrollWheel:(NSEvent*)event {
  shellPlatform->getInputDispatcher().queueEvent(igl::shell::MouseWheelEvent(
      event.scrollingDeltaX * kMouseSpeed, event.scrollingDeltaY * kMouseSpeed));
}

- (CGRect)frame {
  return frame;
}

- (igl::ColorSpace)colorSpace {
  return config.swapchainColorSpace;
}

@end
