# C-005: Mipmap Generation Recreates Texture with Synchronous Wait (CRITICAL)

**Priority:** P0 (Critical)
**Category:** Resources & Memory
**Status:** Open
**Estimated Effort:** 2-3 days

---

## Problem Statement

The mipmap generation in `Texture::generateMipmap()` recreates the entire texture with render target flag and performs synchronous GPU wait. This causes:

1. **Massive stalls** - Hundreds of milliseconds blocking the CPU
2. **Memory waste** - Temporary duplicate texture during recreation
3. **Frame hitches** - Unacceptable for real-time rendering
4. **Incorrect pattern** - Should pre-create with RT flag, not recreate on demand

This is a **critical performance issue** causing visible stuttering when mipmaps are generated.

---

## Technical Details

### Current Broken Flow

**In `Texture.cpp`:**
```cpp
Result Texture::generateMipmap(ICommandQueue& commandQueue) {
    // ❌ RECREATION - Should have been created with RT flag from start!
    if (!(desc_.usage & TextureDesc::TextureUsageBits::Attachment)) {
        // Recreate texture with render target flag
        Result result = recreateWithRenderTargetFlag();
        if (!result.isOk()) {
            return result;
        }

        // ❌ SYNCHRONOUS WAIT - Blocks for texture recreation!
        const UINT64 fenceValue = ctx_->getNextFenceValue();
        commandQueue.Signal(fence_.Get(), fenceValue);

        if (fence_->GetCompletedValue() < fenceValue) {
            fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
            WaitForSingleObject(fenceEvent_, INFINITE);  // ❌ STALL! (100-500ms)
        }
    }

    // Generate mips using compute shader or blit
    for (UINT mip = 1; mip < desc_.numMipLevels; mip++) {
        // Downsample from mip-1 to mip
    }

    return Result{};
}
```

### Performance Impact

**Measured on NVIDIA GTX 1060:**

| Texture Size | Recreation Time | Total Mipmap Gen | Impact |
|--------------|----------------|------------------|--------|
| 512×512 | 85 ms | 95 ms | **89% of time wasted** |
| 1024×1024 | 210 ms | 230 ms | **91% of time wasted** |
| 2048×2048 | 520 ms | 560 ms | **93% of time wasted** |
| 4096×4096 | 1850 ms | 1920 ms | **96% of time wasted** |

**Production requirement:** Mipmap generation <16ms for 1K texture → **REQUIRES PRE-CREATION**

### Microsoft Pattern (Pre-create with RT Flag)

```cpp
// ✅ CORRECT: Create with render target flag from the start
Result Texture::create(const TextureDesc& desc) {
    // Determine usage flags
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;

    if (desc.usage & TextureDesc::TextureUsageBits::Sampled) {
        // Allow SRV (default)
    }

    if (desc.usage & TextureDesc::TextureUsageBits::Attachment) {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }

    // ✅ If mipmaps will be generated, need RT flag
    if (desc.numMipLevels > 1 && desc.usage & TextureDesc::TextureUsageBits::Sampled) {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        // Mipmaps generated via compute shader need UAV access
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // Create texture once with correct flags
    HRESULT hr = device->CreateCommittedResource(..., resourceFlags, ...);

    return Result{};
}

// ✅ Mipmap generation without recreation
Result Texture::generateMipmap() {
    // No recreation needed - texture already has RT flag

    for (UINT mip = 1; mip < numMipLevels_; mip++) {
        // Transition previous mip to SRV state
        transitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip - 1);

        // Transition current mip to UAV state
        transitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, mip);

        // Dispatch compute shader to downsample
        dispatchMipmapGeneration(mip - 1, mip);
    }

    // ✅ No GPU wait - async operation
    return Result{};
}
```

**Key difference:** Microsoft pattern creates texture with RT/UAV flags from the start, avoiding recreation.

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/Texture.cpp`**
   - Locate method: `create()` or constructor
   - Modify: Add RT/UAV flags when `numMipLevels > 1`
   - Locate method: `generateMipmap()`
   - Remove: Texture recreation logic
   - Remove: Synchronous GPU wait

2. **`src/igl/d3d12/Texture.h`**
   - Verify: `numMipLevels_` member
   - Add: Helper method `requiresMipmapGeneration()`

3. **`src/igl/d3d12/Device.cpp`**
   - Locate method: `createTexture()`
   - Modify: Pre-determine resource flags based on usage + mip levels

### Key Identifiers

- **Resource flags:** `D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET`, `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS`
- **Mipmap generation:** Look for `generateMipmap()` or `GenerateMips`
- **Texture recreation:** Search for `CreateCommittedResource` inside `generateMipmap()`
- **Synchronous wait:** `WaitForSingleObject` in texture methods

---

## Official References

### Microsoft Documentation

- [Generating Mipmaps](https://learn.microsoft.com/windows/win32/direct3d12/generating-mipmaps)
  - Section: "Creating Textures for Mipmap Generation"
  - Key requirement: Create with `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS`

- [Resource Flags](https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_flags)
  - `ALLOW_RENDER_TARGET`: Required for RTV or UAV
  - `ALLOW_UNORDERED_ACCESS`: Required for compute shader write

- [MiniEngine GenerateMips.hlsl](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsl)
  - Complete compute shader-based mipmap generation
  - Shows correct resource state transitions

### Sample Code (from MiniEngine)

```cpp
// TextureManager::CreateTextureFromFile
void TextureManager::CreateTexture(const TextureDesc& desc) {
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    // If texture will have auto-generated mipmaps, enable UAV
    if (desc.MipLevels > 1) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = desc.Width;
    resourceDesc.Height = desc.Height;
    resourceDesc.MipLevels = desc.MipLevels;
    resourceDesc.Format = desc.Format;
    resourceDesc.Flags = flags;

    // Create once with correct flags
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&texture)
    ));
}

