# D3D12 Feature Implementation Prompts for Subagents

This document contains detailed prompts for subagents to implement missing D3D12 features in the IGL graphics library.

**IMPORTANT RULES FOR ALL SUBAGENTS:**
1. **NEVER modify session source files** (`shell/renderSessions/*.cpp`) unless explicitly instructed
2. **ONLY modify D3D12 backend files** (`src/igl/d3d12/*.cpp`, `src/igl/d3d12/*.h`)
3. **PRESERVE all existing session vertex layouts, buffer indices, and shader interfaces**
4. **RUN validation after EVERY change**: `cd tools/validation && python validate_d3d12.py`
5. **NO REGRESSIONS ALLOWED**: All previously passing sessions must continue to pass
6. **Run unit tests**: `./build/src/igl/tests/Debug/IGLTests.exe --gtest_brief=1`
7. **Document all changes** in git commits with clear descriptions

---

## Template: Add D3D12 Shaders to Render Session

### Prompt

```
You are implementing D3D12 HLSL shader support for the {SESSION_NAME} render session.

**OBJECTIVE:**
Add D3D12 HLSL shaders to {SESSION_NAME} to eliminate the "Code should NOT be reached" or "Shader stages are required!" crash.

**CONTEXT:**
- Session file: shell/renderSessions/{SESSION_NAME}.cpp
- The session already has shaders for Vulkan, Metal, and OpenGL
- D3D12 backend uses HLSL shader language
- You must add a case for `igl::BackendType::D3D12` in the `getShaderStagesForBackend()` function

**CONSTRAINTS:**
- DO NOT modify session logic, vertex layouts, or buffer indices
- ONLY add the D3D12 shader case in the existing switch statement
- Match the shader interface exactly (same inputs/outputs as other backends)
- Use register bindings: b0, b1, ... for cbuffers; t0, t1, ... for textures; s0, s1, ... for samplers
- Preserve uniform block layouts from other backends

**STEPS:**

1. **Read the session file:**
   - Locate `getShaderStagesForBackend()` function
   - Read existing Vulkan/Metal/OpenGL shaders to understand:
     - Vertex input attributes (position, UV, color, normals, etc.)
     - Uniform buffer structure and layout
     - Texture bindings and sampler bindings
     - Vertex shader outputs (position, UV, color, etc.)
     - Fragment shader outputs (color attachments)

2. **Map shader interfaces to HLSL:**
   - Vulkan/GL `layout(location=N)` → HLSL semantic (POSITION, TEXCOORD0, COLOR0, etc.)
   - Vulkan `layout(set=X, binding=Y)` → HLSL register(tY) for textures, register(sY) for samplers
   - Vulkan `layout(std140) uniform Block` → HLSL `cbuffer Block : register(bN)`
   - Metal `[[buffer(N)]]` → HLSL `register(bN)`
   - Metal `[[texture(N)]]` → HLSL `register(tN)`

3. **Write HLSL shaders:**
   ```cpp
   case igl::BackendType::D3D12: {
     static const char* kVS = R"(
       // Uniform buffers (cbuffer)
       cbuffer UniformBlock : register(b0) {
         float4x4 mvpMatrix;
         // ... other uniforms
       };

       // Vertex input structure
       struct VSInput {
         float3 position : POSITION;
         float2 uv : TEXCOORD0;
         // ... other attributes
       };

       // Vertex output structure (matches pixel shader input)
       struct VSOutput {
         float4 position : SV_POSITION;
         float2 uv : TEXCOORD0;
         // ... other varyings
       };

       VSOutput main(VSInput input) {
         VSOutput output;
         output.position = mul(mvpMatrix, float4(input.position, 1.0));
         output.uv = input.uv;
         return output;
       }
     )";

     static const char* kPS = R"(
       // Textures and samplers
       Texture2D diffuseTex : register(t0);
       SamplerState linearSampler : register(s0);

       // Pixel input structure (matches vertex shader output)
       struct PSInput {
         float4 position : SV_POSITION;
         float2 uv : TEXCOORD0;
       };

       // Pixel output
       float4 main(PSInput input) : SV_Target {
         return diffuseTex.Sample(linearSampler, input.uv);
       }
     )";

     return igl::ShaderStagesCreator::fromModuleStringInput(
         device, kVS, "main", "", kPS, "main", "", nullptr);
   }
   ```

4. **Handle multiple shader variants if needed:**
   - Some sessions have different modes (e.g., ColorSession has gradient vs textured mode)
   - Use if/else or separate static const char* for each variant

5. **Build and test:**
   ```bash
   cmake --build build --config Debug --target {SESSION_NAME}_d3d12 -j 8
   ./build/shell/Debug/{SESSION_NAME}_d3d12.exe --viewport-size 640x360 --screenshot-number 0 --screenshot-file /tmp/test.png
   ```

6. **Validate no regressions:**
   ```bash
   cd tools/validation
   python validate_d3d12.py
   ```
   - Expected: Session changes from CRASH to PASS/NO_VISUAL_CHECK
   - Required: No previously passing sessions regress

7. **Update validation baseline:**
   - Edit `tools/validation/validate_d3d12.py`
   - Change session expected status from 'CRASH' to 'PASS'

**ACCEPTANCE CRITERIA:**
✅ Session runs without "Shader stages are required!" error
✅ Session runs without crashing
✅ No device removal errors
✅ Screenshot captured successfully
✅ Validation script shows no regressions
✅ All other sessions still pass

**DELIVERABLES:**
1. Modified file: `shell/renderSessions/{SESSION_NAME}.cpp` with D3D12 shaders
2. Validation results showing no regressions
3. Screenshot from the session
```

