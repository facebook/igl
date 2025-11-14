# G-009: Copy Operations Not Overlapped with Rendering

**Priority**: MEDIUM
**Category**: Graphics - Performance
**Estimated Effort**: 4-5 days
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend does not overlap copy operations (texture uploads, buffer updates, readbacks) with rendering work. All copies are performed on the direct graphics queue synchronously, causing the GPU to stall waiting for data transfers to complete before rendering can proceed.

**Symptoms**:
- GPU idle time during large texture uploads
- Frame time spikes when updating dynamic buffers
- Underutilization of copy engine capabilities
- Poor performance when streaming assets
- Readback operations blocking the render pipeline

**Impact**:
- Reduced GPU utilization (rendering waits for copies)
- Lower overall frame rate
- Stuttering during asset loading
- Inefficient use of DMA engines on modern GPUs
- Cannot hide latency of CPU-to-GPU data transfers

---

## 2. Root Cause Analysis

The lack of asynchronous copy operations stems from several architectural issues:

### 2.1 Single Queue Architecture
The D3D12 backend likely uses only the direct (graphics) queue for all operations. Modern GPUs have dedicated copy engines that can work in parallel with the graphics pipeline.

**Expected Architecture**:
- Direct Queue (Graphics): Rendering commands
- Copy Queue (DMA): Asynchronous data transfers
- Compute Queue: Compute shaders (if needed)

### 2.2 No Copy Queue Abstraction
The IGL abstraction layer may not expose separate copy queues, or the D3D12 backend doesn't implement them even if the abstraction exists.

### 2.3 Synchronous Upload Pattern
Texture and buffer uploads likely follow this pattern:
```cpp
// Current: Synchronous on graphics queue
void uploadTexture(Texture* tex, const void* data) {
    // Blocks graphics queue until upload completes
    graphicsCommandList->CopyResource(texture, uploadBuffer);
    graphicsQueue->ExecuteCommandLists(...);
    graphicsQueue->Signal(fence, fenceValue);
    waitForFenceValue(fenceValue);  // CPU stall!
}
```

Instead of:
```cpp
// Desired: Asynchronous on copy queue
void uploadTextureAsync(Texture* tex, const void* data) {
    copyCommandList->CopyResource(texture, uploadBuffer);
    copyQueue->ExecuteCommandLists(...);
    copyQueue->Signal(copyFence, copyFenceValue);
    // Graphics queue waits on GPU, not CPU
    graphicsQueue->Wait(copyFence, copyFenceValue);
    // Rendering can proceed once copy completes on GPU
}
```

### 2.4 Missing Resource Synchronization
Proper async copies require careful synchronization between queues using fences, which may not be implemented.

### 2.5 No Upload/Readback Queue Management
There may be no dedicated upload/readback buffer management system that can batch transfers and submit them on the copy queue.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Copy Queues and Command Lists**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization
   - Explains copy queue capabilities and synchronization

2. **Multi-Engine Synchronization**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/multi-engine
   - Official guide to using multiple command queues

3. **Fence Synchronization**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization#fence-based-resource-synchronization
   - How to synchronize between queues using fences

4. **Asynchronous Texture Streaming**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/residency
   - Best practices for async texture uploads

5. **Command Queue Types**:
   - https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_command_list_type
   - D3D12_COMMAND_LIST_TYPE_COPY documentation

**Sample Code**:

6. **DirectX-Graphics-Samples - Multi-Engine**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12MultiGPU
   - Reference implementation of multi-queue synchronization

7. **MiniEngine Copy Queue Management**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/CommandContext.cpp
   - See `CopyContext` and queue management

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for command queue creation**:
```
Pattern: "CreateCommandQueue" OR "ID3D12CommandQueue"
Files: src/igl/d3d12/Device.cpp, src/igl/d3d12/CommandQueue.cpp
Focus: How many queues are created and of what types
```

**Search for copy operations**:
```
Pattern: "CopyResource" OR "CopyTextureRegion" OR "CopyBufferRegion"
Files: src/igl/d3d12/**/*.cpp
Focus: Where copies are submitted and on which queue
```

