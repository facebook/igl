/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import "IGLSnapshotTestCase.h"

#import "IGLBytesToUIImage.h"
#import "TinyRenderable.hpp"

#import <FBServerSnapshotTestCase/FBServerSnapshotTestCase.h>
#import <FBServerSnapshotTestCase/FBServerSnapshotTestData.h>
#import <FBServerSnapshotTestCase/FBServerSnapshotTestRecorder.h>
#import <iglu/kit/Renderable.hpp>
#import <igl/DebugMacros.h> // IWYU pragma: keep
#import <igl/IGL.h> // IWYU pragma: keep
#include <igl/metal/HWDevice.h>
#include <igl/opengl/ios/HWDevice.h>

@implementation IGLSnapshotTestCase {
  std::shared_ptr<igl::IDevice> device;
  std::shared_ptr<igl::ICommandQueue> commandQueue;
  std::shared_ptr<igl::IFramebuffer> framebuffer;
  igl::BackendType backendType;
  std::shared_ptr<iglu::kit::IRenderable> renderable;
}

- (void)setUp {
  [super setUp];

  renderable = std::make_shared<iglu::kit::TinyRenderable>();
  backendType = igl::BackendType::OpenGL;
  [self initIGL];
}

- (void)initIGL {
  igl::HWDeviceQueryDesc queryDesc(igl::HWDeviceType::DiscreteGpu);
  if (backendType == igl::BackendType::Metal) {
    igl::metal::HWDevice hwDevice;
    auto hwDevices = hwDevice.queryDevices(queryDesc, nullptr);
    device = hwDevice.create(hwDevices[0], nullptr);
  } else if (backendType == igl::BackendType::OpenGL) {
    igl::opengl::ios::HWDevice hwDevice;
    device = hwDevice.create(nullptr);
  }

  igl::Result result;
  igl::DeviceScope scope(*device);
  igl::CommandQueueDesc desc{};
  commandQueue = device->createCommandQueue(desc, &result);
  IGL_DEBUG_ASSERT(
      result.isOk(), "Simple sample create command queue failed: %s\n", result.message.c_str());
}

- (std::shared_ptr<igl::ITexture>)createOrUpdateFramebuffer {
  IGL_DEBUG_ASSERT(device->verifyScope());

  auto texDesc = igl::TextureDesc::new2D(
      igl::TextureFormat::RGBA_UNorm8, 720, 1280, igl::TextureDesc::TextureUsageBits::Attachment);
  std::shared_ptr<igl::ITexture> targetTexture = device->createTexture(texDesc, nullptr);

  if (framebuffer) {
    framebuffer->updateDrawable(targetTexture);
  } else {
    igl::Result result;
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = targetTexture;
    framebuffer = device->createFramebuffer(framebufferDesc, &result);
    IGL_DEBUG_ASSERT(result.isOk(), "Could not create framebuffer %s\n", result.message.c_str());

    renderable->initialize(*device, *framebuffer);
  }
  return targetTexture;
}

- (void)render {
  igl::DeviceScope scope(*device);
  auto nativeDrawable = [self createOrUpdateFramebuffer];

  igl::Result result;
  igl::CommandBufferDesc cbDesc;
  auto commandBuffer = commandQueue->createCommandBuffer(cbDesc, &result);
  IGL_DEBUG_ASSERT(result.isOk(), "Could not create cmd buffer %s\n", result.message.c_str());

  igl::RenderPassDesc renderPass;
  renderPass.colorAttachments.resize(1);
  renderPass.colorAttachments[0].loadAction = igl::LoadAction::Clear;
  renderPass.colorAttachments[0].storeAction = igl::StoreAction::Store;
  renderPass.colorAttachments[0].clearColor = {1.0, 1.0, 1.0, 1.0};

  auto cmds = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer);
  IGL_DEBUG_BUFFER_LABEL_START(commandBuffer, "draw renderable");
  renderable->submit(*cmds);
  IGL_DEBUG_BUFFER_LABEL_END(commandBuffer);
  cmds->endEncoding();

  commandQueue->submit(*commandBuffer); // Guarantees ordering between command buffers
}

- (void)testTinySample {
  [self render];

  auto image = IGLFramebufferToUIImage(*framebuffer, *commandQueue, 720, 1280);
  FBRecordSnapshotData([[FBServerSnapshotTestData alloc] initWithSnapshot:image
                                                             coverageInfo:nil
                                                            configuration:nil
                                                               identifier:nil
                                                                 metadata:nil]);
}

@end
