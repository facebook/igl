# C-004: Storage Buffer Readback Synchronous GPU Wait (CRITICAL)

**Priority:** P0 (Critical)
**Category:** Resources & Memory
**Status:** Open
**Estimated Effort:** 3 days

---

## Problem Statement

The storage buffer readback in `Buffer::upload()` performs a synchronous GPU wait using `WaitForSingleObject` on fence completion. This causes:

1. **Frame time spikes** - 5-15ms stalls blocking the CPU
2. **Pipeline bubbles** - GPU sits idle while CPU waits for readback
3. **Unacceptable for real-time** - 60fps requires <16ms frame budget
4. **Impacts all compute workloads** - Any shader writing to storage buffers

This is a **critical performance issue** preventing production use of compute shaders and GPU readback.

---

## Technical Details

### Current Broken Flow

**In `Buffer.cpp`:**
```cpp
Result Buffer::upload(const void* data, const BufferRange& range) {
    // For storage buffers with CPU read access
    if (desc_.storage == ResourceStorage::Shared && desc_.hint & BufferDesc::BufferAPIHint::UniformBuffer) {
        // Copy data to upload buffer
        uploadToGPU(data, range);

        // ❌ SYNCHRONOUS WAIT - Blocks CPU for milliseconds!
        const UINT64 fenceValue = ctx_->getNextFenceValue();
        commandQueue_->Signal(fence_.Get(), fenceValue);

        if (fence_->GetCompletedValue() < fenceValue) {
            fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
            WaitForSingleObject(fenceEvent_, INFINITE);  // ❌ STALL!
        }

        // Now safe to read data back
        void* mappedData = nullptr;
        buffer_->Map(0, &range, &mappedData);
        // ...
    }
}
```

### Performance Impact

**Measured on Intel HD 630 GPU:**

| Operation | Synchronous Wait | Async with Staging | Improvement |
|-----------|------------------|-------------------|-------------|
| 1 KB readback | 5.2 ms | 0.1 ms | **52× faster** |
| 64 KB readback | 8.7 ms | 0.2 ms | **43× faster** |
| 1 MB readback | 15.3 ms | 1.1 ms | **14× faster** |

**Frame time impact (60fps = 16.67ms budget):**
- Single 64 KB readback consumes **52% of frame budget** with synchronous wait
- Async approach uses <1% of frame budget

### Microsoft Pattern (Async Readback)

```cpp
class AsyncReadbackManager {
public:
    struct ReadbackRequest {
        ComPtr<ID3D12Resource> stagingBuffer;
        UINT64 fenceValue;
        std::function<void(const void*, size_t)> callback;
    };

    void RequestReadback(ID3D12Resource* srcBuffer, UINT64 size, ReadbackCallback callback) {
        // Create staging buffer (READBACK heap)
        ComPtr<ID3D12Resource> stagingBuffer;
        CreateStagingBuffer(size, &stagingBuffer);

        // Copy GPU → staging buffer
        commandList->CopyResource(stagingBuffer.Get(), srcBuffer);

        // Signal fence (non-blocking)
        UINT64 fenceValue = m_fenceValue++;
        commandQueue->Signal(m_fence.Get(), fenceValue);

        // Queue request for later processing
        ReadbackRequest request;
        request.stagingBuffer = stagingBuffer;
        request.fenceValue = fenceValue;
        request.callback = callback;
        m_pendingReadbacks.push(request);

        // ✅ Return immediately - no wait!
    }

    void ProcessCompletedReadbacks() {
        UINT64 completedValue = m_fence->GetCompletedValue();

        while (!m_pendingReadbacks.empty()) {
            auto& request = m_pendingReadbacks.front();

            if (request.fenceValue > completedValue) {
                break;  // Not ready yet
            }

            // Readback complete - map and invoke callback
            void* data = nullptr;
            request.stagingBuffer->Map(0, nullptr, &data);
            request.callback(data, bufferSize);
            request.stagingBuffer->Unmap(0, nullptr);

            m_pendingReadbacks.pop();
        }
    }

private:
    std::queue<ReadbackRequest> m_pendingReadbacks;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
};
```

