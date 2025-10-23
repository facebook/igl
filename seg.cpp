    IGL_LOG_INFO("bindBuffer END #%d\n", ++callCount);
  }
}
void RenderCommandEncoder::bindBindGroup(BindGroupTextureHandle /*handle*/) {}
void RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle /*handle*/,
                                          uint32_t /*numDynamicOffsets*/,
                                          const uint32_t* /*dynamicOffsets*/) {}

} // namespace igl::d3d12
/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderCommandEncoder.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/RenderPipelineState.h>
#include <igl/d3d12/Texture.h>
#include <igl/RenderPass.h>
#include <igl/d3d12/HeadlessContext.h>
#include <igl/d3d12/DescriptorHeapManager.h>
#include <igl/d3d12/SamplerState.h>

namespace igl::d3d12 {

RenderCommandEncoder::RenderCommandEncoder(CommandBuffer& commandBuffer,
                                           const RenderPassDesc& renderPass,
                                           const std::shared_ptr<IFramebuffer>& framebuffer)
    : IRenderCommandEncoder(nullptr),
      commandBuffer_(commandBuffer),
      commandList_(commandBuffer.getCommandList()),
      framebuffer_(framebuffer) {
  auto& context = commandBuffer_.getContext();

  // Set descriptor heaps for this command list
  ID3D12DescriptorHeap* heaps[] = {
    context.getCbvSrvUavHeap(),
    context.getSamplerHeap()
  };
  commandList_->SetDescriptorHeaps(2, heaps);

  // Create RTV from framebuffer if provided; otherwise fallback to swapchain RTV
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
  bool usedOffscreenRTV = false;
  // Try to get descriptor heap manager (available in headless context)
  DescriptorHeapManager* heapMgr = context.getDescriptorHeapManager();

  if (framebuffer_ && framebuffer_->getColorAttachment(0)) {
    auto colorTex = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(0));
    ID3D12Device* device = context.getDevice();
    if (device && colorTex && colorTex->getResource()) {
      if (heapMgr) {
        rtvIndex_ = heapMgr->allocateRTV();
        rtvHandle_ = heapMgr->getRTVHandle(rtvIndex_);
      } else {
        // Fallback: transient heap
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> tmpHeap;
        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