**Search for upload buffer management**:
```
Pattern: "upload" + "buffer" OR "staging"
Files: src/igl/d3d12/Buffer.cpp, src/igl/d3d12/Texture.cpp
Focus: How upload resources are created and used
```

**Search for fence usage**:
```
Pattern: "CreateFence" OR "ID3D12Fence" OR "Signal" + "Wait"
Files: src/igl/d3d12/**/*.cpp
Focus: Current fence synchronization patterns
```

### 4.2 Likely File Locations

Based on typical D3D12 backend architecture:
- `src/igl/d3d12/Device.cpp` - Command queue creation
- `src/igl/d3d12/CommandQueue.cpp` - Queue management and execution
- `src/igl/d3d12/CommandBuffer.cpp` - Command list recording
- `src/igl/d3d12/Texture.cpp` - Texture upload operations
- `src/igl/d3d12/Buffer.cpp` - Buffer upload operations
- `src/igl/d3d12/UploadRingBuffer.cpp` - Upload buffer allocation (if exists)

### 4.3 Key Data Structures to Find

Look for:
- `ComPtr<ID3D12CommandQueue>` instances (should have at least 2: graphics + copy)
- Upload buffer allocators
- Fence objects for cross-queue synchronization
- Command list pools for copy operations

---

## 5. Detection Strategy

### 5.1 Add Instrumentation Code

**Step 1: Verify queue types being used**

Add logging in command queue creation:

```cpp
// In Device::createCommandQueue or similar
HRESULT createCommandQueue(D3D12_COMMAND_LIST_TYPE type) {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    ComPtr<ID3D12CommandQueue> queue;
    HRESULT hr = device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue));

    const char* typeStr = (type == D3D12_COMMAND_LIST_TYPE_DIRECT) ? "DIRECT" :
                          (type == D3D12_COMMAND_LIST_TYPE_COPY) ? "COPY" :
                          (type == D3D12_COMMAND_LIST_TYPE_COMPUTE) ? "COMPUTE" : "UNKNOWN";
    IGL_LOG_INFO("Created command queue: Type=%s\n", typeStr);

    return hr;
}
```

**Step 2: Track copy operation queue usage**

```cpp
// Add to copy operation code
void executeCopyOperation() {
    IGL_LOG_DEBUG("Copy operation submitted on queue type: %s\n",
                  currentQueueType_ == D3D12_COMMAND_LIST_TYPE_DIRECT ? "DIRECT (BAD)" : "COPY (GOOD)");
}
```

**Step 3: Measure GPU timeline**

Use PIX or add timing queries:
```cpp
// Measure overlap of copy and render work
struct GPUTimestamps {
    uint64_t copyStart;
    uint64_t copyEnd;
    uint64_t renderStart;
    uint64_t renderEnd;
};

void analyzeOverlap(const GPUTimestamps& ts) {
    bool overlap = (ts.copyStart < ts.renderEnd) && (ts.renderStart < ts.copyEnd);
    IGL_LOG_INFO("Copy/Render Overlap: %s\n", overlap ? "YES" : "NO");
}
```

### 5.2 Manual Testing Procedure

**Test 1: Run tests and check console output**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

Look for queue creation logs. Expected baseline: Only DIRECT queue created.

**Test 2: Run texture-heavy samples**
```bash
./build/Debug/samples/TQSession
./build/Debug/samples/MRTSession
```

Monitor frame times during texture uploads.

**Test 3: Profile with PIX**
```bash
# Capture frame with PIX for Windows
# Analyze GPU timeline for copy vs render overlap
# Look for idle time on graphics queue during copies
```

### 5.3 Expected Baseline Metrics

**Before Fix**:
- Only 1 command queue (DIRECT type)
- All copy operations on graphics queue
- 0% overlap between copies and rendering
- GPU graphics engine idle during large uploads

**After Fix**:
- At least 2 command queues (DIRECT + COPY)
- Copy operations on dedicated copy queue
- Measurable overlap (copy and render in parallel)
- Reduced frame time variance

---

## 6. Fix Guidance

### 6.1 Create Copy Command Queue

**Step 1: Add copy queue to Device class**

