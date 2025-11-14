# E-013: Compute Shader Dispatch Size Not Validated

**Priority:** P1-Medium
**Category:** Enhancement - Compute Pipeline
**Status:** Open
**Effort:** Small (1-2 days)

---

## 1. Problem Statement

The D3D12 backend does not validate compute shader dispatch parameters against hardware limits or shader thread group dimensions. Invalid dispatch calls can cause GPU hangs, device removal, or silently produce incorrect results without any warning.

### The Danger

**GPU hangs and device removal** from invalid compute dispatches:
- Dispatching with 0 thread groups causes GPU hang
- Thread group dimensions exceeding hardware limits (typically 1024 threads per group)
- Total dispatch size exceeding max (65,535 groups per dimension)
- Mismatch between shader `[numthreads(X,Y,Z)]` and dispatch parameters
- Wasted GPU resources dispatching far more threads than needed

**Real-world scenario:**
```cpp
// Shader declares:
[numthreads(16, 16, 1)]  // 256 threads per group
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID) { ... }

// Application dispatches:
commandEncoder->dispatchThreadgroups(1024*1024, 1, 1);  // ← WAY TOO MANY!

Result:
- D3D12 device removed (TDR timeout)
- Application crashes with cryptic error
- No indication that dispatch size was the problem
```

Without validation:
- **Minutes to hours** of GPU hang/timeout debugging
- Risk of TDR (Timeout Detection and Recovery) in production
- Silent computation errors from incorrect thread counts

With validation:
- **Immediate error:** "Dispatch X dimension 1048576 exceeds limit 65535"
- Problem identified and fixed in seconds

---

## 2. Root Cause Analysis

The compute dispatch implementation doesn't validate parameters:

**Current Implementation:**
```cpp
// In ComputeCommandEncoder.cpp or similar
void ComputeCommandEncoder::dispatchThreadgroups(
    uint32_t groupsX,
    uint32_t groupsY,
    uint32_t groupsZ) {

  // NO VALIDATION - directly calls D3D12!
  commandList_->Dispatch(groupsX, groupsY, groupsZ);
}
```

**Missing Validations:**

1. **Zero group count:**
   ```cpp
   dispatchThreadgroups(0, 256, 1);  // ← Should error, currently proceeds
   ```

2. **Exceeds D3D12 limits:**
   ```cpp
   dispatchThreadgroups(100000, 100000, 1);  // ← Exceeds 65535 limit
   ```

3. **Thread group size mismatch:**
   ```cpp
   // Shader: [numthreads(1024, 1, 1)]  // Max for many GPUs
   // But numthreads(1025, 1, 1) would be invalid
   ```

4. **Total thread count excessive:**
   ```cpp
   // groupsX * threadsPerGroupX * groupsY * threadsPerGroupY might overflow
   dispatchThreadgroups(65535, 65535, 65535);  // Billions of threads!
   ```

**D3D12 Limits (Not Checked):**
- Max groups per dimension: `D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION` (65,535)
- Max threads per group: `D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP` (1,024)
- Max threads per dimension: `D3D12_CS_THREAD_GROUP_MAX_X/Y/Z` (1024, 1024, 64)

---

## 3. Official Documentation References

- **Compute Shader Limits:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-feature-levels

- **D3D12_CS Constants:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants

- **Dispatch Method:**
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-dispatch

- **Compute Shader Overview:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader

---

## 4. Code Location Strategy

**Primary Files to Modify:**

1. **Compute command encoder:**
   ```
   Search for: "dispatchThreadgroups" or "Dispatch("
   In files: src/igl/d3d12/ComputeCommandEncoder.cpp, src/igl/d3d12/ComputeCommandEncoder.h
   Pattern: Look for ID3D12GraphicsCommandList::Dispatch() calls
   ```

2. **Compute pipeline state:**
   ```
   Search for: "ComputePipelineState" or "CreateComputePipelineState"
   In files: src/igl/d3d12/ComputePipelineState.cpp, src/igl/d3d12/ComputePipelineState.h
   Pattern: Look for compute shader reflection and numthreads extraction
   ```

3. **Device capabilities:**
   ```
   Search for: "DeviceLimits" or "getCapabilities"
   In files: src/igl/d3d12/Device.cpp
   Pattern: Look for where hardware limits are queried
   ```

4. **Shader reflection:**
   ```
   Search for: "ID3D12ShaderReflection" or "GetThreadGroupSize"
   In files: src/igl/d3d12/ShaderModule.cpp
   Pattern: Look for shader reflection code to extract [numthreads]
   ```

---

## 5. Detection Strategy

### Reproduction Steps:

