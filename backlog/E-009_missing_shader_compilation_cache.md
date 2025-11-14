# E-009: No Persistent Shader Compilation Cache

**Priority:** P1-Medium
**Category:** Enhancement - Performance
**Status:** Open
**Effort:** Medium (3-4 days)

---

## 1. Problem Statement

The D3D12 backend compiles shaders on every application launch, with no persistent cache to disk. This results in significant startup delays, especially for applications with many shaders or complex shader variants.

### The Danger

**Unacceptable startup times** in production applications:
- A game with 100 shaders taking 5-10 seconds to compile on every launch
- Mobile/embedded devices with slower CPUs suffering even worse performance
- Users experiencing "frozen" application during initial shader compilation
- No recovery from driver shader cache invalidation (e.g., after driver updates)
- Development iteration time wasted recompiling identical shaders

**Real-world impact:** Applications appear unresponsive during startup. Users may force-close thinking the app has crashed, leading to poor user experience and negative reviews. Without caching, every cold start pays the full compilation cost.

---

## 2. Root Cause Analysis

The D3D12 backend compiles HLSL to DXBC/DXIL on every run without persistence:

**Current Implementation:**
```cpp
// In ShaderModule.cpp or Device.cpp - no caching mechanism
Result<ComPtr<ID3DBlob>> Device::compileShader(
    const std::string& source,
    const std::string& entryPoint,
    const std::string& target) {

  ComPtr<ID3DBlob> shaderBlob;
  ComPtr<ID3DBlob> errorBlob;

  // Direct compilation every time - NO CACHE CHECK
  HRESULT hr = D3DCompile(
      source.c_str(),
      source.size(),
      nullptr,  // No source name
      nullptr,  // No defines
      nullptr,  // No includes
      entryPoint.c_str(),
      target.c_str(),
      compileFlags,
      0,
      &shaderBlob,
      &errorBlob);

  // Returns compiled bytecode, discarded after pipeline creation
  return shaderBlob;
}
```

**Missing Components:**

1. **No cache key generation:**
   - Source hash not computed
   - Compile flags not included in key
   - Shader model/target not part of key
   - No versioning for cache format

2. **No disk persistence:**
   - Compiled bytecode not saved
   - No cache directory management
   - No cache invalidation strategy

3. **No cache lookup:**
   - Every shader compiled from source
   - No fast path for previously compiled shaders

**Performance Impact:**
```
Without cache: D3DCompile() = 50-200ms per shader
With cache:    Disk read + validation = 1-5ms per shader
Savings:       95-98% reduction in shader startup time
```

---

## 3. Official Documentation References

- **D3DCompile Function:**
  https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile

- **Pipeline State Objects and Caching:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12

- **D3D12 Shader Compilation Best Practices:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/shader-model-6-0

- **PSO Library (Related Pattern):**
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device1-createpipelinelibraryfrommemory

---

## 4. Code Location Strategy

**Primary Files to Modify:**

1. **Shader compilation code:**
   ```
   Search for: "D3DCompile(" or "D3DCompile2("
   In files: src/igl/d3d12/ShaderModule.cpp, src/igl/d3d12/Device.cpp
   Pattern: Look for shader compilation entry points
   ```

2. **Shader module creation:**
   ```
   Search for: "createShaderModule" or "ShaderModule::create"
   In files: src/igl/d3d12/Device.cpp, src/igl/d3d12/ShaderModule.*
   Pattern: Look for IShaderModule implementation
   ```

3. **Device initialization:**
   ```
   Search for: "Device::create" or "Device::initialize"
   In files: src/igl/d3d12/Device.cpp
   Pattern: Look for device creation where cache directory would be set
   ```

**New Files to Create:**

4. **Shader cache manager:**
   ```
   Create: src/igl/d3d12/ShaderCache.h
   Create: src/igl/d3d12/ShaderCache.cpp
   Purpose: Manage persistent shader cache
   ```

---

## 5. Detection Strategy

### Reproduction Steps:

