/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include <IGLU/sentinel/CommandQueue.h>

#include <IGLU/sentinel/Assert.h>

namespace iglu::sentinel {

CommandQueue::CommandQueue(bool shouldAssert) : shouldAssert_(shouldAssert) {}

std::shared_ptr<igl::ICommandBuffer> CommandQueue::createCommandBuffer(
    const igl::CommandBufferDesc& /*desc*/,
    igl::Result* IGL_NULLABLE
    /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

igl::SubmitHandle CommandQueue::submit(const igl::ICommandBuffer& /*commandBuffer*/,
                                       bool /*endOfFrame*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return 0;
}

} // namespace iglu::sentinel
