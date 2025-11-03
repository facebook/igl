# Task: Implement GPU timers via timestamp queries

Context
- Issue IDs: DX12-020, DX12-123
- Symptom: `Device::createTimer` unimplemented; no GPU timing support.
- Code refs: `src/igl/d3d12/Device.cpp:543-549`

What to deliver
- Timestamp query heap and resolve path; start/stop markers around passes.
- CPU calibration and units; minimal API to read durations.
- Validation plan: sanity-check nonzero durations; compare relative costs across passes.

Constraints
- Low overhead; optional when not requested.

References
- ID3D12QueryHeap: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_query_heap_desc
- ResolveQueryData: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resolvequerydata
- MiniEngine timers

Acceptance Criteria
- Timers return plausible values; no validation errors; tests unaffected when timers disabled.

