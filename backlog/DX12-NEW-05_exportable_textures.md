# DX12-NEW-05: Exportable Textures Unimplemented

**Severity:** Medium
**Category:** API Surface / Robustness & Cross-API Parity
**Status:** Open
**Related Issues:** None

---

## Problem Statement

The D3D12 backend **does not implement exportable textures** (cross-process/cross-API resource sharing), while the Vulkan backend fully supports this feature via external memory extensions. When applications request `TextureExportability::Exportable`, the D3D12 implementation returns `Result::Code::Unimplemented`, causing silent failures in code that relies on interop features.

**The Impact:**
- Cross-process texture sharing fails on D3D12 (works on Vulkan/Metal)
- Interop with video decoders, capture libraries, or external renderers broken
- Applications cannot share GPU resources between processes (IPC)
- Cannot export textures to CUDA, OpenCL, or other compute frameworks
- API parity gap between D3D12 and other backends
- Silent failures in production code expecting feature availability

**Example Failure:**
```cpp
// App requests exportable texture for video decode interop
TextureDesc desc;
desc.format = TextureFormat::RGBA_UNorm8;
desc.width = 1920;
desc.height = 1080;
desc.usage = TextureUsageBits::Sampled;
desc.exportability = TextureExportability::Exportable;  // ❌ Fails on D3D12

Result result;
auto texture = device->createTexture(desc, &result);

// D3D12: result.code == Result::Code::Unimplemented
// Vulkan: texture created with VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
// Result: Feature works on Vulkan, silent failure on D3D12
```

---

## Root Cause Analysis

### Current Implementation (Stubbed):

**File:** `src/igl/d3d12/Device.cpp:585-592`

```cpp
std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                 Result* outResult) const {
  // Check for exportable textures
  if (desc.exportability == TextureExportability::Exportable) {
    IGL_LOG_ERROR("Device::createTexture: Exportable textures not implemented on D3D12\n");
    Result::setResult(outResult, Result::Code::Unimplemented,
                      "Exportable textures are not supported on D3D12 backend");
    return nullptr;  // ❌ Feature completely missing
  }

  // ... normal texture creation
}
```

### Vulkan Implementation (Working):

**File:** `src/igl/vulkan/Texture.cpp:180-214`

```cpp
VkExternalMemoryImageCreateInfo externalMemInfo = {};
if (desc.exportability == TextureExportability::Exportable) {
  externalMemInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  externalMemInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  imageCreateInfo.pNext = &externalMemInfo;
}

vkCreateImage(device, &imageCreateInfo, nullptr, &image);

// Allocate memory with external handle support
VkExportMemoryAllocateInfo exportInfo = {};
exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

VkMemoryAllocateInfo allocInfo = {};
allocInfo.pNext = &exportInfo;
vkAllocateMemory(device, &allocInfo, nullptr, &memory);

// Can export handle via vkGetMemoryFdKHR()
```

### Why This Is Wrong:

