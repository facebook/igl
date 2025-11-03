# Task: Implement debug labels/markers for PIX and tooling

Context
- Issue IDs: DX12-019
- Symptom: push/pop markers stubbed; hampers debugging.
- Code refs: `src/igl/d3d12/CommandBuffer.cpp:153-175`

What to deliver
- Use PIX events (pix3.h) or `ID3DUserDefinedAnnotation` where applicable for event scopes.
- API mapping from engine labels to D3D12 events; no-ops when unavailable.
- Validation plan: capture in PIX; verify labeled ranges around encoders/draws.

Constraints
- Zero overhead when disabled; thread-safe.

References
- PIX markers: https://devblogs.microsoft.com/pix/download/
- User annotations: https://learn.microsoft.com/windows/win32/api/d3d11/nn-d3d11-id3duserdefinedannotation (conceptual)

Acceptance Criteria
- Labels visible in PIX; no impact on tests/rendering when disabled.

