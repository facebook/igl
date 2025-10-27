# Phase 7: Missing IGL Features - Subagent Task Specifications

## Overview

Phase 7 focuses on implementing missing IGL features in the D3D12 backend to achieve feature parity with Vulkan and Metal backends. This document provides detailed task specifications for autonomous subagents.

---

## Task 1: Compute Shaders Support

### Objective
Complete the D3D12 ComputeCommandEncoder implementation to support compute shaders and compute pipeline execution.

### Current Status
- ✅ ComputeCommandEncoder.h exists with stub methods
- ✅ ComputeCommandEncoder.cpp has basic structure
- ❌ Compute pipeline state creation incomplete
- ❌ Compute shader dispatch not implemented
- ❌ Buffer/texture binding for compute shaders missing

### Files to Modify
1. `src/igl/d3d12/ComputeCommandEncoder.cpp` - Main implementation
2. `src/igl/d3d12/ComputeCommandEncoder.h` - Header updates if needed
3. `src/igl/d3d12/ComputePipelineState.cpp` - Compute PSO creation
4. `src/igl/d3d12/Device.cpp` - `createComputePipeline()` method
5. `src/igl/d3d12/CommandBuffer.cpp` - Compute encoder creation

### Implementation Steps

#### Step 1: Analyze Vulkan Implementation
**Action**: Study how Vulkan backend implements compute shaders
**Files to read**:
- `src/igl/vulkan/ComputeCommandEncoder.cpp`
- `src/igl/vulkan/ComputePipelineState.cpp`
**Goal**: Understand IGL's compute shader API and expected behavior

#### Step 2: Implement ComputePipelineState
**File**: `src/igl/d3d12/ComputePipelineState.cpp`
**Tasks**:
- Implement constructor to create D3D12 compute PSO
- Set up root signature for compute (CBVs, SRVs, UAVs, samplers)
- Handle shader bytecode (DXIL/DXBC)
- Store compute shader metadata (thread group size, etc.)

**D3D12 APIs to use**:
```cpp
D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
computePsoDesc.pRootSignature = rootSignature;
computePsoDesc.CS = { bytecode, bytecodeSize };
device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&pso));
```

#### Step 3: Implement ComputeCommandEncoder::bindComputePipelineState()
**File**: `src/igl/d3d12/ComputeCommandEncoder.cpp`
**Tasks**:
- Cast pipeline state to ComputePipelineState
- Call `commandList->SetPipelineState(pso)`
- Call `commandList->SetComputeRootSignature(rootSignature)`
- Cache pipeline state for resource binding

#### Step 4: Implement Resource Binding for Compute
**File**: `src/igl/d3d12/ComputeCommandEncoder.cpp`
**Methods to implement**:
- `bindBuffer()` - Bind buffers as SRV/UAV to compute shader
- `bindTexture()` - Bind textures as SRV to compute shader
- `bindUniform()` - Bind uniform buffers (CBV) to compute shader
- `bindBytes()` - Bind inline constants (root constants)

**D3D12 APIs to use**:
```cpp
// UAV binding
commandList->SetComputeRootUnorderedAccessView(rootParamIndex, gpuAddress);

// SRV binding via descriptor table
commandList->SetComputeRootDescriptorTable(rootParamIndex, gpuDescriptorHandle);

// CBV binding
commandList->SetComputeRootConstantBufferView(rootParamIndex, gpuAddress);
```

#### Step 5: Implement Compute Dispatch
**File**: `src/igl/d3d12/ComputeCommandEncoder.cpp`
**Method**: `dispatchThreadGroups()`
**Tasks**:
- Validate pipeline state is bound
- Ensure all resources are bound
- Call `commandList->Dispatch(threadGroupsX, threadGroupsY, threadGroupsZ)`
- Optionally insert UAV barriers for resource synchronization

