# Task E-014: Shader Compilation Threading/Parallelization

## 1. Problem Statement

### Current Behavior
The D3D12 shader compilation system compiles shaders sequentially, blocking the main thread while `D3DCompile` executes. For complex shaders or applications with many shaders, this causes noticeable delays during initialization, asset loading, and runtime shader compilation.

### The Danger
Without parallelized shader compilation:
- **Loading Stalls**: Shader compilation blocks main/render thread, causing visible stutters
- **Asset Pipeline Bottleneck**: Initial loading times increase significantly with many shaders
- **Runtime Compilation Delays**: Runtime shader generation (e.g., procedural shaders) causes frame drops
- **CPU Underutilization**: Single-threaded compilation ignores multi-core systems
- **Poor User Experience**: Loading screens appear frozen, responsiveness degraded

### Impact
- Application startup time increases by 100-500% for shader-heavy projects
- Runtime shader compilation becomes impractical
- User experience degrades noticeably on many-core systems
- Competitive disadvantage vs. multi-threaded engine implementations

---

## 2. Root Cause Analysis

### Current Implementation Limitations

**File Location**: Search for: `ShaderCompiler` class and `D3DCompile` usage

The current shader compilation approach:
```cpp
// Current implementation - blocking compilation
class ShaderCompiler {
    HRESULT compile(const std::string& source, const std::string& entryPoint) {
        // Blocks calling thread until completion
        return D3DCompile(
            source.data(),
            source.size(),
            nullptr, nullptr, nullptr,
            entryPoint.c_str(),
            "ps_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0,
            &codeBlob_,
            &errorBlob_
        );
    }
};

// Usage - synchronous, blocks
auto result = shaderCompiler.compile(source, entryPoint);  // Blocks!
```

### Why It's Not Parallelized

1. **No Thread Pool**: No background compilation infrastructure
2. **No Job System**: No job/task abstraction for shader compilation
3. **Synchronous API**: ShaderCompiler methods are blocking
4. **Device Thread Affinity**: D3D12 device may have thread assumptions
5. **No Caching**: Recompiling identical shaders repeatedly

### Relevant Code Locations
- Search for: `class ShaderCompiler` - Compilation entry point
- Search for: `D3DCompile` - Direct3D compiler invocation
- Search for: `createShaderModule` - Shader creation with blocking compile
- Search for: `UploadRingBuffer` or `Device` - No compilation queue
- Search for: `ThreadPool` or `Job` - May or may not exist

---

## 3. Official Documentation References

### Microsoft Direct3D 12 Documentation

1. **D3DCompile Function**
   - https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile
   - Focus: Compilation flags, optimization levels

2. **Asynchronous Compilation (D3D12)**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d12/pipeline-state-object-creation
   - Focus: PSO creation patterns for asynchronous operation

3. **Threading in Direct3D 12**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d12/threading
   - Focus: Thread safety and multi-threaded device usage

4. **Best Practices for Performance**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-best-practices
   - Focus: Multithreading and workload distribution

### Sample Code References
- **Microsoft Samples**: Direct3D 12 Multithreading
  - GitHub: https://github.com/microsoft/DirectX-Graphics-Samples
  - File: `Samples/Desktop/D3D12Multithreading/src/D3D12Multithreading.cpp`

---

## 4. Code Location Strategy

### Pattern-Based File Search

**Search for these patterns to understand current structure:**

1. **Shader Compiler Implementation**
   - Search for: `class ShaderCompiler` or `ShaderCompile`
   - File: `src/igl/d3d12/ShaderCompiler.h` and `.cpp`
   - What to look for: D3DCompile invocation, no async support

2. **Shader Module Creation**
   - Search for: `createShaderModule(` or `ShaderModuleDesc`
   - File: `src/igl/d3d12/Device.cpp` and `src/igl/d3d12/ShaderModule.cpp`
   - What to look for: Direct compile call, no job queuing

3. **Thread Management**
   - Search for: `std::thread`, `std::future`, `ThreadPool`, or `Job`
   - File: `src/igl/common/` or similar
   - What to look for: Any existing threading infrastructure

4. **Device Initialization**
   - Search for: `class Device` constructor
   - File: `src/igl/d3d12/Device.h` and `.cpp`
   - What to look for: Single-threaded init pattern

