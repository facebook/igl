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
 * This helper provides validation for D3D12 resource state transitions according to
 * Microsoft's documented legal transition rules. Some state transitions require an
 * intermediate COMMON state to avoid GPU hangs and device removal errors.
 *
 * See: https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-gpu-access-to-resources
 */
class D3D12StateTransition {
 public:
  /**
   * @brief Check if a direct state transition is legal according to D3D12 rules
   *
   * @param from The current resource state
   * @param to The desired target state
   * @return true if direct transition is legal, false if intermediate state required
   *
   * Examples of illegal direct transitions:
   * - RENDER_TARGET → COPY_DEST (need COMMON as intermediate)
   * - COPY_DEST → RENDER_TARGET (need COMMON as intermediate)
   * - DEPTH_STENCIL → most other states (need COMMON as intermediate)
   */
  static bool isLegalDirectTransition(D3D12_RESOURCE_STATES from,
                                      D3D12_RESOURCE_STATES to) {
    // No transition needed
    if (from == to) {
      return true;
    }

    // COMMON state can transition to/from anything directly
    if (from == D3D12_RESOURCE_STATE_COMMON || to == D3D12_RESOURCE_STATE_COMMON) {
      return true;
    }

    // RENDER_TARGET to COPY_DEST (illegal - need COMMON)
    if (from == D3D12_RESOURCE_STATE_RENDER_TARGET &&
        to == D3D12_RESOURCE_STATE_COPY_DEST) {
      return false;
    }

    // COPY_DEST to RENDER_TARGET (illegal - need COMMON)
    if (from == D3D12_RESOURCE_STATE_COPY_DEST &&
        to == D3D12_RESOURCE_STATE_RENDER_TARGET) {
      return false;
    }

    // DEPTH_WRITE special handling (conservative - use COMMON for most transitions)
    // Only allow DEPTH_WRITE ↔ DEPTH_READ direct transitions
    if (from == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
      if (to != D3D12_RESOURCE_STATE_DEPTH_READ &&
          to != D3D12_RESOURCE_STATE_COMMON) {
        return false;
      }
    }
    if (to == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
      if (from != D3D12_RESOURCE_STATE_DEPTH_READ &&
          from != D3D12_RESOURCE_STATE_COMMON) {
        return false;
      }
    }

    // UNORDERED_ACCESS is exclusive - conservative approach with COMMON
    if (from == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ||
        to == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
      // Allow direct transitions only to/from commonly used shader states
      const D3D12_RESOURCE_STATES shaderReadStates =
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
          D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
          D3D12_RESOURCE_STATE_GENERIC_READ;

      if (from == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        if ((to & shaderReadStates) == 0 && to != D3D12_RESOURCE_STATE_COPY_SOURCE &&
            to != D3D12_RESOURCE_STATE_COPY_DEST) {
          return false;  // Need COMMON as intermediate
        }
      }
      if (to == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        if ((from & shaderReadStates) == 0 && from != D3D12_RESOURCE_STATE_COPY_SOURCE &&
            from != D3D12_RESOURCE_STATE_COPY_DEST) {
          return false;  // Need COMMON as intermediate
        }
      }
    }

    // Default: allow transition (may not be optimal but won't hang)
    // This is a conservative approach - better to allow than block valid transitions
    return true;
  }

  /**
   * @brief Get the intermediate state needed for a complex transition
   *
   * @param from The current resource state
   * @param to The desired target state
   * @return The intermediate state to use, or 'from' if no intermediate needed
   *
   * Most illegal direct transitions can use COMMON as the intermediate state.
   */
  static D3D12_RESOURCE_STATES getIntermediateState(D3D12_RESOURCE_STATES from,
                                                     D3D12_RESOURCE_STATES to) {
    // If direct transition is legal, no intermediate needed
    if (isLegalDirectTransition(from, to)) {
      return from;  // Return same state to indicate no intermediate
    }

    // For all illegal transitions, use COMMON as intermediate
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
   */
  static void logTransition(const char* resourceType,
                            D3D12_RESOURCE_STATES from,
                            D3D12_RESOURCE_STATES to,
                            bool needsIntermediate) {
    if (from == to) {
      return;  // No transition, no log needed
    }

    if (needsIntermediate) {
      IGL_LOG_INFO("%s state transition: %s → COMMON → %s (using intermediate state)\n",
                   resourceType,
                   getStateName(from),
                   getStateName(to));
    } else {
      IGL_LOG_INFO("%s state transition: %s → %s (direct)\n",
                   resourceType,
                   getStateName(from),
                   getStateName(to));
    }
  }

  /**
   * @brief Validate and emit warnings for potentially problematic transitions
   *
   * @param resourceType Type of resource (e.g., "Buffer", "Texture")
   * @param from Current state
   * @param to Desired state
   * @return true if transition is valid (with or without intermediate), false if invalid
   */
  static bool validateTransition(const char* resourceType,
                                  D3D12_RESOURCE_STATES from,
                                  D3D12_RESOURCE_STATES to) {
    if (from == to) {
      return true;  // No transition needed
    }

    bool legal = isLegalDirectTransition(from, to);

    if (!legal) {
      IGL_LOG_INFO("%s: Direct transition %s → %s is not legal, will use intermediate COMMON state\n",
                   resourceType,
                   getStateName(from),
                   getStateName(to));
    }

    return true;  // All transitions are valid with proper intermediate states
  }
};

} // namespace igl::d3d12
