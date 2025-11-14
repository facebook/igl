# Task I-007: Timestamp Query Frequency Alignment with Vulkan

## 1. Problem Statement

### Current Behavior
The D3D12 timestamp query implementation provides GPU timestamps, but the frequency/resolution differs from Vulkan's implementation. This creates cross-platform inconsistency where timing measurements vary between backends, making performance profiling and timing-critical code difficult.

### The Danger
Without aligned timestamp query frequencies:
- **Cross-Platform Inconsistency**: Timing measurements differ between D3D12 and Vulkan backends
- **Performance Analysis Errors**: Profiling data unreliable when comparing implementations
- **Timing-Critical Code Issues**: Applications relying on GPU timing behave differently per backend
- **Developer Confusion**: Non-obvious source of discrepancies in performance debugging
- **API Compliance**: Fails to provide consistent timing semantics across platforms

### Impact
- Performance profiling produces conflicting data
- Difficult to port and compare performance between graphics backends
- Timing-based game logic may behave differently per platform
- Development and debugging becomes more complex

---

## 2. Root Cause Analysis

### Current Implementation Limitations

**File Location**: Search for: `TimestampQuery` or `QueryHeap` related to timestamps

The current timestamp implementation:
```cpp
// Current implementation - D3D12-specific frequency
class Device {
    void getTimestampFrequency(uint64_t* outFrequency) {
        // Returns D3D12 GPU timestamp frequency
        // This may differ from Vulkan's frequency
        D3D12CommandQueue_->GetTimestampFrequency(outFrequency);
    }
};

// D3D12 uses GPU-specific frequency
// Vulkan uses nanoseconds (1 GHz = 1,000,000,000 Hz)
// Direct mapping not possible
```

### Why It's Not Aligned

1. **API Differences**: D3D12 uses GPU-specific frequency, Vulkan uses nanosecond precision
2. **No Abstraction Layer**: Direct D3D12 frequency exposed without normalization
3. **No Conversion Logic**: Applications must handle different frequencies manually
4. **Inconsistent Semantics**: What 1 tick means differs per backend

### Relevant Code Locations
- Search for: `QueryHeap`, `QUERY_TYPE_TIMESTAMP`
- Search for: `getTimestampFrequency` or similar
- Search for: `Device::queryPoolCreate` - Query pool creation
- Search for: `Vulkan` implementation of timestamp queries (for reference)
- Search for: `IDevice::queryPoolCreate` - Cross-platform interface

---

## 3. Official Documentation References

### Microsoft Direct3D 12 Documentation

1. **Query Heaps and Queries**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d12/queries
   - Focus: Timestamp query types, frequency, retrieval

2. **ID3D12CommandQueue::GetTimestampFrequency**
   - https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-gettimestampfrequency
   - Focus: GPU timestamp frequency in Hz

3. **Timestamp Queries in Direct3D 12**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d12/understanding-query-heaps
   - Focus: Query heap creation, timestamp resolution

4. **Performance Timing**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d12/performance-timing-measurements
   - Focus: Best practices for GPU timing

### Vulkan Documentation (Reference)

1. **VK_EXT_calibrated_timestamps**
   - https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_calibrated_timestamps.html
   - Focus: Nanosecond precision timestamps

2. **VkQueryPool - Timestamp Queries**
   - https://www.khronos.org/registry/vulkan/specs/1.3/html/vkspec.html#queries-timestamps
   - Focus: Vulkan timestamp semantics (nanoseconds)

### Sample Code References
- **Microsoft Samples**: Direct3D 12 Queries
  - GitHub: https://github.com/microsoft/DirectX-Graphics-Samples
  - File: `Samples/Desktop/D3D12Multithreading/src/FrameResource.cpp` (contains timing)

---

## 4. Code Location Strategy

### Pattern-Based File Search

**Search for these patterns to understand current structure:**

1. **Query Heap Implementation**
   - Search for: `class QueryHeap` or `ID3D12QueryHeap`
   - File: `src/igl/d3d12/QueryHeap.h` or similar
   - What to look for: Timestamp query type handling

2. **Timestamp Query Creation**
   - Search for: `createQueryPool` or `QUERY_TYPE_TIMESTAMP`
   - File: `src/igl/d3d12/Device.cpp` and `QueryHeap.cpp`
   - What to look for: Frequency retrieval and query setup

