# G-006: Upload Ring Buffer Not Used Consistently

**Severity:** Medium
**Category:** GPU Performance
**Status:** Open
**Related Issues:** P1_DX12-009 (Upload Ring Buffer), A-003 (UMA Architecture), B-003 (Ring Buffer Race Condition)

---

## Problem Statement

The `UploadRingBuffer` was implemented (P1_DX12-009) to reduce upload heap allocator churn, but it's only used by `Buffer::upload()` for DEFAULT heap buffers. Other upload paths still create dedicated staging buffers:
- Texture uploads (`Texture.cpp`)
- Immediate texture updates
- Compute shader buffer uploads

This means many upload operations bypass the ring buffer, negating its benefits and causing continued allocator fragmentation.

**Impact Analysis:**
- **Allocator Churn:** Dedicated staging buffers for each texture upload cause heap allocations/deallocations
- **Memory Fragmentation:** Repeated small allocations fragment upload heap over time
- **Performance Overhead:** Creating and destroying staging buffers has CPU cost (allocation, fence creation, wait)
- **Inconsistent Strategy:** Mixed usage pattern (ring buffer for some, dedicated for others) makes performance unpredictable

**The Danger:**
- Long-running applications accumulate upload heap fragmentation
- CPU overhead from allocator churn impacts frame time
- Memory usage grows unnecessarily with many pending staging buffers

---

## Root Cause Analysis

### Current Implementation

**Good: Buffer uses UploadRingBuffer (`Buffer.cpp:131-149`):**
```cpp
Result Buffer::upload(const void* data, const BufferRange& range) {
  // ... validation ...

  // P1_DX12-009: Try to allocate from upload ring buffer first
  UploadRingBuffer* ringBuffer = device_->getUploadRingBuffer();
  UploadRingBuffer::Allocation ringAllocation;
  bool useRingBuffer = false;

  const UINT64 uploadFenceValue = device_->getNextUploadFenceValue();

  if (ringBuffer) {
    constexpr uint64_t kUploadAlignment = 256;
    ringAllocation = ringBuffer->allocate(range.size, kUploadAlignment, uploadFenceValue);

    if (ringAllocation.valid) {
      // Successfully allocated from ring buffer
      std::memcpy(ringAllocation.cpuAddress, data, range.size);
      useRingBuffer = true;
    }
  }

  // Fallback to dedicated staging buffer if ring buffer allocation failed
  if (!useRingBuffer) {
    // ... create dedicated staging buffer ...
  }
}
```

**Problem: Texture doesn't use UploadRingBuffer:**

Texture upload paths create dedicated staging buffers without trying ring buffer allocation. Example scenarios:
1. Initial texture data upload during creation
2. Dynamic texture updates (`Texture::upload()`)
3. Mipmap generation copies

**Why This Is Wrong:**

1. **No Ring Buffer Integration:** Texture upload code predates ring buffer implementation and was never updated
2. **Inconsistent Patterns:** Buffer class uses ring buffer; Texture class doesn't
3. **Missed Optimization:** Texture uploads are prime candidates for ring buffer (often small, frequent updates)
4. **Code Duplication:** Texture upload has its own staging buffer creation logic

---

## Official Documentation References

1. **Upload Heap Best Practices**:
   - https://learn.microsoft.com/windows/win32/direct3d12/upload-and-readback-of-texture-data
   - Key guidance: "Use a single large upload heap with sub-allocation rather than many small heaps"

2. **Memory Management Strategies**:
   - https://learn.microsoft.com/windows/win32/direct3d12/memory-management-strategies
   - Guidance: "Pool upload resources to reduce allocator overhead"

3. **Resource Upload Patterns**:
   - https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources
   - Best practice: "Reuse upload buffers across multiple resources"

4. **DirectX-Graphics-Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Bundles/src/FrameResource.cpp
   - Sample demonstrates single upload buffer for multiple resource types

---

## Code Location Strategy

### Files to Modify:

1. **Texture.cpp** (upload method - if it exists):
   - Search for: Texture upload implementation
   - Context: Texture data upload to GPU
   - Action: Add ring buffer allocation attempt before dedicated staging

2. **Texture.cpp** (texture creation with initial data):
   - Search for: Initial data upload during texture creation
   - Context: Constructor or initialization method
   - Action: Integrate ring buffer for initial upload

3. **Device.h** (ring buffer accessor):
   - Search for: `getUploadRingBuffer()` method
   - Context: Already exists for Buffer class usage
   - Action: Ensure it's public and documented for Texture usage

4. **UploadRingBuffer.cpp** (verify alignment support):
   - Search for: Texture-specific alignment requirements
   - Context: Texture pitch alignment (often 256 or 512 bytes)
   - Action: Verify ring buffer handles texture row pitch alignment

