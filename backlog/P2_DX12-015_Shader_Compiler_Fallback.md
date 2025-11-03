# Task: DXC primary + FXC fallback policy

Context
- Issue IDs: DX12-015, DX12-116
- Symptom: DXC unavailable causes failure despite SM5 fallback capability.
- Code refs: `src/igl/d3d12/DXCCompiler.cpp`, `src/igl/d3d12/Device.cpp:1471`

What to deliver
- Policy: Prefer DXC (SM6) with validator; fallback to FXC (SM5) only when features permit.
- Map shader model and root signature features to required compiler; error reporting.
- Validation plan: build shaders with/without DXC present; render correctness unchanged.

Constraints
- Keep reflection and binding layout consistent across compilers.

References
- DXC: https://github.com/microsoft/DirectXShaderCompiler
- FXC: https://learn.microsoft.com/windows/win32/direct3dtools/dx-graphics-tools-fxc-syntax
- Root signatures & SM compatibility: https://learn.microsoft.com/windows/win32/direct3d12/root-signatures

Acceptance Criteria
- Shaders compile/run via DXC or FXC as appropriate; tests pass.

