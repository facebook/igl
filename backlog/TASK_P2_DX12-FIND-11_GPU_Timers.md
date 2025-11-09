# TASK_P2_DX12-FIND-11: Implement GPU Timestamp Query Support

**Priority:** P2 - Medium
**Estimated Effort:** 2-3 days
**Status:** Open
**Issue ID:** DX12-FIND-11 (from Codex Audit)

---

## Problem Statement

`Device::createTimer()` returns `Unimplemented`, preventing GPU timestamp queries and profiling. Applications cannot measure GPU execution time for performance analysis and optimization.

### Current Behavior
- `createTimer()` returns `Result::Code::Unimplemented`
- No GPU timestamp query support
- No GPU profiling capability

### Expected Behavior
- Implement GPU timer API using D3D12 timestamp queries
- Support begin/end timer operations
- Return GPU elapsed time
- Match Vulkan/OpenGL timer functionality

---

## Evidence and Code Location

**File:** `src/igl/d3d12/Device.cpp`

**Search Pattern:**
Find around line 657:
```cpp
std::unique_ptr<ITimer> Device::createTimer(...) {
    return Result::Code::Unimplemented;
}
```

**Search Keywords:**
- `createTimer`
- `Unimplemented`
- GPU timer
- Timestamp query

---

## Impact

**Severity:** Medium - No GPU profiling
**Tooling:** Cannot measure GPU performance
**Development:** Blocks performance optimization work

**Affected Use Cases:**
- GPU performance profiling
- Frame time breakdown
- Optimization work
- Performance HUDs

---

## Official References

### Microsoft Documentation
1. **Timestamp Queries** - [Time Stamp Queries](https://learn.microsoft.com/en-us/windows/win32/direct3d12/time-stamp-queries)
2. **ID3D12QueryHeap** - [D3D12_QUERY_HEAP_TYPE_TIMESTAMP](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_query_heap_type)
3. **EndQuery** - [ID3D12GraphicsCommandList::EndQuery](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-endquery)

### Pattern
```cpp
// Create query heap
D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
queryHeapDesc.Count = maxQueries;
device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap));

// Record timestamp
commandList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, queryIndex);

// Resolve query results
commandList->ResolveQueryData(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP,
                              startIndex, count, resolveBuffer, offset);
```

---

## Implementation Guidance

### High-Level Approach

1. **Create Timer Class**
   ```cpp
   class D3D12Timer : public ITimer {
       ComPtr<ID3D12QueryHeap> queryHeap_;
       ComPtr<ID3D12Resource> resolveBuffer_;
       uint32_t startQueryIndex_;
       uint32_t endQueryIndex_;
   };
   ```

2. **Implement Timer Operations**
   - `start()` - Record begin timestamp
   - `stop()` - Record end timestamp
   - `getElapsedTime()` - Calculate difference

3. **Query Heap Management**
   - Allocate query heap with multiple slots
   - Manage query index allocation
   - Resolve queries to readback buffer

### Detailed Design

**Timer Class:**
```cpp
class D3D12Timer : public ITimer {
public:
    void start(ICommandBuffer& cmdBuf) override;
    void stop(ICommandBuffer& cmdBuf) override;
    float getElapsedTimeInMs() override;

private:
    ID3D12QueryHeap* queryHeap_;
    ID3D12Resource* resolveBuffer_;
    uint32_t beginIndex_;
    uint32_t endIndex_;
    uint64_t frequency_;  // GPU timestamp frequency
};
```

**Start/Stop Implementation:**
```cpp
void D3D12Timer::start(ICommandBuffer& cmdBuf) {
    auto* d3dCmdList = static_cast<CommandBuffer&>(cmdBuf).getD3D12CommandList();
    d3dCmdList->EndQuery(queryHeap_, D3D12_QUERY_TYPE_TIMESTAMP, beginIndex_);
}

void D3D12Timer::stop(ICommandBuffer& cmdBuf) {
    auto* d3dCmdList = static_cast<CommandBuffer&>(cmdBuf).getD3D12CommandList();
    d3dCmdList->EndQuery(queryHeap_, D3D12_QUERY_TYPE_TIMESTAMP, endIndex_);

    // Resolve queries
    d3dCmdList->ResolveQueryData(
        queryHeap_,
        D3D12_QUERY_TYPE_TIMESTAMP,
        beginIndex_,
        2,  // begin + end
        resolveBuffer_,
        0
    );
}
```

**Get Elapsed Time:**
```cpp
float D3D12Timer::getElapsedTimeInMs() {
    // Map resolve buffer
    uint64_t* timestamps = nullptr;
    resolveBuffer_->Map(0, nullptr, (void**)&timestamps);

    uint64_t beginTime = timestamps[0];
    uint64_t endTime = timestamps[1];

    resolveBuffer_->Unmap(0, nullptr);

    // Convert to milliseconds
    uint64_t delta = endTime - beginTime;
    return (float)delta / (float)frequency_ * 1000.0f;
}
```

**Get GPU Frequency:**
```cpp
// In Device initialization
uint64_t frequency;
commandQueue->GetTimestampFrequency(&frequency);
```

---

## Testing Requirements

```bash
test_unittests.bat
test_all_sessions.bat
```

### Validation Steps

1. **Basic Timer Test**
   ```cpp
   auto timer = device->createTimer();
   timer->start(cmdBuf);
   // ... GPU work ...
   timer->stop(cmdBuf);
   float elapsed = timer->getElapsedTimeInMs();
   // Verify elapsed > 0
   ```

2. **Multiple Timers**
   - Create multiple concurrent timers
   - Verify all return correct times

3. **Accuracy**
   - Compare with CPU timing
   - Verify reasonable values

---

## Success Criteria

- [ ] `Device::createTimer()` returns working timer object
- [ ] `ITimer::start()` records begin timestamp
- [ ] `ITimer::stop()` records end timestamp
- [ ] `ITimer::getElapsedTimeInMs()` returns correct GPU time
- [ ] Query heap and resolve buffer managed properly
- [ ] GPU timestamp frequency queried correctly
- [ ] All tests pass
- [ ] Timer returns reasonable values
- [ ] User confirms GPU profiling works

---

## Dependencies

**None** - Standalone feature implementation

---

**Estimated Timeline:** 2-3 days
**Risk Level:** Medium
**Validation Effort:** 2-3 hours

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 11*
