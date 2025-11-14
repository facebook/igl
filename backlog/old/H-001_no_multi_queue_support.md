# H-001: No Multi-Queue Support (High Priority)

**Priority:** P1 (High)
**Category:** Command Queues & Synchronization
**Status:** Open
**Estimated Effort:** 7 days

---

## Problem Statement

The D3D12 backend only uses a single graphics queue for all work submission (render, compute, copy). This causes:

1. **GPU underutilization** - Async compute and DMA transfers blocked behind graphics
2. **Performance loss** - 20% throughput reduction vs multi-engine workloads
3. **Frame latency** - Cannot overlap copy operations with rendering
4. **Feature parity gap** - Vulkan backend supports multiple queues

This is a **high-priority performance issue** - modern GPUs have 3 independent engines (graphics, compute, copy) that should execute in parallel.

---

## Technical Details

### Current Problem

**In `D3D12Context.cpp`:**
```cpp
Result D3D12Context::initialize(const HWDeviceDesc& desc) {
    // ❌ Only creates graphics queue
    HRESULT hr = device_->CreateCommandQueue(
        &queueDesc,  // Type: D3D12_COMMAND_LIST_TYPE_DIRECT
        IID_PPV_ARGS(&commandQueue_)
    );

    // ❌ No compute or copy queues created
    // computeQueue_ = nullptr;
    // copyQueue_ = nullptr;
}
```

**Impact:**
- All operations serialized on single queue
- Async compute workloads block graphics
- Texture uploads stall rendering
- Cannot use DMA engine for background streaming

### Microsoft Multi-Engine Pattern

```cpp
class D3D12Context {
public:
    // ✅ Three independent queues
    ID3D12CommandQueue* graphicsQueue_;
    ID3D12CommandQueue* computeQueue_;
    ID3D12CommandQueue* copyQueue_;

    // Per-queue fences
    ComPtr<ID3D12Fence> graphicsFence_;
    ComPtr<ID3D12Fence> computeFence_;
    ComPtr<ID3D12Fence> copyFence_;

    UINT64 graphicsFenceValue_ = 0;
    UINT64 computeFenceValue_ = 0;
    UINT64 copyFenceValue_ = 0;

    void waitForQueue(D3D12_COMMAND_LIST_TYPE type);
    void signalCrossQueue(D3D12_COMMAND_LIST_TYPE from, D3D12_COMMAND_LIST_TYPE to);
};
```

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/D3D12Context.h`**
   - Add compute and copy queue members
   - Add per-queue fence tracking
   - Add cross-queue synchronization API

2. **`src/igl/d3d12/D3D12Context.cpp`**
   - Create all three queue types in `initialize()`
   - Implement `waitForQueue()` for cross-engine sync
   - Implement `signalCrossQueue()` for dependencies

3. **`src/igl/d3d12/CommandBuffer.h`**
   - Add queue type to command buffer
   - Support COMPUTE and COPY list types

4. **`src/igl/d3d12/Device.cpp`**
   - Route async compute to compute queue
   - Route uploads to copy queue

---

## Official References

### Microsoft Documentation

- [User Mode Heap Synchronization](https://docs.microsoft.com/windows/win32/direct3d12/user-mode-heap-synchronization)
  - Section: "Multi-Engine Synchronization"
  - Shows fence-based cross-queue dependencies

- [Command List Types](https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_command_list_type)
  - `D3D12_COMMAND_LIST_TYPE_DIRECT` - Graphics + compute
  - `D3D12_COMMAND_LIST_TYPE_COMPUTE` - Async compute only
  - `D3D12_COMMAND_LIST_TYPE_COPY` - DMA transfers only

- [MiniEngine Sample](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine)
  - `Core/CommandListManager.cpp` - Multi-queue architecture
  - Shows graphics/compute/copy queue coordination

---

## Implementation Guidance

### Step 1: Add Queue Members

```cpp
// D3D12Context.h
class D3D12Context {
public:
    enum class QueueType {
        Graphics,
        Compute,
        Copy
    };

    ID3D12CommandQueue* getQueue(QueueType type) const;
    ID3D12Fence* getFence(QueueType type) const;
    UINT64 signalQueue(QueueType type);
    void waitForQueueValue(QueueType type, UINT64 fenceValue);

private:
    // Three independent queues
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> graphicsQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> computeQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> copyQueue_;

    // Per-queue fences
    Microsoft::WRL::ComPtr<ID3D12Fence> graphicsFence_;
    Microsoft::WRL::ComPtr<ID3D12Fence> computeFence_;
    Microsoft::WRL::ComPtr<ID3D12Fence> copyFence_;

