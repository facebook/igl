# I-004: Texture Format Support Query Incomplete

**Severity:** Medium
**Category:** API Completeness & Cross-Platform Portability
**Status:** Open
**Related Issues:** I-003 (MSAA Sample Count Limits), I-005 (Descriptor Heap Size Limits)

---

## Problem Statement

The D3D12 backend's `getTextureFormatCapabilities()` implementation provides **incomplete capability information** for texture formats. While it queries `D3D12_FEATURE_FORMAT_SUPPORT`, it does **not expose all relevant D3D12 format support flags** to applications, creating a gap between what D3D12 reports and what IGL surfaces. This leads to:

**The Impact:**
- Applications cannot determine if a format supports **UAV (Unordered Access View)** operations beyond typed load/store
- Missing information about **MSAA support** per format (separate from sample count query)
- No exposure of **mipmap generation support** flags
- **Blend operation support** not reported (important for render targets)
- **Typed UAV additional formats** (D3D12_FORMAT_SUPPORT2) not fully utilized
- Cross-platform code cannot make informed decisions about format capabilities
- Advanced rendering techniques (compute shaders with UAVs, MSAA resolve) lack capability detection

**Real-World Scenario:**
```cpp
// Application wants to use R32_UINT as UAV atomic target
auto device = createDevice(BackendType::D3D12);

// Query format capabilities
auto caps = device->getTextureFormatCapabilities(TextureFormat::R_UInt32);

// IGL reports: caps = Sampled | Storage (if typed load/store supported)
// But D3D12 provides MORE information:
//   - D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD
//   - D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS
//   - D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE
// These atomic operation flags are NOT exposed to application!

// Application attempts to use atomic operations, not knowing if supported
// Result: Runtime error or device removal
```

---

## Root Cause Analysis

### Current Implementation - Incomplete Flag Mapping:

#### D3D12 Device.cpp (`getTextureFormatCapabilities()` - Lines 2495-2568)
```cpp
ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(TextureFormat format) const {
  using CapBits = ICapabilities::TextureFormatCapabilityBits;
  uint8_t caps = 0;

  // ... depth format handling ...

  // Query D3D12 format support
  D3D12_FEATURE_DATA_FORMAT_SUPPORT fs = {};
  fs.Format = dxgi;
  if (FAILED(dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &fs, sizeof(fs)))) {
    return 0;
  }

  const auto s1 = fs.Support1;  // D3D12_FORMAT_SUPPORT1 flags
  const auto s2 = fs.Support2;  // D3D12_FORMAT_SUPPORT2 flags

  // Currently mapped flags:
  if (s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) {
    caps |= CapBits::Sampled;
  }
  if ((s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) && props.hasColor() && !props.isInteger()) {
    caps |= CapBits::SampledFiltered;
  }
  if ((s1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) || (s1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) {
    caps |= CapBits::Attachment;
  }
  // Typed UAV load + store required for Storage
  if ((s2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) && (s2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE) &&
      hasFeature(DeviceFeatures::Compute)) {
    caps |= CapBits::Storage;
  }

  // MISSING: Many D3D12 format support flags NOT exposed!
  // - D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET
  // - D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE
  // - D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD
  // - D3D12_FORMAT_SUPPORT1_BLENDABLE
  // - D3D12_FORMAT_SUPPORT1_MIP
  // - D3D12_FORMAT_SUPPORT1_MIP_AUTOGEN
  // - D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_*
  // - D3D12_FORMAT_SUPPORT2_TILED
  // - And more...

  return caps;
}
```

**Problem:** Only 4-5 capability bits mapped out of 50+ D3D12 format support flags!

### D3D12 Format Support Flags (Full List):