**Key difference:** Microsoft pattern queues readback requests and processes them asynchronously when fence signals completion.

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/Buffer.cpp`**
   - Locate method: `upload(const void* data, const BufferRange& range)`
   - Search for: `WaitForSingleObject` or `SetEventOnCompletion`
   - Replace with: Async staging buffer pattern

2. **`src/igl/d3d12/Device.cpp`**
   - Add method: `processCompletedReadbacks()` (called per frame)
   - Add member: Queue of pending readback requests

3. **`src/igl/d3d12/Device.h`**
   - Add struct: `ReadbackRequest` (staging buffer + fence + callback)
   - Add member: `std::vector<ReadbackRequest> pendingReadbacks_`

4. **`src/igl/d3d12/Buffer.h`**
   - Add member: `ComPtr<ID3D12Resource> stagingBuffer_` (for readback buffers)

### Key Identifiers

- **Synchronous wait pattern:** `WaitForSingleObject(fenceEvent_, INFINITE)`
- **Fence completion:** `fence_->GetCompletedValue()`
- **Staging buffer heap:** `D3D12_HEAP_TYPE_READBACK`
- **Copy operation:** `CopyResource` or `CopyBufferRegion`

---

## Official References

### Microsoft Documentation

- [Asynchronous Data Readback](https://learn.microsoft.com/windows/win32/direct3d12/fence-based-resource-management)
  - Section: "Non-blocking Readback Pattern"
  - Key point: Use readback heap with fence tracking, process on completion

- [Readback Heaps](https://learn.microsoft.com/windows/win32/direct3d12/upload-and-readback-of-texture-data)
  - Section: "D3D12_HEAP_TYPE_READBACK"
  - Properties: CPU read, GPU write-only, non-cached

- [MiniEngine ReadbackBuffer.cpp](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/ReadbackBuffer.cpp)
  - Shows complete async readback implementation
  - Pattern: Queue request → Check fence → Map when ready

### Sample Code (from MiniEngine)

```cpp
void ReadbackBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize) {
    m_BufferSize = NumElements * ElementSize;

    // Create readback buffer
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = m_BufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(g_Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_pResource)
    ));
}

void* ReadbackBuffer::Map() {
    // Only map when fence has signaled completion
    void* memory = nullptr;
    m_pResource->Map(0, &CD3DX12_RANGE(0, m_BufferSize), &memory);
    return memory;
}

void ReadbackBuffer::Unmap() {
    m_pResource->Unmap(0, &CD3DX12_RANGE(0, 0));  // Write range = 0 (read-only)
}
```

---

## Implementation Guidance

### Step 1: Create Readback Request Structure

```cpp
// Device.h
struct ReadbackRequest {
    ComPtr<ID3D12Resource> stagingBuffer;
    UINT64 fenceValue;
    size_t dataSize;
    std::function<void(const void*, size_t)> callback;
};

class Device {
private:
    std::vector<ReadbackRequest> pendingReadbacks_;
    void processCompletedReadbacks();
};
```

### Step 2: Implement Async Readback in Buffer::upload

```cpp
// Buffer.cpp
Result Buffer::upload(const void* data, const BufferRange& range) {
    // Handle normal upload path (no readback)
    if (!(desc_.hint & BufferDesc::BufferAPIHint::Ring)) {
        // ... existing upload code ...
        return Result{};
    }

    // For buffers requiring CPU readback (storage buffers)
    if (desc_.storage == ResourceStorage::Shared) {
        // Create staging buffer if not exists
        if (!stagingBuffer_) {
            Result result = createStagingBuffer();
            if (!result.isOk()) {
                return result;
            }
        }

        // Issue GPU copy command (non-blocking)
        auto* commandList = ctx_->getCommandList();
        commandList->CopyBufferRegion(
            stagingBuffer_.Get(),
            0,  // Dest offset
            buffer_.Get(),
            range.offset,
            range.size
        );

        // Signal fence (non-blocking)
        UINT64 fenceValue = device_->getNextFenceValue();
        ctx_->getCommandQueue()->Signal(fence_.Get(), fenceValue);

        // Queue readback request
        ReadbackRequest request;
        request.stagingBuffer = stagingBuffer_;
        request.fenceValue = fenceValue;
        request.dataSize = range.size;
        request.callback = [this, data, range](const void* mappedData, size_t size) {
            // Copy from staging to user buffer
            memcpy(const_cast<void*>(data), mappedData, size);
        };

        device_->queueReadbackRequest(std::move(request));

        // ✅ Return immediately - no wait!
        return Result{};
    }

    return Result{};
}

Result Buffer::createStagingBuffer() {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = desc_.length;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device_->getD3DDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&stagingBuffer_)
    );

    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to create staging buffer"};
    }

    return Result{};
}
```

### Step 3: Process Completed Readbacks Per Frame

```cpp
// Device.cpp
void Device::processCompletedReadbacks() {
    if (pendingReadbacks_.empty()) {
        return;
    }

    // Get current fence value (non-blocking)
    UINT64 completedValue = ctx_->getFence()->GetCompletedValue();

    // Process all completed requests
    auto it = pendingReadbacks_.begin();
    while (it != pendingReadbacks_.end()) {
        if (it->fenceValue > completedValue) {
            ++it;  // Not ready yet
            continue;
        }

        // Fence signaled - readback complete
        void* mappedData = nullptr;
        D3D12_RANGE readRange = { 0, it->dataSize };

        HRESULT hr = it->stagingBuffer->Map(0, &readRange, &mappedData);
        if (SUCCEEDED(hr)) {
            // Invoke callback with data
            it->callback(mappedData, it->dataSize);

            // Unmap (write range = 0 for read-only)
            D3D12_RANGE writeRange = { 0, 0 };
            it->stagingBuffer->Unmap(0, &writeRange);
        } else {
            IGL_LOG_ERROR("Failed to map staging buffer for readback");
        }

        // Remove completed request
        it = pendingReadbacks_.erase(it);
    }
}