1. **Measure current compile time:**
   ```cpp
   // Add timing to existing code
   auto start = std::chrono::high_resolution_clock::now();

   auto shaderModule = device->createShaderModule(shaderSource, ...);

   auto end = std::chrono::high_resolution_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
   IGL_LOG_INFO("Shader compilation took %lld ms", duration.count());
   ```

2. **Run application twice:**
   ```bash
   # First run - compiles from source
   ./build/Debug/Session/TinyMeshSession.exe
   # Note startup time

   # Second run - should use cache (but currently recompiles)
   ./build/Debug/Session/TinyMeshSession.exe
   # Note startup time is identical (bad)
   ```

3. **Expected improvement with cache:**
   ```
   First run:  Shader compilation: 2500ms (50 shaders * 50ms avg)
   Second run: Shader compilation: 100ms  (50 shaders * 2ms cache read)
   Speedup:    25x faster startup
   ```

### Performance Benchmark:

```cpp
// Test case to measure cache effectiveness
TEST(ShaderCache, PerformanceBenchmark) {
  // Compile 100 identical shaders without cache
  auto startNoCache = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 100; ++i) {
    device->compileShader(testShaderSource, "main", "ps_5_0");
  }
  auto noCache Duration = /* calculate */;

  // Compile 100 identical shaders with cache (first populates, rest use cache)
  auto startWithCache = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 100; ++i) {
    device->compileShaderCached(testShaderSource, "main", "ps_5_0");
  }
  auto withCacheDuration = /* calculate */;

  EXPECT_LT(withCacheDuration.count(), noCacheDuration.count() * 0.2);  // At least 5x faster
}
```

---

## 6. Fix Guidance

### Step 1: Create Shader Cache Key Generator

```cpp
// In ShaderCache.h
#include <string>
#include <vector>
#include <array>

class ShaderCacheKey {
 public:
  ShaderCacheKey(const std::string& source,
                 const std::string& entryPoint,
                 const std::string& target,
                 uint32_t compileFlags);

  std::string toString() const;  // For filename
  bool operator==(const ShaderCacheKey& other) const;

 private:
  std::array<uint8_t, 32> hash_;  // SHA-256 hash

  static std::array<uint8_t, 32> computeHash(
      const std::string& source,
      const std::string& entryPoint,
      const std::string& target,
      uint32_t compileFlags);
};

// In ShaderCache.cpp
#include <igl/Common.h>
// Use a simple hash like FNV-1a or XXHash (avoid crypto overhead)

ShaderCacheKey::ShaderCacheKey(const std::string& source,
                               const std::string& entryPoint,
                               const std::string& target,
                               uint32_t compileFlags) {
  hash_ = computeHash(source, entryPoint, target, compileFlags);
}

std::string ShaderCacheKey::toString() const {
  // Convert hash to hex string for filename
  std::string result;
  result.reserve(64);
  for (uint8_t byte : hash_) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", byte);
    result += hex;
  }
  return result;
}

std::array<uint8_t, 32> ShaderCacheKey::computeHash(
    const std::string& source,
    const std::string& entryPoint,
    const std::string& target,
    uint32_t compileFlags) {

  // Simple FNV-1a hash (fast, non-cryptographic)
  uint64_t hash = 0xcbf29ce484222325ULL;
  const uint64_t prime = 0x100000001b3ULL;

  auto hashData = [&](const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
      hash ^= bytes[i];
      hash *= prime;
    }
  };

  hashData(source.data(), source.size());
  hashData(entryPoint.data(), entryPoint.size());
  hashData(target.data(), target.size());
  hashData(&compileFlags, sizeof(compileFlags));

  // Convert 64-bit hash to 32-byte array (repeat pattern for simplicity)
  std::array<uint8_t, 32> result;
  for (int i = 0; i < 4; ++i) {
    memcpy(result.data() + i * 8, &hash, 8);
    hash ^= (hash << 7);  // Mix for next iteration
  }

  return result;
}
```

### Step 2: Implement Shader Cache Manager