**Microsoft's Resource Sharing Documentation:**
> "Direct3D 12 supports sharing resources between processes and between different graphics APIs (such as Direct3D 11 and Direct3D 12) using NT handles and named sharing." - [Resource Sharing](https://learn.microsoft.com/windows/win32/direct3d12/resource-sharing)

**D3D12 Shared Resource Pattern:**
```cpp
// Create shared heap
D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_SHARED;
device->CreateHeap(&heapDesc, IID_PPV_ARGS(&sharedHeap));

// Get shareable NT handle
HANDLE sharedHandle;
device->CreateSharedHandle(sharedHeap.Get(), nullptr,
                          GENERIC_ALL, nullptr, &sharedHandle);

// In other process: open shared heap
device->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(&importedHeap));

// Create texture resource from shared heap
device->CreatePlacedResource(importedHeap.Get(), heapOffset,
                             &resourceDesc, initialState, nullptr,
                             IID_PPV_ARGS(&sharedTexture));
```

**Cross-API Parity Gap:**
- **Vulkan**: ✅ Full external memory support (export/import handles)
- **Metal**: ✅ IOSurface-backed textures (MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget)
- **D3D12**: ❌ Completely unimplemented (returns error)

---

## Official Documentation References

1. **D3D12 Resource Sharing**:
   - [Resource Sharing in Direct3D 12](https://learn.microsoft.com/windows/win32/direct3d12/resource-sharing)
   - Quote: "Shared resources can be created with D3D12_HEAP_FLAG_SHARED and exported using ID3D12Device::CreateSharedHandle."

2. **NT Handle Sharing**:
   - [ID3D12Device::CreateSharedHandle](https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createsharedhandle)
   - [ID3D12Device::OpenSharedHandle](https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-opensharedhandle)

3. **Cross-Process Interop**:
   - [Sharing Resources Between Processes](https://learn.microsoft.com/windows/win32/direct3d12/shared-resources)
   - Quote: "NT handles can be named or unnamed. Named handles allow sharing by name across processes."

4. **Vulkan External Memory**:
   - [VK_KHR_external_memory](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_external_memory.html)
   - [vkGetMemoryWin32HandleKHR](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetMemoryWin32HandleKHR.html)

---

## Code Location Strategy

### Files to Modify:

1. **Device.cpp** (`createTexture` method):
   - Search for: `TextureExportability::Exportable`
   - Context: Exportability check that returns Unimplemented
   - Action: Implement shared heap/handle creation instead of returning error

2. **Texture.h** (Add shared handle storage):
   - Search for: `class Texture`
   - Context: Texture class definition
   - Action: Add member variable for shared NT handle

3. **Texture.cpp** (Implement export/import):
   - Search for: `Texture::Texture` constructor
   - Context: Texture initialization
   - Action: Create shared heap if exportable, generate NT handle

4. **Device.h** (Add export/import methods):
   - Search for: `class Device`
   - Context: Device class definition
   - Action: Add methods `exportTextureHandle()` and `importTextureHandle()`

---

## Detection Strategy

### How to Reproduce the Issue:

1. **Request Exportable Texture**:
   ```cpp
   auto device = createD3D12Device();

   TextureDesc desc;
   desc.format = TextureFormat::RGBA_UNorm8;
   desc.width = 512;
   desc.height = 512;
   desc.usage = TextureUsageBits::Sampled | TextureUsageBits::Attachment;
   desc.exportability = TextureExportability::Exportable;

   Result result;
   auto texture = device->createTexture(desc, &result);

   // D3D12: texture == nullptr, result.code == Unimplemented
   // Vulkan: texture != nullptr, can export handle
   ```

2. **Cross-Backend Consistency Check**:
   ```cpp
   // Create texture on Vulkan
   auto vkTexture = vkDevice->createTexture(exportableDesc, nullptr);
   EXPECT_NE(vkTexture, nullptr);  // ✓ Works

   // Create same texture on D3D12
   auto d3d12Texture = d3d12Device->createTexture(exportableDesc, nullptr);
   EXPECT_NE(d3d12Texture, nullptr);  // ❌ Fails (returns nullptr)
   ```

3. **Interop Test (After Fix)**:
   ```cpp
   // Process A: Create and export texture
   auto texture = device->createTexture(exportableDesc, nullptr);
   HANDLE sharedHandle = device->exportTextureHandle(texture);

   // Pass handle to Process B (via IPC)
   // Process B: Import texture
   auto importedTexture = device->importTextureHandle(sharedHandle);
   // Should work after fix
   ```

### Verification After Fix:

1. Exportable textures no longer return Unimplemented
2. Can create shared NT handle from texture
3. Can import texture from NT handle in another process
4. Cross-API parity test passes (D3D12 matches Vulkan behavior)

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Shared Handle Storage to Texture Class

**File:** `src/igl/d3d12/Texture.h`

**Add to Texture class:**
```cpp
class Texture final : public ITexture {
 public:
  // ... existing methods ...

  // Get shared NT handle for exportable textures
  HANDLE getSharedHandle() const { return sharedHandle_; }

 private:
  // ... existing members ...

  // Shared NT handle for cross-process/cross-API interop
  HANDLE sharedHandle_ = nullptr;
  bool isExportable_ = false;
};
```

---

#### Step 2: Implement Exportable Texture Creation

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createTexture` method where exportability check occurs

**Current (BROKEN):**
```cpp
std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                 Result* outResult) const {
  if (desc.exportability == TextureExportability::Exportable) {
    IGL_LOG_ERROR("Device::createTexture: Exportable textures not implemented\n");
    Result::setResult(outResult, Result::Code::Unimplemented, ...);
    return nullptr;  // ❌
  }
  // ... normal creation
}
```

**Fixed (WORKING):**
```cpp
std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                 Result* outResult) const {
  const bool isExportable = (desc.exportability == TextureExportability::Exportable);

  // Step 1: Create heap (shared if exportable)
  D3D12_HEAP_DESC heapDesc = {};
  heapDesc.SizeInBytes = calculateTextureSize(desc);  // Compute required size
  heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

  if (isExportable) {
    heapDesc.Flags = D3D12_HEAP_FLAG_SHARED;  // ✓ Enable sharing
    IGL_LOG_INFO("Device::createTexture: Creating exportable texture\n");
  } else {
    heapDesc.Flags = D3D12_HEAP_FLAG_NONE;
  }

  Microsoft::WRL::ComPtr<ID3D12Heap> heap;
  HRESULT hr = device_->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap));
  if (FAILED(hr)) {
    Result::setResult(outResult, Result::Code::RuntimeError,
                      "Failed to create texture heap");
    return nullptr;
  }

  // Step 2: Create placed resource on the heap
  D3D12_RESOURCE_DESC resourceDesc = convertTextureDescToResourceDesc(desc);

  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  hr = device_->CreatePlacedResource(
    heap.Get(),
    0,  // Heap offset
    &resourceDesc,
    D3D12_RESOURCE_STATE_COMMON,
    nullptr,  // Clear value
    IID_PPV_ARGS(&resource)
  );

  if (FAILED(hr)) {
    Result::setResult(outResult, Result::Code::RuntimeError,
                      "Failed to create placed resource");
    return nullptr;
  }

  // Step 3: Generate shared handle if exportable
  HANDLE sharedHandle = nullptr;
  if (isExportable) {
    hr = device_->CreateSharedHandle(
      heap.Get(),
      nullptr,  // Security attributes (nullptr = default)
      GENERIC_ALL,
      nullptr,  // Name (nullptr = unnamed handle)
      &sharedHandle
    );

    if (FAILED(hr)) {
      IGL_LOG_ERROR("Device::createTexture: Failed to create shared handle (HRESULT: 0x%08X)\n", hr);
      Result::setResult(outResult, Result::Code::RuntimeError,
                        "Failed to create shared handle for exportable texture");
      return nullptr;
    }

    IGL_LOG_INFO("Device::createTexture: Created shared handle %p\n", sharedHandle);
  }

  // Step 4: Wrap in Texture object
  auto texture = std::make_shared<Texture>(*ctx_, resource, heap, sharedHandle, desc);

  Result::setOk(outResult);
  return texture;
}
```

---

#### Step 3: Update Texture Constructor

**File:** `src/igl/d3d12/Texture.cpp`

**Add constructor overload to store shared handle:**
```cpp
Texture::Texture(D3D12Context& context,
                 Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                 Microsoft::WRL::ComPtr<ID3D12Heap> heap,
                 HANDLE sharedHandle,
                 const TextureDesc& desc)
  : context_(context),
    resource_(std::move(resource)),
    heap_(std::move(heap)),
    sharedHandle_(sharedHandle),
    isExportable_(sharedHandle != nullptr),
    desc_(desc) {

  if (isExportable_) {
    IGL_LOG_INFO("Texture: Created exportable texture with shared handle %p\n", sharedHandle_);
  }
}

