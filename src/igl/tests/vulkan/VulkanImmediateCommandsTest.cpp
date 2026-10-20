/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanImmediateCommands.h>

#include "../util/TestDevice.h"

#include <memory>
#include <set>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanImmediateCommandsTest : public ::testing::Test {
 public:
  VulkanImmediateCommandsTest() = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;

  igl::vulkan::VulkanContext& getVulkanContext() {
    auto& device = static_cast<igl::vulkan::Device&>(*iglDev_);
    return device.getVulkanContext();
  }

  igl::vulkan::VulkanImmediateCommands::SubmitHandle submitForPresentation(
      igl::vulkan::VulkanImmediateCommands& commands,
      const igl::vulkan::VulkanImmediateCommands::CommandBufferWrapper& wrapper,
      VkSemaphore semaphore) {
    return commands.submit(wrapper, semaphore);
  }
};

TEST_F(VulkanImmediateCommandsTest, AcquireCommandBuffer) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper = ctx.immediate_->acquire();
  EXPECT_TRUE(wrapper.cmdBuf != VK_NULL_HANDLE);
  EXPECT_TRUE(wrapper.isEncoding);
  ctx.immediate_->submit(wrapper);
}

TEST_F(VulkanImmediateCommandsTest, SubmitAndWait) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper = ctx.immediate_->acquire();
  ASSERT_TRUE(wrapper.cmdBuf != VK_NULL_HANDLE);

  auto handle = ctx.immediate_->submit(wrapper);
  EXPECT_FALSE(handle.empty());

  VkResult result = ctx.immediate_->wait(handle);
  EXPECT_EQ(result, VK_SUCCESS);
  EXPECT_TRUE(ctx.immediate_->isReady(handle));
}

TEST_F(VulkanImmediateCommandsTest, MultipleSubmitsGetUniqueHandles) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  std::set<uint32_t> submitIds;

  for (int i = 0; i < 3; ++i) {
    const auto& wrapper = ctx.immediate_->acquire();
    auto handle = ctx.immediate_->submit(wrapper);
    EXPECT_FALSE(handle.empty());
    submitIds.insert(handle.submitId);
  }

  EXPECT_EQ(submitIds.size(), 3u);
}

TEST_F(VulkanImmediateCommandsTest, CommandBufferRecycling) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  std::vector<igl::vulkan::VulkanImmediateCommands::SubmitHandle> handles;

  for (int i = 0; i < 4; ++i) {
    const auto& wrapper = ctx.immediate_->acquire();
    auto handle = ctx.immediate_->submit(wrapper);
    handles.push_back(handle);
  }

  for (auto& h : handles) {
    ctx.immediate_->wait(h);
  }

  const auto& wrapper = ctx.immediate_->acquire();
  EXPECT_TRUE(wrapper.cmdBuf != VK_NULL_HANDLE);
  EXPECT_TRUE(wrapper.isEncoding);
  ctx.immediate_->submit(wrapper);
}

TEST_F(VulkanImmediateCommandsTest, GetLastSubmitHandle) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper1 = ctx.immediate_->acquire();
  ctx.immediate_->submit(wrapper1);

  const auto& wrapper2 = ctx.immediate_->acquire();
  auto handle2 = ctx.immediate_->submit(wrapper2);

  auto lastHandle = ctx.immediate_->getLastSubmitHandle();
  EXPECT_EQ(lastHandle.submitId, handle2.submitId);
  EXPECT_EQ(lastHandle.bufferIndex, handle2.bufferIndex);
}

TEST_F(VulkanImmediateCommandsTest, IsReadyOnEmptyHandle) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  igl::vulkan::VulkanImmediateCommands::SubmitHandle emptyHandle;
  EXPECT_TRUE(emptyHandle.empty());
  EXPECT_TRUE(ctx.immediate_->isReady(emptyHandle));
}

TEST_F(VulkanImmediateCommandsTest, WaitAllCommandBuffers) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  std::vector<igl::vulkan::VulkanImmediateCommands::SubmitHandle> handles;

  for (int i = 0; i < 4; ++i) {
    const auto& wrapper = ctx.immediate_->acquire();
    auto handle = ctx.immediate_->submit(wrapper);
    handles.push_back(handle);
  }

  ctx.immediate_->waitAll();

  for (auto& h : handles) {
    EXPECT_TRUE(ctx.immediate_->isReady(h));
  }
}

TEST_F(VulkanImmediateCommandsTest, GetNextSubmitHandle) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  // Submit one buffer first so getNextSubmitHandle has a valid state
  const auto& wrapper = ctx.immediate_->acquire();
  ctx.immediate_->submit(wrapper);

  auto nextHandle = ctx.immediate_->getNextSubmitHandle();
  EXPECT_GT(nextHandle.submitId, 0u);
}