**Example**:
```cpp
void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                  const Dependencies& dependencies) {
  if (!commandList_ || !currentPipeline_) {
    IGL_LOG_ERROR("Cannot dispatch: missing command list or pipeline\n");
    return;
  }

  // Insert UAV barriers if needed
  if (dependencies.buffers.size() > 0 || dependencies.textures.size() > 0) {
    // Insert resource barriers
  }

  commandList_->Dispatch(threadgroupCount.width,
                        threadgroupCount.height,
                        threadgroupCount.depth);
}
```

#### Step 6: UAV Barriers for Synchronization
**File**: `src/igl/d3d12/ComputeCommandEncoder.cpp`
**Tasks**:
- Implement UAV barriers between compute dispatches
- Handle resource state transitions (COMMON -> UAV -> COMMON)
- Use `D3D12_RESOURCE_BARRIER_TYPE_UAV` for unordered access resources

**Example**:
```cpp
D3D12_RESOURCE_BARRIER barrier = {};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
barrier.UAV.pResource = resource;
commandList->ResourceBarrier(1, &barrier);
```

### Testing Strategy

#### Test 1: Basic Compute Shader (Buffer Fill)
**Goal**: Dispatch a simple compute shader that fills a buffer with values
**Steps**:
1. Create compute pipeline with shader that writes to UAV buffer
2. Bind output buffer as UAV
3. Dispatch compute shader (1D thread groups)
4. Read back buffer and verify contents

#### Test 2: Texture Processing (Image Blur)
**Goal**: Use compute shader to process a texture
**Steps**:
1. Load input texture
2. Create UAV texture for output
3. Dispatch compute shader (2D thread groups)
4. Verify output texture has expected result

#### Test 3: Multiple Dispatches with Barriers
**Goal**: Ensure UAV barriers work correctly
**Steps**:
1. Dispatch shader A writing to buffer
2. Insert UAV barrier
3. Dispatch shader B reading from same buffer
4. Verify correct ordering

### Validation Criteria
- ✅ Compute pipeline state creates successfully
- ✅ Compute shaders dispatch without errors
- ✅ Buffer and texture bindings work correctly
- ✅ UAV barriers prevent race conditions
- ✅ Unit tests pass for compute operations
- ✅ Zero D3D12 validation errors

### Reference Materials
- [D3D12 Compute Shader Documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/pipelines-and-shaders-with-directx-12)
- [Vulkan ComputeCommandEncoder Implementation](src/igl/vulkan/ComputeCommandEncoder.cpp)
- [IGL Compute Pipeline Interface](src/igl/ComputePipelineState.h)

---

## Task 2: Depth Testing Enhancements

### Objective
Enhance depth testing support in D3D12 backend to match Vulkan/Metal feature parity, including depth bias, depth bounds testing, and improved depth buffer management.

### Current Status
- ✅ Basic depth testing works (ThreeCubesRenderSession uses depth)
- ❌ Depth bias (polygon offset) not fully implemented
- ❌ Depth bounds testing not supported
- ❌ Depth-only rendering modes incomplete
- ❌ Stencil reference values may need enhancement

### Files to Modify
1. `src/igl/d3d12/RenderCommandEncoder.cpp` - Depth state commands
2. `src/igl/d3d12/RenderPipelineState.cpp` - Depth state in PSO
3. `src/igl/d3d12/Device.cpp` - Depth feature queries
4. `src/igl/d3d12/Framebuffer.cpp` - Depth buffer handling

### Implementation Steps

#### Step 1: Analyze Current Depth Implementation
**Action**: Review existing depth testing code
**Files to check**:
- `src/igl/d3d12/RenderPipelineState.cpp` - Look for `D3D12_DEPTH_STENCIL_DESC`
- `src/igl/d3d12/RenderCommandEncoder.cpp` - Look for depth-related commands
- `src/igl/d3d12/Framebuffer.cpp` - Check DSV (Depth Stencil View) creation

**Goal**: Document what's already working and what's missing

#### Step 2: Implement Depth Bias (Polygon Offset)
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Set depth bias in pipeline state