1. **Create compute shader with known thread group size:**
   ```hlsl
   // test_compute.hlsl
   RWStructuredBuffer<float> output : register(u0);

   [numthreads(16, 16, 1)]  // 256 threads per group
   void main(uint3 DTid : SV_DispatchThreadID) {
       output[DTid.x] = DTid.x * 2.0;
   }
   ```

2. **Dispatch with invalid parameters:**
   ```cpp
   // Test case 1: Zero groups
   computeEncoder->dispatchThreadgroups(0, 1, 1);  // ← Should error

   // Test case 2: Exceeds limit
   computeEncoder->dispatchThreadgroups(100000, 1, 1);  // ← Should error

   // Test case 3: Negative dimension (if unsigned, large value)
   computeEncoder->dispatchThreadgroups(-1, 1, 1);  // ← Becomes 4294967295!
   ```

3. **Expected behavior:**
   ```
   Current: Crashes or hangs GPU
   Expected: IGL_ERROR with clear message before dispatch
   ```

### Test with Debug Layer:

```bash
# Enable D3D12 debug layer
set D3D12_DEBUG=1
./build/Debug/IGLTests.exe --gtest_filter="*Compute*Dispatch*"

# Current: May see device removed error
# Expected: Validation error before dispatch
```

---

## 6. Fix Guidance

### Step 1: Extract Thread Group Size from Shader Reflection

```cpp
// In ComputePipelineState.h
struct ComputeThreadGroupSize {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;

  bool isValid() const {
    return x > 0 && y > 0 && z > 0;
  }

  uint32_t totalThreads() const {
    return x * y * z;
  }
};

class ComputePipelineState final : public IComputePipelineState {
 public:
  // ... existing methods ...

  ComputeThreadGroupSize getThreadGroupSize() const { return threadGroupSize_; }

 private:
  ComputeThreadGroupSize threadGroupSize_;
};

// In ComputePipelineState.cpp
Result<std::shared_ptr<IComputePipelineState>> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* outResult) {

  // ... compile shader ...

  // Extract thread group size from shader reflection
  ComPtr<ID3D12ShaderReflection> reflection;
  HRESULT hr = D3DReflect(
      shaderBlob->GetBufferPointer(),
      shaderBlob->GetBufferSize(),
      IID_PPV_ARGS(&reflection));

  ComputeThreadGroupSize threadGroupSize;
  if (SUCCEEDED(hr)) {
    UINT sizeX, sizeY, sizeZ;
    reflection->GetThreadGroupSize(&sizeX, &sizeY, &sizeZ);
    threadGroupSize.x = sizeX;
    threadGroupSize.y = sizeY;
    threadGroupSize.z = sizeZ;

    IGL_LOG_INFO("Compute shader thread group size: [%u, %u, %u]",
                 sizeX, sizeY, sizeZ);

    // Validate thread group size against hardware limits
    if (threadGroupSize.totalThreads() > D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP) {
      return Result<...>(
          Result::Code::ArgumentOutOfRange,
          "Thread group size " + std::to_string(threadGroupSize.totalThreads()) +
          " exceeds maximum " + std::to_string(D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP));
    }

    if (sizeX > D3D12_CS_THREAD_GROUP_MAX_X ||
        sizeY > D3D12_CS_THREAD_GROUP_MAX_Y ||
        sizeZ > D3D12_CS_THREAD_GROUP_MAX_Z) {
      return Result<...>(
          Result::Code::ArgumentOutOfRange,
          "Thread group dimensions exceed hardware limits");
    }
  }

  // ... create pipeline and store threadGroupSize ...
}
```

### Step 2: Add Dispatch Validation

```cpp
// In ComputeCommandEncoder.cpp
void ComputeCommandEncoder::dispatchThreadgroups(
    uint32_t groupsX,
    uint32_t groupsY,
    uint32_t groupsZ) {

  // Validate dispatch parameters
  if (groupsX == 0 || groupsY == 0 || groupsZ == 0) {
    IGL_LOG_ERROR("Invalid compute dispatch: group count cannot be zero");
    IGL_ASSERT_MSG(false, "dispatchThreadgroups called with zero groups");
    return;
  }

  // Check against D3D12 limits
  if (groupsX > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION ||
      groupsY > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION ||
      groupsZ > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION) {

    IGL_LOG_ERROR("Compute dispatch exceeds D3D12 limits: "
                  "(%u, %u, %u), max per dimension: %u",
                  groupsX, groupsY, groupsZ,
                  D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION);
    IGL_ASSERT_MSG(false, "dispatchThreadgroups exceeds hardware limits");
    return;
  }

  // Optional: Warn about suspicious dispatch sizes
  if (currentPipeline_) {
    auto computePipeline = static_cast<ComputePipelineState*>(currentPipeline_.get());
    auto threadGroupSize = computePipeline->getThreadGroupSize();

    if (threadGroupSize.isValid()) {
      uint64_t totalThreads =
          static_cast<uint64_t>(groupsX) * threadGroupSize.x *
          static_cast<uint64_t>(groupsY) * threadGroupSize.y *
          static_cast<uint64_t>(groupsZ) * threadGroupSize.z;

      // Warn if dispatching billions of threads (likely a bug)
      if (totalThreads > 1000000000ULL) {  // 1 billion
        IGL_LOG_WARNING("Compute dispatch executes %llu threads - verify this is intended",
                       totalThreads);
      }
    }
  }

  // Proceed with dispatch
  commandList_->Dispatch(groupsX, groupsY, groupsZ);
}
```