TEST_F(VulkanImmediateCommandsTest, SubmitHandleComparison) {
  igl::vulkan::VulkanImmediateCommands::SubmitHandle h1;
  h1.bufferIndex = 0;
  h1.submitId = 1;

  igl::vulkan::VulkanImmediateCommands::SubmitHandle h2;
  h2.bufferIndex = 0;
  h2.submitId = 1;

  EXPECT_EQ(h1, h2);

  h2.submitId = 2;
  EXPECT_NE(h1, h2);
}

TEST_F(VulkanImmediateCommandsTest, SubmitHandleRoundTrip) {
  igl::vulkan::VulkanImmediateCommands::SubmitHandle original;
  original.bufferIndex = 5;
  original.submitId = 42;

  uint64_t packed = original.handle();

  igl::vulkan::VulkanImmediateCommands::SubmitHandle restored(packed);
  EXPECT_EQ(restored.bufferIndex, original.bufferIndex);
  EXPECT_EQ(restored.submitId, original.submitId);
}

TEST_F(VulkanImmediateCommandsTest, GetVkFenceFromSubmitHandle) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper = ctx.immediate_->acquire();
  auto handle = ctx.immediate_->submit(wrapper);
  ASSERT_FALSE(handle.empty());

  VkFence fence = ctx.immediate_->getVkFenceFromSubmitHandle(handle);
  EXPECT_NE(fence, VK_NULL_HANDLE);

  ctx.immediate_->wait(handle);
}

TEST_F(VulkanImmediateCommandsTest, StoreFDAndCachedFDFromSubmitHandle) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper = ctx.immediate_->acquire();
  auto handle = ctx.immediate_->submit(wrapper);

  EXPECT_EQ(ctx.immediate_->cachedFDFromSubmitHandle(handle), -1);

  const int testFD = 42;
  ctx.immediate_->storeFDInSubmitHandle(handle, testFD);
  EXPECT_EQ(ctx.immediate_->cachedFDFromSubmitHandle(handle), testFD);

  ctx.immediate_->wait(handle);
}

TEST_F(VulkanImmediateCommandsTest, InjectedSemaphoreCapacityFailureIsReported) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);
  const auto& wrapper = ctx.immediate_->acquire();

  for (uint32_t i = 0; i < vulkan::VulkanImmediateCommands::kMaxInjectedSemaphores; ++i) {
    EXPECT_TRUE(ctx.immediate_->waitSemaphore(VK_NULL_HANDLE));
    EXPECT_TRUE(ctx.immediate_->signalSemaphore(VK_NULL_HANDLE, i));
  }

  EXPECT_FALSE(ctx.immediate_->waitSemaphore(VK_NULL_HANDLE));
  EXPECT_FALSE(ctx.immediate_->signalSemaphore(VK_NULL_HANDLE, 0));

  ctx.immediate_->discard(wrapper);
  EXPECT_FALSE(ctx.immediate_->hasPendingInjectedSemaphores());

  const auto& recoveredWrapper = ctx.immediate_->acquire();
  EXPECT_TRUE(ctx.immediate_->waitSemaphore(VK_NULL_HANDLE));
  ctx.immediate_->discard(recoveredWrapper);
}

TEST_F(VulkanImmediateCommandsTest, PresentationSignalPreservesInjectedSignalCapacity) {
  auto& ctx = getVulkanContext();
  if (!ctx.features().has_VK_KHR_timeline_semaphore ||
      !ctx.features().has_VK_KHR_synchronization2) {
    GTEST_SKIP() << "Timeline presentation is unavailable on this device";
  }
  std::vector<std::unique_ptr<vulkan::VulkanSemaphore>> signals;
  vulkan::VulkanSemaphore presentation(ctx.vf_, ctx.getVkDevice(), false, "Presentation test");
  const auto& wrapper = ctx.immediate_->acquire();
  for (uint32_t index = 0; index < vulkan::VulkanImmediateCommands::kMaxInjectedSemaphores;
       ++index) {
    signals.push_back(std::make_unique<vulkan::VulkanSemaphore>(
        ctx.vf_, ctx.getVkDevice(), uint64_t{0}, false, "Injected signal test"));
    EXPECT_TRUE(ctx.immediate_->signalSemaphore(signals.back()->getVkSemaphore(), index + 1));
  }
  const auto handle =
      submitForPresentation(*ctx.immediate_, wrapper, presentation.getVkSemaphore());
  ASSERT_EQ(ctx.immediate_->wait(handle), VK_SUCCESS);
  for (size_t index = 0; index < signals.size(); ++index) {
    uint64_t value = 0;
    ASSERT_EQ(ctx.vf_.vkGetSemaphoreCounterValueKHR(
                  ctx.getVkDevice(), signals[index]->getVkSemaphore(), &value),
              VK_SUCCESS);
    EXPECT_EQ(value, index + 1);
  }
  const auto& consumer = ctx.immediate_->acquire();
  EXPECT_TRUE(ctx.immediate_->waitSemaphore(presentation.getVkSemaphore()));
  const auto consumerHandle = ctx.immediate_->submit(consumer);
  EXPECT_EQ(ctx.immediate_->wait(consumerHandle), VK_SUCCESS);
}