```cpp
// In ShaderCache.h
class ShaderCache {
 public:
  explicit ShaderCache(const std::string& cacheDirectory);
  ~ShaderCache();

  // Try to load cached shader bytecode
  Result<ComPtr<ID3DBlob>> tryLoad(const ShaderCacheKey& key);

  // Save compiled shader bytecode to cache
  Result<void> save(const ShaderCacheKey& key, ID3DBlob* bytecode);

  // Clear entire cache
  void clear();

  // Get cache statistics
  struct Stats {
    size_t hits = 0;
    size_t misses = 0;
    size_t totalSize = 0;
  };
  Stats getStats() const;

 private:
  std::string cacheDirectory_;
  mutable Stats stats_;

  std::string getCachePath(const ShaderCacheKey& key) const;
  bool validateCachedShader(const std::vector<uint8_t>& data) const;
};

// In ShaderCache.cpp
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

ShaderCache::ShaderCache(const std::string& cacheDirectory)
    : cacheDirectory_(cacheDirectory) {
  // Create cache directory if it doesn't exist
  if (!fs::exists(cacheDirectory_)) {
    fs::create_directories(cacheDirectory_);
    IGL_LOG_INFO("Created shader cache directory: %s", cacheDirectory_.c_str());
  }
}

Result<ComPtr<ID3DBlob>> ShaderCache::tryLoad(const ShaderCacheKey& key) {
  std::string cachePath = getCachePath(key);

  // Check if cache file exists
  if (!fs::exists(cachePath)) {
    stats_.misses++;
    return Result<ComPtr<ID3DBlob>>(Result::Code::RuntimeError, "Cache miss");
  }

  // Read cached bytecode
  std::ifstream file(cachePath, std::ios::binary | std::ios::ate);
  if (!file) {
    stats_.misses++;
    return Result<ComPtr<ID3DBlob>>(Result::Code::RuntimeError, "Failed to open cache file");
  }

  size_t fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(fileSize);
  file.read(reinterpret_cast<char*>(data.data()), fileSize);

  if (!file) {
    stats_.misses++;
    return Result<ComPtr<ID3DBlob>>(Result::Code::RuntimeError, "Failed to read cache file");
  }

  // Validate cached data (basic sanity check)
  if (!validateCachedShader(data)) {
    stats_.misses++;
    fs::remove(cachePath);  // Delete corrupted cache
    return Result<ComPtr<ID3DBlob>>(Result::Code::RuntimeError, "Corrupted cache file");
  }

  // Create ID3DBlob from cached data
  ComPtr<ID3DBlob> blob;
  HRESULT hr = D3DCreateBlob(data.size(), &blob);
  if (FAILED(hr)) {
    stats_.misses++;
    return Result<ComPtr<ID3DBlob>>(Result::Code::RuntimeError, "Failed to create blob");
  }

  memcpy(blob->GetBufferPointer(), data.data(), data.size());

  stats_.hits++;
  IGL_LOG_INFO("Shader cache hit: %s", key.toString().c_str());
  return blob;
}

Result<void> ShaderCache::save(const ShaderCacheKey& key, ID3DBlob* bytecode) {
  if (!bytecode) {
    return Result<void>(Result::Code::ArgumentInvalid, "Null bytecode");
  }

  std::string cachePath = getCachePath(key);

  // Write bytecode to cache file
  std::ofstream file(cachePath, std::ios::binary | std::ios::trunc);
  if (!file) {
    return Result<void>(Result::Code::RuntimeError, "Failed to create cache file");
  }

  file.write(static_cast<const char*>(bytecode->GetBufferPointer()),
             bytecode->GetBufferSize());

  if (!file) {
    return Result<void>(Result::Code::RuntimeError, "Failed to write cache file");
  }

  stats_.totalSize += bytecode->GetBufferSize();
  IGL_LOG_INFO("Shader cached: %s (%zu bytes)", key.toString().c_str(),
               bytecode->GetBufferSize());

  return Result<void>();
}

void ShaderCache::clear() {
  if (!fs::exists(cacheDirectory_)) {
    return;
  }

  size_t removedCount = 0;
  for (const auto& entry : fs::directory_iterator(cacheDirectory_)) {
    if (entry.is_regular_file()) {
      fs::remove(entry.path());
      removedCount++;
    }
  }

  stats_ = Stats{};  // Reset statistics
  IGL_LOG_INFO("Cleared shader cache: %zu files removed", removedCount);
}

std::string ShaderCache::getCachePath(const ShaderCacheKey& key) const {
  return cacheDirectory_ + "/" + key.toString() + ".cache";
}

bool ShaderCache::validateCachedShader(const std::vector<uint8_t>& data) const {
  // Basic validation: check DXBC header magic number
  if (data.size() < 4) {
    return false;
  }

  // DXBC shaders start with "DXBC" magic
  const char* magic = reinterpret_cast<const char*>(data.data());
  return (magic[0] == 'D' && magic[1] == 'X' && magic[2] == 'B' && magic[3] == 'C');
}

ShaderCache::Stats ShaderCache::getStats() const {
  return stats_;
}
```