#### D3D12_FORMAT_SUPPORT1 (Not Fully Mapped):
```cpp
D3D12_FORMAT_SUPPORT1_BUFFER                      // Buffer support
D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER            // Input assembler vertex buffer
D3D12_FORMAT_SUPPORT1_IA_INDEX_BUFFER             // Input assembler index buffer
D3D12_FORMAT_SUPPORT1_SO_BUFFER                   // Stream output buffer
D3D12_FORMAT_SUPPORT1_TEXTURE1D                   // 1D texture
D3D12_FORMAT_SUPPORT1_TEXTURE2D                   // 2D texture
D3D12_FORMAT_SUPPORT1_TEXTURE3D                   // 3D texture
D3D12_FORMAT_SUPPORT1_TEXTURECUBE                 // Cube texture
D3D12_FORMAT_SUPPORT1_SHADER_LOAD                 // Shader load (sampled) ✓ Mapped
D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE               // Shader sample ✓ Mapped
D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON    // Shadow sampling
D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_MONO_TEXT     // Monochrome text sampling
D3D12_FORMAT_SUPPORT1_MIP                         // Mipmap support ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT1_MIP_AUTOGEN                 // Auto mipmap gen ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT1_RENDER_TARGET               // Render target ✓ Mapped (Attachment)
D3D12_FORMAT_SUPPORT1_BLENDABLE                   // Blendable RT ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL               // Depth/stencil ✓ Mapped (Attachment)
D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE         // MSAA resolve ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET    // MSAA RT ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD            // MSAA load ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT1_SHADER_GATHER               // Gather4 support ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW // Typed UAV ✗ NOT MAPPED
// ... and more ...
```

#### D3D12_FORMAT_SUPPORT2 (Not Fully Mapped):
```cpp
D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD                           // Atomic add ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS                   // Atomic AND/OR/XOR ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE  // Atomic CAS ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE                      // Atomic exchange ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX             // Atomic min/max ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX           // Atomic umin/umax ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD                           // Typed load ✓ Mapped (Storage)
D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE                          // Typed store ✓ Mapped (Storage)
D3D12_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP                   // Logic ops ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT2_TILED                                    // Tiled resource ✗ NOT MAPPED
D3D12_FORMAT_SUPPORT2_MULTIPLANE_OVERLAY                       // Multiplane ✗ NOT MAPPED
```

### Why This Is Wrong:

1. **Incomplete API Surface**: IGL only exposes 5 capability bits (Sampled, SampledFiltered, Attachment, Storage, SampledAttachment)
2. **Missing Critical Information**: UAV atomic operations, MSAA per-format, mipmap generation, blend support not exposed
3. **Cross-Platform Inconsistency**: Vulkan/Metal may expose more format details through their APIs
4. **Silent Failures**: Applications attempt unsupported operations, causing runtime errors
5. **No Fallback Guidance**: Cannot detect if format supports alternative code paths

---

## Official Documentation References

### D3D12 Documentation:

1. **D3D12_FEATURE_FORMAT_SUPPORT**:
   - [CheckFeatureSupport Method](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport)
   - [D3D12_FEATURE_DATA_FORMAT_SUPPORT Structure](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_format_support)
   - Quote: "Describes which resources are supported by the specified format."

2. **D3D12_FORMAT_SUPPORT1 Enumeration**:
   - [D3D12_FORMAT_SUPPORT1](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_format_support1)
   - Quote: "Specifies which resources are supported by a format for a graphics device."
   - 30+ flags covering textures, buffers, render targets, depth, MSAA, mipmaps, etc.

3. **D3D12_FORMAT_SUPPORT2 Enumeration**:
   - [D3D12_FORMAT_SUPPORT2](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_format_support2)
   - Quote: "Specifies which unordered resource options are supported by a format for a graphics device."
   - Includes UAV atomic operations, logic ops, tiled resources

### Vulkan Documentation:

1. **VkFormatProperties**:
   - [vkGetPhysicalDeviceFormatProperties](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceFormatProperties.html)
   - [VkFormatFeatureFlagBits](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkFormatFeatureFlagBits.html)
   - Provides extensive per-format capability flags (60+ flags)

---

## Code Location Strategy

### Files to Modify:

1. **Device.cpp** (`getTextureFormatCapabilities()` method):
   - Location: `src/igl/d3d12/Device.cpp:2495-2568`
   - Search for: `ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities`
   - Context: Format capability query implementation
   - Action: Add mapping for additional D3D12_FORMAT_SUPPORT1/2 flags

2. **ICapabilities.h** (extend capability bits if needed):
   - Location: `src/igl/ICapabilities.h`
   - Search for: `enum class TextureFormatCapabilityBits`
   - Context: Cross-platform texture format capability flags
   - Action: Review if additional capability bits needed (e.g., `MipmapGeneration`, `BlendSupport`, `AtomicOperations`)

