/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/DepthStencilState.h>

#include "../util/Common.h"

namespace igl::tests {

// DepthStencilStateMTLTest
//
// This test covers metal::DepthStencilState.
class DepthStencilStateMTLTest : public ::testing::Test {
 public:
  DepthStencilStateMTLTest() = default;
  ~DepthStencilStateMTLTest() override = default;
  void SetUp() override {
    setDebugBreakEnabled(false);
  }
  void TearDown() override {}
};

//
// Compare Function to MTL Test
//
// Check expected outputs for metal::DepthStencilState::convertStencilOperation
//
TEST_F(DepthStencilStateMTLTest, StencilOpConversionToMTL) {
  struct StencilOpConversion {
    StencilOperation igl = StencilOperation::Keep;
    MTLStencilOperation mtl = MTLStencilOperationKeep;
  };
  const MTLStencilOperation mtlStencilOperation =
      metal::DepthStencilState::convertStencilOperation(StencilOperation::Keep);
  ASSERT_EQ(mtlStencilOperation, MTLStencilOperationKeep);

  const std::vector<StencilOpConversion> conversions{
      StencilOpConversion{igl::StencilOperation::Keep, MTLStencilOperationKeep},
      StencilOpConversion{igl::StencilOperation::Zero, MTLStencilOperationZero},
      StencilOpConversion{igl::StencilOperation::Replace, MTLStencilOperationReplace},
      StencilOpConversion{igl::StencilOperation::IncrementClamp, MTLStencilOperationIncrementClamp},
      StencilOpConversion{igl::StencilOperation::DecrementClamp, MTLStencilOperationDecrementClamp},
      StencilOpConversion{igl::StencilOperation::Invert, MTLStencilOperationInvert},
      StencilOpConversion{igl::StencilOperation::IncrementWrap, MTLStencilOperationIncrementWrap},
      StencilOpConversion{igl::StencilOperation::DecrementWrap, MTLStencilOperationDecrementWrap},
  };

  for (auto data : conversions) {
    enum MTLStencilOperation mtl = metal::DepthStencilState::convertStencilOperation(data.igl);
    ASSERT_EQ(mtl, data.mtl);
  }
}

//
// Compare Function to MTL Test
//
// Check expected outputs for metal::DepthStencilState::convertCompareFunction
//
TEST_F(DepthStencilStateMTLTest, CompareFunctionToMTL) {
  struct CompareFuncConversion {
    CompareFunction igl = igl::CompareFunction::Never;
    MTLCompareFunction mtl = MTLCompareFunctionNever;
  };

  const std::vector<CompareFuncConversion> conversions{
      CompareFuncConversion{igl::CompareFunction::Never, MTLCompareFunctionNever},
      CompareFuncConversion{igl::CompareFunction::Less, MTLCompareFunctionLess},
      CompareFuncConversion{igl::CompareFunction::Equal, MTLCompareFunctionEqual},
      CompareFuncConversion{igl::CompareFunction::LessEqual, MTLCompareFunctionLessEqual},
      CompareFuncConversion{igl::CompareFunction::Greater, MTLCompareFunctionGreater},
      CompareFuncConversion{igl::CompareFunction::NotEqual, MTLCompareFunctionNotEqual},
      CompareFuncConversion{igl::CompareFunction::GreaterEqual, MTLCompareFunctionGreaterEqual},
      CompareFuncConversion{igl::CompareFunction::AlwaysPass, MTLCompareFunctionAlways},
  };

  for (auto data : conversions) {
    const MTLCompareFunction mtl = metal::DepthStencilState::convertCompareFunction(data.igl);
    ASSERT_EQ(mtl, data.mtl);
  }
}

} // namespace igl::tests
