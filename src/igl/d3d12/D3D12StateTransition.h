/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/D3D12Headers.h>
#include <igl/Common.h>

namespace igl::d3d12 {

/**
 * @brief D3D12 Resource State Transition Validation Helper
 *
 * This helper provides conservative validation for D3D12 resource state transitions.
 * While D3D12 technically allows single barriers between any two valid states on the
 * same queue (subject to normal queue/state constraints), this helper enforces a
 * conservative policy: write-to-write transitions must go through an intermediate
 * state (this helper uses COMMON) to ensure resources pass through a neutral state
 * between exclusive write modes.
 *
 * D3D12 Resource State Constraint (from Microsoft documentation):
 * - "At most, only one read/write bit can be set. If a write bit is set,
 *   then no read-only bit may be set."
 * - This constraint applies to the *current* state bitmask, not barrier sequences.
 *
 * Conservative Policy Summary:
 * - Write-to-write: Use COMMON intermediate (e.g., RENDER_TARGET → COMMON → COPY_DEST)
 * - All other transitions: Direct transition allowed
 *
 * See: https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-gpu-access-to-resources
 * See: https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states
 */
class D3D12StateTransition {
 public:
  /**
   * @brief Check if a state is a write state for validation purposes
   *
   * This helper identifies known pure write states that are mutually exclusive per
   * D3D12 spec. These are states where the resource can be written but should not
   * be simultaneously readable in other shader stages.
   *
   * @param state The resource state to check
   * @return true if state is a known pure write state, false otherwise
   *
   * Known pure write states per D3D12_RESOURCE_STATES documentation:
   * - RENDER_TARGET: Render target write
   * - UNORDERED_ACCESS: UAV read/write (treated as write for validation)
   * - DEPTH_WRITE: Depth-stencil write
   * - COPY_DEST: Copy destination
   * - RESOLVE_DEST: Resolve destination
   * - STREAM_OUT: Stream output write
   * - VIDEO_DECODE_WRITE, VIDEO_PROCESS_WRITE, VIDEO_ENCODE_WRITE: Video writes
   *
   * Note: This is not an exhaustive list of all possible D3D12 write states; it
   * covers only the states used by this backend. If future D3D12 SDKs add new
   * write-capable states, this function must be updated to include them.
   */
  static bool isWriteState(D3D12_RESOURCE_STATES state) {
    // Compare against known pure write states (not bitmask checks).
    // D3D12 forbids read+write combinations in the state bitmask; if such
    // an invalid combined state appears (indicating a bug upstream), it will
    // not be classified as a write state by this helper.
    return (state == D3D12_RESOURCE_STATE_RENDER_TARGET) ||
           (state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) ||
           (state == D3D12_RESOURCE_STATE_DEPTH_WRITE) ||
           (state == D3D12_RESOURCE_STATE_COPY_DEST) ||
           (state == D3D12_RESOURCE_STATE_RESOLVE_DEST) ||
           (state == D3D12_RESOURCE_STATE_STREAM_OUT) ||
           (state == D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE) ||
           (state == D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE) ||
           (state == D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE);
  }

  /**
   * @brief Check if a direct state transition is allowed per this helper's policy
   *
   * @param from The current resource state
   * @param to The desired target state
   * @return true if direct transition is allowed, false if intermediate state required
   *
   * Conservative validation policy (not strict D3D12 API requirement):
   * - Same state: No transition needed (allowed)
   * - To/from COMMON: Always allowed (COMMON is universal intermediate)
   * - Write-to-write: Disallowed by this helper (use COMMON intermediate for safety)
   * - Write-to-read: Allowed
   * - Read-to-write: Allowed
   * - Read-to-read: Allowed (read states can be combined)
   *
   * Examples of transitions disallowed by this helper's policy (non-exhaustive,
   * all write-to-write pairs require COMMON intermediate):
   * - RENDER_TARGET → COPY_DEST
   * - COPY_DEST → RENDER_TARGET
   * - UNORDERED_ACCESS → DEPTH_WRITE
   */
  static bool isLegalDirectTransition(D3D12_RESOURCE_STATES from,
                                      D3D12_RESOURCE_STATES to) {
    // No transition needed
    if (from == to) {
      return true;
    }

    // COMMON state can transition to/from anything directly
    // COMMON is the universal intermediate state in D3D12
    if (from == D3D12_RESOURCE_STATE_COMMON || to == D3D12_RESOURCE_STATE_COMMON) {
      return true;
    }

    // Conservative policy: Treat write-to-write transitions as requiring intermediate
    // While D3D12 allows direct barriers, we force COMMON intermediate for safety
    if (isWriteState(from) && isWriteState(to)) {
      return false;
    }

    // All other transitions are allowed:
    // - Read-to-read: Allowed (read states can combine)
    // - Read-to-write: Allowed
    // - Write-to-read: Allowed
    return true;
  }

  /**
   * @brief Get the intermediate state needed for a transition per this helper's policy
   *
   * @param from The current resource state
   * @param to The desired target state
   * @return The intermediate state to use, or 'from' if no intermediate needed
   *
   * For transitions disallowed by this helper (write-to-write pairs), returns
   * COMMON as the intermediate state.
   */
  static D3D12_RESOURCE_STATES getIntermediateState(D3D12_RESOURCE_STATES from,
                                                     D3D12_RESOURCE_STATES to) {
    // If direct transition is allowed by policy, no intermediate needed
    if (isLegalDirectTransition(from, to)) {
      return from;  // Return same state to indicate no intermediate
    }

    // For disallowed transitions (write-to-write), use COMMON as intermediate
    // COMMON is the universal intermediate state in D3D12
    return D3D12_RESOURCE_STATE_COMMON;
  }

