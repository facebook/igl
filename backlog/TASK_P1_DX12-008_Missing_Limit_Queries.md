# TASK_P1_DX12-008: Implement Missing DeviceFeatureLimits Queries

**Priority:** P1 - High
**Estimated Effort:** 3-4 hours
**Status:** Open

---

## Problem Statement

The D3D12 backend does not implement several `IDeviceFeatures::getLimits()` queries, preventing applications from querying hardware capabilities like maximum texture dimensions, compute work group sizes, and other device limits. Applications cannot adapt to hardware constraints.

### Current Behavior
- Many limit queries return 0 or default values
- Applications cannot query actual hardware limits
- May cause failures when exceeding actual limits
- No runtime capability detection

### Expected Behavior
- All IGL limit queries return correct D3D12 hardware values
- Applications can query and adapt to device capabilities
- Matches Vulkan/OpenGL backend query completeness

---

## Evidence and Code Location

**Search Pattern:**
1. Find `Device::getLimits()` or `DeviceFeatures` implementation
2. Look for `IDeviceFeatures` interface implementation
3. Find unimplemented or stub limit queries

**Files:**
- `src/igl/d3d12/Device.cpp`
- `src/igl/d3d12/DeviceFeatures.cpp` (if exists)

**Missing Limits (Typical):**
- `maxTextureDimension1D/2D/3D`
- `maxComputeWorkGroupSize[3]`
- `maxComputeWorkGroupInvocations`
- `maxVertexInputAttributes`
- `maxColorAttachments`
- And others...

---

## Impact

**Severity:** High - Breaks capability detection
**Portability:** Applications can't adapt to hardware
**Reliability:** May exceed limits unknowingly

**Affected Use Cases:**
- Dynamic texture resolution selection
- Compute shader work group sizing
- Render target configuration
- Cross-platform adaptation

---

## Official References

### Microsoft Documentation

1. **D3D12_FEATURE_DATA_D3D12_OPTIONS** - Hardware capabilities
2. **D3D_FEATURE_LEVEL** - Feature level limits
3. **Resource Dimension Limits** - Per feature level

### IGL Contract
- `IDeviceFeatures::getLimits()` must return valid hardware limits
- Applications depend on these for runtime decisions

### Cross-Reference
- **Vulkan:** Queries `VkPhysicalDeviceLimits`
- **OpenGL:** Queries `GL_MAX_*` constants

---

## Implementation Guidance

### High-Level Approach

1. **Query D3D12 Capabilities**
   - Use `CheckFeatureSupport()` to query hardware
   - Query feature level limits

2. **Map to IGL Limits Structure**
   - Fill `DeviceLimits` structure with D3D12 values

3. **Implement getLimits() Methods**
   - Return queried values for each limit

### Detailed Steps

**Step 1: Query D3D12 Feature Data**

```cpp
// In Device initialization:
D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));

D3D12_FEATURE_DATA_ARCHITECTURE arch = {};
device_->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch));

// Store for later use
featureOptions_ = options;
```

**Step 2: Implement Limit Queries**

```cpp
DeviceLimits Device::getLimits() const {
    DeviceLimits limits;

    // Texture limits (based on feature level)
    switch (featureLevel_) {
        case D3D_FEATURE_LEVEL_12_0:
        case D3D_FEATURE_LEVEL_12_1:
        case D3D_FEATURE_LEVEL_12_2:
            limits.maxTextureDimension2D = 16384;
            limits.maxTextureDimension3D = 2048;
            break;
        case D3D_FEATURE_LEVEL_11_0:
        case D3D_FEATURE_LEVEL_11_1:
            limits.maxTextureDimension2D = 16384;
            limits.maxTextureDimension3D = 2048;
            break;
        // ... other feature levels
    }

    // Compute limits
    limits.maxComputeWorkGroupSizeX = D3D12_CS_THREAD_GROUP_MAX_X;  // 1024
    limits.maxComputeWorkGroupSizeY = D3D12_CS_THREAD_GROUP_MAX_Y;  // 1024
    limits.maxComputeWorkGroupSizeZ = D3D12_CS_THREAD_GROUP_MAX_Z;  // 64
    limits.maxComputeWorkGroupInvocations = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;  // 1024

    // Other limits
    limits.maxColorAttachments = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;  // 8
    limits.maxVertexInputAttributes = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;  // 32

    return limits;
}
```

**Step 3: Add Constants**

Use D3D12 constants from `d3d12.h`:
```cpp
#define D3D12_CS_THREAD_GROUP_MAX_X 1024
#define D3D12_CS_THREAD_GROUP_MAX_Y 1024
#define D3D12_CS_THREAD_GROUP_MAX_Z 64
#define D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT 8
// etc.
```

**Step 4: Feature Level Dependent Limits**

```cpp
uint32_t Device::getMaxTextureDimension2D() const {
    // Feature level dependent
    if (featureLevel_ >= D3D_FEATURE_LEVEL_11_0) {
        return 16384;
    } else if (featureLevel_ >= D3D_FEATURE_LEVEL_10_0) {
        return 8192;
    }
    return 2048;  // FL 9.x
}
```

---

## Testing Requirements

### Mandatory Tests
```bash
test_unittests.bat
test_all_sessions.bat
```

### Validation Steps

1. **Limit Query Test**
   ```cpp
   auto limits = device->getLimits();
   assert(limits.maxTextureDimension2D > 0);
   assert(limits.maxComputeWorkGroupSizeX == 1024);
   // etc.
   ```

2. **Cross-API Comparison**
   - Compare D3D12 limits with Vulkan/OpenGL
   - Verify reasonable values

3. **Logging**
   ```cpp
   IGL_LOG_INFO("D3D12 Device Limits:\n");
   IGL_LOG_INFO("  Max Texture 2D: %u\n", limits.maxTextureDimension2D);
   IGL_LOG_INFO("  Max Compute Work Group: %u, %u, %u\n",
                limits.maxComputeWorkGroupSizeX,
                limits.maxComputeWorkGroupSizeY,
                limits.maxComputeWorkGroupSizeZ);
   ```

---

## Success Criteria

- [ ] All IGL limit queries implemented
- [ ] Values match D3D12 hardware/feature level capabilities
- [ ] No limit queries return 0 (unless truly 0)
- [ ] Limits logged at device creation
- [ ] All tests pass
- [ ] User confirms limits are reasonable

---

## Dependencies

**None** - Standalone improvement

---

## Restrictions

1. **Test Immutability:** ❌ DO NOT modify test scripts
2. **Correctness:** Limits must match D3D12 spec

---

**Estimated Timeline:** 3-4 hours
**Risk Level:** Low (query implementation)
**Validation Effort:** 1-2 hours

---

*Task Created: 2025-11-08*
