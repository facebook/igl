# I-002: Buffer Alignment Requirements Differ from Vulkan

**Priority:** P1-Medium
**Category:** Interoperability - Cross-Platform Consistency
**Status:** Open
**Effort:** Small (1-2 days)

---

## 1. Problem Statement

The D3D12 backend has different buffer alignment requirements than Vulkan, causing issues when applications developed on one backend fail on the other. D3D12 requires constant buffers to be aligned to 256 bytes, while Vulkan may have different requirements. This inconsistency breaks cross-platform compatibility.

### The Danger

**Silent data corruption and cross-platform failures:**
- Application works perfectly on Vulkan, crashes on D3D12 with alignment errors
- Constant buffer data appears corrupted (wrong values in shader)
- GPU validation errors only in debug mode: "CBV offset not aligned to 256 bytes"
- Developers waste time tracking down "platform-specific bugs"
- Shader constants mysteriously wrong (reading from wrong offset)

**Real-world scenario:**
```cpp
// Application code that works on Vulkan:
struct LightingData {
  vec3 lightPosition;  // 12 bytes
  float intensity;     // 4 bytes
  // Total: 16 bytes
};

BufferDesc desc;
desc.size = sizeof(LightingData);  // 16 bytes
desc.usage = BufferUsage::Constant;

auto buffer = device->createBuffer(desc);  // ← Works on Vulkan

// On D3D12:
// - Creates 16-byte buffer
// - Attempts to bind as CBV
// - D3D12 validation error: CBV must be 256-byte aligned
// - Shader reads garbage data
// - Application crashes or renders incorrectly
```

**Impact:**
- "Works on my machine" (Vulkan) but fails on D3D12
- QA reports platform-specific rendering bugs
- Hours spent debugging what appears to be random corruption

---

## 2. Root Cause Analysis

D3D12 and Vulkan have different alignment requirements:

**D3D12 Requirements:**
```cpp
// D3D12 constant buffer view requires 256-byte alignment
#define D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT 256

// Current implementation doesn't enforce this:
ComPtr<ID3D12Resource> buffer;
device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &bufferDesc,  // bufferDesc.Width might be 16 bytes!
    initialState,
    nullptr,
    IID_PPV_ARGS(&buffer));

// Later, when creating CBV:
D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
cbvDesc.BufferLocation = buffer->GetGPUVirtualAddress();
cbvDesc.SizeInBytes = 16;  // ← NOT aligned to 256! INVALID!
```

**Vulkan Requirements:**
- Minimum alignment from `VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment`
- Typically 16 or 256 bytes, device-dependent
- Application queries limits and aligns accordingly

**Current IGL D3D12 Backend:**
1. **No automatic alignment:** Creates buffer exactly as requested
2. **No validation:** Doesn't check if size meets D3D12 requirements
3. **No limit exposure:** Doesn't expose alignment requirements to application

**Where It Breaks:**
```cpp
// In Buffer.cpp or Device.cpp
D3D12_RESOURCE_DESC resourceDesc;
resourceDesc.Width = desc.size;  // ← Uses exact size, no rounding!

// Should be:
resourceDesc.Width = alignBufferSize(desc.size, desc.usage);
```

---

## 3. Official Documentation References

- **D3D12 Resource Alignment:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/alignment

- **Constant Buffer View (CBV) Requirements:**
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_constant_buffer_view_desc

- **D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants

- **Vulkan Buffer Alignment:**
  https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html

---

## 4. Code Location Strategy

**Primary Files to Modify:**

1. **Buffer creation:**
   ```
   Search for: "CreateCommittedResource" and "BufferDesc"
   In files: src/igl/d3d12/Buffer.cpp, src/igl/d3d12/Device.cpp
   Pattern: Look for buffer resource creation
   ```

2. **Buffer descriptor:**
   ```
   Search for: "D3D12_RESOURCE_DESC" and "Width"
   In files: src/igl/d3d12/Buffer.cpp
   Pattern: Look for buffer size assignment
   ```

3. **Device capabilities:**
   ```
   Search for: "getCapabilities" or "DeviceLimits"
   In files: src/igl/d3d12/Device.cpp, include/igl/Device.h
   Pattern: Look for where device limits are exposed
   ```

4. **Constant buffer view creation:**
   ```
   Search for: "D3D12_CONSTANT_BUFFER_VIEW_DESC" or "CreateConstantBufferView"
   In files: src/igl/d3d12/RenderCommandEncoder.cpp, src/igl/d3d12/BindGroup.cpp
   Pattern: Look for CBV descriptor creation
   ```

---