Texture::~Texture() {
  // Close shared handle if exportable
  if (sharedHandle_ != nullptr) {
    CloseHandle(sharedHandle_);
    sharedHandle_ = nullptr;
  }
}
```

---

#### Step 4: Add Export/Import Methods to Device

**File:** `src/igl/d3d12/Device.h`

```cpp
class Device final : public IDevice {
 public:
  // ... existing methods ...

  // Export texture as NT handle for cross-process sharing
  HANDLE exportTextureHandle(const std::shared_ptr<ITexture>& texture) const;

  // Import texture from NT handle
  std::shared_ptr<ITexture> importTextureHandle(HANDLE sharedHandle,
                                                 const TextureDesc& desc,
                                                 Result* outResult) const;
};
```

**File:** `src/igl/d3d12/Device.cpp`

```cpp
HANDLE Device::exportTextureHandle(const std::shared_ptr<ITexture>& texture) const {
  auto d3d12Texture = std::static_pointer_cast<Texture>(texture);

  if (!d3d12Texture->getSharedHandle()) {
    IGL_LOG_ERROR("Device::exportTextureHandle: Texture is not exportable\n");
    return nullptr;
  }

  // Duplicate handle for caller (caller must CloseHandle)
  HANDLE duplicatedHandle;
  if (!DuplicateHandle(
        GetCurrentProcess(),
        d3d12Texture->getSharedHandle(),
        GetCurrentProcess(),
        &duplicatedHandle,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS)) {
    IGL_LOG_ERROR("Device::exportTextureHandle: Failed to duplicate handle\n");
    return nullptr;
  }

  return duplicatedHandle;
}

