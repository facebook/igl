/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "igl_c_wrapper.h"
#include "igl_platform_internal.h"

#include <shell/shared/platform/mac/PlatformMac.h>
#include <shell/renderSessions/ThreeCubesRenderSession.h>
#include <igl/metal/HWDevice.h>
#include <igl/metal/macos/Device.h>
#include <igl/metal/PlatformDevice.h>
#include <objc/runtime.h>
#include <memory>
#include <igl/IGL.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>

struct IGLRenderSession {
    std::unique_ptr<igl::shell::ThreeCubesRenderSession> session;
    IGLPlatform* platform;  // Reference to platform for texture access
};

extern "C" {

IGLPlatform* igl_platform_create_metal(IGLNativeWindowHandle window_handle,
                                       int32_t width,
                                       int32_t height) {
    try {
        printf("[C Wrapper] Creating Metal platform: width=%d, height=%d, handle=%p\n",
               width, height, window_handle);

        auto* wrapper = new IGLPlatform();
        wrapper->width = width;
        wrapper->height = height;
        wrapper->currentDrawable = nil;
        wrapper->depthTexture = nil;

        printf("[C Wrapper] Querying Metal devices...\n");
        // Create Metal device
        igl::HWDeviceQueryDesc queryDesc(igl::HWDeviceType::Unknown);
        auto hwDevices = igl::metal::HWDevice().queryDevices(queryDesc, nullptr);

        if (hwDevices.empty()) {
            printf("[C Wrapper] ERROR: No Metal devices found!\n");
            delete wrapper;
            return nullptr;
        }

        printf("[C Wrapper] Found %zu Metal device(s), creating device...\n", hwDevices.size());
        auto device = igl::metal::HWDevice().create(hwDevices[0], nullptr);

        if (!device) {
            printf("[C Wrapper] ERROR: Failed to create Metal device!\n");
            delete wrapper;
            return nullptr;
        }

        printf("[C Wrapper] Getting Metal device handle...\n");
        // Get the Metal device handle
        auto* metalDevice = static_cast<igl::metal::macos::Device*>(device.get())->get();

        printf("[C Wrapper] Creating PlatformMac...\n");
        // Create platform with Metal backend
        wrapper->platform = std::make_shared<igl::shell::PlatformMac>(std::move(device));

        printf("[C Wrapper] Setting up CAMetalLayer on NSView...\n");
        // The window_handle might be a GLFW window - extract the content view
        id windowOrView = (__bridge id)window_handle;
        NSView* view = nil;

        printf("[C Wrapper] Window/View class: %s\n", object_getClassName(windowOrView));

        // Check if it's a GLFW window and get its content view
        if ([windowOrView respondsToSelector:@selector(contentView)]) {
            printf("[C Wrapper] Getting contentView from GLFW window...\n");
            view = [(NSWindow*)windowOrView contentView];
        } else if ([windowOrView isKindOfClass:[NSView class]]) {
            printf("[C Wrapper] Already an NSView\n");
            view = (NSView*)windowOrView;
        }

        if (!view) {
            printf("[C Wrapper] ERROR: Could not get NSView from window_handle!\n");
            delete wrapper;
            return nullptr;
        }

        printf("[C Wrapper] NSView: %p, class: %s\n", view, object_getClassName(view));

        // Create the metal layer
        wrapper->metalLayer = [CAMetalLayer layer];
        wrapper->metalLayer.device = metalDevice;
        wrapper->metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        wrapper->metalLayer.framebufferOnly = NO;
        wrapper->metalLayer.drawableSize = CGSizeMake(width, height);

        // Set up the layer (must be on main thread)
        @try {
            if ([NSThread isMainThread]) {
                printf("[C Wrapper] Already on main thread, setting up layer...\n");
                printf("[C Wrapper] View class: %s\n", object_getClassName(view));
                view.wantsLayer = YES;
                printf("[C Wrapper] wantsLayer set to YES\n");
                view.layer = wrapper->metalLayer;
                printf("[C Wrapper] layer assigned\n");
            } else {
                printf("[C Wrapper] Not on main thread, dispatching to main...\n");
                dispatch_sync(dispatch_get_main_queue(), ^{
                    view.wantsLayer = YES;
                    view.layer = wrapper->metalLayer;
                });
            }
            printf("[C Wrapper] Layer assigned to view\n");
        } @catch (NSException *exception) {
            printf("[C Wrapper] Objective-C exception: %s - %s\n",
                   [[exception name] UTF8String],
                   [[exception reason] UTF8String]);
            delete wrapper;
            return nullptr;
        }

        printf("[C Wrapper] Metal platform created successfully!\n");
        return wrapper;
    } catch (const std::exception& e) {
        printf("[C Wrapper] Exception: %s\n", e.what());
        return nullptr;
    } catch (...) {
        printf("[C Wrapper] Unknown exception\n");
        return nullptr;
    }
}

void igl_platform_destroy(IGLPlatform* platform) {
    if (platform) {
        delete platform;
    }
}

IGLRenderSession* igl_render_session_create(IGLPlatform* platform) {
    if (!platform || !platform->platform) {
        return nullptr;
    }

    try {
        auto* wrapper = new IGLRenderSession();
        wrapper->session = std::make_unique<igl::shell::ThreeCubesRenderSession>(
            platform->platform);
        wrapper->platform = platform;  // Store platform reference
        return wrapper;
    } catch (...) {
        return nullptr;
    }
}

void igl_render_session_destroy(IGLRenderSession* session) {
    if (session) {
        delete session;
    }
}

bool igl_render_session_initialize(IGLRenderSession* session) {
    if (!session || !session->session) {
        return false;
    }

    try {
        session->session->initialize();
        return true;
    } catch (...) {
        return false;
    }
}

bool igl_render_session_update(IGLRenderSession* session) {
    if (!session || !session->session || !session->platform) {
        return false;
    }

    @autoreleasepool {
        try {
            auto* platform = session->platform;

            // Get the next drawable from the Metal layer
            platform->currentDrawable = [platform->metalLayer nextDrawable];
            if (platform->currentDrawable == nil) {
                return false;
            }

            // Get or create depth texture
            if (platform->depthTexture == nil ||
                platform->depthTexture.width != platform->width ||
                platform->depthTexture.height != platform->height) {

                auto* metalDevice = platform->metalLayer.device;
                MTLTextureDescriptor* depthDesc = [MTLTextureDescriptor
                    texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                 width:platform->width
                                                height:platform->height
                                             mipmapped:NO];
                depthDesc.usage = MTLTextureUsageRenderTarget;
                depthDesc.storageMode = MTLStorageModePrivate;
                platform->depthTexture = [metalDevice newTextureWithDescriptor:depthDesc];
            }

            // Create IGL textures from native drawables
            auto& device = platform->platform->getDevice();
            auto* platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();

            igl::SurfaceTextures surfaceTextures;
            surfaceTextures.color = platformDevice->createTextureFromNativeDrawable(
                platform->currentDrawable, nullptr);
            surfaceTextures.depth = platformDevice->createTextureFromNativeDepth(
                platform->depthTexture, nullptr);

            // Render the frame
            // Note: ThreeCubesRenderSession will call present() internally
            session->session->update(surfaceTextures);

            // Clear the drawable reference
            platform->currentDrawable = nil;

            return true;
        } catch (...) {
            return false;
        }
    }
}

void igl_render_session_teardown(IGLRenderSession* session) {
    if (session && session->session) {
        session->session->teardown();
    }
}

bool igl_platform_get_surface_textures(IGLPlatform* platform,
                                       IGLSurfaceTextures* out_textures) {
    // Not used in the new drawable-based approach
    return false;
}

void igl_platform_present(IGLPlatform* platform) {
    // Present is now handled in igl_render_session_update
}

void igl_platform_resize(IGLPlatform* platform, int32_t width, int32_t height) {
    if (!platform || !platform->metalLayer) {
        return;
    }

    platform->width = width;
    platform->height = height;

    // Update Metal layer drawable size
    platform->metalLayer.drawableSize = CGSizeMake(width, height);

    // Clear depth texture so it will be recreated on next frame
    platform->depthTexture = nil;
}

} // extern "C"
