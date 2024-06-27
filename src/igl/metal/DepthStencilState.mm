/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <Foundation/Foundation.h>

#include <igl/metal/DepthStencilState.h>

namespace igl::metal {

DepthStencilState::DepthStencilState(id<MTLDepthStencilState> value) : value_(value) {}

MTLCompareFunction DepthStencilState::convertCompareFunction(CompareFunction value) {
  switch (value) {
  case CompareFunction::Never:
    return MTLCompareFunctionNever;
  case CompareFunction::Less:
    return MTLCompareFunctionLess;
  case CompareFunction::Equal:
    return MTLCompareFunctionEqual;
  case CompareFunction::LessEqual:
    return MTLCompareFunctionLessEqual;
  case CompareFunction::Greater:
    return MTLCompareFunctionGreater;
  case CompareFunction::NotEqual:
    return MTLCompareFunctionNotEqual;
  case CompareFunction::GreaterEqual:
    return MTLCompareFunctionGreaterEqual;
  case CompareFunction::AlwaysPass:
    return MTLCompareFunctionAlways;
  }
}

MTLStencilOperation DepthStencilState::convertStencilOperation(StencilOperation value) {
  switch (value) {
  case StencilOperation::Keep:
    return MTLStencilOperationKeep;
  case StencilOperation::Zero:
    return MTLStencilOperationZero;
  case StencilOperation::Replace:
    return MTLStencilOperationReplace;
  case StencilOperation::IncrementClamp:
    return MTLStencilOperationIncrementClamp;
  case StencilOperation::DecrementClamp:
    return MTLStencilOperationDecrementClamp;
  case StencilOperation::Invert:
    return MTLStencilOperationInvert;
  case StencilOperation::IncrementWrap:
    return MTLStencilOperationIncrementWrap;
  case StencilOperation::DecrementWrap:
    return MTLStencilOperationDecrementWrap;
  }
}

MTLStencilDescriptor* DepthStencilState::convertStencilDescriptor(const StencilStateDesc& desc) {
  MTLStencilDescriptor* metalDesc = [MTLStencilDescriptor new];
  metalDesc.stencilCompareFunction = convertCompareFunction(desc.stencilCompareFunction);
  metalDesc.stencilFailureOperation = convertStencilOperation(desc.stencilFailureOperation);
  metalDesc.depthStencilPassOperation = convertStencilOperation(desc.depthStencilPassOperation);
  metalDesc.depthFailureOperation = convertStencilOperation(desc.depthFailureOperation);
  metalDesc.readMask = desc.readMask;
  metalDesc.writeMask = desc.writeMask;
  return metalDesc;
}
} // namespace igl::metal