3. **Device.h** (add helper methods if needed):
   - Location: `src/igl/d3d12/Device.h`
   - Context: D3D12-specific extended queries
   - Action: Consider adding `getD3D12FormatSupport(TextureFormat)` for full flag access

### Files to Review (No Changes Expected):

1. **Common.cpp** (`textureFormatToDXGIFormat()` - Line 12)
   - Already correctly maps IGL formats to DXGI formats

---

## Detection Strategy

### How to Reproduce the Incomplete Query:

#### Test 1: Query Format Support Flags
```cpp
auto device = createDevice(BackendType::D3D12);

// Query capabilities for R32_UINT (commonly used for UAV atomics)
auto caps = device->getTextureFormatCapabilities(TextureFormat::R_UInt32);

// IGL reports: Sampled | Storage (if typed UAV supported)
// But D3D12 CheckFeatureSupport returns MANY more flags:
//   - D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD
//   - D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS
//   - etc.

// Application has no way to check if atomic operations supported!
// Must attempt use and handle potential device removal
```

#### Test 2: Compare D3D12 Raw Query vs IGL Query
```cpp
auto d3d12Device = static_cast<igl::d3d12::Device*>(device.get());
auto* device = d3d12Device->getContext()->getDevice();

// Direct D3D12 query
D3D12_FEATURE_DATA_FORMAT_SUPPORT fs = {};
fs.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &fs, sizeof(fs));

std::cout << "D3D12 Support1 flags: 0x" << std::hex << fs.Support1 << "\n";
std::cout << "D3D12 Support2 flags: 0x" << std::hex << fs.Support2 << "\n";

// IGL query
auto iglCaps = device->getTextureFormatCapabilities(TextureFormat::RGBA_UNorm8);
std::cout << "IGL capabilities: 0x" << std::hex << static_cast<uint8_t>(iglCaps) << "\n";

// OBSERVE: D3D12 reports 30+ bits set, IGL reports only 3-4 bits
```

#### Test 3: Format-Specific Feature Detection
```cpp
// Test MSAA support per format
std::vector<TextureFormat> formats = {
  TextureFormat::RGBA_UNorm8,
  TextureFormat::RGBA_F16,
  TextureFormat::Z_UNorm24
};

for (auto format : formats) {
  auto caps = device->getTextureFormatCapabilities(format);

  // PROBLEM: Cannot determine if format supports MSAA from IGL query
  // Must check via separate MSAA query or attempt texture creation
  bool supportsMSAA = /* ??? no way to check via caps */;

  std::cout << "Format " << toString(format)
            << " MSAA support: " << (supportsMSAA ? "YES" : "UNKNOWN") << "\n";
}
```

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Extend IGL TextureFormatCapabilityBits (If Needed)

**File:** `src/igl/ICapabilities.h`

**Locate:** `enum class TextureFormatCapabilityBits`

**Consider adding:**
```cpp
enum class TextureFormatCapabilityBits : uint32_t {
  // Existing bits (0-4):
  Sampled = 1 << 0,
  SampledFiltered = 1 << 1,
  Storage = 1 << 2,
  Attachment = 1 << 3,
  SampledAttachment = 1 << 4,

  // Proposed new bits (5-10):
  Blendable = 1 << 5,              // Supports blend operations (D3D12_FORMAT_SUPPORT1_BLENDABLE)
  MipmapGeneration = 1 << 6,       // Supports auto mipmap gen (D3D12_FORMAT_SUPPORT1_MIP_AUTOGEN)
  MSAASupport = 1 << 7,            // Supports MSAA render target (D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET)
  MSAAResolve = 1 << 8,            // Supports MSAA resolve (D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE)
  AtomicOperations = 1 << 9,       // Supports UAV atomic ops (D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_*)
  LogicOperations = 1 << 10,       // Supports output merger logic ops (D3D12_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP)
};
```

**Rationale:** Exposes most commonly needed format capabilities across all backends

**Note:** This change affects cross-platform API - requires careful review!

---

#### Step 2: Enhance D3D12 getTextureFormatCapabilities()

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Line 2557 (after existing capability mapping)