**USAGE EXAMPLES:**

### Example 1: TinyMeshBindGroupSession
```
Session: TinyMeshBindGroupSession
File: shell/renderSessions/TinyMeshBindGroupSession.cpp
Error Location: Line 297: "Code NOT implemented"
```

### Example 2: TQSession
```
Session: TQSession
File: shell/renderSessions/TQSession.cpp
Error Location: Line 170: "Code should NOT be reached"
```

---

## Template: Implement D3D12 Backend Feature

### Prompt

```
You are implementing {FEATURE_NAME} support in the D3D12 IGL backend.

**OBJECTIVE:**
Add full support for {FEATURE_NAME} in the D3D12 backend to match functionality available in Vulkan/Metal/OpenGL backends.

**CONTEXT:**
- Feature: {FEATURE_NAME}
- Required for: {SESSIONS_OR_USE_CASES}
- Current status: {NOT_IMPLEMENTED | PARTIALLY_IMPLEMENTED}
- Issue: {DESCRIBE_CURRENT_PROBLEM}

**REFERENCE IMPLEMENTATIONS:**
Study these backends for reference:
- Vulkan: src/igl/vulkan/{RELEVANT_FILES}
- Metal: src/igl/metal/{RELEVANT_FILES}
- OpenGL: src/igl/opengl/{RELEVANT_FILES}

**FILES TO MODIFY:**
Primary:
- {LIST_PRIMARY_FILES}

Secondary (if needed):
- {LIST_SECONDARY_FILES}

**TECHNICAL REQUIREMENTS:**

{FEATURE_SPECIFIC_REQUIREMENTS}

**IMPLEMENTATION STEPS:**

1. **Study existing implementations:**
   - Read Vulkan implementation as primary reference
   - Note API calls, data structures, and state management
   - Understand the feature's lifecycle (creation → usage → cleanup)

2. **Design D3D12 implementation:**
   - Map IGL API to D3D12 API
   - Plan resource creation, binding, and cleanup
   - Consider descriptor management implications

3. **Implement core functionality:**
   - Add necessary D3D12 API calls
   - Update data structures
   - Implement resource state tracking if needed

4. **Update hasFeature() if applicable:**
   ```cpp
   // In src/igl/d3d12/Device.cpp
   bool Device::hasFeature(DeviceFeatures feature) const {
     switch (feature) {
       // ... existing cases
       case DeviceFeatures::{FEATURE_ENUM}:
         return true;  // Now supported!
     }
   }
   ```

5. **Add validation and error handling:**
   - Check D3D12 HRESULT return values
   - Add IGL_LOG_ERROR for failures
   - Return appropriate Result codes

6. **Test with relevant sessions:**
   ```bash
   # Build affected sessions
   cmake --build build --config Debug --target {SESSION}_d3d12 -j 8

   # Run manually to observe behavior
   ./build/shell/Debug/{SESSION}_d3d12.exe --viewport-size 640x360

   # Capture screenshot
   ./build/shell/Debug/{SESSION}_d3d12.exe --viewport-size 640x360 --screenshot-number 0 --screenshot-file /tmp/{SESSION}.png
   ```

7. **Run full validation:**
   ```bash
   # Unit tests
   ./build/src/igl/tests/Debug/IGLTests.exe --gtest_brief=1

   # Session validation
   cd tools/validation
   python validate_d3d12.py
   ```

8. **Update documentation:**
   - Mark feature as implemented in `docs/D3D12_REQUIRED_FEATURES.md`
   - Update session status if newly working

**ACCEPTANCE CRITERIA:**
✅ Feature implemented according to IGL API contract
✅ No crashes or device removal errors
✅ Relevant sessions now work correctly
✅ Unit tests pass (if applicable)
✅ No regressions in existing functionality
✅ Clean validation errors (no D3D12 debug layer errors)

**DELIVERABLES:**
1. Implementation in D3D12 backend files
2. Updated `docs/D3D12_REQUIRED_FEATURES.md`
3. Validation results
4. Screenshots from newly working sessions
```