### Step 3: Integrate Cache into Device

```cpp
// In Device.h
class Device {
 public:
  // ... existing methods ...

  void setShaderCacheDirectory(const std::string& directory);
  void clearShaderCache();
  ShaderCache::Stats getShaderCacheStats() const;

 private:
  std::unique_ptr<ShaderCache> shaderCache_;

  // Enhanced compilation with caching
  Result<ComPtr<ID3DBlob>> compileShaderWithCache(
      const std::string& source,
      const std::string& entryPoint,
      const std::string& target,
      uint32_t compileFlags);
};

// In Device.cpp
void Device::setShaderCacheDirectory(const std::string& directory) {
  shaderCache_ = std::make_unique<ShaderCache>(directory);
}

Result<ComPtr<ID3DBlob>> Device::compileShaderWithCache(
    const std::string& source,
    const std::string& entryPoint,
    const std::string& target,
    uint32_t compileFlags) {

  // Generate cache key
  ShaderCacheKey cacheKey(source, entryPoint, target, compileFlags);

  // Try to load from cache first
  if (shaderCache_) {
    auto cacheResult = shaderCache_->tryLoad(cacheKey);
    if (cacheResult.isOk()) {
      return cacheResult;  // Cache hit!
    }
  }

  // Cache miss - compile from source
  ComPtr<ID3DBlob> shaderBlob;
  ComPtr<ID3DBlob> errorBlob;

  HRESULT hr = D3DCompile(
      source.c_str(),
      source.size(),
      nullptr,
      nullptr,
      nullptr,
      entryPoint.c_str(),
      target.c_str(),
      compileFlags,
      0,
      &shaderBlob,
      &errorBlob);

  if (FAILED(hr)) {
    std::string errorMsg = "Shader compilation failed";
    if (errorBlob) {
      errorMsg += ": " + std::string(
          static_cast<const char*>(errorBlob->GetBufferPointer()),
          errorBlob->GetBufferSize());
    }
    return Result<ComPtr<ID3DBlob>>(Result::Code::RuntimeError, errorMsg);
  }

  // Save to cache for next time
  if (shaderCache_) {
    shaderCache_->save(cacheKey, shaderBlob.Get());
  }

  return shaderBlob;
}
```

### Step 4: Add Cache Configuration

```cpp
// In Device.cpp - initialization
Device::Device(...) {
  // Set up shader cache in platform-specific location
  #ifdef _WIN32
    char* appData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appData, &len, "LOCALAPPDATA") == 0 && appData != nullptr) {
      std::string cacheDir = std::string(appData) + "/IGL/ShaderCache";
      setShaderCacheDirectory(cacheDir);
      free(appData);
    }
  #else
    // Platform-specific cache locations for other platforms
  #endif
}
```

### Step 5: Add Cache Management API

```cpp
// Allow applications to control caching behavior
struct DeviceConfiguration {
  bool enableShaderCache = true;
  std::string shaderCacheDirectory;  // Empty = use default
  size_t maxCacheSize = 100 * 1024 * 1024;  // 100MB default
};
```

---

## 7. Testing Requirements

### Unit Tests:

```bash
# Run shader cache specific tests
./build/Debug/IGLTests.exe --gtest_filter="*ShaderCache*"

# Run all D3D12 shader compilation tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Shader*"
```

### Performance Tests:

