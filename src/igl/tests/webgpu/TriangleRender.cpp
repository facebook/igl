/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/webgpu/TriangleRender.h>

#include <cstring>
#include <memory>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/ShaderCreator.h>

namespace igl::tests::webgpu {

namespace {

constexpr const char* kShader = R"(
struct VertexOut {
  @builtin(position) position : vec4f,
  @location(0) color : vec3f,
};

var<private> positions : array<vec2f, 3> =
    array<vec2f, 3>(vec2f(-0.6, -0.4), vec2f(0.6, -0.4), vec2f(0.0, 0.6));
var<private> colors : array<vec3f, 3> =
    array<vec3f, 3>(vec3f(1.0, 0.0, 0.0), vec3f(0.0, 1.0, 0.0), vec3f(0.0, 0.0, 1.0));

@vertex
fn vertexMain(@builtin(vertex_index) i : u32) -> VertexOut {
  return VertexOut(vec4f(positions[i], 0.0, 1.0), colors[i]);
}

@fragment
fn fragmentMain(v : VertexOut) -> @location(0) vec4f {
  return vec4f(v.color, 1.0);
}
)";

Result checkCreated(const Result& ret, bool created) {
  if (!ret.isOk() || created) {
    return ret;
  }
  return Result(Result::Code::RuntimeError, "An IGL factory returned null without an error");
}

} // namespace

Result TriangleRenderer::initialize(IDevice& device, TextureFormat format) {
  Result ret;
  device_ = &device;
  queue_ = device.createCommandQueue({}, &ret);
  ret = checkCreated(ret, queue_ != nullptr);
  if (!ret.isOk()) {
    return ret;
  }
  std::unique_ptr<IShaderStages> stages = ShaderStagesCreator::fromLibraryStringInput(
      device, kShader, "vertexMain", "fragmentMain", "triangle", &ret);
  ret = checkCreated(ret, stages != nullptr);
  if (!ret.isOk()) {
    return ret;
  }
  pipeline_ =
      device.createRenderPipeline({.shaderStages = std::move(stages),
                                   .targetDesc = {.colorAttachments = {{.textureFormat = format}}}},
                                  &ret);
  return checkCreated(ret, pipeline_ != nullptr);
}

Result TriangleRenderer::render(const std::shared_ptr<ITexture>& target) {
  Result ret;
  const std::shared_ptr<IFramebuffer> framebuffer =
      device_->createFramebuffer({.colorAttachments = {{.texture = target}}}, &ret);
  ret = checkCreated(ret, framebuffer != nullptr);
  if (!ret.isOk()) {
    return ret;
  }
  const std::shared_ptr<ICommandBuffer> cmdBuffer = queue_->createCommandBuffer({}, &ret);
  ret = checkCreated(ret, cmdBuffer != nullptr);
  if (!ret.isOk()) {
    return ret;
  }
  const std::unique_ptr<IRenderCommandEncoder> encoder = cmdBuffer->createRenderCommandEncoder(
      {.colorAttachments = {{.loadAction = LoadAction::Clear,
                             .storeAction = StoreAction::Store,
                             .clearColor = {0.2f, 0.3f, 0.4f, 1.0f}}}},
      framebuffer,
      {},
      &ret);
  ret = checkCreated(ret, encoder != nullptr);
  if (!ret.isOk()) {
    return ret;
  }
  encoder->bindRenderPipelineState(pipeline_);
  encoder->draw(3);
  encoder->endEncoding();
  cmdBuffer->present(target);
  queue_->submit(*cmdBuffer);
  return Result();
}

Result renderTriangle(IDevice& device, uint32_t size, std::vector<uint8_t>& outRgba) {
  TriangleRenderer renderer;
  Result ret = renderer.initialize(device, TextureFormat::RGBA_UNorm8);
  if (!ret.isOk()) {
    return ret;
  }
  const std::shared_ptr<ITexture> target = device.createTexture(
      TextureDesc::new2D(
          TextureFormat::RGBA_UNorm8, size, size, TextureDesc::TextureUsageBits::Attachment),
      &ret);
  ret = checkCreated(ret, target != nullptr);
  if (!ret.isOk()) {
    return ret;
  }
  ret = renderer.render(target);
  if (!ret.isOk()) {
    return ret;
  }
  const std::shared_ptr<IFramebuffer> framebuffer =
      device.createFramebuffer({.colorAttachments = {{.texture = target}}}, &ret);
  ret = checkCreated(ret, framebuffer != nullptr);
  if (!ret.isOk()) {
    return ret;
  }

  // IGL readbacks store rows bottom-up.
  const size_t rowBytes = size_t{size} * 4;
  std::vector<uint8_t> bottomUp(rowBytes * size);
  framebuffer->copyBytesColorAttachment(
      renderer.getQueue(), 0, bottomUp.data(), TextureRangeDesc::new2D(0, 0, size, size));
  outRgba.resize(bottomUp.size());
  for (uint32_t row = 0; row < size; ++row) {
    std::memcpy(
        outRgba.data() + rowBytes * row, bottomUp.data() + rowBytes * (size - 1 - row), rowBytes);
  }
  return Result();
}

} // namespace igl::tests::webgpu
