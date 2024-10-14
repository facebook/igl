/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

/**
 * @brief Comparison tests used in depth testing and stencil testing.
 *
 * Testing is performed by using the CompareFunction to compare a new fragment's value and the an
 * existing value at the fragment's location.
 *
 * Never        : Never passes.
 * Less         : Passes if the fragment's value is strictly less than the existing value.
 * Equal        : Passes if the fragment's value is exactly equal to the existing value.
 * LessEqual    : Passes if the fragment's value is less than or equal to the existing value.
 * NotEqual     : Passes if the fragment's value is not equal to the existing value.
 * GreaterEqual : Passes if the fragment's value is greater than or equal to the existing value.
 * AlwaysPass   : Always passes.
 */
enum class CompareFunction : uint8_t {
  Never = 0,
  Less,
  Equal,
  LessEqual,
  Greater,
  NotEqual,
  GreaterEqual,
  AlwaysPass
};

/**
 * @brief A stencil operation to perform on a fragment in the stencil buffer.
 *
 * Separate stencil operations can be configured for when the stencil test fails, when it passes
 * and the depth test fails and when both the depth and stencil tests pass. The  operation
 * determines what happens to the fragment's value in the stencil buffer when it is executed.
 *
 * Keep           : The existing value is kept.
 * Zero           : The value is set to zero.
 * Replace        : The value is replaced with the stencil reference value.
 * IncrementClamp : The value is incremented by 1 unless it is already the maximum possible value.
 * DecrementClamp : The value is decremented by 1 unless it is already the minimum possible value.
 * Invert         : The value is logically inverted.
 * IncrementWrap  : The max possible value is set to zero. All other values are incremented by 1.
 * DecrementWrap  : Zero is set to the max possible value. All other values are decremented by 1.
 */
enum class StencilOperation : uint8_t {
  Keep = 0,
  Zero,
  Replace,
  IncrementClamp,
  DecrementClamp,
  Invert,
  IncrementWrap,
  DecrementWrap
};

/**
 * @brief Describes the stencil operations for either the front or back face.
 *
 * A DepthStencilStateDesc has descriptors for both the front and back face. These control
 * how the stencil buffers are updated during rendering.
 */
struct StencilStateDesc {
  /**
   * @brief The operation to perform when the stencil test fails.
   */
  StencilOperation stencilFailureOperation = StencilOperation::Keep;
  /**
   * @brief The operation to perform when the stencil test succeeds but the depth test fails.
   */
  StencilOperation depthFailureOperation = StencilOperation::Keep;
  /**
   * @brief The operation to perform when both the depth and stencil tests functions pass.
   */
  StencilOperation depthStencilPassOperation = StencilOperation::Keep;
  /**
   * @brief The comparison operation to use for stencil testing.
   */
  CompareFunction stencilCompareFunction = CompareFunction::AlwaysPass;
  /**
   * @brief A bit mask that determines which stencil value bits are compared in the stencil test.
   */
  uint32_t readMask = (uint32_t)~0;
  /**
   * @brief A bit mask that determines which stencil value bits are written to by stencil
   * operations.
   */
  uint32_t writeMask = (uint32_t)~0;

  /**
   * @brief Compares two StencilStateDesc objects for equality.
   *
   * Returns true if all descriptor properties are identical.
   */
  bool operator==(const StencilStateDesc& other) const;
  /**
   * @brief Compares two StencilStateDesc objects for inequality.
   *
   * Returns true if any descriptor properties differ.
   */
  bool operator!=(const StencilStateDesc& other) const;
};

/**
 * @brief Describes the depth and stencil configuration for a render pass.
 *
 * This describes what depth comparison function to use, and whether to retain or discard
 * any new depth data. It also describes the stencil testing and stencil operations to use for both
 * the front and back face of polygons.
 */
struct DepthStencilStateDesc {
  /**
   * @brief The comparison operation to use for depth testing.
   */
  CompareFunction compareFunction = CompareFunction::AlwaysPass;
  /**
   * @brief Determines whether new depth values are written.
   *
   * If true, depth values for any fragments that are not discarded are written.
   * If false, the fragment shader will execute for fragments that pass the depth test but
   * their depth values will not be written.
   */
  bool isDepthWriteEnabled = false;

  /**
   * @brief The stencil operation to use for the back face of polygons.
   */
  StencilStateDesc backFaceStencil;
  /**
   * @brief The stencil operation to use for the front face of polygons.
   */
  StencilStateDesc frontFaceStencil;

  /**
   * @brief Compares two DepthStencilStateDesc objects for equality.
   *
   * Returns true if all descriptor properties are identical.
   */
  bool operator==(const DepthStencilStateDesc& other) const;
  /**
   * @brief Compares two DepthStencilStateDesc objects for inequality.
   *
   * Returns true if any descriptor properties differ.
   */
  bool operator!=(const DepthStencilStateDesc& other) const;
};

/**
 * @brief A depth and stencil configuration.
 *
 * This interface is the backend agnostic representation for a depth and stencil configuration.
 * To create an instance, populate a DepthStencilStateDesc and call IDevice.createDepthStencilState.
 * To use an instance in a render pass, call IRenderCommandEncoder.bindDepthStencilState.
 */
class IDepthStencilState {
 public:
 protected:
  IDepthStencilState() = default;
  virtual ~IDepthStencilState() = default;
};

} // namespace igl

/// Hashing function declarations
///
namespace std {

/**
 * @brief Provides a hash function for igl::DepthStencilStateDesc.
 */
template<>
struct hash<igl::DepthStencilStateDesc> {
  /**
   * @brief Computes a hash value for igl::DepthStencilStateDesc.
   *
   * The hash value is based on all properties in the igl::DepthStencilStateDesc;
   */
  size_t operator()(const igl::DepthStencilStateDesc& /*key*/) const;
};

/**
 * @brief Provides a hash function for igl::StencilStateDesc.
 */
template<>
struct hash<igl::StencilStateDesc> {
  /**
   * @brief Computes a hash value for igl::StencilStateDesc.
   *
   * The hash value is based on all properties in the igl::StencilStateDesc;
   */
  size_t operator()(const igl::StencilStateDesc& /*key*/) const;
};

} // namespace std
