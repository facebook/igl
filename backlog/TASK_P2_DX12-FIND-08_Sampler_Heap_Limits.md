# TASK_P2_DX12-FIND-08: Remove 32 Sampler Hard Limit and Support Dynamic Growth

**Priority:** P2 - Medium
**Estimated Effort:** 1-2 days
**Status:** Open
**Issue ID:** DX12-FIND-08 (from Codex Audit)

---

## Problem Statement

Sampler descriptor heaps are hard-coded to 32 descriptors per frame, while D3D12 spec allows 2048 and Vulkan backend dynamically grows to hardware limits. Complex scenes (especially with ImGui + many textures) easily exceed 32 samplers, causing crashes.

### Current Behavior
- Sampler heap fixed at 32 descriptors
- `kMaxSamplers = 32` hardcoded constant
- Exceeding 32 samplers causes overflow or crash
- Vulkan backend supports 2048+

### Expected Behavior
- Dynamic sampler heap growth
- Support up to `D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE` (2048)
- Match Vulkan backend capability
- No arbitrary limits

---

## Evidence and Code Location

**Files:**
- `src/igl/d3d12/Common.h:33` - `kMaxSamplers = 32` definition
- `src/igl/d3d12/D3D12Context.cpp:528-541` - Sampler heap creation

**Search Keywords:**
- `kMaxSamplers`
- `MAX_SHADER_VISIBLE_SAMPLER_HEAP`
- Sampler heap creation

**Comparison:**
- **Vulkan:** `src/igl/vulkan/VulkanContext.cpp:1125-1170` - Dynamic growth to device limits

---

## Impact

**Severity:** Medium - Limits complex scenes
**Scene Complexity:** 32 sampler limit too restrictive
**Portability:** Vulkan allows 2048, D3D12 artificially limited

**Affected Scenarios:**
- ImGui + many textured objects
- Complex materials with multiple samplers
- Large scenes

---

## Official References

### Microsoft Documentation
1. **Sampler Descriptor Heaps** - [D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)
   - Maximum: 2048 samplers

2. **Dynamic Descriptor Management** - Best practices

---

## Implementation Guidance

### High-Level Approach

1. **Query Hardware Limit**
   - Use `D3D12_FEATURE_DATA_D3D12_OPTIONS`
   - Respect hardware maximum (typically 2048)

2. **Dynamic Growth Strategy**
   - Start with reasonable size (e.g., 64)
   - Grow when approaching capacity
   - Or allocate 2048 upfront (simple)

3. **Update Constant**
   ```cpp
   // Change:
   constexpr uint32_t kMaxSamplers = 32;
   // To:
   constexpr uint32_t kMaxSamplers = 2048;  // D3D12 spec limit
   ```

### Detailed Steps

**Step 1: Update kMaxSamplers Constant**

File: `src/igl/d3d12/Common.h`
```cpp
// Before:
constexpr uint32_t kMaxSamplers = 32;

// After:
constexpr uint32_t kMaxSamplers = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;  // 2048
```

**Step 2: Update Heap Creation**

File: `src/igl/d3d12/D3D12Context.cpp`
```cpp
D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
samplerHeapDesc.NumDescriptors = kMaxSamplers;  // Now 2048
samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
```

**Step 3: (Optional) Dynamic Growth**

For more sophisticated approach:
- Track usage
- Grow heap when >80% full
- Requires heap switching logic

---

## Testing Requirements

```bash
test_unittests.bat
test_all_sessions.bat
```

### Validation Steps

1. **Sampler Count Test**
   - Create scene with 100+ unique samplers
   - Verify no crashes or overflow
   - Check all samplers work correctly

2. **ImGui Stress Test**
   - ImGui typically uses many samplers
   - Test with ImGui + complex scene

---

## Success Criteria

- [ ] `kMaxSamplers` increased to 2048 (or dynamic)
- [ ] Sampler heap supports 2048 descriptors
- [ ] Scenes with >32 samplers work correctly
- [ ] All tests pass
- [ ] No crashes from sampler overflow
- [ ] User confirms increased limit works

---

## Dependencies

**Related:**
- Cross-API portability with Vulkan

---

**Estimated Timeline:** 1-2 days
**Risk Level:** Low-Medium
**Validation Effort:** 2-3 hours

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 08*
