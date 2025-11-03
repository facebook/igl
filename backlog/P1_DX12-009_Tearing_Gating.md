# Task: Correct DXGI tearing gating and swapchain flags

Context
- Issue IDs: DX12-009, DX12-106
- Symptom: Uses `DXGI_PRESENT_ALLOW_TEARING` without creating swapchain with `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`.
- Code refs: `src/igl/d3d12/CommandQueue.cpp:85-121`, `src/igl/d3d12/D3D12Context.cpp:260-292`

What to deliver
- Check `DXGI_FEATURE_PRESENT_ALLOW_TEARING`; set swapchain flag when vsync off.
- Validation plan: present with vsync on/off; ensure no invalid Present and correct behavior.

Constraints
- Retain existing vsync control API.

References
- DXGI tearing: https://learn.microsoft.com/windows/win32/direct3ddxgi/variable-refresh-rate-displays

Acceptance Criteria
- No invalid Present calls; tearing only when allowed; tests/render sessions OK.

