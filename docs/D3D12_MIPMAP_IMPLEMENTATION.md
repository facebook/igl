# D3D12 Automatic Mipmap Generation Implementation

## Overview

This document describes the implementation of automatic mipmap generation for the D3D12 backend in IGL.

**Status:** ✅ Implemented (2025-10-26)
**Approach:** Graphics Blit (Fullscreen Triangle Rendering)
**Files:** `src/igl/d3d12/Texture.cpp` (lines 426-621)

## Implementation Approach

### Option A: Compute Shader (Future)
- Higher quality (custom filtering algorithms)
- Requires compute pipeline support
- More flexible (can implement advanced filters like Kaiser, Lanczos, etc.)
- **Status:** Not implemented (requires compute pipeline infrastructure)

### Option B: Graphics Blit (Current Implementation)
- Uses fullscreen triangle rendering
- Bilinear filtering via linear sampler
- Simpler to implement
- Lower quality than custom compute filters but sufficient for most use cases
- **Status:** ✅ Implemented

## Technical Details

### Method Signatures

```cpp
void Texture::generateMipmap(ICommandQueue& cmdQueue,
                             const TextureRangeDesc* range = nullptr) const;

void Texture::generateMipmap(ICommandBuffer& cmdBuffer,
                             const TextureRangeDesc* range = nullptr) const;
```

Both methods have identical implementations but differ in their command submission:
- `ICommandQueue` version: Creates own command allocator/list, submits immediately with fence wait
- `ICommandBuffer` version: Uses existing command buffer (not fully wired up yet)

### Algorithm

For a texture with N mip levels:

1. **Validate Prerequisites:**
   - Check texture has 2+ mip levels
   - Check texture is 2D (3D/Cube not supported yet)
   - Check texture has `D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET` flag
   - If validation fails, log informative message and return

2. **Create Pipeline Resources (once per call):**
   - Root signature with 2 descriptor tables (SRV + Sampler)
   - Vertex shader (fullscreen triangle generator)
   - Pixel shader (texture sampling)
   - Graphics pipeline state object
   - Descriptor heaps (SRV and Sampler)
   - Linear sampler (bilinear filtering)

3. **Generate Mipmaps (for each mip level):**
   ```
   For mip = 0 to N-2:
     a. Ensure mip level N is in PIXEL_SHADER_RESOURCE state
     b. Create SRV for mip level N
     c. Create RTV for mip level N+1
     d. Transition mip level N+1 to RENDER_TARGET state
     e. Set render target to mip level N+1
     f. Set viewport/scissor to mip N+1 dimensions
     g. Draw fullscreen triangle (samples from mip N with bilinear filter)
     h. Transition mip level N+1 to PIXEL_SHADER_RESOURCE state
   ```

4. **Cleanup:**
   - Close command list
   - Execute commands
   - Wait for GPU completion (fence)

### Shaders

**Vertex Shader (Fullscreen Triangle):**
```hlsl
struct VSOut {
  float4 pos: SV_POSITION;
  float2 uv: TEXCOORD0;
};

VSOut main(uint id: SV_VertexID) {
  float2 p = float2((id << 1) & 2, id & 2);
  VSOut o;
  o.pos = float4(p*float2(2,-2)+float2(-1,1), 0, 1);
  o.uv = p;
  return o;
}
```

**Pixel Shader (Bilinear Sample):**
```hlsl
Texture2D tex0 : register(t0);
SamplerState smp : register(s0);

float4 main(float4 pos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET {
  return tex0.SampleLevel(smp, uv, 0);
}
```

### State Tracking Integration

The implementation uses `Texture::transitionTo()` instead of manual barrier creation. This ensures:
- Correct tracking of per-subresource states
- No state mismatch errors
- Proper integration with existing texture state management
- Automatic tracking across multiple operations

**Before (Buggy):**
```cpp
D3D12_RESOURCE_BARRIER barrier = {};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; // Assumed!
barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
// ...
list->ResourceBarrier(1, &barrier);
```

**After (Fixed):**
```cpp
transitionTo(list.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, mipLevel, layer);
// State tracking automatically updated
```

## Usage Requirements

### Texture Creation

Textures must be created with the **Attachment** usage bit to enable mipmap generation:

```cpp
TextureDesc desc;
desc.width = 1024;
desc.height = 1024;
desc.numMipLevels = 11;  // log2(1024) + 1
desc.usage = TextureDesc::TextureUsageBits::Sampled |
             TextureDesc::TextureUsageBits::Attachment;  // Required!
desc.format = TextureFormat::RGBA_UNorm8;

auto texture = device->createTexture(desc);
```

### Mipmap Generation

```cpp
// Upload mip level 0
texture->upload(range, data);

// Generate remaining mip levels
texture->generateMipmap(commandQueue);
```

### Validation Messages