5. **Pipeline State Creation**
   - Search for: `createRenderPipeline` or `D3D12_GRAPHICS_PIPELINE_STATE_DESC`
   - File: `src/igl/d3d12/RenderPipeline.cpp` and `Device.cpp`
   - What to look for: Synchronous PSO creation from compiled shaders

---

## 5. Detection Strategy

### How to Identify the Problem

**Reproduction Steps:**
1. Create a shader-heavy application:
   ```cpp
   // Pseudo-code for reproduction
   struct ShaderCompilationBenchmark {
       void run() {
           auto start = std::chrono::high_resolution_clock::now();

           for (int i = 0; i < 100; ++i) {
               std::string shaderSource = generateComplexShader(i);
               shaderCompiler.compile(shaderSource, "main");  // Blocks!
           }

           auto end = std::chrono::high_resolution_clock::now();
           auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
           printf("100 shaders compiled in %ld ms\n", duration.count());
           // Expected: 1000-5000ms (10-50ms per shader)
           // This is too slow for responsive loading
       }
   };
   ```

2. Profile during loading:
   - Run: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*ShaderCompile*"`
   - Measure: CPU usage (should spike to 100% on single core)
   - Look for: Main thread blocking

3. Monitor frame time with shader compilation:
   - Run: `./test_all_sessions.bat` with ETW tracing
   - Check for: Frame stutters during shader compilation

4. Verify with profiler:
   - Use Windows Task Manager or Intel VTune
   - Observe: Single core at 100%, others idle

### Success Criteria
- 100 shader compilations complete in < 500ms (5ms per shader average)
- Multi-core CPU utilization > 60% during compilation
- No main thread blocking during async compilation
- Frame rate maintained during background shader compilation

---

## 6. Fix Guidance

### Implementation Steps

#### Step 1: Create Shader Compilation Job System
```cpp
// In src/igl/d3d12/ShaderCompilationJob.h (NEW FILE)
class ShaderCompilationJob {
public:
    struct Input {
        std::string source;
        std::string entryPoint;
        std::string profile;
    };

    struct Output {
        ID3DBlob* bytecode;
        ID3DBlob* errorBlob;
        HRESULT result;
    };

    ShaderCompilationJob(const Input& input);
    ~ShaderCompilationJob();

    void execute();  // Runs on worker thread
    bool isComplete() const { return isComplete_; }
    const Output& getOutput() const { return output_; }

private:
    Input input_;
    Output output_;
    std::atomic<bool> isComplete_;
};
```

#### Step 2: Implement Compilation Job Executor
```cpp
// In src/igl/d3d12/ShaderCompilationJob.cpp
void ShaderCompilationJob::execute() {
    output_.result = D3DCompile(
        input_.source.data(),
        input_.source.size(),
        nullptr, nullptr, nullptr,
        input_.entryPoint.c_str(),
        input_.profile.c_str(),
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &output_.bytecode,
        &output_.errorBlob
    );
    isComplete_.store(true, std::memory_order_release);
}
```

#### Step 3: Create Thread Pool for Shader Compilation
```cpp
// In src/igl/d3d12/ShaderCompilationThreadPool.h (NEW FILE)
class ShaderCompilationThreadPool {
public:
    static constexpr size_t DEFAULT_THREAD_COUNT = 4;  // Configurable

    ShaderCompilationThreadPool(size_t numThreads = DEFAULT_THREAD_COUNT);
    ~ShaderCompilationThreadPool();

    // Queue a compilation job
    std::shared_ptr<ShaderCompilationJob> queueCompilation(
        const ShaderCompilationJob::Input& input);

    // Wait for all pending compilations to complete
    void waitForCompletion();

    // Get number of pending jobs
    size_t getPendingJobCount() const;

private:
    std::vector<std::thread> workerThreads_;
    std::queue<std::shared_ptr<ShaderCompilationJob>> jobQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::atomic<bool> shouldShutdown_;

    void workerThreadMain();
};
```

