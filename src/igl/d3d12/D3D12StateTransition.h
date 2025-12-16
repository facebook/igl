/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/D3D12Headers.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

/**
 * @brief D3D12 Resource State Transition Helper
 *
 * Provides conservative validation for D3D12 resource state transitions.
 * Enforces write-to-write transitions through COMMON intermediate state.
 *
 * Conservative Policy (voluntary, not D3D12 spec requirement):
 * - Any state with write bits -> any state with write bits: Use COMMON intermediate
 *   (e.g., RENDER_TARGET -> COMMON -> COPY_DEST)
 * - All other transitions: Direct transition allowed
 *
 * Note: D3D12 spec allows direct write-to-write transitions with a single barrier.
 * This helper uses COMMON intermediate as an extra-conservative policy.
 *
 * See: https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-gpu-access-to-resources
 */
class D3D12StateTransition {
 public:
  /**
   * @brief Check if a state contains any write bits
   *
   * Tests whether the state mask includes any write-capable bits.
   * Used to enforce conservative "write-to-write requires COMMON" policy.
   */
  static bool isWriteState(D3D12_RESOURCE_STATES state) {
    constexpr D3D12_RESOURCE_STATES kWriteMask =
        D3D12_RESOURCE_STATE_RENDER_TARGET |
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
        D3D12_RESOURCE_STATE_DEPTH_WRITE |
        D3D12_RESOURCE_STATE_COPY_DEST |
        D3D12_RESOURCE_STATE_RESOLVE_DEST |
        D3D12_RESOURCE_STATE_STREAM_OUT |
        D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE |
        D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE |
        D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE;
    return (state & kWriteMask) != 0;
  }

  /**
   * @brief Check if a direct state transition is allowed
   *
   * @return true if direct transition allowed, false if COMMON intermediate required
   */
  static bool isLegalDirectTransition(D3D12_RESOURCE_STATES from,
                                      D3D12_RESOURCE_STATES to) {
    if (from == to) {
      return true;
    }

    // COMMON can transition to/from anything directly
    if (from == D3D12_RESOURCE_STATE_COMMON || to == D3D12_RESOURCE_STATE_COMMON) {
      return true;
    }

    // Write-to-write requires COMMON intermediate
    if (isWriteState(from) && isWriteState(to)) {
      return false;
    }

    return true;
  }
};

} // namespace igl::d3d12