If texture doesn't have RENDER_TARGET flag:
```
Texture::generateMipmap() - Skipping: texture not created with RENDER_TARGET usage
  To enable mipmap generation, create texture with TextureDesc::TextureUsageBits::Attachment
```

## Limitations

### Current Limitations

1. **2D Textures Only**
   - 3D textures not supported (requires volumetric blitting)
   - Cube textures not supported (requires face iteration)
   - Array textures not tested (should work but needs validation)

2. **Requires RENDER_TARGET Usage**
   - Texture must have `TextureDesc::TextureUsageBits::Attachment`
   - This sets `D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET`
   - Textures created without this flag will skip mipmap generation with warning

3. **Performance**
   - Creates pipeline resources on each call (not cached)
   - Synchronous execution (CPU wait on fence)
   - Could be optimized with resource caching and async execution

4. **Filter Quality**
   - Uses bilinear filtering (box filter equivalent)
   - Lower quality than custom compute shader approaches
   - No support for advanced filters (Kaiser, Lanczos, etc.)

### Comparison with Other Backends

**Vulkan (`src/igl/vulkan/Texture.cpp`):**
- Uses `vkCmdBlitImage` with `VK_FILTER_LINEAR`
- Hardware-accelerated blit operation
- Checks format support for blit operations
- Similar approach to D3D12 implementation

**Metal (`src/igl/metal/Texture.mm`):**
- Can use `MPSImageGaussianBlur` for high quality
- Can use texture blit for performance
- Hardware-accelerated like Vulkan

**D3D12 (this implementation):**
- Uses graphics pipeline for blitting
- Similar to Vulkan's approach but more manual
- Room for optimization with compute shaders

## Future Improvements

### Short Term

1. **Resource Caching**
   - Cache pipeline state, root signature, descriptor heaps
   - Reuse across multiple calls
   - Store in `Device` or `Texture` class

2. **Async Execution**
   - Remove fence wait
   - Let command queue handle synchronization
   - Better pipelining with other GPU work

3. **Array/Cube Support**
   - Add layer/face iteration
   - Test with texture arrays
   - Validate with cube maps

### Long Term

1. **Compute Shader Implementation**
   - Better quality (custom filters)
   - Potential performance improvement
   - More flexible algorithms
   - Requires compute pipeline infrastructure

2. **Format-Specific Optimization**
   - sRGB-aware filtering
   - HDR format support
   - Compressed format support

3. **Quality Options**
   - Let user choose filter quality
   - Box, Linear, Kaiser, Lanczos options
   - Performance vs quality tradeoff

## Testing

### Manual Testing

```bash
# Build the test session
cmake --build build --config Debug --target BindGroupSession_d3d12 -j 8

# Run with mipmap generation
./build/shell/Debug/BindGroupSession_d3d12.exe --viewport-size 640x360

# Check for log messages:
# "Texture::generateMipmap(cmdQueue) - START: numMips=11"
# "Texture::generateMipmap() - Proceeding with mipmap generation"
```

### Expected Behavior

**With RENDER_TARGET usage:**
```
Texture::generateMipmap(cmdQueue) - START: numMips=11
Texture::generateMipmap() - Proceeding with mipmap generation
```

**Without RENDER_TARGET usage:**
```
Texture::generateMipmap(cmdQueue) - START: numMips=11
Texture::generateMipmap() - Skipping: texture not created with RENDER_TARGET usage
  To enable mipmap generation, create texture with TextureDesc::TextureUsageBits::Attachment
```

### Visual Testing

- Textures should have smooth LOD transitions
- No blockiness or aliasing at distance
- No black/white mip levels
- Proper filtering across mip boundaries

## References

### Related Files
- **Implementation:** `src/igl/d3d12/Texture.cpp` (lines 426-621)
- **Header:** `src/igl/d3d12/Texture.h`
- **Vulkan Reference:** `src/igl/vulkan/VulkanImage.cpp` (line 1104)
- **Documentation:** `docs/D3D12_REQUIRED_FEATURES.md`

### Microsoft Documentation
- [ID3D12GraphicsCommandList::DrawInstanced](https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-drawinstanced)
- [Resource Barriers](https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)
- [Descriptor Heaps](https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)
- [Root Signatures](https://docs.microsoft.com/en-us/windows/win32/direct3d12/root-signatures)

### IGL Resources
- **IGL Texture Interface:** `src/igl/Texture.h`
- **Texture Format Properties:** `src/igl/TextureFormat.h`
- **Command Queue:** `src/igl/CommandQueue.h`

## Conclusion

The D3D12 mipmap generation implementation provides a functional solution using the graphics blit approach. While it has limitations compared to compute shader approaches, it successfully generates mipmaps for 2D textures created with the Attachment usage flag. The implementation integrates well with IGL's existing state tracking system and provides clear validation messages for unsupported cases.

Future improvements can focus on performance optimization (resource caching, async execution) and quality improvements (compute shader implementation, advanced filtering).