Find the Device class and add:

```cpp
// In Device.h or Device class definition
class Device {
private:
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> graphicsQueue_;
    ComPtr<ID3D12CommandQueue> copyQueue_;  // ADD THIS
    ComPtr<ID3D12CommandQueue> computeQueue_;  // Optional for later

    ComPtr<ID3D12Fence> graphicsFence_;
    ComPtr<ID3D12Fence> copyFence_;  // ADD THIS
    uint64_t graphicsFenceValue_ = 0;
    uint64_t copyFenceValue_ = 0;  // ADD THIS
};
```

**Step 2: Initialize copy queue in Device constructor**

```cpp
// In Device initialization
Result Device::initialize() {
    // Create graphics queue
    D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc = {};
    graphicsQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    graphicsQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    graphicsQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    HRESULT hr = device_->CreateCommandQueue(&graphicsQueueDesc, IID_PPV_ARGS(&graphicsQueue_));
    if (FAILED(hr)) {
        return getResultFromHRESULT(hr);
    }
    graphicsQueue_->SetName(L"IGL Graphics Queue");

    // Create copy queue
    D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
    copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    copyQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = device_->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&copyQueue_));
    if (FAILED(hr)) {
        return getResultFromHRESULT(hr);
    }
    copyQueue_->SetName(L"IGL Copy Queue");

    // Create fences
    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&graphicsFence_));
    if (FAILED(hr)) {
        return getResultFromHRESULT(hr);
    }

    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&copyFence_));
    if (FAILED(hr)) {
        return getResultFromHRESULT(hr);
    }

    IGL_LOG_INFO("D3D12 Device initialized with Graphics and Copy queues\n");
    return Result();
}
```

### 6.2 Implement Async Upload Helper

Create a helper class for asynchronous uploads:

```cpp
// CopyContext.h - Manages asynchronous copy operations
class CopyContext {
public:
    CopyContext(ID3D12Device* device, ID3D12CommandQueue* copyQueue, ID3D12Fence* copyFence)
        : device_(device), copyQueue_(copyQueue), copyFence_(copyFence) {
        initializeCommandList();
    }

    // Begin a copy batch
    void begin() {
        commandAllocator_->Reset();
        commandList_->Reset(commandAllocator_.Get(), nullptr);
    }

    // Submit all batched copies and return fence value to wait for
    uint64_t end() {
        commandList_->Close();
        ID3D12CommandList* lists[] = {commandList_.Get()};
        copyQueue_->ExecuteCommandLists(1, lists);

        // Signal fence
        currentFenceValue_++;
        copyQueue_->Signal(copyFence_, currentFenceValue_);

        return currentFenceValue_;
    }

    // Copy buffer asynchronously
    void copyBuffer(ID3D12Resource* dst, ID3D12Resource* src, uint64_t size) {
        commandList_->CopyBufferRegion(dst, 0, src, 0, size);
    }

    // Copy texture asynchronously
    void copyTexture(ID3D12Resource* dst, ID3D12Resource* src) {
        commandList_->CopyResource(dst, src);
    }

private:
    void initializeCommandList() {
        device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
                                        IID_PPV_ARGS(&commandAllocator_));
        device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY,
                                   commandAllocator_.Get(), nullptr,
                                   IID_PPV_ARGS(&commandList_));
        commandList_->Close();
    }

    ID3D12Device* device_;
    ID3D12CommandQueue* copyQueue_;
    ID3D12Fence* copyFence_;
    ComPtr<ID3D12CommandAllocator> commandAllocator_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    uint64_t currentFenceValue_ = 0;
};
```

### 6.3 Modify Texture Upload to Use Copy Queue

Find texture upload code and modify:

```cpp
// In Texture::upload() or similar
Result Texture::upload(const TextureRangeDesc& range, const void* data) {
    // Get upload buffer from ring buffer allocator
    auto uploadAllocation = device_->allocateUploadMemory(uploadSize);

    // Copy CPU data to upload buffer
    memcpy(uploadAllocation.cpuAddress, data, uploadSize);

    // BEGIN ASYNC COPY
    auto& copyContext = device_->getCopyContext();
    copyContext.begin();

    // Record copy from upload buffer to texture
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = uploadAllocation.resource;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    // ... set up footprint ...

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = resource_.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = calculateSubresourceIndex(range);

    copyContext.commandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // Submit async copy and get fence value
    uint64_t copyFenceValue = copyContext.end();

    // Make graphics queue wait for copy to complete
    device_->getGraphicsQueue()->Wait(device_->getCopyFence(), copyFenceValue);

    // Mark texture as "pending copy" so it's not used before copy completes
    pendingCopyFence_ = copyFenceValue;

    return Result();
}
```

### 6.4 Implement Cross-Queue Synchronization

Ensure graphics work waits for copy completion:

```cpp
// In command buffer submission or texture binding
void ensureTextureReady(Texture* texture) {
    if (texture->hasPendingCopy()) {
        uint64_t copyFenceValue = texture->getPendingCopyFence();

        // Option 1: Wait on graphics queue (GPU-side wait, no CPU stall)
        graphicsQueue_->Wait(copyFence_, copyFenceValue);

        // Option 2: Insert fence wait in command list (also GPU-side)
        // commandList_->Wait(copyFence_, copyFenceValue);

        texture->clearPendingCopy();
    }
}

// Call before using texture in rendering
void bindTexture(Texture* texture, uint32_t slot) {
    ensureTextureReady(texture);
    // ... proceed with binding ...
}
```

### 6.5 Add Copy Queue Management API

Expose copy queue in IGL abstraction if needed:

```cpp
// In Device interface or implementation
class Device {
public:
    // Get copy context for async uploads
    CopyContext& getCopyContext() { return copyContext_; }

    // Synchronize all pending copy operations
    void flushCopyQueue() {
        uint64_t fenceValue = copyFence_->GetCompletedValue();
        graphicsQueue_->Wait(copyFence_, fenceValue);
    }

    // For cleanup: wait for all copy operations to complete
    void waitForCopyQueueIdle() {
        copyFenceValue_++;
        copyQueue_->Signal(copyFence_, copyFenceValue_);
        waitForFenceValue(copyFence_, copyFenceValue_);
    }

private:
    CopyContext copyContext_;
};
```

---

## 7. Testing Requirements

### 7.1 Functional Testing

**Test 1: Verify basic functionality**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

**Expected**: All existing tests pass. Rendering output identical.

**Test 2: Texture upload tests**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

**Expected**: Textures upload correctly, no visual artifacts.

**Test 3: Buffer update tests**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Buffer*"
```

**Expected**: Dynamic buffer updates work correctly.

**Test 4: Run all rendering samples**
```bash
./build/Debug/samples/Textured3DCube
./build/Debug/samples/TQSession
./build/Debug/samples/MRTSession
```

**Expected**: All render correctly with no visual regressions.

### 7.2 Performance Testing

**Test 1: Measure queue creation**

Check logs for:
```
Created command queue: Type=DIRECT
Created command queue: Type=COPY
```

**Test 2: Verify copy operations use copy queue**

Check logs for:
```
Copy operation submitted on queue type: COPY (GOOD)
```

**Test 3: Profile with PIX**

Capture a frame and verify:
- Copy queue timeline shows activity
- Graphics queue and copy queue overlap
- Graphics queue has less idle time

**Target Metrics**:
- Copy operations: 100% on copy queue
- GPU utilization: Increased due to parallelism
- Frame time variance: Reduced during asset loading

### 7.3 Synchronization Testing

**Test 1: Verify no race conditions**

Run tests repeatedly:
```bash
for i in {1..20}; do ./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"; done
```

**Expected**: 100% pass rate, no intermittent failures.

**Test 2: Enable D3D12 debug layer**

```cpp
// In Device initialization
ComPtr<ID3D12Debug> debugController;
D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
debugController->EnableDebugLayer();
```

Run tests and check for validation errors related to resource synchronization.

### 7.4 Test Modification Restrictions

**CRITICAL CONSTRAINTS**:
- Do NOT modify any existing test assertions
- Do NOT change test expected values
- Do NOT alter test logic or flow
- Changes must be implementation-only, not test-visible
- All tests must pass without modification

If tests fail, the synchronization logic needs fixing, not the tests.

---

## 8. Definition of Done

### 8.1 Implementation Checklist

- [ ] Copy command queue created and initialized
- [ ] Copy command list management implemented
- [ ] Async texture upload path implemented
- [ ] Async buffer upload path implemented
- [ ] Cross-queue synchronization with fences implemented
- [ ] Texture "pending copy" tracking implemented
- [ ] Graphics queue waits for copy completion before use
- [ ] Instrumentation added to verify queue usage
- [ ] Copy queue properly shut down on device destruction

### 8.2 Testing Checklist

- [ ] All unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"`
- [ ] Logs confirm copy queue creation
- [ ] Logs confirm copy operations use copy queue
- [ ] PIX capture shows copy/render overlap
- [ ] No visual regressions in sample applications
- [ ] No D3D12 validation errors
- [ ] No race conditions in repeated test runs

