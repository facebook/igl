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
#import <igl/DebugMacros.h>
#import <igl/IGL.h>
#import <igl/metal/HWDevice.h>
#import <igl/opengl/ios/Context.h>
#import <igl/opengl/ios/Device.h>
#import <igl/opengl/ios/HWDevice.h>
#import <iglu/kit/Renderable.hpp>

@implementation IGLSnapshotTestCase {
  std::shared_ptr<igl::IDevice> _device;
  std::shared_ptr<igl::ICommandQueue> _commandQueue;
  std::shared_ptr<igl::IFramebuffer> _framebuffer;
  igl::BackendType _backendType;
  std::shared_ptr<iglu::kit::IRenderable> _renderable;
}

- (void)setUp {
  [super setUp];

  _renderable = std::make_shared<iglu::kit::TinyRenderable>();
  _backendType = igl::BackendType::OpenGL;
  [self initIGL];
}

- (void)initIGL {
  igl::HWDeviceQueryDesc queryDesc(igl::HWDeviceType::DiscreteGpu);
  if (_backendType == igl::BackendType::Metal) {
    igl::metal::HWDevice hwDevice;
    auto hwDevices = hwDevice.queryDevices(queryDesc, nullptr);
    _device = hwDevice.create(hwDevices[0], nullptr);
  } else if (_backendType == igl::BackendType::OpenGL) {
    igl::opengl::ios::HWDevice hwDevice;
    auto hwDevices = hwDevice.queryDevices(queryDesc, nullptr);
    _device = hwDevice.create(hwDevices[0], igl::opengl::RenderingAPI::GLES3, nullptr);
  }

  igl::Result result;
  igl::DeviceScope scope(*_device);
  igl::CommandQueueDesc desc{};
  _commandQueue = _device->createCommandQueue(desc, &result);
  IGL_ASSERT_MSG(
      result.isOk(), "Simple sample create command queue failed: %s\n", result.message.c_str());
}

- (std::shared_ptr<igl::ITexture>)createOrUpdateFramebuffer {
  IGL_ASSERT(_device->verifyScope());

  auto texDesc = igl::TextureDesc::new2D(
      igl::TextureFormat::RGBA_UNorm8, 720, 1280, igl::TextureDesc::TextureUsageBits::Attachment);
  std::shared_ptr<igl::ITexture> targetTexture = _device->createTexture(texDesc, nullptr);

  if (_framebuffer) {
    _framebuffer->updateDrawable(targetTexture);
  } else {
    igl::Result result;
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = targetTexture;
    _framebuffer = _device->createFramebuffer(framebufferDesc, &result);
    IGL_ASSERT_MSG(result.isOk(), "Could not create framebuffer %s\n", result.message.c_str());

    _renderable->initialize(*_device, *_framebuffer);
  }
  return targetTexture;
}

- (void)render {
  igl::DeviceScope scope(*_device);
  auto nativeDrawable = [self createOrUpdateFramebuffer];

  igl::Result result;
  igl::CommandBufferDesc cbDesc;
  auto commandBuffer = _commandQueue->createCommandBuffer(cbDesc, &result);
  IGL_ASSERT_MSG(result.isOk(), "Could not create cmd buffer %s\n", result.message.c_str());

  igl::RenderPassDesc renderPass;
  renderPass.colorAttachments.resize(1);
  renderPass.colorAttachments[0].loadAction = igl::LoadAction::Clear;
  renderPass.colorAttachments[0].storeAction = igl::StoreAction::Store;
  renderPass.colorAttachments[0].clearColor = {1.0, 1.0, 1.0, 1.0};

  auto cmds = commandBuffer->createRenderCommandEncoder(renderPass, _framebuffer);
  IGL_DEBUG_BUFFER_LABEL_START(commandBuffer, "draw renderable");
  _renderable->submit(*cmds);
  IGL_DEBUG_BUFFER_LABEL_END(commandBuffer);
  cmds->endEncoding();

  _commandQueue->submit(*commandBuffer); // Guarantees ordering between command buffers
}

- (void)testTinySample {
  [self render];

  auto image = IGLFramebufferToUIImage(*_framebuffer, *_commandQueue, 720, 1280);
  FBRecordSnapshotData([[FBServerSnapshotTestData alloc] initWithSnapshot:image
                                                             coverageInfo:nil
                                                            configuration:nil
                                                               identifier:nil
                                                                 metadata:nil]);
}

@end