3. **Timestamp Retrieval**
   - Search for: `getQueryPoolResults` or `resolveQueryData`
   - File: `src/igl/d3d12/CommandBuffer.cpp` and `QueryHeap.cpp`
   - What to look for: Raw frequency used in calculations

4. **Cross-Platform Interface**
   - Search for: `IQueryPool` or `QueryPoolDesc`
   - File: `include/igl/IQueryPool.h` or similar
   - What to look for: Public API for query results

5. **Vulkan Implementation (for reference)**
   - Search for: `VulkanQueryPool` or `VulkanDevice`
   - File: `src/igl/vulkan/QueryPool.cpp`
   - What to look for: How Vulkan handles timestamps (nanoseconds)

---

## 5. Detection Strategy

### How to Identify the Problem

**Reproduction Steps:**
1. Create timing test on both backends:
   ```cpp
   // Pseudo-code for reproduction
   struct TimingTest {
       void runOnBothBackends() {
           // D3D12 backend
           auto d3d12Device = createDevice(BackendType::D3D12);
           uint64_t d3d12Frequency;
           d3d12Device->getTimestampFrequency(&d3d12Frequency);
           // Example: 10,000,000 Hz (10 MHz)

           // Vulkan backend
           auto vulkanDevice = createDevice(BackendType::Vulkan);
           uint64_t vulkanFrequency;
           vulkanDevice->getTimestampFrequency(&vulkanFrequency);
           // Example: 1,000,000,000 Hz (1 GHz - nanoseconds)

           // Frequencies differ significantly!
           printf("D3D12: %llu Hz\n", d3d12Frequency);
           printf("Vulkan: %llu Hz\n", vulkanFrequency);
           // Output: D3D12: 10000000, Vulkan: 1000000000
           // Same time period has different tick counts!
       }
   };
   ```

2. Create a timed GPU operation:
   ```cpp
   // Example: time a simple draw call
   QueryPoolDesc queryDesc{QueryType::Timestamp, 2};  // 2 queries
   IQueryPool* queryPool = nullptr;
   device->createQueryPool(queryDesc, &queryPool);

   // Stamp before operation
   commandBuffer->addTimestampQuery(queryPool, 0);

   // Do work...
   drawMesh();

   // Stamp after operation
   commandBuffer->addTimestampQuery(queryPool, 1);

   // Get results
   uint64_t results[2];
   queryPool->getResults(0, 2, results, sizeof(results));

   // Calculate duration in milliseconds
   // D3D12: (results[1] - results[0]) / (frequency_in_hz / 1000)
   // Vulkan: (results[1] - results[0]) / 1,000,000  (always nanoseconds)

   // Different math required per backend!
   ```

3. Measure frequency differences:
   - Run: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Query*"`
   - Look for: Raw frequency values in test output
   - Compare with: Vulkan backend frequency

4. Create cross-platform timing test:
   - Run same timing code on both backends
   - Verify: Same GPU operation produces same timing result

### Success Criteria
- Both D3D12 and Vulkan return consistent timestamp frequency
- Timestamp queries produce identical durations for same GPU work
- Frequency is normalized to Vulkan semantics (or documented standard)
- Cross-platform timing comparisons produce consistent results

---

## 6. Fix Guidance

### Implementation Steps

#### Step 1: Define Normalized Timestamp Semantics
```cpp
// In include/igl/IQueryPool.h - Document standard semantics
/// Timestamp Query Semantics (Cross-Platform)
///
/// All timestamp queries return GPU timestamps in a normalized format:
/// - Unit: Nanoseconds (1 billion = 1 second)
/// - Resolution: Device-dependent (typically 1-10 nanoseconds)
/// - Precision: 64-bit unsigned integer
///
/// To calculate GPU duration in seconds:
///   duration_seconds = (timestamp_end - timestamp_start) / 1e9
///
/// This ensures cross-platform consistency with Vulkan semantics.
```

