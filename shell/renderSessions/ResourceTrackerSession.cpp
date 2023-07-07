/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/renderSessions/ResourceTrackerSession.h>

#include <igl/Device.h>
#include <igl/Texture.h>
#include <igl/opengl/Device.h>
#include <iglu/resource_tracker/ResourceTracker.h>
#include <memory>
#include <shell/shared/imageLoader/ImageLoader.h>

#if !IGL_DEBUG
#pragma clang diagnostic ignored "-Wunused-const-variable"
#endif

namespace igl {
namespace shell {

const char* ASSETS_TAG = "assets";
const char* RENDER_PASS_TAG = "renderPass";

constexpr size_t kCompressedExpectedByteCount = 800000;
constexpr size_t k3DExpectedByteCount = 120000;
constexpr size_t kCubeExpectedByteCount = 40000 * 6;

static uint16_t QUAD_IND[] = {
    0,
    1,
    2,
    1,
    3,
    2,
};

void ResourceTrackerSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();
  // Initialize resource tracker
  auto rt = std::make_shared<iglu::resource_tracker::ResourceTracker>();
  device.setResourceTracker(rt);

  // Create texture desc
  auto imageData = getPlatform().getImageLoader().loadImageData("igl.png");
  igl::TextureDesc texDesc = igl::TextureDesc::new2D(igl::TextureFormat::RGBA_SRGB,
                                                     imageData.width,
                                                     imageData.height,
                                                     igl::TextureDesc::TextureUsageBits::Sampled);
  int heightLevels = floor(log2(texDesc.height)) + 1;
  int widthLevels = floor(log2(texDesc.width)) + 1;
  texDesc.numMipLevels = (heightLevels > widthLevels) ? heightLevels : widthLevels;

  // Create compressed tex description
  igl::TextureDesc texDescCompressed = igl::TextureDesc::new2D(
      igl::TextureFormat::RGBA_ASTC_5x4, 1000, 1000, igl::TextureDesc::TextureUsageBits::Sampled);

  // 3D tex desc
  igl::TextureDesc texDesc3D = igl::TextureDesc::new3D(
      igl::TextureFormat::RGBA_UNorm8, 100, 100, 3, igl::TextureDesc::TextureUsageBits::Sampled);

  // Cube desc
  igl::TextureDesc texDescCube = igl::TextureDesc::newCube(
      igl::TextureFormat::RGBA_UNorm8, 100, 100, igl::TextureDesc::TextureUsageBits::Sampled);

  // Create buffer desc
  BufferDesc bufDesc;
  bufDesc.type = BufferDesc::BufferTypeBits::Index;
  bufDesc.data = QUAD_IND;
  bufDesc.length = sizeof(QUAD_IND);

  auto untrackedTexture = device.createTexture(texDesc, nullptr);
  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.count == 0);
  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.bytesUsedEstimate == 0);
  untrackedTexture = nullptr;

  rt->pushTag(ASSETS_TAG);
  if (device.getTextureFormatCapabilities(igl::TextureFormat::RGBA_ASTC_5x4) !=
      ICapabilities::TextureFormatCapabilityBits::Unsupported) {
    auto texture = device.createTexture(texDescCompressed, nullptr);
    IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.bytesUsedEstimate ==
               kCompressedExpectedByteCount);
  }
  if (device.getBackendType() != igl::BackendType::OpenGL ||
      static_cast<igl::opengl::Device&>(device).getContext().deviceFeatures().getGLVersion() >=
          opengl::GLVersion::v3_0_ES) {
    auto texture = device.createTexture(texDesc3D, nullptr);
    IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.bytesUsedEstimate ==
               k3DExpectedByteCount);
  }
  {
    auto texture = device.createTexture(texDescCube, nullptr);
    IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.bytesUsedEstimate ==
               kCubeExpectedByteCount);
  }
  auto texture = device.createTexture(texDesc, nullptr);
  auto texture2 = device.createTexture(texDesc, nullptr);

  auto buffer = device.createBuffer(bufDesc, nullptr);
  auto buffer2 = device.createBuffer(bufDesc, nullptr);
  auto tagGuard = std::make_unique<ResourceTrackerTagGuard>(rt, RENDER_PASS_TAG);
  auto texture3 = device.createTexture(texDesc, nullptr);

  auto buffer3 = device.createBuffer(bufDesc, nullptr);
  tagGuard = nullptr;
  rt->popTag();
  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.count == 2);
  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.bytesUsedEstimate ==
                     texDesc.numMipLevels > 1
                 ? 2 * iglExpectedByteCountWithMipmaps_
                 : 2 * iglExpectedByteCount_);

  IGL_ASSERT(rt->getResourceStats(RENDER_PASS_TAG).textureStats.count == 1);
  IGL_ASSERT(rt->getResourceStats(RENDER_PASS_TAG).textureStats.bytesUsedEstimate ==
                     texDesc.numMipLevels > 1
                 ? iglExpectedByteCountWithMipmaps_
                 : iglExpectedByteCount_);

  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).bufferStats.count == 2);
  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).bufferStats.bytesUsed == 2 * sizeof(QUAD_IND));

  IGL_ASSERT(rt->getResourceStats(RENDER_PASS_TAG).bufferStats.count == 1);
  IGL_ASSERT(rt->getResourceStats(RENDER_PASS_TAG).bufferStats.bytesUsed == sizeof(QUAD_IND));

  // Make nullptr to cause texture destructor and assert resources are removed from tracker
  texture = nullptr;
  buffer = nullptr;
  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.count == 1);
  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).textureStats.bytesUsedEstimate ==
                     texDesc.numMipLevels > 1
                 ? iglExpectedByteCountWithMipmaps_
                 : iglExpectedByteCount_);

  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).bufferStats.count == 1);
  IGL_ASSERT(rt->getResourceStats(ASSETS_TAG).bufferStats.bytesUsed == sizeof(QUAD_IND));
}

void ResourceTrackerSession::update(igl::SurfaceTextures surfaceTextures) noexcept {}

} // namespace shell
} // namespace igl