---

## Specific Feature Prompts

### 1. 3D Texture Support

```
FEATURE_NAME: 3D Texture Rendering Support
REQUIRED_FOR: Textured3DCubeSession
CURRENT_STATUS: Partially implemented - textures created but don't render correctly
ISSUE: 3D textures show as white/black instead of textured

FILES_TO_MODIFY:
Primary:
- src/igl/d3d12/Device.cpp - createTexture() for 3D textures
- src/igl/d3d12/RenderCommandEncoder.cpp - bindTexture() for 3D textures
- src/igl/d3d12/Texture.cpp - upload() for 3D texture data

TECHNICAL_REQUIREMENTS:
1. Properly create D3D12_RESOURCE_DESC with Dimension = TEXTURE3D
2. Set DepthOrArraySize to texture depth (not array size)
3. Create correct SRV descriptor (D3D12_SRV_DIMENSION_TEXTURE3D)
4. Handle 3D texture uploads with proper row/depth pitch calculation
5. Sample 3D textures in shaders with 3D UVW coordinates

SPECIFIC_CHECKS:
- resourceDesc.Dimension must be D3D12_RESOURCE_DIMENSION_TEXTURE3D
- SRV ViewDimension must be D3D12_SRV_DIMENSION_TEXTURE3D
- Upload must handle depthPitch correctly: rowPitch * height * depth
- Shader must use Texture3D.Sample() not Texture2D.Sample()

DEBUG_INFO:
Current log shows:
  bindTexture: resource dimension=4, texture type=3
  bindTexture: using TEXTURE3D view dimension
But texture doesn't render correctly - investigate SRV creation and data upload.

TEST_SESSION: Textured3DCubeSession
EXPECTED_RESULT: Textured cube with 3D procedural texture pattern
```

### 2. Texture View Support

```
FEATURE_NAME: Texture View (Partial Mip Chain)
REQUIRED_FOR: TextureViewSession
CURRENT_STATUS: Not implemented
ISSUE: DeviceFeatures::TexturePartialMipChain returns false

FILES_TO_MODIFY:
Primary:
- src/igl/d3d12/Device.cpp - hasFeature() for TexturePartialMipChain
- src/igl/d3d12/Texture.cpp - createTextureView() implementation
- src/igl/d3d12/Texture.h - Add texture view data structures

TECHNICAL_REQUIREMENTS:
1. Support creating SRVs with MostDetailedMip != 0
2. Support creating SRVs with MipLevels < total mip levels
3. Support creating SRVs for texture array slices
4. Track view lifetime and parent texture relationship

D3D12_API_CALLS:
- CreateShaderResourceView() with custom D3D12_SHADER_RESOURCE_VIEW_DESC
- Set Texture2D.MostDetailedMip for mip offset
- Set Texture2D.MipLevels for mip count

IMPLEMENTATION_PATTERN:
```cpp
// In Texture.cpp
std::shared_ptr<ITexture> Texture::createTextureView(const TextureViewDesc& desc) {
  auto view = std::make_shared<Texture>(device_, format_);
  view->resource_ = resource_;  // Share resource
  view->mipLevelOffset_ = desc.mipLevel;
  view->numMipLevels_ = desc.numMipLevels;
  // Create custom SRV when binding
  return view;
}
```

TEST_SESSION: TextureViewSession
EXPECTED_RESULT: Session runs without "Texture views are not supported" error
```

