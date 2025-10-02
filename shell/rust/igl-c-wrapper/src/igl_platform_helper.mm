/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "igl_platform_helper.h"
#include "igl_c_wrapper.h"
#include "igl_platform_internal.h"

#include <igl/metal/PlatformDevice.h>

extern "C" {

bool igl_platform_get_frame_textures(IGLPlatform* platform, IGLFrameTextures* out_textures) {
    if (!platform || !out_textures) {
        return false;
    }

    @autoreleasepool {
        // Get the next drawable
        platform->currentDrawable = [platform->metalLayer nextDrawable];
        if (platform->currentDrawable == nil) {
            return false;
        }

        // Create or get depth texture
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

        auto colorTexture = platformDevice->createTextureFromNativeDrawable(
            platform->currentDrawable, nullptr);
        auto depthTexture = platformDevice->createTextureFromNativeDepth(
            platform->depthTexture, nullptr);

        // Return as raw pointers (allocate shared_ptr on heap)
        out_textures->color = reinterpret_cast<IGLTexture*>(
            new std::shared_ptr<igl::ITexture>(std::move(colorTexture)));
        out_textures->depth = reinterpret_cast<IGLTexture*>(
            new std::shared_ptr<igl::ITexture>(std::move(depthTexture)));

        return true;
    }
}

void igl_platform_present_frame(IGLPlatform* platform) {
    if (platform) {
        // The present() is now handled in the render session
        // Just clear the drawable reference
        platform->currentDrawable = nil;
    }
}

} // extern "C"