## 5. Detection Strategy

### Reproduction Steps:

1. **Create undersized constant buffer:**
   ```cpp
   BufferDesc desc;
   desc.size = 16;  // ← Less than 256 bytes
   desc.usage = BufferUsage::Constant;
   desc.storage = BufferStorage::Device;

   auto buffer = device->createBuffer(desc);
   EXPECT_TRUE(buffer);  // Creates successfully (shouldn't!)
   ```

2. **Attempt to bind as constant buffer:**
   ```cpp
   renderEncoder->bindBuffer(buffer, 0, ShaderStage::Vertex);

   // D3D12 debug layer should complain (if enabled):
   // "D3D12 ERROR: ID3D12Device::CreateConstantBufferView:
   //  SizeInBytes of 16 is invalid. Device requires SizeInBytes
   //  be a multiple of 256."
   ```

3. **Enable D3D12 debug layer:**
   ```bash
   set D3D12_DEBUG=1
   ./build/Debug/IGLTests.exe --gtest_filter="*Buffer*Alignment*"
   ```

4. **Expected error (currently missing):**
   ```
   IGL_ERROR: Constant buffer size 16 does not meet D3D12
   alignment requirement of 256 bytes. Buffer will be padded to 256 bytes.
   ```

### Cross-Platform Test:

```cpp
TEST(BufferAlignment, ConsistentAcrossPlatforms) {
  // Same buffer description
  BufferDesc desc;
  desc.size = 100;
  desc.usage = BufferUsage::Constant;

  auto buffer = device->createBuffer(desc);

  // Actual buffer size should be aligned on all platforms
  size_t actualSize = buffer->getSize();

  // On D3D12: should be 256 (rounded up)
  // On Vulkan: should be aligned to minUniformBufferOffsetAlignment
  EXPECT_GE(actualSize, desc.size);
  EXPECT_EQ(actualSize % getConstantBufferAlignment(), 0);
}
```

---

## 6. Fix Guidance

### Step 1: Define Alignment Helper Functions

```cpp
// In Buffer.cpp or Device.cpp
namespace {

constexpr size_t alignUp(size_t size, size_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

size_t getBufferAlignment(BufferUsage usage, BufferStorage storage) {
  // D3D12 alignment requirements
  switch (usage) {
    case BufferUsage::Constant:
      return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;  // 256

    case BufferUsage::Index:
      // Index buffers have no special alignment in D3D12
      return 4;  // Align to DWORD for safety

    case BufferUsage::Vertex:
      // Vertex buffers have no special alignment
      return 4;

    case BufferUsage::Storage:
      // UAVs require 4-byte alignment for structured buffers
      return 4;

    default:
      return 16;  // Conservative default
  }
}

size_t alignBufferSize(size_t requestedSize, BufferUsage usage, BufferStorage storage) {
  size_t alignment = getBufferAlignment(usage, storage);
  return alignUp(requestedSize, alignment);
}

} // anonymous namespace
```

### Step 2: Apply Alignment in Buffer Creation

```cpp
// In Buffer.cpp or Device.cpp
Result<std::shared_ptr<IBuffer>> Device::createBuffer(
    const BufferDesc& desc,
    Result* outResult) {

  // Calculate aligned size
  size_t requestedSize = desc.size;
  size_t alignedSize = alignBufferSize(requestedSize, desc.usage, desc.storage);

  // Warn if alignment changed size
  if (alignedSize != requestedSize) {
    IGL_LOG_INFO("Buffer size %zu aligned to %zu bytes for D3D12 %s buffer requirements",
                 requestedSize, alignedSize,
                 desc.usage == BufferUsage::Constant ? "constant" : "");
  }

  // Create resource with aligned size
  D3D12_RESOURCE_DESC resourceDesc = {};
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resourceDesc.Width = alignedSize;  // ← Use aligned size
  resourceDesc.Height = 1;
  resourceDesc.DepthOrArraySize = 1;
  resourceDesc.MipLevels = 1;
  resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.SampleDesc.Quality = 0;
  resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resourceDesc.Flags = getResourceFlags(desc.usage);

  // ... rest of buffer creation ...

  // Store both requested and actual size
  auto buffer = std::make_shared<Buffer>(resource, requestedSize, alignedSize);
  return buffer;
}
```

### Step 3: Update Buffer Class to Track Sizes

