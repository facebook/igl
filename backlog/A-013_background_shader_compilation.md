# A-013: No Background Shader Compilation

**Priority**: LOW
**Category**: Architecture - Performance Optimization
**Estimated Effort**: 6-8 hours
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend compiles shaders synchronously on the rendering thread when they are first used. This causes frame hitches and stuttering when new shaders are encountered, especially problematic for:

1. **Runtime Shader Generation**: Applications that generate shaders at runtime
2. **Deferred Compilation**: Shaders only needed later in the application lifecycle
3. **Large Shader Collections**: Games with many unique shader variants
4. **First-Time-Use Hitches**: Noticeable frame drops when new shaders are compiled

**Current Behavior**:
- Shader compilation happens on the rendering thread
- Frame drops when new shaders are compiled
- No way to pre-compile shaders
- No compilation queuing or background compilation
- No progress indication for long compilations

**Symptoms**:
- 16-100ms frame hitches when new shaders are used
- Stuttering during shader-heavy scenes
- High-end CPUs sometimes bottlenecked by synchronous compilation
- No ability to schedule compilation off-peak
- Long loading screens due to synchronous shader compilation

**Impact**:
- Poor frame consistency and user experience
- Inability to ship applications with dynamic shaders
- Inefficient use of available CPU cores
- Long loading times
- Inconsistent frame delivery
- Difficulty predicting performance

---

## 2. Root Cause Analysis

### 2.1 Current Shader Compilation

**File**: `src/igl/d3d12/ShaderModule.cpp` and `src/igl/d3d12/DXCCompiler.cpp`

```cpp
// Shader compilation happens synchronously when module is created or first used
Result ShaderModule::compile(...) {
    // Blocking compilation on rendering thread
    HRESULT hr = compiler_->Compile(...);
    // Frame resumes only after compilation completes
}
```

### 2.2 Why This Is Wrong

**Problem 1**: No mechanism for asynchronous compilation. When a pipeline requests a compiled shader, the entire rendering pipeline blocks until compilation completes.

**Problem 2**: DXC compilation can take 10-100ms depending on shader complexity, which directly translates to frame time impact (16.67ms target for 60 FPS).

**Problem 3**: No priority system. Critical shaders aren't prioritized over less-critical ones.

**Problem 4**: No pre-compilation capability. Applications must wait for first use to compile shaders.

**Problem 5**: No compilation caching. Binary cache could be used to eliminate recompilation of identical shaders.

**Problem 6**: Single-threaded compilation. Modern CPUs have multiple cores, but only one is used for shader compilation.

**Problem 7**: No progress indication. Long compilations provide no feedback to the user.

**Problem 8**: No compilation queue. Burst shader compilation (many shaders at once) can cause extended hitches.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Asynchronous Shader Compilation**:
   - https://learn.microsoft.com/windows/win32/direct3d12/pipeline-state-objects
   - Pipeline state object creation patterns

2. **DXC Compiler Threading**:
   - https://github.com/microsoft/DirectXShaderCompiler
   - DXC threading and async patterns

3. **Shader Caching Best Practices**:
   - https://learn.microsoft.com/windows/win32/direct3d12/direct3d-12-best-practices
   - Performance optimization patterns

4. **Threading in D3D12**:
   - https://learn.microsoft.com/windows/win32/direct3d12/using-d3d12-multithreading
   - Multi-threaded D3D12 patterns

5. **Deferred Compilation**:
   - https://learn.microsoft.com/windows/win32/direct3d12/pipeline-state-objects
   - PSO caching and reuse

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for shader compilation**:
```
Pattern: "compile" OR "Compile" OR "DXCCompiler"
Files: src/igl/d3d12/ShaderModule.cpp, src/igl/d3d12/DXCCompiler.cpp
Focus: Where synchronous compilation occurs
```

