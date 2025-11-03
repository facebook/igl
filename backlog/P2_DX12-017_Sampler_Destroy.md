# Task: Implement Device::destroy(SamplerHandle)

Context
- Issue IDs: DX12-017, DX12-117
- Symptom: Stubbed; potential descriptor leaks.
- Code refs: `src/igl/d3d12/Device.cpp:70-76`

What to deliver
- Descriptor heap free path for samplers; integrate with heap manager/free lists.
- Validation plan: create/destroy sampler stress; verify descriptor reuse and no leaks.

Constraints
- Thread-safe; align with descriptor heap management changes.

References
- Descriptor heaps: https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps

Acceptance Criteria
- No sampler descriptor leaks; tests pass.