---

## Detection Strategy

### How to Reproduce:

Monitor upload heap allocations during texture-heavy workload:
```cpp
// Create many textures with data
for (int i = 0; i < 100; ++i) {
  TextureDesc desc;
  desc.width = 256;
  desc.height = 256;
  desc.format = TextureFormat::RGBA_UNorm8;

  std::vector<uint8_t> data(256 * 256 * 4);
  // Fill with test pattern

  auto texture = device->createTexture(desc, &data[0]);
}

// Expected with ring buffer: Few upload heap allocations, ring buffer utilization
// Actual without: 100+ upload heap allocations, no ring buffer usage
```

### Verification After Fix:

1. Enable upload ring buffer telemetry (allocation count, failure count)
2. Run texture-heavy workload
3. Verify ring buffer allocation count increases
4. Verify ring buffer failure count is low (most allocations succeed)
5. Check upload heap allocation count decreased

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Ring Buffer Support to Texture Upload Path

**File:** `src/igl/d3d12/Texture.cpp`

**Locate:** Search for texture upload implementation (method that copies data to GPU)

**Pattern to Search:**
- Look for `CreateCommittedResource` with `D3D12_HEAP_TYPE_UPLOAD`
- Look for texture data copying logic
- Look for staging buffer creation

**Current (PROBLEM - Hypothetical):**
```cpp
// Hypothetical texture upload code (actual code may vary)
Result Texture::upload(const void* data, const TextureRangeDesc& range) {
  // PROBLEM: Always creates dedicated staging buffer
  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC stagingDesc = {};
  // ... fill staging buffer description ...

  Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;
  HRESULT hr = device_->CreateCommittedResource(
      &uploadHeapProps,
      D3D12_HEAP_FLAG_NONE,
      &stagingDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&stagingBuffer));

  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create staging buffer");
  }

  // Map, copy data, unmap
  // ... staging buffer usage ...

  // Copy staging to texture
  // ... copy command ...
}
```

**Fixed (SOLUTION):**
```cpp
Result Texture::upload(const void* data, const TextureRangeDesc& range) {
  // G-006: Try to allocate from upload ring buffer first
  UploadRingBuffer* ringBuffer = device_->getUploadRingBuffer();
  const UINT64 uploadFenceValue = device_->getNextUploadFenceValue();

  // Calculate required size and alignment
  // Textures require row-pitch alignment (typically 256 bytes)
  const uint64_t rowPitch = /* calculate based on format and width */;
  const uint64_t uploadSize = rowPitch * range.height * range.depth;
  constexpr uint64_t kTextureUploadAlignment = 512; // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT

  bool useRingBuffer = false;
  UploadRingBuffer::Allocation ringAllocation;

  if (ringBuffer) {
    ringAllocation = ringBuffer->allocate(uploadSize, kTextureUploadAlignment, uploadFenceValue);

    if (ringAllocation.valid) {
      // Successfully allocated from ring buffer
      // Copy data to ring buffer allocation with row pitch consideration
      const uint8_t* srcData = static_cast<const uint8_t*>(data);
      uint8_t* dstData = static_cast<uint8_t*>(ringAllocation.cpuAddress);

      // Copy row by row respecting pitch
      for (uint32_t row = 0; row < range.height * range.depth; ++row) {
        std::memcpy(dstData, srcData, /* bytes per row */);
        srcData += /* source row stride */;
        dstData += rowPitch;
      }

      useRingBuffer = true;
      IGL_LOG_INFO("Texture::upload: Using ring buffer allocation (%llu bytes)\n", uploadSize);
    } else {
      IGL_LOG_INFO("Texture::upload: Ring buffer allocation failed, using dedicated staging\n");
    }
  }

  if (useRingBuffer) {
    // Use ring buffer allocation for copy
    // Setup copy source location pointing to ring buffer
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = ringBuffer->getUploadHeap();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint.Offset = ringAllocation.offset;
    srcLocation.PlacedFootprint.Footprint.Format = /* texture format */;
    srcLocation.PlacedFootprint.Footprint.Width = range.width;
    srcLocation.PlacedFootprint.Footprint.Height = range.height;
    srcLocation.PlacedFootprint.Footprint.Depth = range.depth;
    srcLocation.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = resource_.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = /* calculate subresource index */;

    // Issue copy command
    commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // No need to destroy staging buffer - ring buffer manages memory
  } else {
    // Fallback: Create dedicated staging buffer (existing code path)
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC stagingDesc = {};
    // ... fill staging buffer description ...

    Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;
    HRESULT hr = device_->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &stagingDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&stagingBuffer));

    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to create staging buffer");
    }

    // ... existing dedicated staging buffer path ...
  }

  return Result();
}
```

