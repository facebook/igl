/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12DeviceCapabilities.h>

#include <igl/d3d12/D3D12Context.h>

namespace igl::d3d12 {

void D3D12DeviceCapabilities::initialize(D3D12Context& ctx) {
  validateDeviceLimits(ctx);
}

void D3D12DeviceCapabilities::validateDeviceLimits(D3D12Context& ctx) {
  auto* device = ctx.getDevice();
  if (!device) {
    IGL_LOG_ERROR("D3D12DeviceCapabilities::validateDeviceLimits: D3D12 device is null\n");
    return;
  }

  IGL_D3D12_LOG_VERBOSE("=== D3D12 Device Capabilities and Limits Validation ===\n");

  // Query D3D12_FEATURE_D3D12_OPTIONS for resource binding tier and other capabilities
  HRESULT hr =
      device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &deviceOptions_, sizeof(deviceOptions_));

  if (SUCCEEDED(hr)) {
    // Log resource binding tier
    const char* tierName = "Unknown";
    switch (deviceOptions_.ResourceBindingTier) {
    case D3D12_RESOURCE_BINDING_TIER_1:
      tierName = "Tier 1 (bounded descriptors required)";
      break;
    case D3D12_RESOURCE_BINDING_TIER_2:
      tierName = "Tier 2 (unbounded arrays except samplers)";
      break;
    case D3D12_RESOURCE_BINDING_TIER_3:
      tierName = "Tier 3 (fully unbounded)";
      break;
    }
    IGL_D3D12_LOG_VERBOSE("  Resource Binding Tier: %s\n", tierName);

    // Log other relevant capabilities
    IGL_D3D12_LOG_VERBOSE("  Standard Swizzle 64KB Supported: %s\n",
                 deviceOptions_.StandardSwizzle64KBSupported ? "Yes" : "No");
    IGL_D3D12_LOG_VERBOSE("  Cross-Node Sharing Tier: %d\n", deviceOptions_.CrossNodeSharingTier);
    IGL_D3D12_LOG_VERBOSE("  Conservative Rasterization Tier: %d\n",
                 deviceOptions_.ConservativeRasterizationTier);
  } else {
    IGL_LOG_ERROR(
        "  Failed to query D3D12_FEATURE_D3D12_OPTIONS (HRESULT: 0x%08X)\n", hr);
  }

  // Query D3D12_FEATURE_D3D12_OPTIONS1 for root signature version
  hr = device->CheckFeatureSupport(
      D3D12_FEATURE_D3D12_OPTIONS1, &deviceOptions1_, sizeof(deviceOptions1_));

  if (SUCCEEDED(hr)) {
    IGL_D3D12_LOG_VERBOSE("  Wave Intrinsics Supported: %s\n",
                 deviceOptions1_.WaveOps ? "Yes" : "No");
    IGL_D3D12_LOG_VERBOSE("  Wave Lane Count Min: %u\n", deviceOptions1_.WaveLaneCountMin);
    IGL_D3D12_LOG_VERBOSE("  Wave Lane Count Max: %u\n", deviceOptions1_.WaveLaneCountMax);
    IGL_D3D12_LOG_VERBOSE("  Total Lane Count: %u\n", deviceOptions1_.TotalLaneCount);
  } else {
    IGL_D3D12_LOG_VERBOSE(
        "  D3D12_FEATURE_D3D12_OPTIONS1 query failed (not critical)\n");
  }

  // The rest of the original validation logic lives in Device::getFeatureLimits()
  // and related capability queries, so no additional checks are needed here.
}

} // namespace igl::d3d12