**Search for shader creation**:
```
Pattern: "createShader" OR "createShaderStages" OR "ShaderModule"
Files: src/igl/d3d12/Device.cpp, src/igl/d3d12/ShaderModule.cpp
Focus: Shader creation workflow
```

**Search for pipeline state creation**:
```
Pattern: "RenderPipelineState" OR "ComputePipelineState" OR "createPipeline"
Files: src/igl/d3d12/RenderPipelineState.cpp, src/igl/d3d12/ComputePipelineState.cpp
Focus: Where shaders are used to create pipelines
```

**Search for shader caching**:
```
Pattern: "cache" OR "Cache" OR "binary"
Files: src/igl/d3d12/*.cpp
Focus: Existing caching mechanisms
```

### 4.2 File Locations

- `src/igl/d3d12/DXCCompiler.cpp` - Shader compilation implementation
- `src/igl/d3d12/DXCCompiler.h` - Compiler interface
- `src/igl/d3d12/ShaderModule.cpp` - Shader module management
- `src/igl/d3d12/RenderPipelineState.cpp` - Pipeline creation (uses shaders)
- `src/igl/d3d12/Device.cpp` - Device-level shader management

### 4.3 Key Code Patterns

Look for:
- `DXCCompiler::compile()` calls
- `ShaderModule` creation and destruction
- Pipeline state creation paths
- Synchronous compilation points

---

## 5. Detection Strategy

### 5.1 How to Reproduce

**Scenario 1: Observe frame hitches on new shader**
```
1. Run application with frame time logging
2. Introduce new shader (not previously compiled)
3. Observe frame time when shader is first used
4. Expected: Should be near target (60fps = 16.67ms)
5. Current: Likely see spike to 50-200ms depending on shader
```

**Scenario 2: Test with complex shader**
```
1. Create complex shader with heavy computation
2. Use shader for first time
3. Measure frame time impact
4. Expected: Frame time should not be significantly affected
5. Current: Large frame spike due to compilation
```

**Scenario 3: Monitor compilation thread**
```
1. Run profiler while rendering
2. Observe which thread does shader compilation
3. Expected: Could be background thread
4. Current: Likely rendering thread, blocks rendering
```

### 5.2 Verification After Fix

1. **Background Compilation**: Shaders compile without blocking rendering
2. **No Frame Hitches**: New shaders don't cause visible stuttering
3. **Compilation Progress**: Can query compilation status
4. **Shader Caching**: Compiled shaders are cached (optional but recommended)
5. **Multi-threaded**: Multiple shaders compile in parallel

---

## 6. Fix Guidance

### 6.1 Step-by-Step Implementation

#### Step 1: Create Shader Compilation Queue

**File**: `src/igl/d3d12/ShaderCompilationQueue.h` (NEW FILE)

**Current (PROBLEM)**:
```cpp
// No file exists - synchronous compilation only
```

