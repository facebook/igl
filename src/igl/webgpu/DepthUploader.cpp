/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/DepthUploader.h>

#include <array>
#include <utility>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

constexpr const char* kDepthUploadShader = R"(
struct Origin { xy : vec2u, };
@group(0) @binding(0) var depths : texture_2d<f32>;
@group(0) @binding(1) var<uniform> origin : Origin;

@vertex
fn vs(@builtin(vertex_index) i : u32) -> @builtin(position) vec4f {
  let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
  return vec4f(uv * 2.0 - 1.0, 0.0, 1.0);
}

@fragment
fn fs(@builtin(position) position : vec4f) -> @builtin(frag_depth) f32 {
  return textureLoad(depths, vec2u(position.xy) - origin.xy, 0).r;
}
)";

bool hasStencil(WGPUTextureFormat format) {
  return format == WGPUTextureFormat_Depth24PlusStencil8 ||
         format == WGPUTextureFormat_Depth32FloatStencil8;
}

} // namespace

WGPURenderPipeline IGL_NULLABLE DepthUploader::getPipeline(WGPUTextureFormat format,
                                                           Result* IGL_NULLABLE outResult) {
  if (const auto it = pipelines_.find(format); it != pipelines_.end()) {
    Result::setOk(outResult);
    return it->second.get();
  }
  const WGPUDevice device = ctx_.getDevice();
  if (!module_) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = toWGPUStringView(kDepthUploadShader);
    WGPUShaderModuleDescriptor moduleDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    moduleDesc.nextInChain = &wgsl.chain;
    moduleDesc.label = toWGPUStringView("igl.webgpu.depthUpload");
    module_.reset(wgpuDeviceCreateShaderModule(device, &moduleDesc));

    std::array<WGPUBindGroupLayoutEntry, 2> entries = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
                                                       WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Fragment;
    entries[0].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Fragment;
    entries[1].buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutDescriptor layoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    layoutDesc.entryCount = entries.size();
    layoutDesc.entries = entries.data();
    bindGroupLayout_.reset(wgpuDeviceCreateBindGroupLayout(device, &layoutDesc));
    const WGPUBindGroupLayout groupLayout = bindGroupLayout_.get();
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &groupLayout;
    pipelineLayout_.reset(wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc));
  }

  WGPUDepthStencilState depthStencil = WGPU_DEPTH_STENCIL_STATE_INIT;
  depthStencil.format = format;
  depthStencil.depthWriteEnabled = WGPUOptionalBool_True;
  depthStencil.depthCompare = WGPUCompareFunction_Always;
  WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
  fragment.module = module_.get();
  fragment.entryPoint = toWGPUStringView("fs");
  WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
  pipelineDesc.label = toWGPUStringView("igl.webgpu.depthUpload");
  pipelineDesc.layout = pipelineLayout_.get();
  pipelineDesc.vertex.module = module_.get();
  pipelineDesc.vertex.entryPoint = toWGPUStringView("vs");
  pipelineDesc.depthStencil = &depthStencil;
  pipelineDesc.fragment = &fragment;

  ctx_.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPURenderPipeline> pipeline(wgpuDeviceCreateRenderPipeline(device, &pipelineDesc));
  Result result = ctx_.popErrorScope(ErrorScopeKind::Pipeline);
  if (!result.isOk() || !pipeline) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setOk(outResult);
  return pipelines_.emplace(format, std::move(pipeline)).first->second.get();
}