### 3. Mipmap Generation

```
FEATURE_NAME: Automatic Mipmap Generation
REQUIRED_FOR: All textured sessions with mipmapping
CURRENT_STATUS: Disabled - stub implementation
ISSUE: Texture::generateMipmap() shows "DISABLED: not yet fully implemented"

FILES_TO_MODIFY:
Primary:
- src/igl/d3d12/Texture.cpp - generateMipmap() implementation
- src/igl/d3d12/CommandQueue.cpp - submit() with compute dispatch (optional)

TECHNICAL_REQUIREMENTS:
1. Use compute shader to generate mipmaps OR
2. Use D3D12 copy/blit operations to downsample

OPTION_A_COMPUTE_SHADER:
- Create compute pipeline for mipmap generation
- Dispatch for each mip level
- Use UAV for destination, SRV for source
- Requires compute shader support

OPTION_B_COPY_OPERATIONS:
- Use ID3D12GraphicsCommandList::CopyTextureRegion()
- Box filter downsample with 2x2 average
- Simpler but lower quality than compute

REFERENCE:
- Vulkan: Uses vkCmdBlitImage with VK_FILTER_LINEAR
- Metal: Uses MPSImageGaussianBlur or texture blit
- DirectX: Common pattern is compute shader with CS_5_0

TEST_SESSIONS:
- BindGroupSession (uses mipmaps)
- Textured3DCubeSession (calls generateMipmap)

EXPECTED_RESULT: Textures have smooth LOD transitions
```

### 4. Compute Pipeline Support

```
FEATURE_NAME: Compute Pipeline and Dispatch
REQUIRED_FOR: Future compute-based sessions, mipmap generation
CURRENT_STATUS: Not implemented
ISSUE: No compute shader compilation or dispatch support

FILES_TO_MODIFY:
Primary:
- src/igl/d3d12/Device.cpp - createComputePipeline()
- src/igl/d3d12/ComputeCommandEncoder.cpp - NEW FILE
- src/igl/d3d12/ComputeCommandEncoder.h - NEW FILE
- src/igl/d3d12/CommandBuffer.cpp - createComputeCommandEncoder()

TECHNICAL_REQUIREMENTS:
1. Compile HLSL compute shaders (CS_5_0 or CS_6_0)
2. Create D3D12_COMPUTE_PIPELINE_STATE_DESC
3. Create root signature for compute (CBV/SRV/UAV)
4. Dispatch compute work: Dispatch(groupX, groupY, groupZ)
5. Handle UAV barriers for write-after-read hazards

D3D12_API_CALLS:
- CreateComputePipelineState()
- SetComputeRootSignature()
- SetPipelineState() for compute PSO
- Dispatch()

COMPUTE_ENCODER_INTERFACE:
```cpp
class ComputeCommandEncoder {
  void bindComputePipelineState(const std::shared_ptr<IComputePipelineState>& pipelineState);
  void bindBuffer(uint32_t index, const std::shared_ptr<IBuffer>& buffer);
  void bindTexture(uint32_t index, const std::shared_ptr<ITexture>& texture);
  void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
  void endEncoding();
};
```

TEST_APPROACH:
1. Create simple compute shader that fills UAV buffer
2. Verify buffer contents after dispatch
3. Create compute-based mipmap generator
4. Test with textured sessions

EXPECTED_RESULT: Compute shaders execute correctly, UAVs writable
```

---

## Validation and Testing Checklist

After implementing any feature, run through this checklist:

### Build Verification
```bash
# Clean build
cmake --build build --config Debug --target ALL_BUILD -j 8

# Check for compilation errors
# Check for linker warnings
```

### Unit Tests
```bash
# Run all tests
./build/src/igl/tests/Debug/IGLTests.exe --gtest_brief=1

# Check for new failures
# Expected: 1709+ passing, 230 or fewer failing
```

### Session Testing
```bash
# Test affected sessions manually
./build/shell/Debug/{SESSION}_d3d12.exe --viewport-size 640x360

# Capture screenshot for validation
./build/shell/Debug/{SESSION}_d3d12.exe --viewport-size 640x360 --screenshot-number 10 --screenshot-file artifacts/captures/d3d12/{SESSION}/640x360/{SESSION}.png

# Check logs for errors
# Look for: [IGL] Error, Device removed, ABORT, validation errors
```