**Fixed (SOLUTION)**:
```cpp
#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <igl/Result.h>
#include <igl/d3d12/DXCCompiler.h>

namespace igl::d3d12 {

/// Shader compilation task
struct ShaderCompilationTask {
    /// Unique identifier for this compilation
    size_t taskId = 0;

    /// Shader source code
    std::string sourceCode;

    /// Entry point (e.g., "main")
    std::string entryPoint;

    /// Compilation target (e.g., "ps_6_0")
    std::string target;

    /// Debug name for logging
    std::string debugName;

    /// Compilation flags (optimization level, etc.)
    uint32_t compileFlags = 0;

    /// Compiled bytecode output
    std::vector<uint8_t> compiledBytecode;

    /// Compilation errors (if any)
    std::string compileErrors;

    /// Compilation result
    Result compileResult;

    /// Priority (higher = compile sooner)
    int priority = 0;

    /// Callback when compilation completes (optional)
    std::function<void(const ShaderCompilationTask&)> onComplete;
};

/// Background shader compilation queue
/// Manages asynchronous shader compilation on worker threads
class ShaderCompilationQueue {
public:
    explicit ShaderCompilationQueue(size_t numWorkerThreads = 2);
    ~ShaderCompilationQueue();

    /// Submit a shader for compilation
    /// @param task Compilation task with source, target, etc.
    /// @return Task ID for tracking compilation status
    size_t submitCompilation(const ShaderCompilationTask& task);

    /// Check if compilation is complete
    /// @param taskId Task ID from submitCompilation
    /// @return true if compilation finished
    bool isCompilationComplete(size_t taskId) const;

    /// Wait for compilation to complete (blocking)
    /// @param taskId Task ID from submitCompilation
    /// @param timeoutMs Timeout in milliseconds (0 = infinite)
    /// @return true if compilation completed, false if timeout
    bool waitForCompletion(size_t taskId, uint64_t timeoutMs = 0) const;

    /// Get compilation result
    /// @param taskId Task ID from submitCompilation
    /// @param outTask Output task with results
    /// @return true if result is available
    bool getCompilationResult(size_t taskId, ShaderCompilationTask& outTask) const;

    /// Set number of worker threads
    /// @param numThreads Number of compilation threads to use
    void setWorkerThreadCount(size_t numThreads);

    /// Get queue length (waiting + in-progress)
    size_t getQueueLength() const;

    /// Pause compilation (pauses workers, preserves queue)
    void pauseCompilation();

    /// Resume compilation
    void resumeCompilation();

    /// Flush queue (wait for all pending compilations)
    void flushQueue();

    /// Shutdown compilation queue (waits for workers)
    void shutdown();

private:
    /// Worker thread function
    void workerThreadFunc();

    /// Get next task from queue (respects priority)
    bool getNextTask(ShaderCompilationTask& outTask);

    /// Compilation worker thread
    std::vector<std::thread> workerThreads_;

    /// Queue of pending compilations (priority queue based on priority field)
    std::priority_queue<ShaderCompilationTask,
                       std::vector<ShaderCompilationTask>,
                       std::function<bool(const ShaderCompilationTask&, const ShaderCompilationTask&)>>
        compilationQueue_;

    /// Completed compilations (taskId -> result)
    std::unordered_map<size_t, ShaderCompilationTask> completedCompilations_;

    /// DXC compiler instance (shared by workers)
    std::shared_ptr<DXCCompiler> compiler_;

    /// Synchronization
    mutable std::mutex queueMutex_;
    mutable std::condition_variable queueNotEmpty_;
    std::atomic<bool> shutdown_ = false;
    std::atomic<bool> paused_ = false;

    /// Task ID counter
    std::atomic<size_t> nextTaskId_ = 1;
};

}  // namespace igl::d3d12
```

**Rationale**:
- Decouples compilation from rendering thread
- Supports priority-based compilation ordering
- Provides progress tracking and completion notifications
- Allows background compilation with worker threads
- Thread-safe result retrieval

#### Step 2: Implement Compilation Queue

**File**: `src/igl/d3d12/ShaderCompilationQueue.cpp` (NEW FILE)

**Current (PROBLEM)**:
```cpp
// No file exists
```