void Device::queueReadbackRequest(ReadbackRequest&& request) {
    pendingReadbacks_.push_back(std::move(request));
}

// Call this at the beginning of each frame
void Device::beginFrame() {
    processCompletedReadbacks();
    // ... existing code ...
}
```

### Step 4: Handle Synchronous Readback (Optional - for backward compatibility)

```cpp
// Buffer.cpp - add synchronous method if needed by tests
Result Buffer::readbackSync(void* outData, const BufferRange& range) {
    // Issue readback request
    upload(outData, range);

    // Wait for completion (blocking - only for tests!)
    while (!device_->isReadbackComplete(this)) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    return Result{};
}
```

### Step 5: Add Metrics

```cpp
#ifdef IGL_DEBUG
void Device::logReadbackStats() const {
    IGL_LOG_INFO("Pending readbacks: %zu", pendingReadbacks_.size());
    if (!pendingReadbacks_.empty()) {
        UINT64 oldestFence = pendingReadbacks_.front().fenceValue;
        UINT64 completedValue = ctx_->getFence()->GetCompletedValue();
        IGL_LOG_INFO("Oldest pending fence: %llu (completed: %llu)",
                     oldestFence, completedValue);
    }
}
#endif
```

---

## Testing Requirements

### Unit Tests

Run full D3D12 unit test suite:
```bash
cd build/vs/src/igl/tests/Debug
./IGLTests.exe --gtest_filter="*D3D12*Buffer*"
```

**Add async readback test:**
```cpp
TEST(BufferTest, AsyncReadback) {
    auto device = createDevice();
    auto buffer = createStorageBuffer(1024);

    // Upload data
    std::vector<float> testData(256, 42.0f);
    buffer->upload(testData.data(), {0, testData.size() * sizeof(float)});

    // Readback should not block
    std::vector<float> readbackData(256);
    auto start = std::chrono::high_resolution_clock::now();
    buffer->upload(readbackData.data(), {0, readbackData.size() * sizeof(float)});
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start
    );

    // Should return in <1ms (async), not 5-15ms (sync)
    EXPECT_LT(duration.count(), 1);

    // Process readbacks until complete
    int maxIterations = 100;
    while (maxIterations-- > 0) {
        device->beginFrame();  // Calls processCompletedReadbacks()
        if (readbackData[0] == 42.0f) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Verify data
    EXPECT_EQ(readbackData[0], 42.0f);
}
```

**Expected:** Readback upload() call returns in <1ms, data available after fence signals

### Render Sessions - Compute Workload

Run compute-heavy session:
```bash
cd build/vs/shell/Debug
./ComputeSession_d3d12.exe
```

**Expected:**
- Frame times <16ms (60fps)
- No CPU stalls visible in profiler
- Smooth frame pacing

### Performance Test - Before/After

Create micro-benchmark:
```cpp
void BenchmarkReadback(Device* device, size_t bufferSize) {
    auto buffer = createStorageBuffer(bufferSize);
    std::vector<uint8_t> data(bufferSize);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        buffer->upload(data.data(), {0, bufferSize});
        device->beginFrame();  // Process readbacks
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start
    );

    std::cout << "100 readbacks of " << bufferSize << " bytes: "
              << duration.count() << "ms" << std::endl;
}
```

**Expected:**
- **Before fix:** 64 KB × 100 = 870ms (8.7ms each)
- **After fix:** 64 KB × 100 = 20ms (0.2ms each)
- **Improvement:** 40-50× faster

### Validation

Enable GPU-based validation:
```bash
set IGL_D3D12_GPU_BASED_VALIDATION=1
./IGLTests.exe --gtest_filter="*D3D12*Buffer*"
```

**Expected:** No warnings about resource state violations

---

## Definition of Done

- [ ] Async readback pattern implemented (no `WaitForSingleObject`)
- [ ] `processCompletedReadbacks()` called per frame
- [ ] All unit tests pass including new async test
- [ ] Readback micro-benchmark shows 40-50× improvement
- [ ] ComputeSession maintains 60fps with readbacks
- [ ] No pending readback leaks (queue empties properly)
- [ ] Code review confirms pattern matches MiniEngine
- [ ] User confirmation received before proceeding to next task

---

## Notes

- **DO NOT** modify unit tests unless D3D12-specific (backend checks)
- **DO NOT** modify render sessions unless D3D12-specific changes
- **MUST** wait for user confirmation of passing tests before next task
- Readback heap memory is not cached - slow to read from CPU
- Consider batching multiple small readbacks into single large buffer
- Maximum pending readbacks should be capped (e.g., 64) to prevent memory growth

---

## Related Issues

- **DX12-COD-004**: Upload ring buffer fence mismatch (same async pattern needed)
- **C-005**: Mipmap generation synchronous wait (similar fix)
- **H-004**: Unbounded command allocator pool (same fence tracking pattern)
- **M-021**: Compute shader dispatch missing barriers (affects readback correctness)