std::shared_ptr<ITexture> Device::importTextureHandle(HANDLE sharedHandle,
                                                       const TextureDesc& desc,
                                                       Result* outResult) const {
  // Open shared heap from handle
  Microsoft::WRL::ComPtr<ID3D12Heap> importedHeap;
  HRESULT hr = device_->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(&importedHeap));

  if (FAILED(hr)) {
    Result::setResult(outResult, Result::Code::RuntimeError,
                      "Failed to open shared handle");
    return nullptr;
  }

  // Create placed resource on imported heap
  D3D12_RESOURCE_DESC resourceDesc = convertTextureDescToResourceDesc(desc);

  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  hr = device_->CreatePlacedResource(
    importedHeap.Get(),
    0,
    &resourceDesc,
    D3D12_RESOURCE_STATE_COMMON,
    nullptr,
    IID_PPV_ARGS(&resource)
  );

  if (FAILED(hr)) {
    Result::setResult(outResult, Result::Code::RuntimeError,
                      "Failed to create placed resource from imported heap");
    return nullptr;
  }

  // Wrap in Texture object (shared handle remains with original texture)
  auto texture = std::make_shared<Texture>(*ctx_, resource, importedHeap, nullptr, desc);

  Result::setOk(outResult);
  return texture;
}
```

---

#### Step 5: Add Feature Detection

**File:** `src/igl/d3d12/Device.cpp`

**In `hasFeature` method:**
```cpp
bool Device::hasFeature(DeviceFeatures feature) const {
  switch (feature) {
    // ... existing features ...

    case DeviceFeatures::ExternalMemoryObjects:
      // D3D12 supports shared resources via NT handles on Windows
      #ifdef _WIN32
        return true;  // ✓ Now supported
      #else
        return false;
      #endif

    default:
      return false;
  }
}
```

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Specific exportable texture tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Exportable*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*SharedHandle*"
```

**Test Modifications Allowed:**
- ✅ Add exportable texture creation tests
- ✅ Add shared handle export/import tests
- ✅ Add cross-platform parity tests
- ❌ **DO NOT modify cross-platform test logic**

### New Test Cases Required:

```cpp
TEST(D3D12Device, CreateExportableTexture) {
  auto device = createD3D12Device();

  TextureDesc desc;
  desc.format = TextureFormat::RGBA_UNorm8;
  desc.width = 512;
  desc.height = 512;
  desc.usage = TextureUsageBits::Sampled;
  desc.exportability = TextureExportability::Exportable;

  Result result;
  auto texture = device->createTexture(desc, &result);

  // Should no longer return Unimplemented
  EXPECT_EQ(result.code, Result::Code::Ok);
  EXPECT_NE(texture, nullptr);

  // Should have valid shared handle
  auto d3d12Texture = std::static_pointer_cast<igl::d3d12::Texture>(texture);
  EXPECT_NE(d3d12Texture->getSharedHandle(), nullptr);
}

TEST(D3D12Device, ExportImportTexture) {
  auto device = createD3D12Device();

  // Create exportable texture
  TextureDesc desc;
  desc.format = TextureFormat::RGBA_UNorm8;
  desc.width = 256;
  desc.height = 256;
  desc.usage = TextureUsageBits::Sampled;
  desc.exportability = TextureExportability::Exportable;

  auto originalTexture = device->createTexture(desc, nullptr);
  ASSERT_NE(originalTexture, nullptr);

  // Export handle
  HANDLE sharedHandle = device->exportTextureHandle(originalTexture);
  ASSERT_NE(sharedHandle, nullptr);

  // Import in same process (simulates cross-process scenario)
  auto importedTexture = device->importTextureHandle(sharedHandle, desc, nullptr);
  ASSERT_NE(importedTexture, nullptr);

  // Both textures should point to the same underlying resource
  auto orig = std::static_pointer_cast<igl::d3d12::Texture>(originalTexture);
  auto imported = std::static_pointer_cast<igl::d3d12::Texture>(importedTexture);

  // (Optional: verify via UAV write + SRV read that they share data)

  CloseHandle(sharedHandle);
}

TEST(D3D12Device, CrossPlatformExportableParity) {
  // Verify D3D12 and Vulkan both support exportable textures
  auto d3d12Device = createD3D12Device();
  auto vulkanDevice = createVulkanDevice();

  TextureDesc desc;
  desc.exportability = TextureExportability::Exportable;
  // ... other fields

  // Both should support the feature
  EXPECT_TRUE(d3d12Device->hasFeature(DeviceFeatures::ExternalMemoryObjects));
  EXPECT_TRUE(vulkanDevice->hasFeature(DeviceFeatures::ExternalMemoryObjects));

  // Both should create exportable textures successfully
  auto d3d12Texture = d3d12Device->createTexture(desc, nullptr);
  auto vulkanTexture = vulkanDevice->createTexture(desc, nullptr);

  EXPECT_NE(d3d12Texture, nullptr);
  EXPECT_NE(vulkanTexture, nullptr);
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass (exportable textures optional feature)
./test_all_sessions.bat
```

**Expected Changes:**
- Sessions continue working (exportable textures not required)
- No regressions in texture creation

**Modifications Allowed:**
- ✅ None required
- ❌ **DO NOT modify session logic**

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass
- [ ] All render sessions pass
- [ ] Exportable texture creation no longer returns Unimplemented
- [ ] Can create shared NT handle from exportable texture
- [ ] Can import texture from shared NT handle
- [ ] Feature parity test passes (D3D12 matches Vulkan)
- [ ] `hasFeature(ExternalMemoryObjects)` returns true on D3D12

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Export/import test passes (single process simulation)
2. Cross-platform parity test passes
3. All render sessions pass without regression

**Post in chat:**
```
DX12-NEW-05 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- Exportable textures: Now supported
- Cross-platform parity: D3D12 matches Vulkan

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- None (independent implementation)

### Blocks:
- Video decode interop features
- Cross-process rendering pipelines
- CUDA/OpenCL interop

### Related:
- **DX12-NEW-06** (bindUniform stub) - Similar API parity gap

---

## Implementation Priority

**Priority:** P1 - MEDIUM (Feature Parity Gap)
**Estimated Effort:** 8-12 hours (2 sprints per Codex report)
**Risk:** Medium (requires understanding of shared heaps, cross-process handles)
**Impact:** MEDIUM - Enables interop scenarios, improves cross-platform consistency

---

## References

- [Resource Sharing in Direct3D 12](https://learn.microsoft.com/windows/win32/direct3d12/resource-sharing)
- [ID3D12Device::CreateSharedHandle](https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createsharedhandle)
- [ID3D12Device::OpenSharedHandle](https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-opensharedhandle)
- [VK_KHR_external_memory](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_external_memory.html)
- [Sharing Resources Between Processes](https://learn.microsoft.com/windows/win32/direct3d12/shared-resources)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
