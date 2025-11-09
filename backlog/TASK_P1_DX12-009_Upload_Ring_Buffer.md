# TASK_P1_DX12-009: Implement Upload Ring Buffer for Streaming Resources

**Priority:** P1 - High
**Estimated Effort:** 1-2 weeks
**Status:** Open

---

## Problem Statement

The D3D12 backend allocates separate staging buffers for each resource upload, causing allocator churn, memory fragmentation, and synchronization overhead. A ring buffer pattern would reuse staging memory across frames, reducing allocations and improving performance for streaming scenarios.

### Current Behavior
- Each upload creates new staging buffer
- Allocator creates/destroys many small buffers
- Memory fragmentation increases over time
- No reuse of staging memory across frames

### Expected Behavior
- Ring buffer allocates large staging region
- Sub-allocate from ring buffer for uploads
- Recycle staging memory after GPU completion
- Reduced allocations, better performance

---

## Evidence and Code Location

**Search Pattern:**
1. Find buffer/texture upload functions
2. Look for staging buffer allocation
3. Find `D3D12MA::Allocator` usage for uploads

**Files:**
- `src/igl/d3d12/Buffer.cpp` - Buffer uploads
- `src/igl/d3d12/Texture.cpp` - Texture uploads
- `src/igl/d3d12/Device.cpp` - Staging buffer management

**Typical Pattern:**
```cpp
// Creates new staging buffer each time
ComPtr<ID3D12Resource> stagingBuffer;
allocator_->CreateResource(..., &stagingBuffer);
// Upload data
// GPU copy
// Staging buffer released (or leaked)
```

---

## Impact

**Severity:** High - Performance and memory efficiency
**Performance:** Allocator overhead on upload-heavy workloads
**Memory:** Fragmentation and waste

**Affected Scenarios:**
- Streaming textures
- Dynamic buffers (frequent updates)
- Large data uploads
- Long-running applications

---

## Official References

### Microsoft Documentation
1. **Upload Heaps** - [Uploading Resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources)
2. **Ring Buffer Pattern** - Common in D3D12 samples

### DirectX-Graphics-Samples
- **MiniEngine** - Implements sophisticated ring buffer allocator
- **D3D12nBodyGravity** - Per-frame upload buffer pattern

### Best Practice
- Allocate large upload heap (e.g., 64MB-256MB)
- Sub-allocate linearly for uploads
- Track fence values for retirement
- Wrap around when reaching end

---

## Implementation Guidance

### High-Level Approach

1. **Create Upload Ring Buffer Class**
   - Manages large upload heap resource
   - Tracks allocation offset and available space
   - Fence-based retirement of used regions

2. **Sub-Allocator Interface**
   ```cpp
   struct UploadAllocation {
       void* cpuAddress;
       D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
       uint64_t offset;
       uint64_t size;
   };

   UploadAllocation allocate(uint64_t size, uint64_t alignment);
   void retire(uint64_t fenceValue);  // Mark allocations as free
   ```

3. **Integration**
   - Modify `Buffer::upload()` to use ring buffer
   - Modify `Texture::upload()` to use ring buffer
   - Track fence for each allocation

### Detailed Design

**Ring Buffer Class:**
```cpp
class UploadRingBuffer {
public:
    UploadRingBuffer(ID3D12Device* device, uint64_t size);

    // Allocate staging memory for upload
    UploadAllocation allocate(uint64_t size, uint64_t alignment);

    // Called when fence signaled to reclaim memory
    void retire(uint64_t completedFenceValue);

private:
    ComPtr<ID3D12Resource> uploadHeap_;
    void* cpuBase_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuBase_ = 0;

    uint64_t size_ = 0;
    uint64_t head_ = 0;  // Next allocation offset
    uint64_t tail_ = 0;  // Oldest in-use allocation

    struct PendingAllocation {
        uint64_t offset;
        uint64_t size;
        uint64_t fenceValue;
    };
    std::queue<PendingAllocation> pendingAllocations_;
};
```

**Usage in Upload:**
```cpp
void Buffer::upload(const void* data, size_t size) {
    // Allocate from ring buffer instead of creating new staging buffer
    auto allocation = device_->getUploadRingBuffer()->allocate(size, 256);

    // Copy CPU data to staging
    memcpy(allocation.cpuAddress, data, size);

    // Record GPU copy command
    commandList->CopyBufferRegion(
        targetBuffer,
        0,
        uploadHeap,
        allocation.offset,
        size
    );

    // Track fence for retirement
    uint64_t fenceValue = submitCommands();
    allocation.fenceValue = fenceValue;
}
```

---

## Testing Requirements

### Mandatory Tests
```bash
test_unittests.bat
test_all_sessions.bat
```

### Validation Steps

1. **Functional Correctness**
   - All uploads work correctly
   - No corruption in uploaded data

2. **Memory Efficiency**
   - Monitor allocator calls (should decrease significantly)
   - Check memory usage (should be stable)

3. **Performance**
   - Measure upload performance (should improve for many uploads)
   - Check frame times in upload-heavy scenarios

4. **Ring Buffer Behavior**
   - Verify wraparound works correctly
   - Verify retirement happens properly
   - No out-of-memory when reusing correctly

---

## Success Criteria

- [ ] UploadRingBuffer class implemented
- [ ] Ring buffer integrated into buffer uploads
- [ ] Ring buffer integrated into texture uploads
- [ ] Fence-based retirement working
- [ ] Reduced allocator churn (measurable)
- [ ] All tests pass
- [ ] No upload data corruption
- [ ] Performance improvement in upload scenarios
- [ ] User confirms improved performance

---

## Dependencies

**Depends On:**
- TASK_P0_DX12-005 (Upload Buffer Sync) - Proper fence tracking required

**Enhances:**
- TASK_P2_DX12-FIND-07 (Upload GPU Wait) - Works together for upload optimization

---

## Restrictions

1. **Test Immutability:** ❌ DO NOT modify test scripts
2. **Complexity:** Large implementation, requires careful testing
3. **Thread Safety:** Consider multi-threaded upload scenarios

---

**Estimated Timeline:** 1-2 weeks
**Risk Level:** Medium-High (complex system, affects all uploads)
**Validation Effort:** 1 week (thorough testing required)

---

*Task Created: 2025-11-08*
