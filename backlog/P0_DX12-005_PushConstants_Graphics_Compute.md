# Task: Correct push constant binding for graphics and compute

Context
- Issue IDs: DX12-005, DX12-103, DX12-104
- Symptoms: Graphics root param declared as 32-bit constants but bound via CBV; compute push constants unimplemented.
- Code refs: `src/igl/d3d12/Device.cpp`, `src/igl/d3d12/RenderCommandEncoder.cpp`, `src/igl/d3d12/ComputeCommandEncoder.cpp`

What to deliver
- Correct mapping: use `SetGraphics/ComputeRoot32BitConstants` when layout requests constants; use small dynamic CBV otherwise.
- Update root signature declaration to match binding path; handle size/offset packing rules.
- Pseudocode + diffs in encoders and device setup; ensure reflection/plumbing consistent.
- Validation plan: shader that reads push constants in VS/PS/CS; compare against Vulkan/GL outputs.

Constraints
- Do not break existing pipelines; handle both constant and CBV paths.

References
- Root signatures & constants: https://learn.microsoft.com/windows/win32/direct3d12/root-signatures
- SetGraphicsRoot32BitConstants: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setgraphicsroot32bitconstants
- SetComputeRoot32BitConstants: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setcomputeroot32bitconstants
- Samples: MiniEngine constant data usage

Acceptance Criteria
- Constants read correctly in graphics/compute; tests/render sessions remain stable.