**Add additional mappings:**
```cpp
ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(TextureFormat format) const {
  using CapBits = ICapabilities::TextureFormatCapabilityBits;
  uint8_t caps = 0;  // Change to uint32_t if extending beyond 8 bits

  // ... existing code ...

  const auto s1 = fs.Support1;
  const auto s2 = fs.Support2;

  // Existing mappings:
  if (s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) {
    caps |= CapBits::Sampled;
  }
  if ((s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) && props.hasColor() && !props.isInteger()) {
    caps |= CapBits::SampledFiltered;
  }
  if (!isThreeChannelRgbFormat &&
      ((s1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) || (s1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL))) {
    caps |= CapBits::Attachment;
  }
  if ((s2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) && (s2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE) &&
      hasFeature(DeviceFeatures::Compute)) {
    caps |= CapBits::Storage;
  }

  // NEW MAPPINGS (if capability bits extended):

  // Blendable render target support
  if (s1 & D3D12_FORMAT_SUPPORT1_BLENDABLE) {
    caps |= CapBits::Blendable;
  }

  // Mipmap generation support (useful for texture utilities)
  if (s1 & D3D12_FORMAT_SUPPORT1_MIP_AUTOGEN) {
    caps |= CapBits::MipmapGeneration;
  }

  // MSAA render target support (per-format)
  if (s1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET) {
    caps |= CapBits::MSAASupport;
  }

  // MSAA resolve support
  if (s1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE) {
    caps |= CapBits::MSAAResolve;
  }

  // UAV atomic operations (check any atomic op flag)
  const bool supportsAtomics =
      (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD) ||
      (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS) ||
      (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE) ||
      (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE) ||
      (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX) ||
      (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX);
  if (supportsAtomics && hasFeature(DeviceFeatures::Compute)) {
    caps |= CapBits::AtomicOperations;
  }

  // Output merger logic operations (bitwise AND/OR/XOR on render targets)
  if (s2 & D3D12_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP) {
    caps |= CapBits::LogicOperations;
  }

  // SampledAttachment (existing logic)
  if ((caps & CapBits::Sampled) && (caps & CapBits::Attachment)) {
    caps |= CapBits::SampledAttachment;
  }

  return caps;
}
```

---

#### Step 3: Add D3D12-Specific Extended Query (Optional)

**File:** `src/igl/d3d12/Device.h`

**Add public method:**
```cpp
class Device final : public IDevice {
 public:
  // ... existing methods ...

  // D3D12-specific: Get full format support flags directly from D3D12 API
  // Useful for advanced applications needing complete information
  struct D3D12FormatSupport {
    D3D12_FORMAT_SUPPORT1 support1;
    D3D12_FORMAT_SUPPORT2 support2;
  };
  [[nodiscard]] Result getD3D12FormatSupport(TextureFormat format, D3D12FormatSupport& outSupport) const;

  // ... rest of class ...
};
```

**File:** `src/igl/d3d12/Device.cpp`

**Add implementation:**
```cpp
Result Device::getD3D12FormatSupport(TextureFormat format, D3D12FormatSupport& outSupport) const {
  auto* device = ctx_->getDevice();
  if (!device) {
    return Result(Result::Code::RuntimeError, "D3D12 device is null");
  }

  const DXGI_FORMAT dxgiFormat = textureFormatToDXGIFormat(format);
  if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
    return Result(Result::Code::ArgumentInvalid, "Unknown texture format");
  }

  D3D12_FEATURE_DATA_FORMAT_SUPPORT fs = {};
  fs.Format = dxgiFormat;

  HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &fs, sizeof(fs));
  if (FAILED(hr)) {
    char msg[128];
    snprintf(msg, sizeof(msg), "CheckFeatureSupport failed: 0x%08X", static_cast<uint32_t>(hr));
    return Result(Result::Code::RuntimeError, msg);
  }

  outSupport.support1 = fs.Support1;
  outSupport.support2 = fs.Support2;

  return Result();
}
```

**Rationale:** Provides escape hatch for advanced D3D12-specific applications needing full format support information

---

#### Step 4: Add Logging for Unmapped Flags (Debug Build)

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** End of `getTextureFormatCapabilities()` method