#### Step 4: Implement Thread Pool Logic
```cpp
// In src/igl/d3d12/ShaderCompilationThreadPool.cpp
ShaderCompilationThreadPool::ShaderCompilationThreadPool(size_t numThreads)
    : shouldShutdown_(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workerThreads_.emplace_back(&ShaderCompilationThreadPool::workerThreadMain, this);
    }
}

void ShaderCompilationThreadPool::workerThreadMain() {
    while (true) {
        std::shared_ptr<ShaderCompilationJob> job;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait(lock, [this] {
                return !jobQueue_.empty() || shouldShutdown_;
            });

            if (shouldShutdown_ && jobQueue_.empty()) {
                break;
            }

            if (!jobQueue_.empty()) {
                job = jobQueue_.front();
                jobQueue_.pop();
            }
        }

        if (job) {
            job->execute();  // Compile on worker thread
        }
    }
}

std::shared_ptr<ShaderCompilationJob> ShaderCompilationThreadPool::queueCompilation(
    const ShaderCompilationJob::Input& input) {
    auto job = std::make_shared<ShaderCompilationJob>(input);
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        jobQueue_.push(job);
    }
    queueCV_.notify_one();
    return job;
}

void ShaderCompilationThreadPool::waitForCompletion() {
    while (getPendingJobCount() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
```

#### Step 5: Integrate Thread Pool with Device
```cpp
// In src/igl/d3d12/Device.h - Add thread pool member
class Device : public IDevice {
private:
    std::unique_ptr<ShaderCompilationThreadPool> shaderCompilationThreadPool_;
};

// In Device constructor
Device::Device(const DeviceDesc& desc) {
    // ... existing init ...
    size_t numThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    shaderCompilationThreadPool_ = std::make_unique<ShaderCompilationThreadPool>(numThreads);
}
```

#### Step 6: Update Shader Compilation API
```cpp
// In src/igl/d3d12/ShaderCompiler.h - Add async method
class ShaderCompiler {
public:
    // Synchronous (blocking) - for critical shaders
    HRESULT compile(const std::string& source, const std::string& entryPoint);

    // Asynchronous (non-blocking) - NEW
    std::shared_ptr<ShaderCompilationJob> compileAsync(
        const std::string& source,
        const std::string& entryPoint);

    // Wait for async compilation to complete
    HRESULT waitForAsyncCompilation(const std::shared_ptr<ShaderCompilationJob>& job);
};
```

#### Step 7: Update ShaderModule to Support Deferred Compilation
```cpp
// In src/igl/d3d12/ShaderModule.h - Add deferred compilation support
class ShaderModule : public IShaderModule {
private:
    std::shared_ptr<ShaderCompilationJob> compilationJob_;
    bool isCompiled_;

    void ensureCompiled() const {
        if (!isCompiled_) {
            if (compilationJob_) {
                compilationJob_->execute();  // Should already be done
                // Use compilation result
            }
            isCompiled_ = true;
        }
    }
};
```

---

## 7. Testing Requirements

### Unit Tests to Create/Modify

**File**: `tests/d3d12/ShaderCompilationThreadingTests.cpp` (NEW)

1. **Thread Pool Creation Test**
   ```cpp
   TEST(D3D12ShaderThreading, CreateThreadPool) {
       ShaderCompilationThreadPool pool(4);
       ASSERT_EQ(pool.getPendingJobCount(), 0);
   }
   ```

2. **Async Compilation Test**
   ```cpp
   TEST(D3D12ShaderThreading, CompileAsync) {
       ShaderCompilationThreadPool pool(4);

       ShaderCompilationJob::Input input{
           "float4 main() : SV_TARGET { return float4(1,0,0,1); }",
           "main",
           "ps_5_0"
       };

       auto job = pool.queueCompilation(input);
       ASSERT_NE(job, nullptr);

       pool.waitForCompletion();
       ASSERT_TRUE(job->isComplete());
       ASSERT_EQ(job->getOutput().result, S_OK);
   }
   ```

3. **Parallel Compilation Benchmark**
   ```cpp
   TEST(D3D12ShaderThreading, BENCHMARK_ParallelCompilation) {
       ShaderCompilationThreadPool pool(4);
       auto start = std::chrono::high_resolution_clock::now();

       for (int i = 0; i < 100; ++i) {
           ShaderCompilationJob::Input input{
               generateComplexShader(i),
               "main",
               "ps_5_0"
           };
           pool.queueCompilation(input);
       }

       pool.waitForCompletion();
       auto end = std::chrono::high_resolution_clock::now();
       auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

       // With 4 threads, expect ~4x speedup
       // Single-threaded: ~1000-5000ms
       // Multi-threaded: ~250-1250ms
       ASSERT_LT(duration.count(), 1500);  // Assert < 1.5 seconds
   }
   ```