### 8.3 Performance Checklist

- [ ] Frame time variance reduced during uploads
- [ ] GPU utilization increased (less idle time)
- [ ] Copy operations don't block graphics queue
- [ ] Large texture uploads show measurable speedup

### 8.4 Documentation Checklist

- [ ] Code comments explain async copy strategy
- [ ] Synchronization points documented
- [ ] Queue usage patterns documented

### 8.5 Sign-Off Criteria

**Before proceeding with this task, YOU MUST**:
1. Read and understand all 11 sections of this document
2. Verify you can locate command queue creation code
3. Understand D3D12 fence synchronization model
4. Confirm you can build and run the test suite
5. Get explicit approval to proceed with implementation

**Do not make any code changes until all criteria are met and approval is given.**

---

## 9. Related Issues

### 9.1 Blocking Issues
None - this can be implemented independently.

### 9.2 Blocked Issues
- May improve performance for other async operations
- Enables future enhancements like texture streaming

### 9.3 Related Performance Issues
- G-008: PSO cache miss rate (both affect frame time consistency)
- G-010: Resource barrier stalls (both affect pipeline efficiency)
- P0 DX12-004: copyTextureToBuffer implementation (could benefit from async copy)

---

## 10. Implementation Priority

**Priority Level**: MEDIUM

**Rationale**:
- Significant performance improvement potential
- Enables better GPU utilization
- Relatively isolated change
- Low risk if synchronization is correct
- Standard D3D12 best practice

**Recommended Order**:
1. Add instrumentation to measure baseline
2. Create copy queue and basic infrastructure
3. Implement async texture uploads first (simpler)
4. Add async buffer uploads
5. Thorough synchronization testing
6. Performance validation with PIX

**Estimated Timeline**:
- Day 1: Investigation, instrumentation, copy queue creation
- Day 2: Async texture upload implementation
- Day 3: Async buffer upload implementation
- Day 4: Synchronization refinement and testing
- Day 5: Performance validation and final testing

---

## 11. References

### 11.1 Microsoft Official Documentation
- Copy Queues: https://docs.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization
- Multi-Engine: https://docs.microsoft.com/en-us/windows/win32/direct3d12/multi-engine
- Fence Synchronization: https://docs.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization#fence-based-resource-synchronization
- Command List Types: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_command_list_type
- Async Texture Streaming: https://docs.microsoft.com/en-us/windows/win32/direct3d12/residency

### 11.2 Sample Code References
- DirectX-Graphics-Samples Multi-GPU: https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12MultiGPU
- MiniEngine CommandContext: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/CommandContext.cpp
  - See `CopyContext` implementation

### 11.3 Additional Reading
- PIX for Windows: https://devblogs.microsoft.com/pix/
- Multi-Engine Optimization: https://gpuopen.com/learn/concurrent-execution-asynchronous-queues/

### 11.4 Internal Codebase
- Search for "CreateCommandQueue" in src/igl/d3d12/
- Review UploadRingBuffer implementation
- Check existing fence usage patterns

---

**Document Version**: 1.0
**Last Updated**: 2025-11-12
**Author**: Development Team
**Reviewer**: [Pending]