**Fixed (SOLUTION)**:
```cpp
#include <igl/d3d12/ShaderCompilationQueue.h>
#include <igl/Log.h>

namespace igl::d3d12 {

ShaderCompilationQueue::ShaderCompilationQueue(size_t numWorkerThreads)
    : compilationQueue_([](const ShaderCompilationTask& a, const ShaderCompilationTask& b) {
        // Higher priority first, then FIFO
        return a.priority < b.priority;
      }) {

  compiler_ = std::make_shared<DXCCompiler>();
  if (!compiler_->initialize().isOk()) {
    IGL_LOG_ERROR("ShaderCompilationQueue: Failed to initialize DXC compiler\n");
  }

  // Start worker threads
  for (size_t i = 0; i < numWorkerThreads; ++i) {
    workerThreads_.emplace_back([this] { workerThreadFunc(); });
  }

  IGL_LOG_INFO("ShaderCompilationQueue: Initialized with %zu worker threads\n",
               numWorkerThreads);
}

ShaderCompilationQueue::~ShaderCompilationQueue() {
  shutdown();
}

size_t ShaderCompilationQueue::submitCompilation(const ShaderCompilationTask& task) {
  if (!compiler_ || !compiler_->isInitialized()) {
    IGL_LOG_ERROR("ShaderCompilationQueue: Compiler not initialized\n");
    return 0;
  }

  std::lock_guard<std::mutex> lock(queueMutex_);

  ShaderCompilationTask newTask = task;
  newTask.taskId = nextTaskId_.fetch_add(1);

  compilationQueue_.push(newTask);

  IGL_LOG_INFO("ShaderCompilationQueue: Submitted '%s' (task #%zu, priority %d)\n",
               task.debugName.c_str(), newTask.taskId, task.priority);

  queueNotEmpty_.notify_one();  // Wake up worker
  return newTask.taskId;
}

bool ShaderCompilationQueue::isCompilationComplete(size_t taskId) const {
  std::lock_guard<std::mutex> lock(queueMutex_);
  return completedCompilations_.find(taskId) != completedCompilations_.end();
}

bool ShaderCompilationQueue::waitForCompletion(size_t taskId, uint64_t timeoutMs) const {
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs : UINT64_MAX);

  while (true) {
    {
      std::lock_guard<std::mutex> lock(queueMutex_);
      if (completedCompilations_.find(taskId) != completedCompilations_.end()) {
        return true;
      }
    }

    if (timeoutMs > 0 && std::chrono::steady_clock::now() >= deadline) {
      return false;  // Timeout
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

bool ShaderCompilationQueue::getCompilationResult(size_t taskId,
                                                   ShaderCompilationTask& outTask) const {
  std::lock_guard<std::mutex> lock(queueMutex_);
  auto it = completedCompilations_.find(taskId);
  if (it == completedCompilations_.end()) {
    return false;
  }
  outTask = it->second;
  return true;
}

void ShaderCompilationQueue::setWorkerThreadCount(size_t numThreads) {
  // Simple implementation: just log (full implementation would resize thread pool)
  IGL_LOG_INFO("ShaderCompilationQueue: Requested worker thread count: %zu\n", numThreads);
}

size_t ShaderCompilationQueue::getQueueLength() const {
  std::lock_guard<std::mutex> lock(queueMutex_);
  return compilationQueue_.size();
}

void ShaderCompilationQueue::pauseCompilation() {
  paused_ = true;
  IGL_LOG_INFO("ShaderCompilationQueue: Compilation paused\n");
}

void ShaderCompilationQueue::resumeCompilation() {
  paused_ = false;
  queueNotEmpty_.notify_all();  // Wake all workers
  IGL_LOG_INFO("ShaderCompilationQueue: Compilation resumed\n");
}

void ShaderCompilationQueue::flushQueue() {
  while (getQueueLength() > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  IGL_LOG_INFO("ShaderCompilationQueue: Queue flushed\n");
}

void ShaderCompilationQueue::shutdown() {
  shutdown_ = true;
  queueNotEmpty_.notify_all();

  for (auto& thread : workerThreads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  IGL_LOG_INFO("ShaderCompilationQueue: Shutdown complete\n");
}

void ShaderCompilationQueue::workerThreadFunc() {
  while (!shutdown_) {
    // Wait for work or shutdown signal
    std::unique_lock<std::mutex> lock(queueMutex_);
    queueNotEmpty_.wait(lock, [this] { return !compilationQueue_.empty() || shutdown_; });

    if (shutdown_) break;

    if (paused_) continue;

    ShaderCompilationTask task;
    if (!getNextTask(task)) {
      continue;
    }

    lock.unlock();

    // Compile outside lock
    IGL_LOG_INFO("ShaderCompilationQueue: Compiling '%s' (task #%zu)\n",
                 task.debugName.c_str(), task.taskId);

    task.compileResult =
        compiler_->compile(task.sourceCode.c_str(),
                          task.sourceCode.size(),
                          task.entryPoint.c_str(),
                          task.target.c_str(),
                          task.debugName.c_str(),
                          task.compileFlags,
                          task.compiledBytecode,
                          task.compileErrors);

    if (task.compileResult.isOk()) {
      IGL_LOG_INFO("ShaderCompilationQueue: Successfully compiled '%s' (%zu bytes)\n",
                   task.debugName.c_str(), task.compiledBytecode.size());
    } else {
      IGL_LOG_ERROR("ShaderCompilationQueue: Failed to compile '%s': %s\n",
                    task.debugName.c_str(), task.compileErrors.c_str());
    }

    // Store result
    lock.lock();
    completedCompilations_[task.taskId] = task;

    if (task.onComplete) {
      lock.unlock();
      task.onComplete(task);
      lock.lock();
    }
  }
}

bool ShaderCompilationQueue::getNextTask(ShaderCompilationTask& outTask) {
  if (compilationQueue_.empty()) {
    return false;
  }
  outTask = compilationQueue_.top();
  compilationQueue_.pop();
  return true;
}

}  // namespace igl::d3d12
```