**Add debug logging:**
```cpp
ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(TextureFormat format) const {
  // ... existing implementation ...

#if IGL_DEBUG
  // Log unmapped D3D12 flags in debug builds to track missing capabilities
  const uint32_t unmappedSupport1 = s1 & ~(
      D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE |
      D3D12_FORMAT_SUPPORT1_RENDER_TARGET |
      D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL |
      D3D12_FORMAT_SUPPORT1_BLENDABLE |
      D3D12_FORMAT_SUPPORT1_MIP_AUTOGEN |
      D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET |
      D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE
  );

  const uint32_t unmappedSupport2 = s2 & ~(
      D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD |
      D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE |
      D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD |
      D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS |
      D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE |
      D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE |
      D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX |
      D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX |
      D3D12_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP
  );

  if (unmappedSupport1 != 0 || unmappedSupport2 != 0) {
    IGL_LOG_INFO("Device::getTextureFormatCapabilities: Format %d has unmapped D3D12 flags: "
                 "Support1=0x%08X, Support2=0x%08X\n",
                 static_cast<int>(format), unmappedSupport1, unmappedSupport2);
  }
#endif

  return caps;
}
```

**Rationale:** Helps identify when new D3D12 format support flags are available but not exposed through IGL

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 format capability tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Format*Capabilit*"

# Run texture creation tests (validate capability accuracy)
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*Create*"