**D3D12 API**:
```cpp
D3D12_RASTERIZER_DESC rasterizerDesc = {};
rasterizerDesc.DepthBias = depthBias;                    // Integer bias
rasterizerDesc.DepthBiasClamp = depthBiasClamp;         // Max bias value
rasterizerDesc.SlopeScaledDepthBias = slopeScaleBias;   // Slope-based bias
```

**IGL Mapping**:
```cpp
// From RenderPipelineDesc::DepthStencilState
const auto& depthState = desc.targetDesc.depthAttachmentDesc;
rasterizerDesc.DepthBias = static_cast<INT>(depthState.depthBias);
rasterizerDesc.SlopeScaledDepthBias = depthState.slopeScale;
rasterizerDesc.DepthBiasClamp = depthState.depthBiasClamp;
```

#### Step 3: Enhance Depth Comparison Functions
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Ensure all comparison functions are supported

**D3D12 Comparison Functions**:
```cpp
D3D12_COMPARISON_FUNC convertCompareFunction(CompareFunction func) {
  switch (func) {
    case CompareFunction::Never: return D3D12_COMPARISON_FUNC_NEVER;
    case CompareFunction::Less: return D3D12_COMPARISON_FUNC_LESS;
    case CompareFunction::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
    case CompareFunction::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CompareFunction::Greater: return D3D12_COMPARISON_FUNC_GREATER;
    case CompareFunction::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CompareFunction::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CompareFunction::AlwaysPass: return D3D12_COMPARISON_FUNC_ALWAYS;
    default: return D3D12_COMPARISON_FUNC_LESS;
  }
}
```

#### Step 4: Implement Depth Bounds Testing (Optional)
**File**: `src/igl/d3d12/Device.cpp`
**Note**: Depth bounds testing is a Vulkan-specific feature, not available in D3D12
**Task**: Report feature as unsupported via `hasFeature(DeviceFeatures::DepthBounds)`

```cpp
bool Device::hasFeature(DeviceFeatures feature) const {
  switch (feature) {
    case DeviceFeatures::DepthBounds:
      return false; // Not supported in D3D12
    // ... other features
  }
}
```

#### Step 5: Improve Depth Clear Operations
**File**: `src/igl/d3d12/RenderCommandEncoder.cpp`
**Task**: Ensure depth clear works in all scenarios

**D3D12 API**:
```cpp
// Clear depth buffer
commandList->ClearDepthStencilView(
  dsvHandle,
  D3D12_CLEAR_FLAG_DEPTH,
  clearDepth,     // Depth clear value (0.0 to 1.0)
  clearStencil,   // Stencil clear value (ignored if not clearing stencil)
  0, nullptr      // Rect count and rects (0 = full buffer)
);
```

#### Step 6: Support Depth-Only Rendering
**File**: `src/igl/d3d12/RenderCommandEncoder.cpp`
**Task**: Allow rendering with depth attachment but no color attachments

**Check in beginRenderPass()**:
```cpp
// Allow depth-only rendering
if (framebuffer_->hasDepth() && !framebuffer_->hasColor()) {
  // Valid: depth-only pass (e.g., shadow mapping)
  commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
}
```

### Testing Strategy

#### Test 1: Depth Bias (Shadow Mapping)
**Goal**: Verify polygon offset works to prevent shadow acne
**Steps**:
1. Create render pass with depth bias enabled
2. Render scene with depth bias = 1.0, slope = 2.0
3. Compare depth values with/without bias
4. Verify no Z-fighting artifacts

#### Test 2: Depth Comparison Functions
**Goal**: Test all comparison modes (Less, Greater, Equal, etc.)
**Steps**:
1. Render overlapping geometry with different depth functions
2. Verify correct pixels pass/fail depth test
3. Test edge cases (Equal with floating-point depth)

#### Test 3: Depth Clear Values
**Goal**: Ensure depth buffer clears correctly
**Steps**:
1. Clear depth to 0.0 (near plane)
2. Clear depth to 1.0 (far plane)
3. Clear depth to 0.5 (mid-range)
4. Verify depth buffer contains expected values