**Rationale:**
- Attempt ring buffer allocation first (fast path)
- Fall back to dedicated staging buffer if ring buffer exhausted (safety)
- Use `D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT` to copy from ring buffer at specific offset
- Respects texture row pitch alignment requirements

---

#### Step 2: Add Ring Buffer Telemetry

**File:** `src/igl/d3d12/UploadRingBuffer.cpp`

**Locate:** Search for `getFailureCount()` accessor

**Current (ALREADY EXISTS):**
```cpp
uint64_t UploadRingBuffer::getFailureCount() const {
  return failureCount_;
}

uint64_t UploadRingBuffer::getAllocationCount() const {
  return allocationCount_;
}
```

**Enhancement - Add Logging Method:**
```cpp
void UploadRingBuffer::logStats() const {
  std::lock_guard<std::mutex> lock(mutex_);

  IGL_LOG_INFO("=== Upload Ring Buffer Statistics ===\n");
  IGL_LOG_INFO("  Total Size:        %llu MB\n", size_ / (1024 * 1024));
  IGL_LOG_INFO("  Used Size:         %llu MB (%.1f%%)\n",
               getUsedSize() / (1024 * 1024),
               (getUsedSize() * 100.0f) / size_);
  IGL_LOG_INFO("  Allocation Count:  %llu\n", allocationCount_);
  IGL_LOG_INFO("  Failure Count:     %llu (%.2f%% failure rate)\n",
               failureCount_,
               allocationCount_ > 0 ? (failureCount_ * 100.0f) / allocationCount_ : 0.0f);
  IGL_LOG_INFO("  Success Rate:      %.2f%%\n",
               allocationCount_ > 0 ? ((allocationCount_ - failureCount_) * 100.0f) / allocationCount_ : 100.0f);
  IGL_LOG_INFO("========================================\n");
}
```

**Add to UploadRingBuffer.h:**
```cpp
class UploadRingBuffer {
 public:
  // ... existing methods ...

  // G-006: Log ring buffer usage statistics
  void logStats() const;
};
```

**Rationale:** Provides visibility into ring buffer utilization and helps diagnose allocation failures.

---

#### Step 3: Call Ring Buffer Telemetry Periodically

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Per-frame maintenance or telemetry logging

**Add Periodic Logging:**
```cpp
// In Device periodic maintenance (e.g., every 300 frames)
void Device::logTelemetry() {
  // ... existing telemetry ...

  // G-006: Log upload ring buffer statistics
  if (uploadRingBuffer_) {
    uploadRingBuffer_->logStats();
  }
}
```

**Rationale:** Periodic telemetry logging helps identify if ring buffer size is appropriate or if failure rate is too high.

---

#### Step 4: Document Ring Buffer Usage Pattern

**File:** `src/igl/d3d12/UploadRingBuffer.h`

**Locate:** Class documentation comment

**Current:**
```cpp
/**
 * @brief Upload Ring Buffer for Streaming Resources (P1_DX12-009)
 *
 * Manages a large staging buffer (64-256MB) for efficient resource uploads.
 * ...
 */
class UploadRingBuffer {
  // ...
};
```

**Enhanced:**
```cpp
/**
 * @brief Upload Ring Buffer for Streaming Resources (P1_DX12-009, G-006)
 *
 * Manages a large staging buffer (64-256MB) for efficient resource uploads.
 * Implements a ring buffer pattern with fence-based memory retirement to
 * reduce allocator churn and memory fragmentation.
 *
 * Key Features:
 * - Large pre-allocated upload heap
 * - Linear sub-allocation with wraparound
 * - Fence-based memory retirement and recycling
 * - Thread-safe allocation
 *
 * Usage Pattern:
 * 1. Attempt ring buffer allocation first via allocate()
 * 2. If successful (valid flag true), copy data to cpuAddress
 * 3. Use gpuAddress for copy source in CopyBufferRegion/CopyTextureRegion
 * 4. If allocation fails (valid flag false), fall back to dedicated staging buffer
 * 5. Call retire() with completed fence value to reclaim memory
 *
 * Supported Resource Types:
 * - Buffer uploads (Buffer::upload)
 * - Texture uploads (Texture::upload) - G-006
 * - Compute shader buffers
 * - Any upload heap usage pattern
 *
 * Alignment Requirements:
 * - Constant buffers: 256 bytes (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
 * - Textures: 512 bytes (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
 * - General buffers: 16 bytes (minimum)
 */
class UploadRingBuffer {
  // ...
};
```

**Rationale:** Comprehensive documentation guides implementers on correct usage patterns for different resource types.

---

#### Step 5: Update Buffer Upload to Log Ring Buffer Usage

**File:** `src/igl/d3d12/Buffer.cpp`

**Locate:** Ring buffer allocation success path