    UINT64 graphicsFenceValue_ = 0;
    UINT64 computeFenceValue_ = 0;
    UINT64 copyFenceValue_ = 0;

    HANDLE graphicsEvent_ = nullptr;
    HANDLE computeEvent_ = nullptr;
    HANDLE copyEvent_ = nullptr;
};
```

### Step 2: Create All Queues

```cpp
// D3D12Context.cpp
Result D3D12Context::initialize(const HWDeviceDesc& desc) {
    // Create graphics queue (can execute compute + graphics)
    D3D12_COMMAND_QUEUE_DESC graphicsDesc = {};
    graphicsDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    graphicsDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    graphicsDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    HRESULT hr = device_->CreateCommandQueue(&graphicsDesc, IID_PPV_ARGS(&graphicsQueue_));
    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to create graphics queue"};
    }

    // Create async compute queue
    D3D12_COMMAND_QUEUE_DESC computeDesc = {};
    computeDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    computeDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    hr = device_->CreateCommandQueue(&computeDesc, IID_PPV_ARGS(&computeQueue_));
    if (FAILED(hr)) {
        IGL_LOG_WARNING("Failed to create compute queue (async compute unavailable)");
        computeQueue_ = nullptr;  // Fallback to graphics queue
    }

    // Create copy queue
    D3D12_COMMAND_QUEUE_DESC copyDesc = {};
    copyDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    copyDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    hr = device_->CreateCommandQueue(&copyDesc, IID_PPV_ARGS(&copyQueue_));
    if (FAILED(hr)) {
        IGL_LOG_WARNING("Failed to create copy queue (DMA transfers unavailable)");
        copyQueue_ = nullptr;  // Fallback to graphics queue
    }

    // Create per-queue fences
    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&graphicsFence_));
    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to create graphics fence"};
    }

    if (computeQueue_) {
        hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&computeFence_));
        if (FAILED(hr)) {
            IGL_LOG_WARNING("Failed to create compute fence");
        }
    }

    if (copyQueue_) {
        hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&copyFence_));
        if (FAILED(hr)) {
            IGL_LOG_WARNING("Failed to create copy fence");
        }
    }

    // Create fence events
    graphicsEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (computeFence_) computeEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (copyFence_) copyEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    return Result{};
}
```

### Step 3: Implement Cross-Queue Synchronization

```cpp
// D3D12Context.cpp
ID3D12CommandQueue* D3D12Context::getQueue(QueueType type) const {
    switch (type) {
        case QueueType::Graphics: return graphicsQueue_.Get();
        case QueueType::Compute: return computeQueue_ ? computeQueue_.Get() : graphicsQueue_.Get();
        case QueueType::Copy: return copyQueue_ ? copyQueue_.Get() : graphicsQueue_.Get();
    }
    return graphicsQueue_.Get();
}

ID3D12Fence* D3D12Context::getFence(QueueType type) const {
    switch (type) {
        case QueueType::Graphics: return graphicsFence_.Get();
        case QueueType::Compute: return computeFence_ ? computeFence_.Get() : graphicsFence_.Get();
        case QueueType::Copy: return copyFence_ ? copyFence_.Get() : graphicsFence_.Get();
    }
    return graphicsFence_.Get();
}

UINT64 D3D12Context::signalQueue(QueueType type) {
    ID3D12CommandQueue* queue = getQueue(type);
    ID3D12Fence* fence = getFence(type);

    UINT64 fenceValue = 0;
    switch (type) {
        case QueueType::Graphics: fenceValue = ++graphicsFenceValue_; break;
        case QueueType::Compute: fenceValue = ++computeFenceValue_; break;
        case QueueType::Copy: fenceValue = ++copyFenceValue_; break;
    }

    queue->Signal(fence, fenceValue);
    return fenceValue;
}

void D3D12Context::waitForQueueValue(QueueType type, UINT64 fenceValue) {
    ID3D12Fence* fence = getFence(type);

    if (fence->GetCompletedValue() < fenceValue) {
        HANDLE event = nullptr;
        switch (type) {
            case QueueType::Graphics: event = graphicsEvent_; break;
            case QueueType::Compute: event = computeEvent_; break;
            case QueueType::Copy: event = copyEvent_; break;
        }

        fence->SetEventOnCompletion(fenceValue, event);
        WaitForSingleObject(event, INFINITE);
    }
}