#### Test 4: Depth-Only Rendering
**Goal**: Shadow map generation without color output
**Steps**:
1. Create framebuffer with depth attachment only
2. Render scene to depth buffer
3. Read back depth texture
4. Verify depth values are correct

### Validation Criteria
- ✅ Depth bias works correctly (no Z-fighting)
- ✅ All depth comparison functions implemented
- ✅ Depth clear operations work for all clear values
- ✅ Depth-only rendering works (shadow mapping scenario)
- ✅ Existing depth tests (ThreeCubesRenderSession) still pass
- ✅ Zero validation errors

### Reference Materials
- [D3D12 Depth-Stencil State](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_depth_stencil_desc)
- [D3D12 Rasterizer State](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_rasterizer_desc)
- [IGL Depth Stencil Documentation](src/igl/RenderPipelineState.h)

---

## Task 3: Stencil Operations

### Objective
Implement full stencil buffer support in D3D12 backend, including stencil operations, stencil masks, and stencil reference values.

### Current Status
- ❓ Stencil buffer creation may work (framebuffer creates DSV)
- ❌ Stencil operations not implemented
- ❌ Stencil masks not configured
- ❌ Dynamic stencil reference value not supported

### Files to Modify
1. `src/igl/d3d12/RenderPipelineState.cpp` - Stencil state in PSO
2. `src/igl/d3d12/RenderCommandEncoder.cpp` - Dynamic stencil commands
3. `src/igl/d3d12/Framebuffer.cpp` - Stencil buffer formats

### Implementation Steps

#### Step 1: Configure Stencil State in PSO
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Set up `D3D12_DEPTH_STENCIL_DESC` for stencil operations

**D3D12 API**:
```cpp
D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};

// Enable stencil test
depthStencilDesc.StencilEnable = TRUE;
depthStencilDesc.StencilReadMask = 0xFF;
depthStencilDesc.StencilWriteMask = 0xFF;

// Front-facing stencil operations
depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

// Back-facing stencil operations (can be different)
depthStencilDesc.BackFace = depthStencilDesc.FrontFace;
```

**IGL Mapping**:
```cpp
// From RenderPipelineDesc::DepthStencilState
const auto& stencilState = desc.targetDesc.stencilAttachmentDesc;

depthStencilDesc.StencilEnable = stencilState.isEnabled ? TRUE : FALSE;
depthStencilDesc.StencilReadMask = stencilState.readMask;
depthStencilDesc.StencilWriteMask = stencilState.writeMask;

// Front face ops
depthStencilDesc.FrontFace.StencilFailOp = convertStencilOp(stencilState.stencilFailureOp);
depthStencilDesc.FrontFace.StencilDepthFailOp = convertStencilOp(stencilState.depthFailureOp);
depthStencilDesc.FrontFace.StencilPassOp = convertStencilOp(stencilState.depthStencilPassOp);
depthStencilDesc.FrontFace.StencilFunc = convertCompareFunction(stencilState.stencilCompareFunction);

// Back face ops (if different)
if (stencilState.backFaceStencil.isEnabled) {
  depthStencilDesc.BackFace.StencilFailOp = convertStencilOp(stencilState.backFaceStencil.stencilFailureOp);
  // ... etc
} else {
  depthStencilDesc.BackFace = depthStencilDesc.FrontFace;
}
```

#### Step 2: Implement Stencil Operation Conversion
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Convert IGL stencil ops to D3D12 stencil ops