### Automated Validation
```bash
cd tools/validation
python validate_d3d12.py

# Expected output:
# - No regressions in previously passing sessions
# - Newly fixed sessions show improvement
# - Exit code 0 if no regressions
```

### Visual Inspection
- Run session in window (no --screenshot flag)
- Observe rendering for full 5-10 seconds
- Check for:
  - Black/white/incorrect geometry
  - Missing textures
  - Flickering or artifacts
  - Crashes after initial frames

### Documentation Update
```bash
# Update status in required features doc
vim docs/D3D12_REQUIRED_FEATURES.md

# Mark feature as implemented
# Update session status
# Add to version history
```

---

## Common Pitfalls and Solutions

### Pitfall 1: HLSL Semantic Mismatches
**Problem:** Vertex input semantics don't match shader
**Solution:** Use shader reflection to verify - logs show expected semantics
```
Shader expects 3 input parameters:
  [0]: POSITION0 (semantic index 0), mask 0x07
  [1]: TEXCOORD0 (semantic index 0), mask 0x03
```

### Pitfall 2: Wrong Texture View Dimension
**Problem:** Using TEXTURE2D view for TEXTURE3D resource
**Solution:** Always query resourceDesc.Dimension before creating SRV
```cpp
auto resourceDesc = resource->GetDesc();
if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
}
```

### Pitfall 3: Resource State Transitions
**Problem:** Resource in wrong state for operation
**Solution:** Track states, transition before use
```cpp
texture->transitionTo(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
```

### Pitfall 4: Descriptor Heap Exhaustion
**Problem:** Running out of descriptor slots
**Solution:** Use unbounded ranges or increase heap size
```cpp
srvRange.NumDescriptors = UINT_MAX;  // Unbounded
```

### Pitfall 5: SRGB Format Confusion
**Problem:** SRGB vs UNORM format mismatches
**Solution:** Use actual resource format from D3D12, not IGL format
```cpp
D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
rtvDesc.Format = resourceDesc.Format;  // Not textureFormatToDXGIFormat()
```

---

## Git Commit Guidelines

### Commit Message Format
```
[D3D12] {Component}: {Brief description}

{Detailed description of what was changed and why}

{List of modified files}
{Test results}
{Session status changes}
```

### Example Commits

#### Shader Addition
```
[D3D12] ColorSession: Add D3D12 HLSL shaders

Implemented D3D12 shaders for both textured and gradient modes.
Shader interface matches Vulkan/Metal with cbuffer for uniforms,
t0 for texture, s0 for sampler.

Modified:
- shell/renderSessions/ColorSession.cpp (lines 350-458)

Tested:
- ColorSession_d3d12.exe runs without crashes
- Validation: No regressions detected

Status: ColorSession CRASH → PASS
```

#### Feature Implementation
```
[D3D12] Texture: Implement 3D texture rendering support

Added proper 3D texture creation, SRV setup, and upload handling.
Fixed dimension check in bindTexture() to correctly handle TEXTURE3D.

Modified:
- src/igl/d3d12/Device.cpp - createTexture() depth handling
- src/igl/d3d12/RenderCommandEncoder.cpp - 3D SRV creation
- src/igl/d3d12/Texture.cpp - 3D upload with depthPitch

Tested:
- Textured3DCubeSession now renders textured cube correctly
- Visual parity: 99.5% similarity to Vulkan baseline

Status: Textured3DCubeSession VISUAL_FAIL → PASS
```

---

## Resources

### D3D12 Documentation
- [D3D12 Programming Guide](https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [HLSL Reference](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-reference)
- [Resource Binding](https://docs.microsoft.com/en-us/windows/win32/direct3d12/resource-binding)

### IGL Reference Implementations
- **Vulkan Backend:** `src/igl/vulkan/` - Most complete reference
- **Metal Backend:** `src/igl/metal/` - Apple platforms
- **OpenGL Backend:** `src/igl/opengl/` - Cross-platform GL

### Debugging Tools
- **PIX for Windows:** GPU debugging and profiling
- **RenderDoc:** Frame capture and analysis (limited D3D12 support)
- **Visual Studio Graphics Debugger:** Built-in GPU debugging

---

## Version History

- **2025-10-26:** Initial subagent prompt document created
  - Added shader implementation template
  - Added feature implementation template
  - Added specific prompts for 3D textures, texture views, mipmaps, compute
  - Added validation checklist and common pitfalls