#### Step 2: Store Hardware Frequency
```cpp
// In src/igl/d3d12/Device.h - Add frequency tracking
class Device : public IDevice {
private:
    uint64_t gpuTimestampFrequencyHz_;  // Store raw GPU frequency

    void initializeTimestampFrequency() {
        commandQueue_->GetTimestampFrequency(&gpuTimestampFrequencyHz_);
    }

public:
    uint64_t getGpuTimestampFrequencyHz() const {
        return gpuTimestampFrequencyHz_;
    }
};
```

#### Step 3: Implement Timestamp Normalization
```cpp
// In src/igl/d3d12/Device.h or separate utility file
class TimestampConverter {
public:
    explicit TimestampConverter(uint64_t gpuFrequencyHz)
        : gpuFrequencyHz_(gpuFrequencyHz) {}

    // Convert GPU timestamp (in GPU ticks) to nanoseconds
    uint64_t toNanoseconds(uint64_t gpuTimestamp) const {
        // Formula: (timestamp * 1e9) / frequency_hz
        // Using integer math to avoid floating point precision loss
        return (gpuTimestamp * 1000000000ULL) / gpuFrequencyHz_;
    }

    // Convert GPU duration (in GPU ticks) to nanoseconds
    uint64_t durationToNanoseconds(uint64_t gpuTicksDuration) const {
        return toNanoseconds(gpuTicksDuration);
    }

    // Helper: Get duration in milliseconds
    double durationToMilliseconds(uint64_t gpuTicksDuration) const {
        return durationToNanoseconds(gpuTicksDuration) / 1e6;
    }

    // Helper: Get duration in microseconds
    uint64_t durationToMicroseconds(uint64_t gpuTicksDuration) const {
        return durationToNanoseconds(gpuTicksDuration) / 1000ULL;
    }

private:
    uint64_t gpuFrequencyHz_;
};
```

#### Step 4: Update Query Pool Implementation
```cpp
// In src/igl/d3d12/QueryHeap.h - Add timestamp converter
class QueryHeap : public IQueryHeap {
private:
    TimestampConverter timestampConverter_;

public:
    QueryHeap(Device* device, const QueryPoolDesc& desc)
        : timestampConverter_(device->getGpuTimestampFrequencyHz()) {}

    // Override result retrieval to normalize timestamps
    Result getResults(size_t index, size_t count,
                     uint64_t* outResults, size_t resultsSize) override {
        // Get raw GPU timestamps
        uint64_t rawResults[256];
        Result res = getResultsRaw(index, count, rawResults, sizeof(rawResults));

        if (!res.isOk()) return res;

        // Convert to nanoseconds
        for (size_t i = 0; i < count; ++i) {
            outResults[i] = timestampConverter_.toNanoseconds(rawResults[i]);
        }

        return Result();
    }

private:
    Result getResultsRaw(size_t index, size_t count,
                        uint64_t* outResults, size_t resultsSize);
};
```

#### Step 5: Update CommandBuffer Timestamp Commands
```cpp
// In src/igl/d3d12/CommandBuffer.h - Document semantics
class CommandBuffer : public ICommandBuffer {
public:
    /// Add a timestamp query marker
    ///
    /// Timestamps are automatically normalized to nanoseconds for consistency
    /// with Vulkan semantics. Use (timestamp_end - timestamp_start) / 1e9
    /// to get duration in seconds.
    ///
    /// @param queryPool Query pool for timestamp storage
    /// @param queryIndex Index in query pool (0-based)
    void addTimestampQuery(IQueryPool* queryPool, size_t queryIndex) override;
};
```

#### Step 6: Add Utility Functions to Device
```cpp
// In src/igl/d3d12/Device.h - Add convenience methods
class Device : public IDevice {
public:
    /// Get the GPU timestamp converter for manual timestamp handling
    TimestampConverter getTimestampConverter() const {
        return TimestampConverter(gpuTimestampFrequencyHz_);
    }

    /// Create a query pool for timestamp queries
    /// All timestamps are returned normalized to nanoseconds
    Result createQueryPool(const QueryPoolDesc& desc,
                          IQueryPool** outQueryPool) override;
};
```