```cpp
D3D12_STENCIL_OP convertStencilOp(StencilOperation op) {
  switch (op) {
    case StencilOperation::Keep: return D3D12_STENCIL_OP_KEEP;
    case StencilOperation::Zero: return D3D12_STENCIL_OP_ZERO;
    case StencilOperation::Replace: return D3D12_STENCIL_OP_REPLACE;
    case StencilOperation::IncrementClamp: return D3D12_STENCIL_OP_INCR_SAT;
    case StencilOperation::DecrementClamp: return D3D12_STENCIL_OP_DECR_SAT;
    case StencilOperation::Invert: return D3D12_STENCIL_OP_INVERT;
    case StencilOperation::IncrementWrap: return D3D12_STENCIL_OP_INCR;
    case StencilOperation::DecrementWrap: return D3D12_STENCIL_OP_DECR;
    default: return D3D12_STENCIL_OP_KEEP;
  }
}
```

#### Step 3: Implement Dynamic Stencil Reference Value
**File**: `src/igl/d3d12/RenderCommandEncoder.cpp`
**Method**: `setStencilReferenceValue()` or similar
**Task**: Set stencil reference value dynamically during rendering

**D3D12 API**:
```cpp
void RenderCommandEncoder::setStencilReferenceValue(uint32_t value) {
  if (!commandList_) {
    return;
  }
  commandList_->OMSetStencilRef(value);
}
```

#### Step 4: Support Stencil Clear Operations
**File**: `src/igl/d3d12/RenderCommandEncoder.cpp`
**Task**: Clear stencil buffer to specified value

**D3D12 API**:
```cpp
// Clear stencil buffer
commandList->ClearDepthStencilView(
  dsvHandle,
  D3D12_CLEAR_FLAG_STENCIL,
  0.0f,         // Depth value (ignored)
  clearStencil, // Stencil clear value (0-255)
  0, nullptr
);

// Clear both depth and stencil
commandList->ClearDepthStencilView(
  dsvHandle,
  D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
  clearDepth,
  clearStencil,
  0, nullptr
);
```

#### Step 5: Ensure Stencil Formats are Supported
**File**: `src/igl/d3d12/Framebuffer.cpp`
**Task**: Verify depth-stencil formats are created correctly

**Common Stencil Formats**:
- `DXGI_FORMAT_D24_UNORM_S8_UINT` - 24-bit depth + 8-bit stencil
- `DXGI_FORMAT_D32_FLOAT_S8X24_UINT` - 32-bit float depth + 8-bit stencil

**Check DSV creation**:
```cpp
// In Framebuffer constructor
if (depthAttachment && depthAttachment->getProperties().hasStencil()) {
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Or appropriate format
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  device->CreateDepthStencilView(resource, &dsvDesc, dsvHandle);
}
```

### Testing Strategy

#### Test 1: Basic Stencil Masking
**Goal**: Use stencil to mask out regions
**Steps**:
1. Clear stencil to 0
2. Draw shape with stencil write (set ref = 1, op = REPLACE)
3. Draw another shape with stencil test (only pass if stencil == 1)
4. Verify only overlapping region renders

#### Test 2: Stencil Operations (Increment/Decrement)
**Goal**: Test increment and decrement operations
**Steps**:
1. Clear stencil to 0
2. Draw overlapping triangles with stencil op = INCREMENT
3. Verify stencil buffer increments correctly
4. Test DECREMENT, INVERT, etc.

#### Test 3: Front/Back Face Stencil
**Goal**: Verify different stencil ops for front vs back faces
**Steps**:
1. Configure front face stencil = INCREMENT
2. Configure back face stencil = DECREMENT
3. Draw two-sided geometry
4. Verify stencil values differ based on face direction

#### Test 4: Dynamic Stencil Reference
**Goal**: Change stencil reference during rendering
**Steps**:
1. Set stencil ref = 1, draw some geometry
2. Set stencil ref = 2, draw more geometry
3. Verify stencil test uses updated reference value

### Validation Criteria
- ✅ Stencil operations work (KEEP, REPLACE, INCREMENT, etc.)
- ✅ Stencil masks (read/write) work correctly
- ✅ Dynamic stencil reference value can be set
- ✅ Stencil clear operations work
- ✅ Front/back face stencil operations work independently
- ✅ Depth-stencil formats created correctly
- ✅ Zero validation errors

