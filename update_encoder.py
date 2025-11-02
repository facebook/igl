#!/usr/bin/env python3
import re

# Read the file
with open('src/igl/d3d12/RenderCommandEncoder.cpp', 'r') as f:
    content = f.read()

# Replace CBV bindings: b0 moves from param 0 to param 1, b1 moves from param 1 to param 2
content = content.replace(
    'SetGraphicsRootConstantBufferView(0, cachedConstantBuffers_[0])',
    'SetGraphicsRootConstantBufferView(1, cachedConstantBuffers_[0])'
)
content = content.replace(
    'SetGraphicsRootConstantBufferView(1, cachedConstantBuffers_[1])',
    'SetGraphicsRootConstantBufferView(2, cachedConstantBuffers_[1])'
)

# Replace descriptor table bindings for textures: param 2 -> param 3
content = content.replace(
    'SetGraphicsRootDescriptorTable(2, cachedTextureGpuHandles_[0])',
    'SetGraphicsRootDescriptorTable(3, cachedTextureGpuHandles_[0])'
)

# Replace descriptor table bindings for samplers: param 3 -> param 4
content = content.replace(
    'SetGraphicsRootDescriptorTable(3, cachedSamplerGpuHandles_[0])',
    'SetGraphicsRootDescriptorTable(4, cachedSamplerGpuHandles_[0])'
)

# Update bindPushConstants implementation
old_bind = '''void RenderCommandEncoder::bindPushConstants(const void* /*data*/,
                                             size_t /*length*/,
                                             size_t /*offset*/) {
  // Push constants not yet implemented for D3D12
  // Requires proper root signature design and backward compatibility considerations
}'''

new_bind = '''void RenderCommandEncoder::bindPushConstants(const void* data,
                                             size_t length,
                                             size_t offset) {
  if (!commandList_ || !data || length == 0 || length > 64 || offset != 0) {
    IGL_LOG_ERROR("bindPushConstants: Invalid parameters (list=%p, data=%p, len=%zu, offset=%zu)\\n",
                  commandList_, data, length, offset);
    return;
  }

  // Convert byte length to DWORD count
  const UINT num32BitValues = static_cast<UINT>(length / 4);

  // Set root constants at parameter 0 (mapped to shader register b2)
  // Root signature layout: param 0 = push constants (b2), param 1 = b0, param 2 = b1, param 3 = SRVs, param 4 = samplers
  commandList_->SetGraphicsRoot32BitConstants(0, num32BitValues, data, 0);

  IGL_LOG_INFO("bindPushConstants: Set %u DWORDs at root parameter 0\\n", num32BitValues);
}'''

content = content.replace(old_bind, new_bind)

# Write back
with open('src/igl/d3d12/RenderCommandEncoder.cpp', 'w') as f:
    f.write(content)

print("RenderCommandEncoder.cpp updated successfully!")
print("Changes:")
print("- CBV b0: param 0 -> param 1")
print("- CBV b1: param 1 -> param 2")
print("- SRVs (textures): param 2 -> param 3")
print("- Samplers: param 3 -> param 4")
print("- Push constants: implemented at param 0")
