# H-003: Texture State Tracking Race Condition (High Priority)

**Priority:** P1 (High)
**Category:** Resources & Memory Lifetime
**Status:** Open
**Estimated Effort:** 2 days

---

## Problem Statement

The D3D12 backend tracks texture resource states in mutable member variables without synchronization. This causes:

1. **Race condition** - Multiple threads reading/writing `currentState_` simultaneously
2. **Incorrect barriers** - Threads see stale state, emit wrong barrier transitions
3. **Validation errors** - D3D12 debug layer reports state mismatches
4. **Visual corruption** - GPU reads texture in wrong state (COPY_DEST instead of SHADER_RESOURCE)

This is a **high-priority correctness issue** - multithreaded rendering will produce incorrect barriers and validation failures.

---

## Technical Details

### Current Problem

**In `Texture.cpp:1108`:**
```cpp
class Texture {
public:
    void transition(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList) {
        // ❌ RACE: Multiple threads can read/write currentState_ without mutex
        if (currentState_ != newState) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource_.Get();
            barrier.Transition.StateBefore = currentState_;  // ❌ Read without lock
            barrier.Transition.StateAfter = newState;

            cmdList->ResourceBarrier(1, &barrier);

            currentState_ = newState;  // ❌ Write without lock
        }
    }

private:
    D3D12_RESOURCE_STATES currentState_;  // ❌ Mutable, non-atomic, no mutex
};
```

**Impact:**
- Thread A reads `currentState_ = PIXEL_SHADER_RESOURCE`
- Thread B writes `currentState_ = RENDER_TARGET` simultaneously
- Thread A emits barrier with corrupted `StateBefore`
- D3D12 validation error: "Resource barrier state mismatch"
- GPU undefined behavior if barriers are incorrect

### Microsoft D3D12 Threading Rules

From [Resource Barriers documentation](https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12):

> **Resource state tracking must be synchronized across threads.**
> If multiple command lists transition the same resource, the application must ensure:
> 1. State transitions are sequenced correctly via fences
> 2. State tracking is protected by mutexes or atomic operations
> 3. Each command list sees the correct "before" state

### Correct Pattern (from MiniEngine)

```cpp
// MiniEngine GpuResource.h
class GpuResource {
public:
    void TransitionResource(
        ID3D12GraphicsCommandList* cmdList,
        D3D12_RESOURCE_STATES newState) {

        // ✅ Thread-safe state transition
        std::lock_guard<std::mutex> lock(m_StateMutex);

        if (m_CurrentState != newState) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_pResource.Get();
            barrier.Transition.StateBefore = m_CurrentState;
            barrier.Transition.StateAfter = newState;

            cmdList->ResourceBarrier(1, &barrier);

            m_CurrentState = newState;
        }
    }

private:
    D3D12_RESOURCE_STATES m_CurrentState;
    mutable std::mutex m_StateMutex;  // ✅ Protects state transitions
};
```

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/Texture.h`**
   - Add `mutable std::mutex stateMutex_;`
   - Add thread-safety documentation
   - Consider per-subresource state tracking for advanced use

2. **`src/igl/d3d12/Texture.cpp:1108`**
   - Wrap `currentState_` reads/writes in mutex
   - Add validation for state transition validity
   - Log state transitions in debug builds

3. **`src/igl/d3d12/Buffer.h`** (similar issue)
   - Add mutex for buffer state tracking
   - Protect UAV counter state transitions

4. **`src/igl/d3d12/RenderCommandEncoder.cpp`**
   - Verify barrier sequences when transitioning textures
   - Add debug logging for multi-threaded barrier ordering

### Key Identifiers

- **State tracking:** `currentState_`, `transitionResource()`
- **Barrier emission:** `cmdList->ResourceBarrier()`
- **Mutex protection:** Add `std::lock_guard<std::mutex>`

---

## Official References

### Microsoft Documentation

- [Resource Barriers](https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)
  - Section: "Multithreaded Considerations"
  - Key rule: "Protect state tracking with synchronization primitives"

- [Multithreading in D3D12](https://learn.microsoft.com/windows/win32/direct3d12/important-changes-from-directx-11-to-directx-12#multithreading)
  - Section: "Resource State Tracking"
  - Shows fence-based cross-thread synchronization

- [MiniEngine GpuResource.cpp](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/GpuResource.cpp)
  - Lines 80-120: Thread-safe state transitions
  - Shows mutex-protected state tracking pattern

---

## Implementation Guidance

### Step 1: Add State Mutex

```cpp
// Texture.h
class Texture {
public:
    // ✅ Thread-safe state transition
    void transition(
        D3D12_RESOURCE_STATES newState,
        ID3D12GraphicsCommandList* cmdList);