### Reference Materials
- [D3D12 Stencil Operations](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_stencil_op)
- [IGL Stencil State](src/igl/RenderPipelineState.h)

---

## Task 4: Advanced Blending Modes

### Objective
Implement advanced blend modes in D3D12 backend to support all IGL blend operations, including separate alpha blending and dual-source blending.

### Current Status
- ✅ Basic blending works (ImGui uses alpha blending)
- ❌ Not all blend operations tested
- ❌ Separate alpha blend factors not implemented
- ❌ Dual-source blending not supported
- ❌ Logic operations (bitwise blend) not implemented

### Files to Modify
1. `src/igl/d3d12/RenderPipelineState.cpp` - Blend state configuration
2. `src/igl/d3d12/Device.cpp` - Feature queries for blend modes

### Implementation Steps

#### Step 1: Review Current Blend Implementation
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Analyze existing blend state code

**Find**:
```cpp
D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {};
blendDesc.BlendEnable = TRUE/FALSE;
blendDesc.SrcBlend = D3D12_BLEND_???;
blendDesc.DestBlend = D3D12_BLEND_???;
blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
// etc.
```

#### Step 2: Implement All Blend Factors
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Convert all IGL blend factors to D3D12

```cpp
D3D12_BLEND convertBlendFactor(BlendFactor factor) {
  switch (factor) {
    case BlendFactor::Zero: return D3D12_BLEND_ZERO;
    case BlendFactor::One: return D3D12_BLEND_ONE;
    case BlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
    case BlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
    case BlendFactor::OneMinusDstColor: return D3D12_BLEND_INV_DEST_COLOR;
    case BlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
    case BlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
    case BlendFactor::OneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
    case BlendFactor::BlendColor: return D3D12_BLEND_BLEND_FACTOR;
    case BlendFactor::OneMinusBlendColor: return D3D12_BLEND_INV_BLEND_FACTOR;
    case BlendFactor::SrcAlphaSaturated: return D3D12_BLEND_SRC_ALPHA_SAT;
    case BlendFactor::Src1Color: return D3D12_BLEND_SRC1_COLOR;
    case BlendFactor::OneMinusSrc1Color: return D3D12_BLEND_INV_SRC1_COLOR;
    case BlendFactor::Src1Alpha: return D3D12_BLEND_SRC1_ALPHA;
    case BlendFactor::OneMinusSrc1Alpha: return D3D12_BLEND_INV_SRC1_ALPHA;
    default: return D3D12_BLEND_ONE;
  }
}
```

#### Step 3: Implement All Blend Operations
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Support all blend ops (Add, Subtract, Min, Max, etc.)

```cpp
D3D12_BLEND_OP convertBlendOp(BlendOp op) {
  switch (op) {
    case BlendOp::Add: return D3D12_BLEND_OP_ADD;
    case BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
    case BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
    case BlendOp::Min: return D3D12_BLEND_OP_MIN;
    case BlendOp::Max: return D3D12_BLEND_OP_MAX;
    default: return D3D12_BLEND_OP_ADD;
  }
}
```

#### Step 4: Implement Separate Alpha Blending
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Allow different blend factors for RGB vs Alpha

```cpp
// RGB blend
blendDesc.SrcBlend = convertBlendFactor(desc.rgbBlendOp.srcFactor);
blendDesc.DestBlend = convertBlendFactor(desc.rgbBlendOp.dstFactor);
blendDesc.BlendOp = convertBlendOp(desc.rgbBlendOp.operation);

// Alpha blend (separate)
blendDesc.SrcBlendAlpha = convertBlendFactor(desc.alphaBlendOp.srcFactor);
blendDesc.DestBlendAlpha = convertBlendFactor(desc.alphaBlendOp.dstFactor);
blendDesc.BlendOpAlpha = convertBlendOp(desc.alphaBlendOp.operation);
```

#### Step 5: Implement Render Target Write Mask
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Task**: Support color write mask (R, G, B, A)