#### Step 7: Add Documentation and Examples
```cpp
// In src/igl/d3d12/QueryHeap.cpp - Add usage examples in comments
/*
 * TIMESTAMP QUERY USAGE EXAMPLE
 *
 * // Create query pool for 2 timestamps
 * QueryPoolDesc queryDesc{QueryType::Timestamp, 2};
 * IQueryPool* queryPool = nullptr;
 * device->createQueryPool(queryDesc, &queryPool);
 *
 * // Record timestamps
 * commandBuffer->addTimestampQuery(queryPool, 0);  // Before work
 * // ... GPU work ...
 * commandBuffer->addTimestampQuery(queryPool, 1);  // After work
 *
 * // Submit and wait for completion
 * commandBuffer->submit();
 * device->waitForGPU();
 *
 * // Retrieve results (automatically normalized to nanoseconds)
 * uint64_t results[2];
 * queryPool->getResults(0, 2, results, sizeof(results));
 *
 * // Calculate duration
 * uint64_t durationNs = results[1] - results[0];
 * double durationMs = durationNs / 1e6;
 * printf("GPU work took %.2f ms\n", durationMs);
 *
 * // This same code works identically on all backends!
 */
```

---

## 7. Testing Requirements

### Unit Tests to Create/Modify

**File**: `tests/d3d12/TimestampQueryTests.cpp` (NEW)

1. **Frequency Retrieval Test**
   ```cpp
   TEST(D3D12Timestamp, GetFrequency) {
       auto device = createTestDevice();

       // Should retrieve valid frequency
       uint64_t frequency = device->getGpuTimestampFrequencyHz();
       ASSERT_GT(frequency, 0);
       ASSERT_LT(frequency, 1e12);  // Reasonable bounds (100MHz - 1THz)
   }
   ```

2. **Timestamp Normalization Test**
   ```cpp
   TEST(D3D12Timestamp, NormalizeTimestamps) {
       // Assume GPU frequency is 10 MHz (10,000,000 Hz)
       TimestampConverter converter(10000000);

       uint64_t gpuTicks = 10000000;  // 1 second worth of ticks
       uint64_t nanoseconds = converter.toNanoseconds(gpuTicks);

       // Should be 1 second = 1,000,000,000 nanoseconds
       ASSERT_EQ(nanoseconds, 1000000000ULL);
   }
   ```

3. **Cross-Platform Consistency Test**
   ```cpp
   TEST(TimestampQuery, CROSS_PLATFORM_Consistency) {
       // Create devices for both backends
       auto d3d12Device = createDevice(BackendType::D3D12);
       auto vulkanDevice = createDevice(BackendType::Vulkan);

       // Create identical query pools
       QueryPoolDesc desc{QueryType::Timestamp, 2};
       IQueryPool *d3d12Pool, *vulkanPool;
       d3d12Device->createQueryPool(desc, &d3d12Pool);
       vulkanDevice->createQueryPool(desc, &vulkanPool);

       // Record same GPU work on both
       recordTimedWork(d3d12Device, d3d12Pool);
       recordTimedWork(vulkanDevice, vulkanPool);

       // Get results
       uint64_t d3d12Results[2], vulkanResults[2];
       d3d12Pool->getResults(0, 2, d3d12Results, sizeof(d3d12Results));
       vulkanPool->getResults(0, 2, vulkanResults, sizeof(vulkanResults));

       // Duration should be nearly identical (within 10% tolerance)
       uint64_t d3d12Duration = d3d12Results[1] - d3d12Results[0];
       uint64_t vulkanDuration = vulkanResults[1] - vulkanResults[0];
       double tolerance = 0.1;  // 10%

       double diff = static_cast<double>(abs(static_cast<long long>(d3d12Duration - vulkanDuration)));
       double relDiff = diff / std::min(d3d12Duration, vulkanDuration);
       ASSERT_LT(relDiff, tolerance);
   }
   ```

4. **Duration Calculation Test**
   ```cpp
   TEST(D3D12Timestamp, CalculateDuration) {
       QueryPoolDesc desc{QueryType::Timestamp, 2};
       IQueryPool* pool = nullptr;
       ASSERT_TRUE(device->createQueryPool(desc, &pool).isOk());

       uint64_t results[2];
       // Results returned in nanoseconds (after normalization)
       pool->getResults(0, 2, results, sizeof(results));

       uint64_t durationNs = results[1] - results[0];
       double durationMs = durationNs / 1e6;
       double durationSec = durationNs / 1e9;

       // Should be reasonable (GPU work took microseconds to milliseconds)
       ASSERT_GT(durationNs, 0);
       ASSERT_LT(durationMs, 100);  // Reasonable upper bound for simple operation
   }
   ```

