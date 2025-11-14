# Task: Implement bindBytes for graphics and compute

Context
- Issue IDs: DX12-021, DX12-022, DX12-118, DX12-119
- Symptom: bindBytes no-op in render/compute; DeviceFeatures indicates support.
- Code refs: `src/igl/d3d12/RenderCommandEncoder.cpp:546-553,568`, `src/igl/d3d12/ComputeCommandEncoder.cpp:351-354`

What to deliver
- Small constant data path via root 32-bit constants or dynamic CBV with proper alignment.
- API surface mapping (size, offset) and packing rules.
- Validation plan: shaders reading bindBytes data; compare with Vulkan/GL backend outputs.

Constraints
- Honor 256B CBV alignment if using CBV; avoid excessive root parameters.

References
- Root constants: https://learn.microsoft.com/windows/win32/direct3d12/root-signatures
- Constant buffer alignment: `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`

Acceptance Criteria
- bindBytes functional in graphics/compute; tests/render sessions unchanged otherwise.

