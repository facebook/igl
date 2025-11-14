# H-015: Hard-Coded D3D12 Limits vs Dynamic Queries (High Priority)

**Priority:** P1 (High)
**Category:** Cross-API Limits Validation
**Status:** Open
**Estimated Effort:** 2 days

---

## Problem Statement

The D3D12 backend uses hard-coded limits instead of querying device capabilities. This causes:

1. **Portability issues** - Limits don't match actual hardware capabilities
2. **Cross-API inconsistency** - D3D12 advertises different limits than Vulkan for same GPU
3. **Feature underutilization** - Cannot use full hardware capabilities (e.g., 2048 texture size when GPU supports 16384)
4. **Deployment fragility** - App assumes capabilities that may not exist on all D3D12 hardware

---

## Technical Details

### Current Problem

**In `Common.h:36` and `Device.cpp`:**
```cpp
// ❌ Hard-coded limits (D3D12 backend)
static constexpr UINT kMaxTextureSize = 2048;  // ❌ GPU supports 16384
static constexpr UINT kMaxDescriptors = 1024;  // ❌ Arbitrary
static constexpr UINT kMaxSamplers = 2048;     // ❌ Arbitrary
static constexpr UINT kMaxColorAttachments = 8;  // ❌ Correct by luck

// Vulkan backend (for comparison):
DeviceFeatures Device::getFeatureLimits() {
    // ✅ Queries VkPhysicalDeviceLimits
    limits.maxTextureSize = physicalDeviceLimits.maxImageDimension2D;  // 16384
    limits.maxColorAttachments = physicalDeviceLimits.maxColorAttachments;  // 8
    return limits;
}
```

**Impact:**
- D3D12 reports `maxTextureSize = 2048`
- Vulkan reports `maxTextureSize = 16384` (same GPU!)
- Apps see inconsistent capabilities across backends

---

## Correct Pattern

```cpp
// ✅ Query D3D12 resource limits
DeviceFeatures Device::queryDeviceLimits() {
    DeviceFeatures limits = {};

    // Maximum texture size (16384 for FL11+)
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    if (SUCCEEDED(device_->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)))) {

        limits.maxTextureSize = options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_1
            ? 16384 : 2048;
    }

    // Maximum color attachments (8 for all FL11+)
    limits.maxColorAttachments = 8;  // D3D12 spec

    // Maximum samplers
    limits.maxSamplerAnisotropy = 16;  // D3D12 spec

    // Maximum push constants (root signature limit)
    limits.maxPushConstantsSize = 256;  // 64 DWORDs

    // Compute limits
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 options3 = {};
    if (SUCCEEDED(device_->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS3, &options3, sizeof(options3)))) {

        // ... query compute capabilities ...
    }

    return limits;
}
```

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/Common.h:36`**
   - Remove hard-coded limits
   - Replace with runtime queries

2. **`src/igl/d3d12/Device.cpp`**
   - Implement `queryDeviceLimits()` method
   - Use `CheckFeatureSupport` for capabilities

3. **`src/igl/d3d12/D3D12Context.cpp`**
   - Query limits during initialization
   - Store in context

---

## Official References

- [Hardware Feature Levels](https://learn.microsoft.com/windows/win32/direct3d12/hardware-feature-levels#feature-level-support)
  - Table of guaranteed limits per feature level
- [CheckFeatureSupport](https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport)

---

## Implementation Guidance

```cpp
// Device.cpp
DeviceFeatures Device::getFeatureLimits() {
    DeviceFeatures limits = {};

    // Query D3D12 options
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));

    // ✅ Texture limits (from D3D12 spec + feature level)
    switch (ctx_->getFeatureLevel()) {
        case D3D_FEATURE_LEVEL_12_0:
        case D3D_FEATURE_LEVEL_12_1:
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            limits.maxTextureSize = 16384;  // FL11+ guaranteed
            limits.maxTexture3DSize = 2048;
            limits.maxCubeMapSize = 16384;
            break;
        default:
            limits.maxTextureSize = 2048;
            break;
    }

    // ✅ Render target limits (D3D12 spec)
    limits.maxColorAttachments = 8;  // All FL11+

    // ✅ Buffer limits
    limits.maxUniformBufferSize = 65536;  // 64 KB
    limits.maxStorageBufferSize = UINT32_MAX;  // No limit

    // ✅ Push constants (root signature limit)
    limits.maxPushConstantsSize = 256;  // 64 DWORDs max

    // ✅ Descriptor limits (heap sizes)
    limits.maxDescriptorsPerHeap = 1000000;  // CBV/SRV/UAV heap
    limits.maxSamplersPerHeap = 2048;        // Sampler heap

    return limits;
}
```

---

## Testing Requirements

```cpp
TEST(DeviceTest, FeatureLimitsQuery) {
    auto device = createDevice();

    auto limits = device->getFeatureLimits();

    // FL11+ should support 16384 texture size
    EXPECT_GE(limits.maxTextureSize, 16384);

    // D3D12 spec guarantees 8 color attachments
    EXPECT_EQ(limits.maxColorAttachments, 8);

    // Compare with Vulkan backend (same GPU)
    auto vulkanDevice = createVulkanDevice();
    auto vulkanLimits = vulkanDevice->getFeatureLimits();

    EXPECT_EQ(limits.maxTextureSize, vulkanLimits.maxTextureSize);
}
```

---

## Definition of Done

- [ ] Hard-coded limits replaced with queries
- [ ] `CheckFeatureSupport` used for capabilities
- [ ] Cross-API limits match (D3D12 vs Vulkan)
- [ ] Unit tests pass
- [ ] User confirmation received

---

## Related Issues

- **DX12-COD-009**: Hard-coded push constant limits (same issue)
- **H-009**: Missing shader model detection (related capability query)
