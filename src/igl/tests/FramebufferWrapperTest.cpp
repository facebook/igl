/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/FramebufferWrapper.h>

#include <cstddef>
#include <memory>
#include <vector>
#include <igl/Framebuffer.h>
#include <igl/Texture.h>

namespace igl::tests {
namespace {

// Minimal headless IFramebuffer that returns empty/null for everything. It lets
// us exercise FramebufferWrapper's delegation logic without a GPU device. All
// attachment accessors return null shared_ptrs so the wrapper's passthrough
// produces null IAttachmentInterop pointers.
struct StubFramebuffer : IFramebuffer {
  [[nodiscard]] std::vector<size_t> getColorAttachmentIndices() const override {
    return {};
  }
  [[nodiscard]] std::shared_ptr<ITexture> getColorAttachment(size_t /*index*/) const override {
    return nullptr;
  }
  [[nodiscard]] std::shared_ptr<ITexture> getResolveColorAttachment(
      size_t /*index*/) const override {
    return nullptr;
  }
  [[nodiscard]] std::shared_ptr<ITexture> getDepthAttachment() const override {
    return nullptr;
  }
  [[nodiscard]] std::shared_ptr<ITexture> getResolveDepthAttachment() const override {
    return nullptr;
  }
  [[nodiscard]] std::shared_ptr<ITexture> getStencilAttachment() const override {
    return nullptr;
  }
  [[nodiscard]] FramebufferMode getMode() const override {
    return FramebufferMode::Mono;
  }
  [[nodiscard]] bool isSwapchainBound() const override {
    return false;
  }

  void copyBytesColorAttachment(ICommandQueue& /*cmdQueue*/,
                                size_t /*index*/,
                                void* /*pixelBytes*/,
                                const TextureRangeDesc& /*range*/,
                                size_t /*bytesPerRow*/) const override {}
  void copyBytesDepthAttachment(ICommandQueue& /*cmdQueue*/,
                                void* /*pixelBytes*/,
                                const TextureRangeDesc& /*range*/,
                                size_t /*bytesPerRow*/) const override {}
  void copyBytesStencilAttachment(ICommandQueue& /*cmdQueue*/,
                                  void* /*pixelBytes*/,
                                  const TextureRangeDesc& /*range*/,
                                  size_t /*bytesPerRow*/) const override {}
  void copyTextureColorAttachment(ICommandQueue& /*cmdQueue*/,
                                  size_t /*index*/,
                                  std::shared_ptr<ITexture> /*destTexture*/,
                                  const TextureRangeDesc& /*range*/) const override {}

  void updateDrawable(std::shared_ptr<ITexture> /*texture*/) override {}
  void updateDrawable(SurfaceTextures /*surfaceTextures*/) override {}
  void updateResolveAttachment(std::shared_ptr<ITexture> /*texture*/) override {}
};

} // namespace

// --- Null-framebuffer path: wrapper must guard against a null IFramebuffer ---

TEST(FramebufferWrapperTest, NullFramebufferGetColorAttachmentReturnsNull) {
  const FramebufferWrapper wrapper(nullptr);
  EXPECT_EQ(wrapper.getColorAttachment(0), nullptr);
}

TEST(FramebufferWrapperTest, NullFramebufferGetDepthAttachmentReturnsNull) {
  const FramebufferWrapper wrapper(nullptr);
  EXPECT_EQ(wrapper.getDepthAttachment(), nullptr);
}

TEST(FramebufferWrapperTest, NullFramebufferGetNativeFramebufferReturnsNull) {
  const FramebufferWrapper wrapper(nullptr);
  EXPECT_EQ(wrapper.getNativeFramebuffer(), nullptr);
}

TEST(FramebufferWrapperTest, NullFramebufferGetFramebufferReturnsNull) {
  const FramebufferWrapper wrapper(nullptr);
  EXPECT_EQ(wrapper.getFramebuffer().get(), nullptr);
}

// --- Stub-framebuffer path: wrapper delegates to the underlying IFramebuffer ---

TEST(FramebufferWrapperTest, StubFramebufferGetFramebufferNotNull) {
  auto stub = std::make_shared<StubFramebuffer>();
  const FramebufferWrapper wrapper(stub);
  EXPECT_EQ(wrapper.getFramebuffer().get(), stub.get());
}

TEST(FramebufferWrapperTest, StubFramebufferGetColorAttachmentReturnsNull) {
  const FramebufferWrapper wrapper(std::make_shared<StubFramebuffer>());
  // The stub returns a null shared_ptr<ITexture>, so the wrapper's passthrough
  // yields a null IAttachmentInterop pointer.
  EXPECT_EQ(wrapper.getColorAttachment(0), nullptr);
}

TEST(FramebufferWrapperTest, StubFramebufferGetDepthAttachmentReturnsNull) {
  const FramebufferWrapper wrapper(std::make_shared<StubFramebuffer>());
  EXPECT_EQ(wrapper.getDepthAttachment(), nullptr);
}

TEST(FramebufferWrapperTest, StubFramebufferGetNativeFramebufferReturnsNull) {
  // getNativeFramebuffer() is a fixed null in the base implementation; it does
  // not consult the underlying framebuffer.
  const FramebufferWrapper wrapper(std::make_shared<StubFramebuffer>());
  EXPECT_EQ(wrapper.getNativeFramebuffer(), nullptr);
}

} // namespace igl::tests