  /**
   * @brief Get human-readable name for a D3D12 resource state
   *
   * @param state The resource state
   * @return String representation of the state
   */
  static const char* getStateName(D3D12_RESOURCE_STATES state) {
    switch (state) {
      case D3D12_RESOURCE_STATE_COMMON:
        return "COMMON";
      case D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER:
        return "VERTEX_AND_CONSTANT_BUFFER";
      case D3D12_RESOURCE_STATE_INDEX_BUFFER:
        return "INDEX_BUFFER";
      case D3D12_RESOURCE_STATE_RENDER_TARGET:
        return "RENDER_TARGET";
      case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
        return "UNORDERED_ACCESS";
      case D3D12_RESOURCE_STATE_DEPTH_WRITE:
        return "DEPTH_WRITE";
      case D3D12_RESOURCE_STATE_DEPTH_READ:
        return "DEPTH_READ";
      case D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
        return "NON_PIXEL_SHADER_RESOURCE";
      case D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
        return "PIXEL_SHADER_RESOURCE";
      case D3D12_RESOURCE_STATE_STREAM_OUT:
        return "STREAM_OUT";
      case D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT:
        return "INDIRECT_ARGUMENT";
      case D3D12_RESOURCE_STATE_COPY_DEST:
        return "COPY_DEST";
      case D3D12_RESOURCE_STATE_COPY_SOURCE:
        return "COPY_SOURCE";
      case D3D12_RESOURCE_STATE_RESOLVE_DEST:
        return "RESOLVE_DEST";
      case D3D12_RESOURCE_STATE_RESOLVE_SOURCE:
        return "RESOLVE_SOURCE";
      case D3D12_RESOURCE_STATE_GENERIC_READ:
        return "GENERIC_READ";
      // D3D12_RESOURCE_STATE_PRESENT has same value as COMMON (0)
      // D3D12_RESOURCE_STATE_PREDICATION has same value as GENERIC_READ (512)
      case D3D12_RESOURCE_STATE_VIDEO_DECODE_READ:
        return "VIDEO_DECODE_READ";
      case D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE:
        return "VIDEO_DECODE_WRITE";
      case D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ:
        return "VIDEO_PROCESS_READ";
      case D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE:
        return "VIDEO_PROCESS_WRITE";
      case D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ:
        return "VIDEO_ENCODE_READ";
      case D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE:
        return "VIDEO_ENCODE_WRITE";
      default:
        // Handle combined states (GENERIC_READ, etc.)
        if ((state & D3D12_RESOURCE_STATE_GENERIC_READ) == D3D12_RESOURCE_STATE_GENERIC_READ) {
          return "GENERIC_READ (combined)";
        }
        return "UNKNOWN/COMBINED";
    }
  }

  /**
   * @brief Log a state transition with validation information
   *
   * @param resourceType Type of resource (e.g., "Buffer", "Texture")
   * @param from Current state
   * @param to Desired state
   * @param needsIntermediate Whether intermediate state is required
   *
   * Note: These logs are at INFO level and intended for diagnostic/debug builds.
   * If logging overhead becomes an issue in production, consider using a lower
   * severity level or conditional compilation.
   */
  static void logTransition(const char* resourceType,
                            D3D12_RESOURCE_STATES from,
                            D3D12_RESOURCE_STATES to,
                            bool needsIntermediate) {
    if (from == to) {
      return;  // No transition, no log needed
    }

    if (needsIntermediate) {
      IGL_D3D12_LOG_VERBOSE("%s state transition: %s → COMMON → %s (using intermediate state)\n",
                   resourceType,
                   getStateName(from),
                   getStateName(to));
    } else {
      IGL_D3D12_LOG_VERBOSE("%s state transition: %s → %s (direct)\n",
                   resourceType,
                   getStateName(from),
                   getStateName(to));
    }
  }

  /**
   * @brief Validate a state transition and log if intermediate state is needed
   *
   * @param resourceType Type of resource (e.g., "Buffer", "Texture")
   * @param from Current state
   * @param to Desired state
   * @return Always returns true (all transitions are treatable with intermediate states)
   *
   * This is a diagnostic helper that logs when write-to-write transitions are detected
   * per this helper's conservative policy. The return value is always true because any
   * transition can be made valid by inserting an intermediate COMMON state.
   *
   * Note: If you need to check whether a transition requires an intermediate state,
   * use isLegalDirectTransition() instead. This function's bool return is maintained
   * for backward compatibility but has no meaningful value (always true).
   *
   * Note: Logging is at INFO level for diagnostic builds. Consider lower severity
   * if log volume becomes excessive.
   *
   * TODO (low priority): Consider refactoring to void return type after auditing
   * all call sites, since the bool return has no meaningful value. This could be
   * addressed in a future cleanup task (e.g., T21 or similar refactoring work).
   */
  static bool validateTransition(const char* resourceType,
                                  D3D12_RESOURCE_STATES from,
                                  D3D12_RESOURCE_STATES to) {
    if (from == to) {
      return true;  // No transition needed
    }

    bool allowedDirect = isLegalDirectTransition(from, to);

    if (!allowedDirect) {
      // Write-to-write transition detected per conservative policy
      IGL_D3D12_LOG_VERBOSE("%s: Direct transition %s → %s disallowed by policy (write-to-write), will use intermediate COMMON state\n",
                   resourceType,
                   getStateName(from),
                   getStateName(to));
    }

    return true;  // All transitions are valid with proper intermediate states
  }
};

} // namespace igl::d3d12