```cpp
// In Buffer.h
class Buffer final : public IBuffer {
 public:
  Buffer(ComPtr<ID3D12Resource> resource,
         size_t requestedSize,
         size_t actualSize);

  size_t getSize() const override { return requestedSize_; }
  size_t getActualSize() const { return actualSize_; }

 private:
  ComPtr<ID3D12Resource> resource_;
  size_t requestedSize_;  // What application asked for
  size_t actualSize_;     // What D3D12 allocated (aligned)
};

// In Buffer.cpp
Buffer::Buffer(ComPtr<ID3D12Resource> resource,
               size_t requestedSize,
               size_t actualSize)
    : resource_(std::move(resource))
    , requestedSize_(requestedSize)
    , actualSize_(actualSize) {

  IGL_ASSERT(actualSize >= requestedSize);
  IGL_ASSERT(resource_);
}
```

### Step 4: Validate CBV Descriptor Creation

```cpp
// In RenderCommandEncoder.cpp or BindGroup.cpp
void createConstantBufferView(ID3D12Resource* buffer, size_t size, size_t offset) {
  // Validate size alignment
  if (size % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0) {
    IGL_LOG_ERROR("Constant buffer size %zu not aligned to %u bytes",
                  size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    IGL_ASSERT_MSG(false, "Invalid CBV size");
    return;
  }

  // Validate offset alignment
  if (offset % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0) {
    IGL_LOG_ERROR("Constant buffer offset %zu not aligned to %u bytes",
                  offset, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    IGL_ASSERT_MSG(false, "Invalid CBV offset");
    return;
  }

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
  cbvDesc.BufferLocation = buffer->GetGPUVirtualAddress() + offset;
  cbvDesc.SizeInBytes = static_cast<UINT>(size);

  // Create CBV...
}
```

### Step 5: Expose Alignment Requirements via Device Limits

```cpp
// In Device.h
struct BufferLimits {
  size_t minConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
  size_t minStorageBufferAlignment = 4;
  size_t minVertexBufferAlignment = 4;
  size_t minIndexBufferAlignment = 4;
};

class Device final : public IDevice {
 public:
  BufferLimits getBufferLimits() const;
};

// In Device.cpp
BufferLimits Device::getBufferLimits() const {
  BufferLimits limits;
  // D3D12 has fixed alignment requirements
  return limits;
}
```

### Step 6: Add Cross-Platform Compatibility Helper

```cpp
// In include/igl/Device.h or util/Alignment.h
namespace igl {

// Cross-platform buffer size alignment
inline size_t getAlignedBufferSize(size_t size, BufferUsage usage) {
  size_t alignment = 1;

  #ifdef IGL_D3D12
    if (usage == BufferUsage::Constant) {
      alignment = 256;  // D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
    }
  #elif defined(IGL_VULKAN)
    // Query from device limits
    alignment = getDevice()->getBufferLimits().minConstantBufferAlignment;
  #else
    alignment = 16;  // Conservative default
  #endif

  return (size + alignment - 1) & ~(alignment - 1);
}

} // namespace igl
```

---

## 7. Testing Requirements

### Unit Tests:

```bash
# Run buffer alignment tests
./build/Debug/IGLTests.exe --gtest_filter="*Buffer*Alignment*"

# Run all D3D12 buffer tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Buffer*"
```

### Test Cases:

```cpp
TEST(BufferAlignment, ConstantBufferAligned) {
  BufferDesc desc;
  desc.size = 16;  // Small constant buffer
  desc.usage = BufferUsage::Constant;

  auto buffer = device->createBuffer(desc);
  ASSERT_TRUE(buffer);

  // Actual size should be aligned to 256
  auto d3dBuffer = std::static_pointer_cast<Buffer>(buffer);
  EXPECT_EQ(d3dBuffer->getActualSize(), 256);
  EXPECT_EQ(buffer->getSize(), 16);  // Requested size preserved
}

TEST(BufferAlignment, VertexBufferNotOveraligned) {
  BufferDesc desc;
  desc.size = 100;
  desc.usage = BufferUsage::Vertex;

  auto buffer = device->createBuffer(desc);
  ASSERT_TRUE(buffer);

  // Vertex buffers don't need 256-byte alignment
  auto d3dBuffer = std::static_pointer_cast<Buffer>(buffer);
  EXPECT_LT(d3dBuffer->getActualSize(), 256);
  EXPECT_GE(d3dBuffer->getActualSize(), 100);
}

TEST(BufferAlignment, LargeConstantBufferAligned) {
  BufferDesc desc;
  desc.size = 1000;  // Larger than alignment
  desc.usage = BufferUsage::Constant;

  auto buffer = device->createBuffer(desc);
  ASSERT_TRUE(buffer);

  auto d3dBuffer = std::static_pointer_cast<Buffer>(buffer);
  EXPECT_EQ(d3dBuffer->getActualSize(), 1024);  // Next multiple of 256
}

TEST(BufferAlignment, AlignmentExposedViaLimits) {
  auto limits = device->getBufferLimits();
  EXPECT_EQ(limits.minConstantBufferAlignment, 256);
}

TEST(BufferAlignment, CrossPlatformConsistency) {
  // This test verifies that the same buffer description
  // produces a valid buffer on both D3D12 and Vulkan
  BufferDesc desc;
  desc.size = 64;
  desc.usage = BufferUsage::Constant;

  auto buffer = device->createBuffer(desc);
  ASSERT_TRUE(buffer);

  // Buffer should work when bound to pipeline
  renderEncoder->bindBuffer(buffer, 0, ShaderStage::Vertex);
  // Should not produce validation errors
}
```

