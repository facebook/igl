/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/MipmapGenerator.h>

#include <array>
#include <utility>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

constexpr const char* kMipmapShader = R"(
@group(0) @binding(0) var src : texture_2d<f32>;
@group(0) @binding(1) var srcSampler : sampler;

struct VertexOut {
  @builtin(position) position : vec4f,
  @location(0) uv : vec2f,
};

@vertex
fn vs(@builtin(vertex_index) i : u32) -> VertexOut {
  let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
  return VertexOut(vec4f(uv * vec2f(2.0, -2.0) + vec2f(-1.0, 1.0), 0.0, 1.0), uv);
}

@fragment
fn fs(v : VertexOut) -> @location(0) vec4f {
  return textureSampleLevel(src, srcSampler, v.uv, 0.0);
}
)";

} // namespace

WGPURenderPipeline IGL_NULLABLE MipmapGenerator::getPipeline(WGPUTextureFormat format,
                                                             bool filterable,
                                                             Result* IGL_NULLABLE outResult) {
  const auto key = std::make_tuple(format, filterable);
  if (const auto it = pipelines_.find(key); it != pipelines_.end()) {
    Result::setOk(outResult);
    return it->second.get();
  }
  const WGPUDevice device = ctx_.getDevice();
  if (!module_) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = toWGPUStringView(kMipmapShader);
    WGPUShaderModuleDescriptor moduleDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    moduleDesc.nextInChain = &wgsl.chain;
    moduleDesc.label = toWGPUStringView("igl.webgpu.mipmaps");
    module_.reset(wgpuDeviceCreateShaderModule(device, &moduleDesc));
  }
  const size_t index = filterable ? 1 : 0;
  if (!pipelineLayouts_[index]) {
    std::array<WGPUBindGroupLayoutEntry, 2> entries = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
                                                       WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Fragment;
    entries[0].texture.sampleType = filterable ? WGPUTextureSampleType_Float
                                               : WGPUTextureSampleType_UnfilterableFloat;
    entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Fragment;
    entries[1].sampler.type = filterable ? WGPUSamplerBindingType_Filtering
                                         : WGPUSamplerBindingType_NonFiltering;
    WGPUBindGroupLayoutDescriptor layoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    layoutDesc.entryCount = entries.size();
    layoutDesc.entries = entries.data();
    bindGroupLayouts_[index].reset(wgpuDeviceCreateBindGroupLayout(device, &layoutDesc));
    const WGPUBindGroupLayout groupLayout = bindGroupLayouts_[index].get();
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &groupLayout;
    pipelineLayouts_[index].reset(wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc));

    WGPUSamplerDescriptor samplerDesc = WGPU_SAMPLER_DESCRIPTOR_INIT;
    samplerDesc.magFilter = filterable ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
    samplerDesc.minFilter = samplerDesc.magFilter;
    samplers_[index].reset(wgpuDeviceCreateSampler(device, &samplerDesc));
  }

  WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
  colorTarget.format = format;
  WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
  fragment.module = module_.get();
  fragment.entryPoint = toWGPUStringView("fs");
  fragment.targetCount = 1;
  fragment.targets = &colorTarget;
  WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
  pipelineDesc.label = toWGPUStringView("igl.webgpu.mipmaps");
  pipelineDesc.layout = pipelineLayouts_[index].get();
  pipelineDesc.vertex.module = module_.get();
  pipelineDesc.vertex.entryPoint = toWGPUStringView("vs");
  pipelineDesc.fragment = &fragment;

  ctx_.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPURenderPipeline> pipeline(wgpuDeviceCreateRenderPipeline(device, &pipelineDesc));
  Result result = ctx_.popErrorScope(ErrorScopeKind::Pipeline);
  if (!result.isOk() || !pipeline) {
    Result::setResult(outResult,
                      !result.isOk() ? std::move(result)
                                     : Result(Result::Code::RuntimeError,
                                              "wgpuDeviceCreateRenderPipeline() failed"));
    return nullptr;
  }
  Result::setOk(outResult);
  return pipelines_.emplace(key, std::move(pipeline)).first->second.get();
}

Result MipmapGenerator::preparePipeline(WGPUTextureFormat format, bool filterable) {
  Result result;
  (void)getPipeline(format, filterable, &result);
  return result;
}

Result MipmapGenerator::encode(WGPUCommandEncoder IGL_NONNULL encoder, const Target& target) {
  if (target.texture == nullptr || target.numMipLevels < 2) {
    return Result();
  }
  Result result;
  WGPURenderPipeline pipeline = getPipeline(target.format, target.filterable, &result);
  if (pipeline == nullptr) {
    return result;
  }
  const size_t index = target.filterable ? 1 : 0;
  const WGPUDevice device = ctx_.getDevice();
  wgpuCommandEncoderPushDebugGroup(encoder, toWGPUStringView("igl.webgpu.mipmaps"));
  for (uint32_t layer = target.baseLayer; layer < target.baseLayer + target.numLayers; ++layer) {
    for (uint32_t level = target.baseMipLevel + 1;
         level < target.baseMipLevel + target.numMipLevels;
         ++level) {
      WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
      viewDesc.format = target.format;
      viewDesc.dimension = WGPUTextureViewDimension_2D;
      viewDesc.baseMipLevel = level - 1;
      viewDesc.mipLevelCount = 1;
      viewDesc.baseArrayLayer = layer;
      viewDesc.arrayLayerCount = 1;
      const Handle<WGPUTextureView> srcView(wgpuTextureCreateView(target.texture, &viewDesc));
      viewDesc.baseMipLevel = level;
      const Handle<WGPUTextureView> dstView(wgpuTextureCreateView(target.texture, &viewDesc));

      std::array<WGPUBindGroupEntry, 2> entries = {WGPU_BIND_GROUP_ENTRY_INIT,
                                                   WGPU_BIND_GROUP_ENTRY_INIT};
      entries[0].binding = 0;
      entries[0].textureView = srcView.get();
      entries[1].binding = 1;
      entries[1].sampler = samplers_[index].get();
      WGPUBindGroupDescriptor bindGroupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
      bindGroupDesc.layout = bindGroupLayouts_[index].get();
      bindGroupDesc.entryCount = entries.size();
      bindGroupDesc.entries = entries.data();
      const Handle<WGPUBindGroup> bindGroup(wgpuDeviceCreateBindGroup(device, &bindGroupDesc));

      WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
      color.view = dstView.get();
      color.loadOp = WGPULoadOp_Clear;
      color.storeOp = WGPUStoreOp_Store;
      WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
      passDesc.colorAttachmentCount = 1;
      passDesc.colorAttachments = &color;
      const Handle<WGPURenderPassEncoder> pass(
          wgpuCommandEncoderBeginRenderPass(encoder, &passDesc));
      wgpuRenderPassEncoderSetPipeline(pass.get(), pipeline);
      wgpuRenderPassEncoderSetBindGroup(pass.get(), 0, bindGroup.get(), 0, nullptr);
      wgpuRenderPassEncoderDraw(pass.get(), 3, 1, 0, 0);
      wgpuRenderPassEncoderEnd(pass.get());
    }
  }
  wgpuCommandEncoderPopDebugGroup(encoder);
  return Result();
}

} // namespace igl::webgpu