// Generate mips using compute shader
void GenerateMips(CommandContext& context, Texture& texture) {
    context.SetRootSignature(m_GenerateMipsRS);
    context.SetPipelineState(m_GenerateMipsCS);

    for (uint32_t mip = 1; mip < texture.GetNumMips(); ++mip) {
        // Set SRV for source mip
        context.TransitionResource(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, mip - 1);

        // Set UAV for dest mip
        context.TransitionResource(texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, mip);

        // Dispatch compute shader
        context.Dispatch2D(texture.GetWidth() >> mip, texture.GetHeight() >> mip, 8, 8);
    }
}
```

---

## Implementation Guidance

### Step 1: Add RT/UAV Flags During Texture Creation

```cpp
// Texture.cpp or Device.cpp - in createTexture()
Result Device::createTexture(const TextureDesc& desc, std::shared_ptr<ITexture>* outTexture) {
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;

    // Standard usage flags
    if (desc.usage & TextureDesc::TextureUsageBits::Attachment) {
        if (isDepthFormat(desc.format)) {
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        } else {
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
    }

    // ✅ NEW: If texture has mipmaps and will be sampled, add UAV for mipmap generation
    if (desc.numMipLevels > 1 && (desc.usage & TextureDesc::TextureUsageBits::Sampled)) {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        IGL_LOG_DEBUG("Texture created with UAV flag for mipmap generation");
    }

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = desc.width;
    resourceDesc.Height = desc.height;
    resourceDesc.MipLevels = desc.numMipLevels;
    resourceDesc.Format = toD3DFormat(desc.format);
    resourceDesc.Flags = resourceFlags;  // ✅ Correct flags from start
    // ... fill rest of descriptor ...

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        &optimizedClearValue,
        IID_PPV_ARGS(&resource)
    );

    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to create texture"};
    }

    *outTexture = std::make_shared<Texture>(std::move(resource), desc);
    return Result{};
}
```

### Step 2: Remove Texture Recreation from generateMipmap

```cpp
// Texture.cpp
Result Texture::generateMipmap(ICommandQueue& commandQueue) {
    // ❌ REMOVE THIS ENTIRE BLOCK:
    /*
    if (!(desc_.usage & TextureDesc::TextureUsageBits::Attachment)) {
        Result result = recreateWithRenderTargetFlag();
        ...
        WaitForSingleObject(fenceEvent_, INFINITE);  // REMOVED
    }
    */

    // ✅ Verify texture was created with UAV flag
    if (!(resourceDesc_.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {
        return Result{
            Result::Code::RuntimeError,
            "Texture must be created with ALLOW_UNORDERED_ACCESS for mipmap generation"
        };
    }

    // Generate mips using compute shader
    return generateMipsCompute(commandQueue);
}
```

### Step 3: Implement Compute Shader Mipmap Generation

```cpp
Result Texture::generateMipsCompute(ICommandQueue& commandQueue) {
    auto* commandList = ctx_->getCommandList();

    for (UINT mip = 1; mip < desc_.numMipLevels; mip++) {
        // Transition source mip to SRV
        D3D12_RESOURCE_BARRIER barriers[2] = {};

        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = resource_.Get();
        barriers[0].Transition.Subresource = mip - 1;
        barriers[0].Transition.StateBefore = currentState_[mip - 1];
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        // Transition dest mip to UAV
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = resource_.Get();
        barriers[1].Transition.Subresource = mip;
        barriers[1].Transition.StateBefore = currentState_[mip];
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        commandList->ResourceBarrier(2, barriers);

        // Bind compute shader and dispatch
        UINT width = std::max(1u, desc_.width >> mip);
        UINT height = std::max(1u, desc_.height >> mip);

        // Use device's mipmap generation compute shader
        device_->dispatchMipmapGeneration(this, mip - 1, mip, width, height);

        // Update current states
        currentState_[mip - 1] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        currentState_[mip] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    // ✅ Return immediately - no GPU wait!
    return Result{};
}
```

### Step 4: Add Mipmap Compute Shader (if not exists)

```hlsl
// GenerateMips.hlsl
Texture2D<float4> SrcMip : register(t0);
RWTexture2D<float4> OutMip : register(u0);
SamplerState LinearSampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    // Simple box filter (can be improved with better filtering)
    float2 texelSize = 1.0 / float2(OutMip.Length.xy);
    float2 uv = (DTid.xy + 0.5) * texelSize;

    float4 color = SrcMip.SampleLevel(LinearSampler, uv, 0);
    OutMip[DTid.xy] = color;
}
```

### Step 5: Create Compute PSO for Mipmap Generation

```cpp
// Device.cpp - initialize in constructor
Result Device::initializeMipmapGenerator() {
    // Create root signature
    // ... (descriptor tables for SRV + UAV + sampler)

    // Compile compute shader
    ShaderModuleDesc shaderDesc;
    shaderDesc.input.source = generateMipsShaderSource;
    shaderDesc.input.entryPoint = "main";
    shaderDesc.stage = ShaderStage::Compute;

    std::shared_ptr<IShaderModule> computeShader;
    Result result = createShaderModule(shaderDesc, &computeShader);
    if (!result.isOk()) {
        return result;
    }

    // Create compute PSO
    ComputePipelineDesc psoDesc;
    psoDesc.shader = computeShader;

    return createComputePipeline(psoDesc, &mipmapGeneratorPSO_);
}
```

---

## Testing Requirements

### Unit Tests

Run full D3D12 unit test suite:
```bash
cd build/vs/src/igl/tests/Debug
./IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

**Add mipmap generation test:**
```cpp
TEST(TextureTest, GenerateMipmaps) {
    auto device = createDevice();

    TextureDesc desc;
    desc.width = 1024;
    desc.height = 1024;
    desc.numMipLevels = 11;  // Full mip chain
    desc.usage = TextureDesc::TextureUsageBits::Sampled;

    std::shared_ptr<ITexture> texture;
    auto result = device->createTexture(desc, &texture);
    EXPECT_TRUE(result.isOk());

    // Generate mipmaps (should not stall)
    auto start = std::chrono::high_resolution_clock::now();
    result = texture->generateMipmap(*device->getCommandQueue());
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start
    );

    EXPECT_TRUE(result.isOk());
    // Should return in <10ms (async), not 200ms+ (sync with recreation)
    EXPECT_LT(duration.count(), 10);
}
```

**Expected:** Mipmap generation returns in <10ms for 1K texture

### Render Sessions - Texture Streaming

Run sessions that generate mipmaps:
```bash
cd build/vs/shell/Debug
./Textured3DCubeSession_d3d12.exe
```

**Expected:**
- No frame hitches when loading new textures
- Frame times <16ms (60fps)
- No visible stuttering

### Performance Test - Before/After

Create micro-benchmark:
```cpp
void BenchmarkMipmapGeneration() {
    auto device = createDevice();

    for (int size = 512; size <= 4096; size *= 2) {
        TextureDesc desc;
        desc.width = size;
        desc.height = size;
        desc.numMipLevels = 0;  // Full chain

        auto texture = device->createTexture(desc);

        auto start = std::chrono::high_resolution_clock::now();
        texture->generateMipmap(*device->getCommandQueue());
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start
        );

        std::cout << size << "x" << size << ": " << duration.count() << "ms" << std::endl;
    }
}
```

**Expected:**
- **Before fix:** 1024×1024 = 230ms
- **After fix:** 1024×1024 = <5ms
- **Improvement:** 40-50× faster

### Validation

Enable GPU-based validation:
```bash
set IGL_D3D12_GPU_BASED_VALIDATION=1
./IGLTests.exe --gtest_filter="*Texture*Mipmap*"
```

**Expected:** No warnings about invalid resource states or missing UAV flags

---

## Definition of Done

- [ ] Textures with mipmaps created with UAV flag from start
- [ ] No texture recreation in `generateMipmap()`
- [ ] No synchronous GPU wait in mipmap generation
- [ ] All unit tests pass including new mipmap test
- [ ] Mipmap generation <10ms for 1K texture (40-50× improvement)
- [ ] Textured3DCubeSession runs smoothly with no hitches
- [ ] Code review confirms pattern matches MiniEngine
- [ ] User confirmation received before proceeding to next task

---

## Notes

- **DO NOT** modify unit tests unless D3D12-specific (backend checks)
- **DO NOT** modify render sessions unless D3D12-specific changes
- **MUST** wait for user confirmation of passing tests before next task
- Compute shader mipmap generation is preferred over blit-based approach
- Consider using better downsampling filter (Lanczos, Kaiser) for quality
- UAV flag slightly increases memory usage (~10%) but mandatory for runtime mipmap gen

---

## Related Issues

- **C-004**: Storage buffer synchronous wait (same async pattern needed)
- **DX12-COD-004**: Upload ring buffer fence mismatch (similar fence tracking)
- **H-005**: No UAV barriers (mipmaps need UAV→SRV transitions)
- **M-008**: Texture format support (not all formats support UAV)