```cpp
// From IGL ColorWriteMask to D3D12
UINT8 writeMask = 0;
if (desc.colorWriteMask & ColorWriteMask::Red)   writeMask |= D3D12_COLOR_WRITE_ENABLE_RED;
if (desc.colorWriteMask & ColorWriteMask::Green) writeMask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
if (desc.colorWriteMask & ColorWriteMask::Blue)  writeMask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
if (desc.colorWriteMask & ColorWriteMask::Alpha) writeMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;

blendDesc.RenderTargetWriteMask = writeMask;
```

#### Step 6: Implement Logic Operations (Optional)
**File**: `src/igl/d3d12/RenderPipelineState.cpp`
**Note**: Logic ops require specific feature level support
**Task**: Check feature support and enable if available

```cpp
// Check if logic ops are supported
D3D12_FEATURE_DATA_D3D12_OPTIONS options;
device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));

if (options.OutputMergerLogicOp) {
  blendDesc.LogicOpEnable = desc.logicOpEnabled ? TRUE : FALSE;
  blendDesc.LogicOp = convertLogicOp(desc.logicOp);
}
```

**Logic Op Conversion**:
```cpp
D3D12_LOGIC_OP convertLogicOp(LogicOp op) {
  switch (op) {
    case LogicOp::Clear: return D3D12_LOGIC_OP_CLEAR;
    case LogicOp::And: return D3D12_LOGIC_OP_AND;
    case LogicOp::AndReverse: return D3D12_LOGIC_OP_AND_REVERSE;
    case LogicOp::Copy: return D3D12_LOGIC_OP_COPY;
    case LogicOp::AndInverted: return D3D12_LOGIC_OP_AND_INVERTED;
    case LogicOp::NoOp: return D3D12_LOGIC_OP_NOOP;
    case LogicOp::Xor: return D3D12_LOGIC_OP_XOR;
    case LogicOp::Or: return D3D12_LOGIC_OP_OR;
    case LogicOp::Nor: return D3D12_LOGIC_OP_NOR;
    case LogicOp::Equivalent: return D3D12_LOGIC_OP_EQUIV;
    case LogicOp::Invert: return D3D12_LOGIC_OP_INVERT;
    case LogicOp::OrReverse: return D3D12_LOGIC_OP_OR_REVERSE;
    case LogicOp::CopyInverted: return D3D12_LOGIC_OP_COPY_INVERTED;
    case LogicOp::OrInverted: return D3D12_LOGIC_OP_OR_INVERTED;
    case LogicOp::Nand: return D3D12_LOGIC_OP_NAND;
    case LogicOp::Set: return D3D12_LOGIC_OP_SET;
    default: return D3D12_LOGIC_OP_NOOP;
  }
}
```

#### Step 7: Support Blend Factor Constants
**File**: `src/igl/d3d12/RenderCommandEncoder.cpp`
**Method**: `setBlendColor()` or similar
**Task**: Set blend factor constant color

**D3D12 API**:
```cpp
void RenderCommandEncoder::setBlendColor(const Color& color) {
  if (!commandList_) {
    return;
  }
  const float blendFactor[4] = { color.r, color.g, color.b, color.a };
  commandList_->OMSetBlendFactor(blendFactor);
}
```

### Testing Strategy

#### Test 1: All Blend Factors
**Goal**: Verify all blend factor combinations work
**Steps**:
1. Test common modes (SrcAlpha, OneMinusSrcAlpha)
2. Test color factors (SrcColor, DstColor)
3. Test constant blend factor (BlendColor)
4. Test dual-source blending (Src1Color, Src1Alpha)

#### Test 2: Separate RGB/Alpha Blending
**Goal**: Use different blend modes for color vs alpha
**Steps**:
1. Set RGB blend = (SrcAlpha, OneMinusSrcAlpha, Add)
2. Set Alpha blend = (One, Zero, Add)
3. Verify color and alpha blend independently