**Rationale**:
- Implements background worker threads for compilation
- Priority queue for shader ordering
- Thread-safe result tracking
- Pause/resume and shutdown support
- Progress tracking

#### Step 3: Integrate Queue with Device

**File**: `src/igl/d3d12/Device.h`

**Locate**: Device class member variables

**Current (PROBLEM)**:
```cpp
// No compilation queue
private:
    // ... other members ...
```

**Fixed (SOLUTION)**:
```cpp
// In Device.h
private:
    std::unique_ptr<ShaderCompilationQueue> shaderCompilationQueue_;

public:
    /// Get shader compilation queue for background compilation
    ShaderCompilationQueue* getShaderCompilationQueue() {
        return shaderCompilationQueue_.get();
    }
```

#### Step 4: Update ShaderModule to Use Queue

**File**: `src/igl/d3d12/ShaderModule.cpp`

**Locate**: ShaderModule creation and compilation

**Current (PROBLEM)**:
```cpp
// Synchronous compilation
Result ShaderModule::compile(...) {
    return compiler_->compile(...);  // Blocks here
}
```

**Fixed (SOLUTION)**:
```cpp
// Add compilation mode
enum class ShaderCompilationMode {
    Synchronous = 0,    // Compile immediately (current behavior)
    Background = 1,     // Compile in background queue
    Deferred = 2,       // Don't compile until explicitly requested
};

// In ShaderModule, allow specifying compilation mode
Result ShaderModule::compileAsync(
    ShaderCompilationMode mode,
    ShaderCompilationQueue* queue,
    ShaderCompilationCallback callback) {

    if (mode == ShaderCompilationMode::Synchronous) {
        // Current path: synchronous compilation
        return compiler_->compile(...);
    } else if (mode == ShaderCompilationMode::Background) {
        if (!queue) {
            return Result(Result::Code::ArgumentInvalid, "Queue required for background compilation");
        }

        // Submit to background queue
        ShaderCompilationTask task;
        task.sourceCode = sourceCode_;
        task.entryPoint = entryPoint_;
        task.target = compilationTarget_;
        task.debugName = debugName_;
        task.priority = 0;  // Default priority
        task.onComplete = [this, callback](const ShaderCompilationTask& task) {
            if (task.compileResult.isOk()) {
                compiledBytecode_ = task.compiledBytecode;
                IGL_LOG_INFO("ShaderModule: Background compilation completed for '%s'\n",
                           debugName_.c_str());
            }
            if (callback) {
                callback(task.compileResult);
            }
        };

        taskId_ = queue->submitCompilation(task);
        compilationMode_ = mode;

        return Result();  // Success - compilation in progress
    } else {
        // Deferred: don't compile yet
        compilationMode_ = mode;
        return Result();
    }
}

/// Query if background compilation is complete
bool ShaderModule::isCompilationComplete() const {
    if (compilationMode_ == ShaderCompilationMode::Synchronous) {
        return true;  // Already compiled
    }
    // Check with queue...
}
```