4. **Job Status Tracking Test**
   ```cpp
   TEST(D3D12ShaderThreading, TrackJobStatus) {
       ShaderCompilationThreadPool pool(2);

       auto job = pool.queueCompilation({shaderSource, "main", "ps_5_0"});
       ASSERT_FALSE(job->isComplete());

       pool.waitForCompletion();
       ASSERT_TRUE(job->isComplete());
   }
   ```

### Integration Tests

**Command**: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Threading*"`

- Run all shader threading tests
- Verify: 100% pass rate, no race conditions

**Command**: `./test_all_sessions.bat`

- Ensure no regressions with async compilation
- Verify: Frame rate maintained, no crashes

### Restrictions
- ❌ DO NOT modify cross-platform API without discussion
- ❌ DO NOT force async compilation for all shaders (must be optional)
- ❌ DO NOT change shader compilation results or bytecode
- ✅ DO focus changes on D3D12-specific threading in `src/igl/d3d12/`

---

## 8. Definition of Done

### Completion Checklist

- [ ] ShaderCompilationJob class implemented with execute() method
- [ ] ShaderCompilationThreadPool implemented with configurable thread count
- [ ] Worker threads properly manage job queue with synchronization
- [ ] ShaderCompiler::compileAsync() added and integrated
- [ ] Device creates and manages shader compilation thread pool
- [ ] ShaderModule supports deferred/async compilation
- [ ] All new unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Threading*"`
- [ ] Performance benchmark shows < 1.5 seconds for 100 shader compilations
- [ ] No race conditions or thread safety issues
- [ ] Integration tests pass: `./test_all_sessions.bat`
- [ ] Code reviewed and approved by maintainer

### User Confirmation Required
⚠️ **STOP** - Do NOT proceed to next task until user confirms:
- "✅ I have verified async shader compilation works correctly"
- "✅ Performance benchmark shows significant speedup (> 2x)"
- "✅ No thread safety issues or race conditions observed"
- "✅ No regressions in existing shader functionality"

---

## 9. Related Issues

### Blocks
- **H-015**: Dynamic shader compilation system
- **C-010**: Asset pipeline optimization

### Related
- **E-008**: Shader include paths (works with async compilation)
- **E-007**: Shader validation
- **G-008**: Upload ring buffer optimization

---

## 10. Implementation Priority

### Severity: **Low** | Priority: **P2-Low**

**Effort**: 50-70 hours
- Design: 10 hours (thread pool architecture, synchronization)
- Implementation: 24-32 hours (thread pool + job system)
- Testing: 10-16 hours (unit + threading race condition tests)
- Documentation: 4-6 hours

**Risk**: **Medium-High**
- Risk of race conditions and deadlocks if synchronization incorrect
- Risk of D3D12 device thread safety if not carefully managed
- Risk of GPU memory pressure if many compilations queued
- Mitigation: Comprehensive thread safety testing, lock-free patterns

**Impact**: **Medium**
- 4-8x speedup for parallel shader compilation (4 cores)
- Significant improvement in loading times for shader-heavy projects
- Enables practical runtime shader generation

**Blockers**: None

**Dependencies**:
- Requires C++11 threading support (standard library)
- Requires existing ShaderCompiler infrastructure

---

## 11. References

### Official Microsoft Documentation
1. **D3DCompile Function**: https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile
2. **Threading in Direct3D 12**: https://learn.microsoft.com/en-us/windows/win32/direct3d12/threading
3. **Performance Best Practices**: https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-best-practices
4. **Pipeline State Object Caching**: https://learn.microsoft.com/en-us/windows/win32/direct3d12/pipeline-state-object-creation

### GitHub References
- **Microsoft D3D12 Samples**: https://github.com/microsoft/DirectX-Graphics-Samples
- **IGL Repository**: https://github.com/facebook/igl

### Related Issues
- Task H-015: Dynamic shader compilation
- Task E-008: Shader include paths