    // ✅ Thread-safe state query
    D3D12_RESOURCE_STATES getCurrentState() const;

private:
    D3D12_RESOURCE_STATES currentState_;
    mutable std::mutex stateMutex_;  // ✅ Protects currentState_
};
```

### Step 2: Protect State Transitions

```cpp
// Texture.cpp
void Texture::transition(
    D3D12_RESOURCE_STATES newState,
    ID3D12GraphicsCommandList* cmdList) {

    // ✅ Lock mutex for entire transition
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (currentState_ == newState) {
        return;  // No transition needed
    }

    // Validate state transition (optional but recommended)
    if (!isValidTransition(currentState_, newState)) {
        IGL_LOG_ERROR("Invalid state transition: %d -> %d",
                     currentState_, newState);
        IGL_ASSERT_MSG(false, "Invalid resource state transition");
        return;
    }

    // Emit barrier
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = currentState_;
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    cmdList->ResourceBarrier(1, &barrier);

    // Update state
    currentState_ = newState;

#ifdef IGL_DEBUG
    IGL_LOG_DEBUG("Texture state transition: %s -> %s",
                 getStateName(barrier.Transition.StateBefore),
                 getStateName(newState));
#endif
}

D3D12_RESOURCE_STATES Texture::getCurrentState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentState_;
}
```

### Step 3: Add State Transition Validation

```cpp
// Texture.cpp - helper function
bool Texture::isValidTransition(
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {

    // Cannot transition FROM common/present to other states without GPU sync
    if (before == D3D12_RESOURCE_STATE_COMMON &&
        after != D3D12_RESOURCE_STATE_COMMON) {
        // This is allowed for UPLOAD/READBACK heaps but not DEFAULT
        if (heapType_ == D3D12_HEAP_TYPE_DEFAULT) {
            return false;
        }
    }

    // Cannot write to resource in multiple states simultaneously
    const D3D12_RESOURCE_STATES writeStates =
        D3D12_RESOURCE_STATE_RENDER_TARGET |
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
        D3D12_RESOURCE_STATE_COPY_DEST;

    bool beforeIsWrite = (before & writeStates) != 0;
    bool afterIsWrite = (after & writeStates) != 0;

    // Read-after-write or write-after-write requires explicit barrier
    if (beforeIsWrite || afterIsWrite) {
        return true;  // Barrier required
    }

    // Read-after-read is allowed (no barrier needed but not an error)
    return true;
}
```

### Step 4: Per-Subresource State Tracking (Advanced)

```cpp
// Texture.h - for advanced multi-threaded scenarios
class Texture {
private:
    // ✅ Track state per mip level / array slice
    struct SubresourceState {
        D3D12_RESOURCE_STATES state;
        mutable std::mutex mutex;
    };

    std::vector<SubresourceState> subresourceStates_;

public:
    void transitionSubresource(
        UINT mipLevel,
        UINT arraySlice,
        D3D12_RESOURCE_STATES newState,
        ID3D12GraphicsCommandList* cmdList) {

        UINT subresource = D3D12CalcSubresource(
            mipLevel, arraySlice, 0,
            mipLevels_, arrayLayers_);

        if (subresource >= subresourceStates_.size()) {
            IGL_ASSERT_MSG(false, "Invalid subresource index");
            return;
        }

        auto& subres = subresourceStates_[subresource];
        std::lock_guard<std::mutex> lock(subres.mutex);

        if (subres.state == newState) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource_.Get();
        barrier.Transition.StateBefore = subres.state;
        barrier.Transition.StateAfter = newState;
        barrier.Transition.Subresource = subresource;

        cmdList->ResourceBarrier(1, &barrier);

        subres.state = newState;
    }
};
```

---

## Testing Requirements

### Unit Tests

```cpp
TEST(TextureTest, ThreadSafeStateTransitions) {
    auto device = createDevice();
    auto texture = device->createTexture({
        .width = 1024,
        .height = 1024,
        .usage = TextureUsage::Sampled | TextureUsage::Storage
    });

    constexpr UINT kNumThreads = 8;
    std::vector<std::thread> threads;
    std::atomic<UINT> transitionCount{0};

    for (UINT i = 0; i < kNumThreads; i++) {
        threads.emplace_back([&, i]() {
            auto cmdBuffer = device->createCommandBuffer();
            auto cmdList = cmdBuffer->getD3D12CommandList();

            // Alternate between two states
            D3D12_RESOURCE_STATES state = (i % 2 == 0)
                ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                : D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

            for (UINT j = 0; j < 100; j++) {
                texture->transition(state, cmdList);
                transitionCount.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Expected: No crashes, no validation errors
    EXPECT_EQ(transitionCount.load(), kNumThreads * 100);
}

TEST(TextureTest, NoRaceConditionValidation) {
    auto device = createDevice();
    auto texture = device->createTexture({...});

    // Use ThreadSanitizer (TSAN) to detect races
    // Build with: cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ..

    std::thread thread1([&]() {
        texture->transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, ...);
    });

    std::thread thread2([&]() {
        texture->transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, ...);
    });

    thread1.join();
    thread2.join();

    // Expected: TSAN reports zero data races
}
```

### Validation Tests

```bash
# Enable D3D12 debug layer
set D3D12_DEBUG=1

# Run multi-threaded rendering test
cd build/vs/shell/Debug
./MultiThreadedRenderSession_d3d12.exe

# Check for validation errors:
# ❌ BAD: "Resource barrier state mismatch"
# ✅ GOOD: No validation errors
```

### Stress Test

```cpp
void StressTestMultiThreadedTextureAccess() {
    auto device = createDevice();

    // Create shared texture
    auto texture = device->createTexture({
        .width = 2048,
        .height = 2048,
        .usage = TextureUsage::Sampled | TextureUsage::Storage
    });

    // 8 threads transitioning texture 10000 times
    constexpr UINT kNumThreads = 8;
    constexpr UINT kIterations = 10000;

    std::vector<std::thread> threads;
    for (UINT i = 0; i < kNumThreads; i++) {
        threads.emplace_back([&]() {
            for (UINT j = 0; j < kIterations; j++) {
                auto cmdBuffer = device->createCommandBuffer();
                auto cmdList = cmdBuffer->getD3D12CommandList();

                // Random state transitions
                D3D12_RESOURCE_STATES state =
                    (j % 3 == 0) ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE :
                    (j % 3 == 1) ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS :
                                   D3D12_RESOURCE_STATE_COPY_SOURCE;

                texture->transition(state, cmdList);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Expected: No crashes, no validation errors, no TSAN warnings
}
```

---

## Definition of Done

- [ ] `stateMutex_` added to Texture class
- [ ] All `currentState_` accesses protected by mutex
- [ ] State transition validation implemented
- [ ] Unit tests pass (thread-safe transitions)
- [ ] TSAN reports zero data races
- [ ] D3D12 debug layer shows no state mismatch errors
- [ ] Stress test passes (8 threads, 10000 transitions)
- [ ] Performance impact measured (mutex overhead <1% frame time)
- [ ] User confirmation received before proceeding to next task

---

## Notes

- **DO NOT** use spinlocks or atomics for state tracking (complex state transitions need mutexes)
- **MUST** protect both read and write of `currentState_` (not just writes)
- Consider lock-free state tracking for high-frequency scenarios (advanced optimization)
- MiniEngine uses per-subresource state tracking for fine-grained parallelism
- Mutex contention should be minimal if threads work on different resources

---

## Related Issues

- **H-002**: Parallel command recording (creates the multi-threading scenario)
- **H-013**: PSO cache not thread-safe (similar synchronization issue)
- **C-004**: Storage buffer synchronous wait (affects barrier timing)
