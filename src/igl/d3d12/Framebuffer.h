/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Framebuffer.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Framebuffer final : public IFramebuffer {
 public:
  Framebuffer(const FramebufferDesc& desc) : desc_(desc) {}
  ~Framebuffer() override = default;

  std::vector<size_t> getColorAttachmentIndices() const override;
  std::shared_ptr<ITexture> getColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getDepthAttachment() const override;
  std::shared_ptr<ITexture> getResolveDepthAttachment() const override;
  std::shared_ptr<ITexture> getStencilAttachment() const override;
  FramebufferMode getMode() const override;
  bool isSwapchainBound() const override;

 private:
  FramebufferDesc desc_;
};

} // namespace igl::d3d12