Result DepthUploader::upload(WGPUTexture IGL_NONNULL texture,
                             WGPUTextureFormat format,
                             const Region& region,
                             const float* IGL_NONNULL depths) {
  if (region.width == 0 || region.height == 0) {
    return Result();
  }
  Result result;
  WGPURenderPipeline pipeline = getPipeline(format, &result);
  if (pipeline == nullptr) {
    return result;
  }
  const WGPUDevice device = ctx_.getDevice();
  ctx_.pushErrorScope(WGPUErrorFilter_Validation);

  WGPUTextureDescriptor stagingDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
  stagingDesc.label = toWGPUStringView("igl.webgpu.depthUpload");
  stagingDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  stagingDesc.size = {.width = region.width, .height = region.height, .depthOrArrayLayers = 1};
  stagingDesc.format = WGPUTextureFormat_R32Float;
  const Handle<WGPUTexture> staging(wgpuDeviceCreateTexture(device, &stagingDesc));
  const WGPUTexelCopyTextureInfo destination = {
      .texture = staging.get(),
      .mipLevel = 0,
      .origin = {.x = 0, .y = 0, .z = 0},
      .aspect = WGPUTextureAspect_All,
  };
  const WGPUTexelCopyBufferLayout layout = {
      .offset = 0,
      .bytesPerRow = region.width * 4,
      .rowsPerImage = region.height,
  };
  wgpuQueueWriteTexture(ctx_.getQueue(),
                        &destination,
                        depths,
                        size_t{region.width} * region.height * 4,
                        &layout,
                        &stagingDesc.size);

  const std::array<uint32_t, 4> origin = {region.x, region.y, 0, 0};
  const WGPUBufferDescriptor originDesc = {
      .nextInChain = nullptr,
      .label = toWGPUStringView("igl.webgpu.depthUpload"),
      .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
      .size = sizeof(origin),
      .mappedAtCreation = 0,
  };
  const Handle<WGPUBuffer> originBuffer(wgpuDeviceCreateBuffer(device, &originDesc));
  wgpuQueueWriteBuffer(ctx_.getQueue(), originBuffer.get(), 0, origin.data(), sizeof(origin));

  WGPUTextureViewDescriptor stagingViewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  const Handle<WGPUTextureView> stagingView(wgpuTextureCreateView(staging.get(), &stagingViewDesc));
  std::array<WGPUBindGroupEntry, 2> entries = {WGPU_BIND_GROUP_ENTRY_INIT,
                                               WGPU_BIND_GROUP_ENTRY_INIT};
  entries[0].binding = 0;
  entries[0].textureView = stagingView.get();
  entries[1].binding = 1;
  entries[1].buffer = originBuffer.get();
  entries[1].size = sizeof(origin);
  WGPUBindGroupDescriptor bindGroupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
  bindGroupDesc.layout = bindGroupLayout_.get();
  bindGroupDesc.entryCount = entries.size();
  bindGroupDesc.entries = entries.data();
  const Handle<WGPUBindGroup> bindGroup(wgpuDeviceCreateBindGroup(device, &bindGroupDesc));

  WGPUTextureViewDescriptor targetViewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  targetViewDesc.format = format;
  targetViewDesc.dimension = WGPUTextureViewDimension_2D;
  targetViewDesc.baseMipLevel = region.mipLevel;
  targetViewDesc.mipLevelCount = 1;
  targetViewDesc.baseArrayLayer = region.layer;
  targetViewDesc.arrayLayerCount = 1;
  const Handle<WGPUTextureView> targetView(wgpuTextureCreateView(texture, &targetViewDesc));

  WGPURenderPassDepthStencilAttachment depthAttachment =
      WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
  depthAttachment.view = targetView.get();
  depthAttachment.depthLoadOp = WGPULoadOp_Load;
  depthAttachment.depthStoreOp = WGPUStoreOp_Store;
  if (hasStencil(format)) {
    depthAttachment.stencilLoadOp = WGPULoadOp_Load;
    depthAttachment.stencilStoreOp = WGPUStoreOp_Store;
  }
  WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
  passDesc.depthStencilAttachment = &depthAttachment;

  const Handle<WGPUCommandEncoder> encoder(wgpuDeviceCreateCommandEncoder(device, nullptr));
  const Handle<WGPURenderPassEncoder> pass(
      wgpuCommandEncoderBeginRenderPass(encoder.get(), &passDesc));
  wgpuRenderPassEncoderSetViewport(pass.get(),
                                   static_cast<float>(region.x),
                                   static_cast<float>(region.y),
                                   static_cast<float>(region.width),
                                   static_cast<float>(region.height),
                                   0.0f,
                                   1.0f);
  wgpuRenderPassEncoderSetScissorRect(pass.get(), region.x, region.y, region.width, region.height);
  wgpuRenderPassEncoderSetPipeline(pass.get(), pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass.get(), 0, bindGroup.get(), 0, nullptr);
  wgpuRenderPassEncoderDraw(pass.get(), 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(pass.get());
  const Handle<WGPUCommandBuffer> commands(wgpuCommandEncoderFinish(encoder.get(), nullptr));
  const WGPUCommandBuffer rawCommands = commands.get();
  wgpuQueueSubmit(ctx_.getQueue(), 1, &rawCommands);
  return ctx_.popErrorScope();
}

} // namespace igl::webgpu