### Integration Tests

**Command**: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Timestamp*"`

- Run all timestamp query tests
- Verify: 100% pass rate

**Command**: `./test_all_sessions.bat`

- Ensure sessions can use timestamp queries
- Verify: No crashes or invalid results

### Cross-Platform Tests

**Command**: Create test that runs on both D3D12 and Vulkan:
```bash
./build/Debug/IGLTests.exe --gtest_filter="*Timestamp*CROSS_PLATFORM*"
```

### Restrictions
- ❌ DO NOT change raw GPU timestamp values (only normalize output)
- ❌ DO NOT modify Vulkan implementation
- ❌ DO NOT add floating-point semantics to timestamp storage
- ✅ DO focus changes on normalization in `src/igl/d3d12/QueryHeap.*`

---

## 8. Definition of Done

### Completion Checklist

- [ ] TimestampConverter class implemented with conversion logic
- [ ] Device stores and exposes GPU timestamp frequency
- [ ] QueryHeap normalizes timestamps to nanoseconds
- [ ] CommandBuffer timestamp operations documented with semantics
- [ ] Utility methods added to Device for timestamp handling
- [ ] All new unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Timestamp*"`
- [ ] Cross-platform timing consistency verified (< 10% difference)
- [ ] Integration tests pass: `./test_all_sessions.bat`
- [ ] Documentation updated with timestamp semantics
- [ ] Example code provided in comments
- [ ] Code reviewed and approved by maintainer

### User Confirmation Required
⚠️ **STOP** - Do NOT proceed to next task until user confirms:
- "✅ I have verified timestamp normalization works correctly"
- "✅ Cross-platform timestamp consistency is within acceptable range"
- "✅ No regressions in existing query functionality"
- "✅ Documentation clearly explains normalized semantics"

---

## 9. Related Issues

### Blocks
- **I-008**: GPU performance profiling (depends on consistent timestamps)
- **H-016**: Frame timing measurements (depends on consistent timestamps)

### Related
- **I-006**: Query pool implementation
- **I-009**: GPU memory measurements
- **G-009**: Performance analysis tools

---

## 10. Implementation Priority

### Severity: **Low** | Priority: **P2-Low**

**Effort**: 25-35 hours
- Design: 6 hours (timestamp semantics, conversion strategy)
- Implementation: 12-16 hours (converter + QueryHeap changes)
- Testing: 6-8 hours (unit + cross-platform tests)
- Documentation: 2-4 hours

**Risk**: **Low**
- Risk of precision loss if conversion math incorrect
- Risk of integer overflow in large timestamps (unlikely)
- Mitigation: Unit tests with edge cases, bounds checking

**Impact**: **Medium**
- Enables reliable cross-platform performance profiling
- Simplifies timing-critical application code
- Improves development experience for performance optimization

**Blockers**: None

**Dependencies**:
- Requires existing QueryPool implementation
- Requires Vulkan backend for cross-platform testing

---

## 11. References

### Official Microsoft Documentation
1. **ID3D12CommandQueue::GetTimestampFrequency**: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-gettimestampfrequency
2. **Queries**: https://learn.microsoft.com/en-us/windows/win32/direct3d12/queries
3. **Understanding Query Heaps**: https://learn.microsoft.com/en-us/windows/win32/direct3d12/understanding-query-heaps
4. **Performance Timing**: https://learn.microsoft.com/en-us/windows/win32/direct3d12/performance-timing-measurements

### Vulkan Documentation
1. **VK_EXT_calibrated_timestamps**: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_calibrated_timestamps.html
2. **VkQueryPool Timestamps**: https://www.khronos.org/registry/vulkan/specs/1.3/html/vkspec.html#queries-timestamps

### GitHub References
- **Microsoft D3D12 Samples**: https://github.com/microsoft/DirectX-Graphics-Samples
- **IGL Repository**: https://github.com/facebook/igl

### Related Issues
- Task I-006: Query pool implementation
- Task I-008: GPU performance profiling