### Step 3: Add Helper for Thread Count Calculation

```cpp
// In ComputeCommandEncoder.h
class ComputeCommandEncoder final : public IComputeCommandEncoder {
 public:
  // ... existing methods ...

  // Optional: Helper to dispatch based on thread count instead of groups
  void dispatchThreads(uint32_t threadsX,
                       uint32_t threadsY,
                       uint32_t threadsZ);

 private:
  std::shared_ptr<IComputePipelineState> currentPipeline_;
};

// In ComputeCommandEncoder.cpp
void ComputeCommandEncoder::dispatchThreads(
    uint32_t threadsX,
    uint32_t threadsY,
    uint32_t threadsZ) {

  if (!currentPipeline_) {
    IGL_LOG_ERROR("dispatchThreads called without bound compute pipeline");
    return;
  }

  auto computePipeline = static_cast<ComputePipelineState*>(currentPipeline_.get());
  auto threadGroupSize = computePipeline->getThreadGroupSize();

  if (!threadGroupSize.isValid()) {
    IGL_LOG_ERROR("Cannot dispatch threads: shader thread group size unknown");
    return;
  }

  // Calculate required thread groups (round up)
  uint32_t groupsX = (threadsX + threadGroupSize.x - 1) / threadGroupSize.x;
  uint32_t groupsY = (threadsY + threadGroupSize.y - 1) / threadGroupSize.y;
  uint32_t groupsZ = (threadsZ + threadGroupSize.z - 1) / threadGroupSize.z;

  // Dispatch with calculated groups
  dispatchThreadgroups(groupsX, groupsY, groupsZ);
}
```

### Step 4: Track Current Pipeline in Encoder

```cpp
// In ComputeCommandEncoder.cpp - ensure pipeline is tracked
void ComputeCommandEncoder::bindComputePipeline(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {

  // Store current pipeline for validation
  currentPipeline_ = pipelineState;

  // ... existing pipeline binding code ...
}
```

### Step 5: Add Device Capability Query

```cpp
// In Device.cpp - expose compute limits
struct ComputeLimits {
  uint32_t maxThreadGroupsPerDimension = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
  uint32_t maxThreadsPerGroup = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
  uint32_t maxThreadGroupSizeX = D3D12_CS_THREAD_GROUP_MAX_X;
  uint32_t maxThreadGroupSizeY = D3D12_CS_THREAD_GROUP_MAX_Y;
  uint32_t maxThreadGroupSizeZ = D3D12_CS_THREAD_GROUP_MAX_Z;
};

ComputeLimits Device::getComputeLimits() const {
  ComputeLimits limits;

  // D3D12 limits are fixed, but could query adapter for extended limits
  D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
  if (SUCCEEDED(device_->CheckFeatureSupport(
          D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)))) {
    // Some GPUs may have different limits
  }

  return limits;
}
```

---

## 7. Testing Requirements

### Unit Tests:

```bash
# Run compute shader validation tests
./build/Debug/IGLTests.exe --gtest_filter="*Compute*Validation*"

# Run all compute shader tests
./build/Debug/IGLTests.exe --gtest_filter="*Compute*"
```

### Test Cases:

```cpp
TEST(ComputeDispatch, RejectsZeroGroups) {
  auto encoder = createComputeEncoder();
  encoder->bindComputePipeline(testComputePipeline);

  // Should log error and not crash
  encoder->dispatchThreadgroups(0, 1, 1);
  encoder->dispatchThreadgroups(1, 0, 1);
  encoder->dispatchThreadgroups(1, 1, 0);
}

TEST(ComputeDispatch, RejectsExcessiveGroups) {
  auto encoder = createComputeEncoder();
  encoder->bindComputePipeline(testComputePipeline);

  // Exceeds D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION (65535)
  encoder->dispatchThreadgroups(100000, 1, 1);
}

TEST(ComputeDispatch, ValidatesThreadGroupSize) {
  // Shader with [numthreads(1025, 1, 1)] should fail pipeline creation
  auto result = device->createComputePipeline(invalidThreadGroupDesc);
  EXPECT_FALSE(result.isOk());
  EXPECT_NE(result.message().find("exceeds maximum"), std::string::npos);
}

TEST(ComputeDispatch, CalculatesThreadGroupsCorrectly) {
  auto encoder = createComputeEncoder();
  encoder->bindComputePipeline(testComputePipeline);  // [numthreads(16,16,1)]

  // Dispatch 100x100 threads -> should be 7x7 groups (rounded up)
  encoder->dispatchThreads(100, 100, 1);
  // Verify Dispatch(7, 7, 1) was called
}

TEST(ComputePipeline, ExtractsThreadGroupSize) {
  // Shader with [numthreads(8, 8, 1)]
  auto pipeline = device->createComputePipeline(desc);
  ASSERT_TRUE(pipeline.isOk());

  auto computePipeline = std::static_pointer_cast<ComputePipelineState>(pipeline.value());
  auto threadGroupSize = computePipeline->getThreadGroupSize();

  EXPECT_EQ(threadGroupSize.x, 8);
  EXPECT_EQ(threadGroupSize.y, 8);
  EXPECT_EQ(threadGroupSize.z, 1);
  EXPECT_EQ(threadGroupSize.totalThreads(), 64);
}
```

### Integration Tests:

```bash
# Test compute-heavy sessions
./build/Debug/Session/ComputeSession.exe  # If exists

# Test with various dispatch sizes
./build/Debug/IGLTests.exe --gtest_filter="*Compute*"
```

### Expected Results:

1. **Zero group dispatch:** Error logged, no GPU hang
2. **Excessive dispatch:** Error logged before D3D12 call
3. **Invalid thread group size:** Pipeline creation fails
4. **Valid dispatches:** Work as before with no regression

### Modification Restrictions:

- **DO NOT modify** cross-platform IGL compute API
- **DO NOT change** D3D12 dispatch behavior for valid inputs
- **DO NOT break** existing compute shaders
- **ONLY add** validation, no functional changes to valid code paths

---

## 8. Definition of Done

### Validation Checklist:

- [ ] Thread group size extracted from compute shader reflection
- [ ] Dispatch validates group counts against D3D12 limits
- [ ] Zero group counts are rejected with error
- [ ] Excessive group counts are rejected with error
- [ ] Warning for suspicious dispatch sizes (billions of threads)
- [ ] Helper method dispatchThreads() implemented and tested
- [ ] All unit tests pass
- [ ] Integration tests pass without GPU hangs
- [ ] Debug layer validation enabled during testing

### Performance Validation:

- [ ] Validation overhead negligible (only debug builds or always?)
- [ ] No regression in valid compute shader performance

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**
- [ ] Invalid dispatches are caught before GPU submission
- [ ] Clear error messages guide developers to fix issues
- [ ] No regressions in existing compute shader workloads
- [ ] GPU hangs/TDRs eliminated for invalid dispatch parameters

---

## 9. Related Issues

### Blocks:
- None - Standalone enhancement

### Related:
- **E-007** - Shader reflection incomplete (uses same reflection API)
- **C-010** - Compute shader support (general compute shader improvements)
- **H-004** - Feature detection (compute capabilities could be queried)

### Depends On:
- None - Can be implemented independently

---

## 10. Implementation Priority

**Priority:** P1-Medium

**Effort Estimate:** 1-2 days
- Day 1: Implement thread group extraction and dispatch validation
- Day 2: Testing, add dispatchThreads() helper, polish

**Risk Assessment:** Very Low
- Changes are primarily additive (validation only)
- No impact on valid dispatch calls
- Easy to test with intentionally invalid parameters
- Prevents serious GPU hangs/crashes

**Impact:** Medium-High
- **Prevents GPU hangs and TDRs** from invalid compute dispatches
- **Faster debugging** with immediate error messages
- **Safer API** that catches common mistakes before GPU submission
- **Better developer experience** with helper methods
- Minimal performance impact (validation is cheap)

---

## 11. References

### Official Documentation:

1. **ID3D12GraphicsCommandList::Dispatch:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-dispatch

2. **D3D12 Compute Shader Constants:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants

3. **Hardware Feature Levels:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-feature-levels

4. **ID3D12ShaderReflection::GetThreadGroupSize:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3d12shader/nf-d3d12shader-id3d12shaderreflection-getthreadgroupsize

5. **Compute Shader Overview:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader

### Additional Resources:

6. **Introduction to Compute Shaders:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-intro

7. **DirectX Graphics Samples - Compute Shader:**
   https://github.com/microsoft/DirectX-Graphics-Samples

---

**Last Updated:** 2025-11-12
**Assignee:** Unassigned
**Estimated Completion:** 1-2 days after start
