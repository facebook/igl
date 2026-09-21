/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalDepthStencilCreationTest
//
// This test covers creation of depth stencil states on Metal.
// Tests default, depth write, all compare functions, and front/back stencil ops.
//
class MetalDepthStencilCreationTest : public ::testing::Test {
 public:
  MetalDepthStencilCreationTest() = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// DefaultDepthStencil
//
// Test creating a depth stencil state with default descriptor values.
//
TEST_F(MetalDepthStencilCreationTest, DefaultDepthStencil) {
  Result res;
  const DepthStencilStateDesc desc{.debugName = "defaultDepthStencil"};

  auto depthStencilState = device_->createDepthStencilState(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(depthStencilState, nullptr);
}

//
// DepthWriteEnabled
//
// Test creating a depth stencil state with depth write enabled.
//
TEST_F(MetalDepthStencilCreationTest, DepthWriteEnabled) {
  Result res;
  const DepthStencilStateDesc desc{.debugName = "depthWriteEnabled",
                                   .compareFunction = CompareFunction::Less,
                                   .isDepthWriteEnabled = true};

  auto depthStencilState = device_->createDepthStencilState(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(depthStencilState, nullptr);
}

//
// AllCompareFunctions
//
// Test creating depth stencil states with all compare functions.
//
TEST_F(MetalDepthStencilCreationTest, AllCompareFunctions) {
  const CompareFunction functions[] = {
      CompareFunction::Never,
      CompareFunction::Less,
      CompareFunction::Equal,
      CompareFunction::LessEqual,
      CompareFunction::Greater,
      CompareFunction::NotEqual,
      CompareFunction::GreaterEqual,
      CompareFunction::AlwaysPass,
  };

  for (auto func : functions) {
    Result res;
    const DepthStencilStateDesc desc{.compareFunction = func, .isDepthWriteEnabled = true};

    auto depthStencilState = device_->createDepthStencilState(desc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;
    ASSERT_NE(depthStencilState, nullptr)
        << "Failed to create depth stencil state with CompareFunction " << static_cast<int>(func);
  }
}

//
// FrontBackStencil
//
// Test creating a depth stencil state with front and back face stencil ops.
//
TEST_F(MetalDepthStencilCreationTest, FrontBackStencil) {
  Result res;
  const DepthStencilStateDesc desc{
      .debugName = "frontBackStencil",
      .compareFunction = CompareFunction::LessEqual,
      .isDepthWriteEnabled = true,
      .backFaceStencil = {.stencilFailureOperation = StencilOperation::Zero,
                          .depthFailureOperation = StencilOperation::DecrementClamp,
                          .depthStencilPassOperation = StencilOperation::Invert,
                          .stencilCompareFunction = CompareFunction::NotEqual,
                          .readMask = 0x0F,
                          .writeMask = 0xF0},
      .frontFaceStencil = {.stencilFailureOperation = StencilOperation::Keep,
                           .depthFailureOperation = StencilOperation::IncrementClamp,
                           .depthStencilPassOperation = StencilOperation::Replace,
                           .stencilCompareFunction = CompareFunction::AlwaysPass,
                           .readMask = 0xFF,
                           .writeMask = 0xFF}};

  auto depthStencilState = device_->createDepthStencilState(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(depthStencilState, nullptr);
}

} // namespace igl::tests