#### Test 3: Blend Operations
**Goal**: Test all blend ops (Add, Subtract, Min, Max, etc.)
**Steps**:
1. Render overlapping geometry with BlendOp = Subtract
2. Render with BlendOp = Min
3. Render with BlendOp = Max
4. Verify correct output for each mode

#### Test 4: Color Write Mask
**Goal**: Selectively write to RGBA channels
**Steps**:
1. Set write mask = Red only, draw red square
2. Set write mask = Green|Blue, draw cyan over it
3. Verify final color is (red from 1, green+blue from 2)

#### Test 5: Logic Operations (if supported)
**Goal**: Test bitwise blend operations
**Steps**:
1. Check if logic ops are supported
2. If yes, test LogicOp = XOR
3. Draw overlapping shapes and verify XOR result

### Validation Criteria
- ✅ All blend factors work correctly
- ✅ All blend operations work (Add, Subtract, Min, Max)
- ✅ Separate RGB/Alpha blending works
- ✅ Color write mask works
- ✅ Blend color constants can be set
- ✅ Logic operations work (if hardware supports)
- ✅ ImGui rendering still works (regression test)
- ✅ Zero validation errors

### Reference Materials
- [D3D12 Blend State](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_render_target_blend_desc)
- [IGL Blend State](src/igl/RenderPipelineState.h)

---

## General Guidelines for All Tasks

### Code Quality Standards
1. **Follow existing code style**: Match D3D12 backend conventions
2. **Add logging**: Use `IGL_LOG_INFO`, `IGL_LOG_ERROR` for debugging
3. **Error handling**: Check return values, handle failures gracefully
4. **Documentation**: Add comments explaining D3D12-specific behavior
5. **Validation**: Ensure zero D3D12 validation layer errors

### Testing Requirements
1. **Unit tests**: Add tests to `src/igl/d3d12/tests/`
2. **Regression tests**: Ensure existing sessions still pass
3. **Visual verification**: Capture screenshots for visual features
4. **Performance**: Note any performance impacts

### Reporting Back
For each task, provide:
1. **Summary**: What was implemented
2. **Files modified**: List of changed files with line numbers
3. **Test results**: Pass/fail status of all tests
4. **Screenshots**: Visual proof of working features (if applicable)
5. **Issues encountered**: Any problems or limitations
6. **Recommendations**: Suggestions for further improvements

### Build and Test Commands
```bash
# Build D3D12 backend
cd build
cmake --build . --config Debug

# Run specific session test
./build/shell/Debug/[SessionName]_d3d12.exe --viewport-size 640x360 --screenshot-number 0 --screenshot-file test.png

# Run unit tests (when available)
./build/src/igl/tests/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

### Reference Implementations
Always check Vulkan backend for reference:
- `src/igl/vulkan/` - Vulkan implementation (similar to D3D12)
- `src/igl/metal/` - Metal implementation (for comparison)
- `src/igl/opengl/` - OpenGL implementation (different architecture)

---

## Task Prioritization

**Recommended Order**:
1. **Task 1: Compute Shaders** (High impact, most complex)
2. **Task 2: Depth Testing** (Medium impact, moderate complexity)
3. **Task 3: Stencil Operations** (Medium impact, moderate complexity)
4. **Task 4: Advanced Blending** (Low impact, low complexity - mostly validation)

**Estimated Time**:
- Task 1: 8-12 hours
- Task 2: 4-6 hours
- Task 3: 4-6 hours
- Task 4: 2-4 hours
- **Total**: 18-28 hours

---

## Success Criteria for Phase 7 Objective 2

- ✅ All 4 tasks completed and tested
- ✅ Unit tests pass for new features
- ✅ All 14 existing sessions still pass (no regressions)
- ✅ Zero D3D12 validation errors
- ✅ Feature parity documented vs Vulkan/Metal
- ✅ Code committed with detailed commit messages
- ✅ Documentation updated

**End of Phase 7 Subagent Task Specifications**