// Cross-queue dependency: Make 'to' queue wait for 'from' queue
void D3D12Context::signalCrossQueue(QueueType from, QueueType to) {
    ID3D12CommandQueue* fromQueue = getQueue(from);
    ID3D12CommandQueue* toQueue = getQueue(to);
    ID3D12Fence* fence = getFence(from);

    // Get current fence value for 'from' queue
    UINT64 fenceValue = 0;
    switch (from) {
        case QueueType::Graphics: fenceValue = ++graphicsFenceValue_; break;
        case QueueType::Compute: fenceValue = ++computeFenceValue_; break;
        case QueueType::Copy: fenceValue = ++copyFenceValue_; break;
    }

    // Signal fence on 'from' queue
    fromQueue->Signal(fence, fenceValue);

    // Wait for fence on 'to' queue
    toQueue->Wait(fence, fenceValue);
}
```

### Step 4: Route Work to Appropriate Queues

```cpp
// Device.cpp - Route uploads to copy queue
Result Device::uploadTexture(...) {
    // Use copy queue for large uploads
    QueueType queueType = (dataSize > 1024 * 1024)
        ? QueueType::Copy
        : QueueType::Graphics;

    ID3D12CommandQueue* queue = ctx_->getQueue(queueType);

    // Record copy commands
    // ...

    // Submit to copy queue
    queue->ExecuteCommandLists(1, &cmdList);
    UINT64 fenceValue = ctx_->signalQueue(queueType);

    // Cross-queue sync: Graphics queue waits for copy completion
    if (queueType == QueueType::Copy) {
        ctx_->signalCrossQueue(QueueType::Copy, QueueType::Graphics);
    }

    return Result{};
}

// Device.cpp - Route async compute to compute queue
Result Device::dispatchCompute(...) {
    // Check if workload can benefit from async compute
    bool useAsyncCompute = computeQueue_ && workload.canOverlapWithGraphics;

    QueueType queueType = useAsyncCompute
        ? QueueType::Compute
        : QueueType::Graphics;

    ID3D12CommandQueue* queue = ctx_->getQueue(queueType);

    // Record compute commands
    // ...

    // Submit to compute queue
    queue->ExecuteCommandLists(1, &cmdList);
    UINT64 fenceValue = ctx_->signalQueue(queueType);

    return Result{};
}
```

---

## Testing Requirements

### Unit Tests

```cpp
TEST(D3D12ContextTest, MultiQueueCreation) {
    auto ctx = createContext();

    // Graphics queue always available
    EXPECT_NE(ctx->getQueue(QueueType::Graphics), nullptr);

    // Compute and copy queues may be fallback
    ID3D12CommandQueue* computeQueue = ctx->getQueue(QueueType::Compute);
    ID3D12CommandQueue* copyQueue = ctx->getQueue(QueueType::Copy);

    EXPECT_NE(computeQueue, nullptr);
    EXPECT_NE(copyQueue, nullptr);
}

TEST(D3D12ContextTest, CrossQueueSync) {
    auto ctx = createContext();

    // Signal graphics queue
    UINT64 fenceValue = ctx->signalQueue(QueueType::Graphics);

    // Make compute queue wait for graphics
    ctx->signalCrossQueue(QueueType::Graphics, QueueType::Compute);

    // Graphics fence should be signaled
    EXPECT_GE(ctx->getFence(QueueType::Graphics)->GetCompletedValue(), fenceValue);
}
```

### Performance Tests

```bash
# Test async compute overlap
cd build/vs/shell/Debug
./AsyncComputeSession_d3d12.exe --benchmark --multi-queue

# Expected: 15-20% performance improvement vs single-queue
# Measure: GPU utilization should be >80% (vs ~65% single-queue)
```

### Render Sessions

```bash
# All sessions should work with multi-queue enabled
./test_all_sessions.bat

# No regressions expected (fallback to graphics queue if needed)
```

---

## Definition of Done

- [ ] Three queues created (graphics, compute, copy)
- [ ] Per-queue fence tracking implemented
- [ ] Cross-queue synchronization API implemented
- [ ] Uploads routed to copy queue when beneficial
- [ ] Async compute routed to compute queue when available
- [ ] All unit tests pass
- [ ] Render sessions show no regressions
- [ ] Performance improvement measured (15-20% in compute-heavy workloads)
- [ ] User confirmation received before proceeding to next task

---

## Notes

- **Fallback strategy**: If compute/copy queue creation fails, use graphics queue
- **Must** coordinate descriptor heaps across queues (shader-visible heaps not thread-safe)
- **Must** use cross-queue fences for dependencies (e.g., copy → graphics)
- Modern GPUs (NVIDIA/AMD/Intel) all support 3 independent engines
- DMA engine can transfer 10-20 GB/s while rendering continues

---

## Related Issues

- **H-002**: Parallel command recording (needs per-queue command allocators)
- **H-003**: Texture state tracking race (needs per-queue barriers)
- **H-004**: Command allocator pool growth (needs per-queue pools)