# Run compute shader tests (UAV format validation)
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Compute*UAV*"
```

**Expected Results:**
- Format capabilities now include additional flags (if ICapabilities extended)
- All texture creation tests pass with accurate capability reporting
- UAV atomic tests can query support before attempting operations

### New Test Cases Required:

#### Test 1: Extended Format Capability Query

**File:** `src/igl/tests/d3d12/Device.cpp`

```cpp
TEST_F(DeviceD3D12Test, ExtendedFormatCapabilities) {
  // Test RGBA8 format (should support most operations)
  auto caps = device_->getTextureFormatCapabilities(TextureFormat::RGBA_UNorm8);

  // Validate existing capabilities
  EXPECT_TRUE(caps & ICapabilities::TextureFormatCapabilityBits::Sampled);
  EXPECT_TRUE(caps & ICapabilities::TextureFormatCapabilityBits::SampledFiltered);
  EXPECT_TRUE(caps & ICapabilities::TextureFormatCapabilityBits::Attachment);

  // Validate new capabilities (if extended):
  EXPECT_TRUE(caps & ICapabilities::TextureFormatCapabilityBits::Blendable);
  EXPECT_TRUE(caps & ICapabilities::TextureFormatCapabilityBits::MSAASupport);

  // R32_UINT should support UAV atomic operations
  auto atomicCaps = device_->getTextureFormatCapabilities(TextureFormat::R_UInt32);
  EXPECT_TRUE(atomicCaps & ICapabilities::TextureFormatCapabilityBits::Storage);
  EXPECT_TRUE(atomicCaps & ICapabilities::TextureFormatCapabilityBits::AtomicOperations);
}
```

#### Test 2: D3D12-Specific Format Support Query

```cpp
TEST_F(DeviceD3D12Test, D3D12FormatSupportQuery) {
  auto d3d12Device = static_cast<igl::d3d12::Device*>(device_.get());

  igl::d3d12::Device::D3D12FormatSupport support;
  Result result = d3d12Device->getD3D12FormatSupport(TextureFormat::RGBA_UNorm8, support);

  ASSERT_TRUE(result.isOk()) << result.message;

  // Validate expected flags for RGBA8
  EXPECT_NE(support.support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE, 0u);
  EXPECT_NE(support.support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET, 0u);
  EXPECT_NE(support.support1 & D3D12_FORMAT_SUPPORT1_BLENDABLE, 0u);
  EXPECT_NE(support.support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET, 0u);

  // Print all flags for inspection
  std::cout << "RGBA8 D3D12 Support1: 0x" << std::hex << support.support1 << "\n";
  std::cout << "RGBA8 D3D12 Support2: 0x" << std::hex << support.support2 << "\n";
}
```

#### Test 3: Capability Accuracy Validation

```cpp
TEST_F(DeviceD3D12Test, CapabilityAccuracyValidation) {
  std::vector<TextureFormat> formats = {
    TextureFormat::RGBA_UNorm8,
    TextureFormat::R_UInt32,
    TextureFormat::RGBA_F16,
    TextureFormat::Z_UNorm24
  };

  for (auto format : formats) {
    auto caps = device_->getTextureFormatCapabilities(format);

    // If format reports Attachment capability, texture creation should succeed
    if (caps & ICapabilities::TextureFormatCapabilityBits::Attachment) {
      TextureDesc desc;
      desc.format = format;
      desc.width = 256;
      desc.height = 256;
      desc.usage = TextureDesc::TextureUsageBits::Attachment;

      Result result;
      auto texture = device_->createTexture(desc, &result);

      EXPECT_TRUE(result.isOk())
          << "Format " << static_cast<int>(format)
          << " reports Attachment capability but texture creation failed: " << result.message;
    }

    // If format reports Storage capability, UAV texture should succeed
    if (caps & ICapabilities::TextureFormatCapabilityBits::Storage) {
      TextureDesc desc;
      desc.format = format;
      desc.width = 256;
      desc.height = 256;
      desc.usage = TextureDesc::TextureUsageBits::Storage;

      Result result;
      auto texture = device_->createTexture(desc, &result);

      EXPECT_TRUE(result.isOk())
          << "Format " << static_cast<int>(format)
          << " reports Storage capability but UAV texture creation failed: " << result.message;
    }
  }
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should continue working (no behavior change)
./test_all_sessions.bat
```

**Expected Changes:**
- No behavior change for existing sessions
- Enhanced capability queries provide more information

**Test Modifications Allowed:**
- Add D3D12 backend-specific format capability tests
- DO NOT modify cross-platform test logic

---

## Definition of Done

### Completion Criteria:

- [ ] `getTextureFormatCapabilities()` maps additional D3D12 format support flags
- [ ] ICapabilities extended with new capability bits (if needed, requires cross-platform review)
- [ ] `getD3D12FormatSupport()` helper method implemented for advanced queries
- [ ] Debug logging added for unmapped D3D12 flags
- [ ] All unit tests pass (including new format capability tests)
- [ ] All render sessions pass
- [ ] Format capabilities accurately reflect D3D12 support
- [ ] Applications can query MSAA support, blend support, atomic support per format
- [ ] Documentation comments updated

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**

1. All D3D12 format capability tests pass
2. Extended capability query tests pass
3. D3D12-specific format support query works correctly
4. All render sessions continue working

**Post in chat:**
```
I-004 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- Extended format capabilities: IMPLEMENTED
- D3D12-specific format query: IMPLEMENTED
- New capability bits: [list which ones added]
- Format support accuracy: VALIDATED

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- None (independent fix, but consider ICapabilities API impact)

### Blocks:
- Advanced compute shader workflows (UAV atomics)
- Dynamic rendering quality settings (MSAA per-format detection)
- Cross-platform format capability parity

### Related:
- **I-003** (MSAA Sample Count Limits)
- **I-005** (Descriptor Heap Size Limits)

---

## Implementation Priority

**Priority:** P2 - MEDIUM (API Completeness)
**Estimated Effort:** 3-4 hours (including cross-platform ICapabilities review if extending)
**Risk:** MEDIUM (changes cross-platform API if extending TextureFormatCapabilityBits)
**Impact:** MEDIUM - Improves format capability detection, enables advanced rendering techniques

**Rationale:**
- Exposes more D3D12 format support information to applications
- Aligns with Vulkan's extensive format feature flags
- Requires careful review if extending cross-platform API
- Low-risk D3D12 implementation, medium-risk API changes

---

## References

### D3D12:
- [ID3D12Device::CheckFeatureSupport](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport)
- [D3D12_FEATURE_DATA_FORMAT_SUPPORT](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_format_support)
- [D3D12_FORMAT_SUPPORT1](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_format_support1)
- [D3D12_FORMAT_SUPPORT2](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_format_support2)

### Vulkan:
- [vkGetPhysicalDeviceFormatProperties](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceFormatProperties.html)
- [VkFormatFeatureFlagBits](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkFormatFeatureFlagBits.html)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