TEST_F(VulkanImmediateCommandsTest, LegacyPresentationSignalPreservesInjectedSignalCapacity) {
  auto& ctx = getVulkanContext();
  vulkan::VulkanImmediateCommands commands(ctx.vf_,
                                           ctx.getVkDevice(),
                                           ctx.deviceQueues_.graphicsQueueFamilyIndex,
                                           false,
                                           false,
                                           "Legacy presentation test");
  std::vector<std::unique_ptr<vulkan::VulkanSemaphore>> signals;
  vulkan::VulkanSemaphore presentation(ctx.vf_, ctx.getVkDevice(), false, "Presentation test");
  const auto& wrapper = commands.acquire();
  for (uint32_t index = 0; index < vulkan::VulkanImmediateCommands::kMaxInjectedSemaphores;
       ++index) {
    signals.push_back(std::make_unique<vulkan::VulkanSemaphore>(
        ctx.vf_, ctx.getVkDevice(), false, "Injected legacy signal test"));
    EXPECT_TRUE(commands.signalSemaphore(signals.back()->getVkSemaphore(), 0));
  }
  const auto handle = submitForPresentation(commands, wrapper, presentation.getVkSemaphore());
  ASSERT_EQ(commands.wait(handle), VK_SUCCESS);
  const auto& consumer = commands.acquire();
  for (const auto& signal : signals) {
    EXPECT_TRUE(commands.waitSemaphore(signal->getVkSemaphore()));
  }
  const auto consumerHandle = commands.submit(consumer);
  ASSERT_EQ(commands.wait(consumerHandle), VK_SUCCESS);
  const auto& presentationConsumer = commands.acquire();
  EXPECT_TRUE(commands.waitSemaphore(presentation.getVkSemaphore()));
  const auto presentationHandle = commands.submit(presentationConsumer);
  EXPECT_EQ(commands.wait(presentationHandle), VK_SUCCESS);
}

TEST_F(VulkanImmediateCommandsTest, InjectedWaitSemaphoreReachesSubmit) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  vulkan::VulkanSemaphore signal(ctx.vf_, ctx.getVkDevice(), false, "Injected wait test");
  const auto& first = ctx.immediate_->acquire();
  ASSERT_TRUE(ctx.immediate_->signalSemaphore(signal.getVkSemaphore(), 0));
  ctx.immediate_->submit(first);

  // Inject the real semaphore so the array-building path is exercised end-to-end through
  // vkQueueSubmit(2KHR) with a non-null handle, not just the counting/discard path.
  const auto& second = ctx.immediate_->acquire();
  EXPECT_TRUE(ctx.immediate_->waitSemaphore(signal.getVkSemaphore()));
  const auto handle = ctx.immediate_->submit(second);
  EXPECT_FALSE(handle.empty());
  EXPECT_EQ(ctx.immediate_->wait(handle), VK_SUCCESS);
}

TEST_F(VulkanImmediateCommandsTest, CachedFDDefaultIsNegativeOne) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper = ctx.immediate_->acquire();
  auto handle = ctx.immediate_->submit(wrapper);
  ASSERT_FALSE(handle.empty());

  EXPECT_EQ(ctx.immediate_->cachedFDFromSubmitHandle(handle), -1);

  ctx.immediate_->wait(handle);
}

TEST_F(VulkanImmediateCommandsTest, StoreFDAndCachedFDRoundTrip) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper = ctx.immediate_->acquire();
  auto handle = ctx.immediate_->submit(wrapper);
  ASSERT_FALSE(handle.empty());

  constexpr int kTestFD = 42;
  ctx.immediate_->storeFDInSubmitHandle(handle, kTestFD);
  EXPECT_EQ(ctx.immediate_->cachedFDFromSubmitHandle(handle), kTestFD);

  ctx.immediate_->wait(handle);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