### Integration Tests:

```bash
# Test all render sessions with small constant buffers
./test_all_sessions.bat

# Specific test with constant buffers
./build/Debug/Session/TinyMeshSession.exe
./build/Debug/Session/Textured3DCubeSession.exe
```

### Cross-Platform Validation:

1. **Same application on both backends:**
   ```cpp
   // Application creates 64-byte constant buffer
   // D3D12: Allocates 256 bytes
   // Vulkan: Allocates aligned to minUniformBufferOffsetAlignment
   // Both: Work correctly without modification
   ```

2. **Debug layer validation:**
   ```bash
   set D3D12_DEBUG=1
   ./build/Debug/IGLTests.exe --gtest_filter="*Buffer*"
   # Should produce NO alignment warnings
   ```

### Modification Restrictions:

- **DO NOT modify** cross-platform IGL buffer API semantics
- **DO NOT change** the requested size returned by getSize()
- **DO NOT break** existing code that creates aligned buffers
- **ONLY add** automatic alignment, transparent to application

---

## 8. Definition of Done

### Validation Checklist:

- [ ] Constant buffers automatically aligned to 256 bytes
- [ ] Buffer class tracks both requested and actual sizes
- [ ] getSize() returns requested size (API compatibility)
- [ ] getActualSize() returns aligned size (for debugging)
- [ ] Buffer limits exposed via device capabilities
- [ ] All unit tests pass
- [ ] D3D12 debug layer produces no alignment warnings
- [ ] Integration tests pass on both D3D12 and Vulkan
- [ ] Documentation updated with alignment behavior

### Cross-Platform Validation:

- [ ] Same buffer code works on D3D12 and Vulkan
- [ ] No platform-specific alignment required in application code
- [ ] Visual Studio Graphics Debugger shows valid CBVs

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**
- [ ] Application works on both D3D12 and Vulkan without changes
- [ ] No D3D12 validation errors related to buffer alignment
- [ ] No visual regressions in rendering
- [ ] Performance not impacted by alignment (memory usage acceptable)

---

## 9. Related Issues

### Blocks:
- None - Standalone enhancement

### Related:
- **I-001** - Resource state mismatch (both about D3D12 resource requirements)
- **C-005** - Buffer creation validation (alignment is part of validation)
- **H-004** - Feature detection (should expose alignment requirements)

### Depends On:
- None - Can be implemented independently

---

## 10. Implementation Priority

**Priority:** P1-Medium

**Effort Estimate:** 1-2 days
- Day 1: Implement alignment logic and buffer size tracking
- Day 2: Testing, expose limits via API, documentation

**Risk Assessment:** Low
- Changes are transparent to applications (automatic alignment)
- Easy to test with debug layer validation
- Clear validation criteria (alignment check)
- No breaking changes (only adds padding)

**Impact:** Medium-High
- **Eliminates cross-platform incompatibility** for constant buffers
- **Prevents subtle bugs** from unaligned buffers
- **Better developer experience** - no manual alignment required
- **Matches industry best practices** (automatic alignment)
- Slight memory overhead (padding to 256 bytes for small constant buffers)

---

## 11. References

### Official Documentation:

1. **D3D12 Resource Alignment:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/alignment

2. **D3D12_CONSTANT_BUFFER_VIEW_DESC:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_constant_buffer_view_desc

3. **D3D12 Constants (Alignment Macros):**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants

4. **Vulkan Physical Device Limits:**
   https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html

5. **Creating Buffers in D3D12:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-descriptor-heaps

### Additional Resources:

6. **Resource Binding Tier:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support

7. **Buffer Alignment Best Practices:**
   https://developer.nvidia.com/blog/dx12-dos-and-donts/

---

**Last Updated:** 2025-11-12
**Assignee:** Unassigned
**Estimated Completion:** 1-2 days after start
