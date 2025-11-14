# Task: Implement D3D12 Texture2DArray support

Context
- Issue ID: DX12-014
- Symptom: TextureArrayFloatTest suite (11 tests) fails with `stages == nullptr` assertion
- Root cause: D3D12 backend branch not implemented in test shader setup
- Code refs: `src/igl/tests/TextureArrayFloat.cpp:134-180`, `src/igl/tests/data/ShaderData.h`

What to deliver
- Create D3D12 HLSL vertex shader for texture array sampling (kD3D12SimpleVertShaderTex2dArray)
  - cbuffer with layer uniform at register(b2)
  - Pass layer as uint to pixel shader via semantic
- Create D3D12 HLSL pixel shader for texture array sampling (kD3D12SimpleFragShaderTex2dArray)
  - Texture2DArray<float4> resource at register(t0)
  - SamplerState at register(s0)
  - Sample with .Sample(sampler, float3(uv.xy, layer))
- Add shaders to ShaderData.h following existing D3D12 naming conventions
- Add D3D12 backend branch in TextureArrayFloat.cpp calling createShaderStages()
- Ensure RenderCommandEncoder creates D3D12_SRV_DIMENSION_TEXTURE2DARRAY descriptors correctly

Constraints
- Must NOT regress render sessions (all existing sessions must continue working)
- Must pass all 11 TextureArrayFloatTest unit tests:
  - Upload_SingleUpload
  - Upload_LayerByLayer
  - Upload_SingleUpload_ModifySubTexture
  - Upload_LayerByLayer_ModifySubTexture
  - UploadToMip_SingleUpload
  - UploadToMip_LayerByLayer
  - Passthrough_SampleFromArray
  - Passthrough_RenderToArray
  - ValidateRange2DArray
  - GetEstimatedSizeInBytes
  - GetRange
- Support both compressed and uncompressed texture formats

References
- Vulkan reference shaders: ShaderData.h:553-596 (kVulkanSimpleVertShaderTex2dArray, kVulkanSimpleFragShaderTex2dArray)
- D3D12 cube texture reference: ShaderData.h:796-831 (kD3D12SimpleVertShaderCube, kD3D12SimpleFragShaderCube)
- HLSL Texture2DArray docs: https://learn.microsoft.com/windows/win32/direct3dhlsl/sm5-object-texture2darray
- D3D12 SRV dimension: https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_srv_dimension

Acceptance Criteria
- All 11 TextureArrayFloatTest tests pass on D3D12 backend
- No regression in existing render sessions (all sessions run without errors)
- Texture array layer sampling returns correct per-layer data
- Implementation matches Vulkan/Metal test behavior
