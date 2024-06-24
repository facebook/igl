/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#pragma once

#include <IGLU/simple_renderer/Drawable.h>
#include <igl/IGL.h>
#include <memory>
#include <string>
#include <vector>

namespace iglu::renderpass {

/// A simple "render pass" abstraction that hides low level graphics API details
/// like command queue, command encoder, render pipeline state and presentation.
///
/// An application frame will always have one render pass targeting the "onscreen"
/// framebuffer, but it can have multiple intermediate offscreen render passes.
class ForwardRenderPass final {
 public:
  /// Call before any graphics bind/draw calls to this render pass.
  void begin(std::shared_ptr<igl::IFramebuffer> target,
             const igl::RenderPassDesc* renderPassDescOverride = nullptr);

  /// Call once per drawable.
  void draw(drawable::Drawable& drawable, igl::IDevice& device) const;

  /// Call after all drawing within this render pass is finished. The 'present'
  /// parameter controls whether to present the target framebuffer and must be set
  /// to true exactly once per frame, when targeting the "onscreen" framebuffer.
  void end(bool present = false);

  /// Optional. By default, a viewport matching the size of the target framebuffer
  /// will be used.
  ///
  /// [Convention] Framebuffer origin is bottom left corner.
  void bindViewport(const igl::Viewport& viewport, const igl::Size& surfaceSize);

  //// The render pass is considered active when in between begin() and end() calls.
  bool isActive() const;
  igl::IFramebuffer& activeTarget();
  igl::IRenderCommandEncoder& activeCommandEncoder();

  explicit ForwardRenderPass(igl::IDevice& device);
  ~ForwardRenderPass() = default;

 private:
  igl::BackendType _backendType;

  std::shared_ptr<igl::ICommandQueue> _commandQueue;
  std::shared_ptr<igl::IFramebuffer> _framebuffer;
  igl::RenderPipelineDesc _renderPipelineDesc;

  std::shared_ptr<igl::ICommandBuffer> _commandBuffer;
  std::unique_ptr<igl::IRenderCommandEncoder> _commandEncoder;
};

} // namespace iglu::renderpass