```cpp
// Add to test suite
TEST(ShaderCache, ColdStartPerformance) {
  // Clear cache, compile 50 shaders, measure time
}

TEST(ShaderCache, WarmStartPerformance) {
  // Use populated cache, load 50 shaders, should be 10x faster
}

TEST(ShaderCache, CacheInvalidation) {
  // Modify shader source, ensure recompilation
}
```

### Integration Tests:

```bash
# Test with render sessions
# First run - populates cache
del /Q %LOCALAPPDATA%\IGL\ShaderCache\*
./build/Debug/Session/TinyMeshSession.exe
# Measure startup time

# Second run - uses cache
./build/Debug/Session/TinyMeshSession.exe
# Startup should be significantly faster

# Test all sessions
./test_all_sessions.bat
```

### Expected Results:

1. **First run (cold cache):** Normal compilation time
2. **Second run (warm cache):** 10-25x faster shader loading
3. **Cache persistence:** Cache survives application restarts
4. **Cache invalidation:** Modified shaders recompile correctly

### Modification Restrictions:

- **DO NOT modify** cross-platform shader API in `include/igl/Shader.h`
- **DO NOT change** compilation behavior (output must be identical)
- **DO NOT break** applications that don't use caching (default = enabled but graceful fallback)
- **ONLY add** caching layer, keep existing compilation path intact

---

## 8. Definition of Done

### Validation Checklist:

- [ ] Shader cache key generation implemented with proper hashing
- [ ] Disk-based cache save/load implemented
- [ ] Cache integrated into Device shader compilation path
- [ ] Cache validation (detect corrupted cache files)
- [ ] Default cache location configured per platform
- [ ] All unit tests pass (including new cache-specific tests)
- [ ] Performance improvement measured and documented
- [ ] Cache can be disabled via configuration
- [ ] Cache statistics available for debugging

### Performance Validation:

- [ ] Cold start: Shaders compile normally
- [ ] Warm start: At least 10x faster than cold start
- [ ] Cache hit rate > 95% on second launch
- [ ] No regression in rendering output

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**
- [ ] Warm start time is acceptable (target: <500ms for typical shader count)
- [ ] Cache directory location is appropriate
- [ ] No rendering regressions (visual comparison test)
- [ ] All render sessions start faster on second launch

---

## 9. Related Issues

### Blocks:
- None - Standalone enhancement

### Related:
- **E-010** - Shader error messages (compilation errors need caching too)
- **H-013** - PSO cache (related caching strategy for pipeline states)
- **E-011** - Shader debug names (debug names should be part of cache key)

### Depends On:
- None - Can be implemented independently

---

## 10. Implementation Priority

**Priority:** P1-Medium

**Effort Estimate:** 3-4 days
- Day 1: Implement cache key generation and validation
- Day 2: Implement ShaderCache class with save/load
- Day 3: Integrate into Device, add configuration
- Day 4: Testing, performance measurement, polish

**Risk Assessment:** Low-Medium
- Risk: Cache corruption could cause silent failures
  - Mitigation: Validate cached bytecode before use
- Risk: Disk space usage could grow unbounded
  - Mitigation: Add cache size limits and LRU eviction
- Risk: Platform-specific path issues
  - Mitigation: Use standard platform APIs for temp/cache directories

**Impact:** High
- **10-25x faster application startup** on subsequent launches
- **Improved user experience** - no frozen startup
- **Better development iteration time** - instant shader reloads
- **Graceful degradation** - falls back to compilation if cache unavailable
- Minimal runtime overhead (only during shader creation)

---

## 11. References

### Official Documentation:

1. **D3DCompile API:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile

2. **D3DCreateBlob:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcreateblob

3. **Pipeline State Objects:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12

4. **PSO Library (Related):**
   https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device1-createpipelinelibraryfrommemory

5. **Shader Compilation Best Practices:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/shader-model-6-0

### Additional Resources:

6. **DirectX Shader Compiler (DXC):**
   https://github.com/microsoft/DirectXShaderCompiler

7. **Windows App Storage Locations:**
   https://learn.microsoft.com/en-us/windows/apps/design/app-settings/store-and-retrieve-app-data

---

**Last Updated:** 2025-11-12
**Assignee:** Unassigned
**Estimated Completion:** 3-4 days after start