**Rationale**:
- Adds optional background compilation path
- Maintains backward compatibility with synchronous compilation
- Provides callbacks for completion notification

---

## 7. Testing Requirements

### 7.1 Unit Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Run shader compilation tests
./build/Debug/IGLTests.exe --gtest_filter="*Shader*"

# Run compilation queue tests (if implemented)
./build/Debug/IGLTests.exe --gtest_filter="*Compilation*"
```

**Test Modifications Allowed**:
- ✅ Add background compilation queue tests
- ✅ Add shader async compilation tests
- ✅ Test compilation priority and ordering
- ✅ Test completion callbacks
- ❌ **DO NOT modify core shader semantics**

### 7.2 Integration Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# All render sessions (should be faster with caching)
./test_all_sessions.bat

# Frame time consistency tests
./build/Debug/RenderSessions.exe --session=Textured3DCube --iterations=100
```

**Expected Results**:
- Sessions complete without hitches
- Frame time more consistent
- No shader compilation errors
- Memory usage reasonable

### 7.3 Manual Verification

1. **Background Compilation Works**:
   - Submit shader for background compilation
   - Verify compilation completes asynchronously
   - Check callback is invoked

2. **No Frame Hitches**:
   - Use background compilation mode
   - Render while shaders compile
   - Verify smooth frame rate (no jumps > 5ms)

3. **Synchronous Fallback**:
   - Disable background compilation
   - Verify synchronous compilation still works
   - Check legacy behavior unchanged

---

## 8. Definition of Done

### 8.1 Completion Criteria

- [ ] Shader compilation queue implemented
- [ ] Background worker threads functional
- [ ] Priority queue for shader ordering
- [ ] Completion tracking and callbacks
- [ ] ShaderModule integrated with queue
- [ ] Synchronous compilation still supported
- [ ] All unit tests pass
- [ ] All integration tests (render sessions) pass
- [ ] Frame time measurements show improvement
- [ ] Code review approved
- [ ] No shader compilation regressions

### 8.2 User Confirmation Required

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Background compilation queue is functional
2. Shaders compile without blocking rendering
3. Frame times are consistent
4. No errors during render sessions

**Post in chat**:
```
A-013 Fix Complete - Ready for Review
- Compilation queue: PASS (Background workers functional)
- Async compilation: PASS (Non-blocking shader compilation)
- Priority handling: PASS (Shaders compiled in correct order)
- Synchronous fallback: PASS (Legacy mode still works)
- Unit tests: PASS (All compilation tests)
- Render sessions: PASS (All sessions run smoothly)
- Frame time analysis: PASS (Consistent frame delivery)

Awaiting confirmation to proceed.
```

---

## 9. Related Issues

### 9.1 Blocks

- (None - this is a performance optimization)

### 9.2 Related

- **A-005** - Shader Model Detection (uses detected shader model in compilation)
- **A-012** - Video Memory Budget (background compilation should track memory)

---

## 10. Implementation Priority

**Priority**: P2 - Low (Performance Optimization)
**Estimated Effort**: 6-8 hours (background queue implementation is complex)
**Risk**: Medium (multi-threaded, needs careful synchronization testing)
**Impact**: Eliminates frame hitches from shader compilation, improves user experience, enables dynamic shader scenarios

---

## 11. References

- https://learn.microsoft.com/windows/win32/direct3d12/using-d3d12-multithreading
- https://learn.microsoft.com/windows/win32/direct3d12/direct3d-12-best-practices
- https://github.com/microsoft/DirectXShaderCompiler

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