**Current:**
```cpp
if (ringAllocation.valid) {
  // Successfully allocated from ring buffer
  std::memcpy(ringAllocation.cpuAddress, data, range.size);
  useRingBuffer = true;
}
```

**Enhanced:**
```cpp
if (ringAllocation.valid) {
  // Successfully allocated from ring buffer
  std::memcpy(ringAllocation.cpuAddress, data, range.size);
  useRingBuffer = true;
  IGL_LOG_INFO("Buffer::upload: Using ring buffer (%llu bytes, offset=%llu)\n",
               range.size, ringAllocation.offset);
} else {
  IGL_LOG_INFO("Buffer::upload: Ring buffer allocation failed, using dedicated staging\n");
}
```

**Rationale:** Logs ring buffer usage for diagnostic visibility (can be removed or conditionally compiled for release builds).

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Upload and texture tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Upload*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

**Test Modifications Allowed:**
- ✅ Add test for texture upload via ring buffer
- ✅ Add test for ring buffer telemetry
- ❌ **DO NOT modify cross-platform test logic**

**New Test to Add:**
```cpp
// Test texture upload uses ring buffer when available
TEST(Texture, UsesRingBufferForUpload) {
  auto device = createTestDevice();
  auto ringBuffer = device->getUploadRingBuffer();
  ASSERT_NE(ringBuffer, nullptr);

  const uint64_t initialAllocCount = ringBuffer->getAllocationCount();

  // Create texture with data
  TextureDesc desc;
  desc.width = 256;
  desc.height = 256;
  desc.format = TextureFormat::RGBA_UNorm8;

  std::vector<uint8_t> data(256 * 256 * 4, 0xFF);
  auto texture = device->createTexture(desc, &data[0]);

  // Verify ring buffer allocation count increased
  const uint64_t finalAllocCount = ringBuffer->getAllocationCount();
  EXPECT_GT(finalAllocCount, initialAllocCount);
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass (ring buffer is transparent optimization)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Modifications Allowed:**
- ✅ None required - ring buffer usage should be transparent
- ❌ **DO NOT modify session logic**

### Performance Verification:

```bash
# Run texture-heavy workload and compare telemetry
# Before: High upload heap allocation count, low ring buffer usage
# After: Low upload heap allocation count, high ring buffer usage
```

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass (existing + new ring buffer usage test)
- [ ] Texture upload path attempts ring buffer allocation first
- [ ] Fallback to dedicated staging buffer if ring buffer exhausted
- [ ] Ring buffer telemetry shows allocation count from both buffers and textures
- [ ] Ring buffer failure rate is low (<5%) in typical workloads
- [ ] All render sessions pass without visual changes
- [ ] Documentation updated to reflect texture upload support
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Ring buffer telemetry shows allocations from both Buffer and Texture uploads
2. Ring buffer failure rate is acceptable (<5% for typical workloads)
3. All render sessions pass

**Post in chat:**
```
G-006 Fix Complete - Ready for Review

Upload Ring Buffer Utilization:
- Texture Integration: Texture upload now uses ring buffer
- Fallback: Dedicated staging buffer if ring buffer exhausted
- Telemetry: logStats() shows allocation/failure counts
- Alignment: Supports texture pitch alignment (512 bytes)

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Ring buffer test: PASS (texture uploads use ring buffer)
- Render sessions: PASS (no visual changes)
- Telemetry: Allocation count increased, failure rate <5%

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- None

### Related:
- **P1_DX12-009** - Upload ring buffer implementation (base infrastructure)
- **A-003** - UMA architecture query (ring buffer optimization for UMA)
- **B-003** - Ring buffer race condition (thread safety)

---

## Implementation Priority

**Priority:** P1 - Medium (GPU Performance)
**Estimated Effort:** 3-4 hours
**Risk:** Low (Ring buffer is proven for buffers; extend to textures)
**Impact:** Reduces upload heap allocator churn; improves memory efficiency in texture-heavy workloads

**Notes:**
- Ring buffer already implemented and tested for buffers (P1_DX12-009)
- Extension to textures is straightforward pattern replication
- Key consideration: Texture row pitch alignment (512 bytes vs. 256 for constant buffers)
- Telemetry is critical to verify ring buffer size is appropriate for workload
- Consider increasing ring buffer size (128MB default) if failure rate is high

---

## References

- https://learn.microsoft.com/windows/win32/direct3d12/upload-and-readback-of-texture-data
- https://learn.microsoft.com/windows/win32/direct3d12/memory-management-strategies
- https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources
- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Bundles/src/FrameResource.cpp
- https://learn.microsoft.com/windows/win32/direct3d12/subresources
- https://learn.microsoft.com/windows/win32/direct3d12/d3d12_texture_data_pitch_alignment

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
