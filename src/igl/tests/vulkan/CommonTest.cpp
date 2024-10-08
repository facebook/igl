/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/vulkan/Common.h>
#include <vulkan/vulkan_core.h>

#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

namespace igl::tests {

//
// CommonTest
//
// Unit tests for Common.cpp
//

class CommonTest : public ::testing::Test {};

// getResultFromVkResult ***********************************************************************
TEST(CommonTest, GetResultFromVkResultTest) {
  EXPECT_TRUE(igl::vulkan::getResultFromVkResult(VK_SUCCESS).isOk());

  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_LAYER_NOT_PRESENT).code,
            Result::Code::Unimplemented);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_EXTENSION_NOT_PRESENT).code,
            Result::Code::Unimplemented);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_FEATURE_NOT_PRESENT).code,
            Result::Code::Unimplemented);

  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_INCOMPATIBLE_DRIVER).code,
            Result::Code::Unsupported);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_FORMAT_NOT_SUPPORTED).code,
            Result::Code::Unsupported);

  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_OUT_OF_HOST_MEMORY).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_OUT_OF_DEVICE_MEMORY).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_OUT_OF_POOL_MEMORY).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_TOO_MANY_OBJECTS).code,
            Result::Code::ArgumentOutOfRange);
}

// setResultFrom ***********************************************************************
TEST(CommonTest, SetResultFromTest) {
  igl::Result result;
  igl::vulkan::setResultFrom(&result, VK_SUCCESS);
  EXPECT_TRUE(result.isOk());

  igl::vulkan::setResultFrom(&result, VK_ERROR_LAYER_NOT_PRESENT);
  EXPECT_EQ(result.code, Result::Code::Unimplemented);
  igl::vulkan::setResultFrom(&result, VK_ERROR_EXTENSION_NOT_PRESENT);
  EXPECT_EQ(result.code, Result::Code::Unimplemented);
  igl::vulkan::setResultFrom(&result, VK_ERROR_FEATURE_NOT_PRESENT);
  EXPECT_EQ(result.code, Result::Code::Unimplemented);

  igl::vulkan::setResultFrom(&result, VK_ERROR_INCOMPATIBLE_DRIVER);
  EXPECT_EQ(result.code, Result::Code::Unsupported);
  igl::vulkan::setResultFrom(&result, VK_ERROR_FORMAT_NOT_SUPPORTED);
  EXPECT_EQ(result.code, Result::Code::Unsupported);

  igl::vulkan::setResultFrom(&result, VK_ERROR_OUT_OF_HOST_MEMORY);
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  igl::vulkan::setResultFrom(&result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  igl::vulkan::setResultFrom(&result, VK_ERROR_OUT_OF_POOL_MEMORY);
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  igl::vulkan::setResultFrom(&result, VK_ERROR_TOO_MANY_OBJECTS);
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
}

// stencilOperationToVkStencilOp
// ***********************************************************************
TEST(CommonTest, StencilOperationToVkStencilOpTest) {
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::Keep),
            VK_STENCIL_OP_KEEP);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::Zero),
            VK_STENCIL_OP_ZERO);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::Replace),
            VK_STENCIL_OP_REPLACE);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::IncrementClamp),
            VK_STENCIL_OP_INCREMENT_AND_CLAMP);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::DecrementClamp),
            VK_STENCIL_OP_DECREMENT_AND_CLAMP);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::Invert),
            VK_STENCIL_OP_INVERT);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::IncrementWrap),
            VK_STENCIL_OP_INCREMENT_AND_WRAP);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::DecrementWrap),
            VK_STENCIL_OP_DECREMENT_AND_WRAP);
}

} // namespace igl::tests
