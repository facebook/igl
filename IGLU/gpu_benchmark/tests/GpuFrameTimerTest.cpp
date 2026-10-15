/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/gpu_benchmark/GpuFrameTimer.h>

#include "Fakes.h"

#include <chrono>

namespace iglu::gpu_benchmark::tests {

TEST(GpuFrameTimerTest, GpuTimerCreatesNewTimerPerBuffer) {
  FakeDevice device;
  FakeQueue queue;
  events().clear();
  const GpuFrameTimer timer(device);
  EXPECT_EQ(timer.getKind(), TimerKind::GpuTimer);
  igl::Result r1;
  igl::Result r2;
  const TimedCommandBuffer first = timer.createCommandBuffer(queue, "first", &r1);
  const TimedCommandBuffer second = timer.createCommandBuffer(queue, "second", &r2);
  EXPECT_TRUE(r1.isOk());
  EXPECT_TRUE(r2.isOk());
  ASSERT_TRUE(first.timer);
  ASSERT_TRUE(second.timer);
  EXPECT_NE(first.timer, second.timer);
  EXPECT_EQ(events(),
            (std::vector<std::string>{"timer", "commandBuffer", "timer", "commandBuffer"}));
  ASSERT_EQ(queue.descs.size(), 2u);
  EXPECT_EQ(queue.descs[0].debugName, "first");
  EXPECT_EQ(queue.descs[0].timer, first.timer);
  EXPECT_EQ(queue.descs[1].debugName, "second");
  EXPECT_EQ(queue.descs[1].timer, second.timer);
  EXPECT_TRUE(first.commandBuffer);
  EXPECT_EQ(first.commandBuffer->desc.timer, first.timer);
  const TimedCommandBuffer third = timer.createCommandBuffer(queue, "third", nullptr);
  EXPECT_TRUE(third.commandBuffer);
}

TEST(GpuFrameTimerTest, CpuWallClock) {
  FakeDevice device;
  device.hasTimers = false;
  FakeQueue queue;
  events().clear();
  const GpuFrameTimer timer(device);
  EXPECT_EQ(timer.getKind(), TimerKind::CpuWallClock);
  const TimedCommandBuffer buffer = timer.createCommandBuffer(queue, "x", nullptr);
  EXPECT_TRUE(buffer.commandBuffer);
  EXPECT_FALSE(buffer.timer);
  EXPECT_EQ(events(), (std::vector<std::string>{"commandBuffer"}));
  EXPECT_FALSE(queue.descs[0].timer);
  EXPECT_FALSE(timer.getElapsedNanos(buffer).has_value());
}

TEST(GpuFrameTimerTest, TimerCreationFailureStillCreatesBuffer) {
  FakeDevice device;
  device.timerCreationFails = true;
  FakeQueue queue;
  const GpuFrameTimer timer(device);
  igl::Result result;
  const TimedCommandBuffer buffer = timer.createCommandBuffer(queue, "x", &result);
  EXPECT_TRUE(result.isOk());
  EXPECT_TRUE(buffer.commandBuffer);
  EXPECT_FALSE(buffer.timer);
  EXPECT_FALSE(timer.getElapsedNanos(buffer).has_value());
}

TEST(GpuFrameTimerTest, GetElapsedNanosUsesADeadline) {
  FakeDevice device;
  const GpuFrameTimer timer(device);

  auto immediate = std::make_shared<FakeTimer>();
  immediate->nanos = 1234;
  EXPECT_EQ(timer.getElapsedNanos({.commandBuffer = nullptr, .timer = immediate}),
            std::optional<uint64_t>(1234));
  EXPECT_EQ(immediate->pollCount, 1u);

  auto never = std::make_shared<FakeTimer>();
  never->nanos = 5;
  never->pollsUntilAvailable = 100000000;
  const auto start = std::chrono::steady_clock::now();
  EXPECT_FALSE(
      timer
          .getElapsedNanos({.commandBuffer = nullptr, .timer = never}, std::chrono::milliseconds(5))
          .has_value());
  const auto waited = std::chrono::steady_clock::now() - start;
  EXPECT_GE(waited, std::chrono::milliseconds(5));
  EXPECT_LE(never->pollCount, 60u);

  auto neverDefault = std::make_shared<FakeTimer>();
  neverDefault->nanos = 5;
  neverDefault->pollsUntilAvailable = 100000000;
  const auto defaultStart = std::chrono::steady_clock::now();
  EXPECT_FALSE(
      timer.getElapsedNanos({.commandBuffer = nullptr, .timer = neverDefault}).has_value());
  const auto defaultWaited = std::chrono::steady_clock::now() - defaultStart;
  EXPECT_GE(defaultWaited, std::chrono::milliseconds(20));

  auto late = std::make_shared<FakeTimer>();
  late->nanos = 99;
  late->pollsUntilAvailable = 3;
  EXPECT_EQ(
      timer.getElapsedNanos({.commandBuffer = nullptr, .timer = late}, std::chrono::seconds(1)),
      std::optional<uint64_t>(99));
  EXPECT_EQ(late->pollCount, 4u);

  auto once = std::make_shared<FakeTimer>();
  once->nanos = 99;
  once->pollsUntilAvailable = 5;
  EXPECT_FALSE(
      timer.getElapsedNanos({.commandBuffer = nullptr, .timer = once}, std::chrono::microseconds(0))
          .has_value());
  EXPECT_EQ(once->pollCount, 1u);
  auto negative = std::make_shared<FakeTimer>();
  negative->nanos = 99;
  negative->pollsUntilAvailable = 5;
  EXPECT_FALSE(timer
                   .getElapsedNanos({.commandBuffer = nullptr, .timer = negative},
                                    std::chrono::microseconds(-50))
                   .has_value());
  EXPECT_EQ(negative->pollCount, 1u);
  auto readyOnce = std::make_shared<FakeTimer>();
  readyOnce->nanos = 99;
  EXPECT_EQ(timer.getElapsedNanos({.commandBuffer = nullptr, .timer = readyOnce},
                                  std::chrono::microseconds(0)),
            std::optional<uint64_t>(99));

  auto zero = std::make_shared<FakeTimer>();
  zero->nanos = 0;
  EXPECT_FALSE(
      timer.getElapsedNanos({.commandBuffer = nullptr, .timer = zero}, std::chrono::seconds(1))
          .has_value());
  EXPECT_EQ(zero->pollCount, 1u);
  EXPECT_FALSE(timer.getElapsedNanos({}).has_value());
}

TEST(GpuFrameTimerTest, TryGetElapsedNanosPollsOnce) {
  FakeDevice device;
  const GpuFrameTimer timer(device);
  auto ready = std::make_shared<FakeTimer>();
  ready->nanos = 7;
  EXPECT_EQ(timer.tryGetElapsedNanos({.commandBuffer = nullptr, .timer = ready}),
            std::optional<uint64_t>(7));
  EXPECT_EQ(ready->pollCount, 1u);
  auto notReady = std::make_shared<FakeTimer>();
  notReady->nanos = 7;
  notReady->pollsUntilAvailable = 1;
  EXPECT_FALSE(timer.tryGetElapsedNanos({.commandBuffer = nullptr, .timer = notReady}).has_value());
  EXPECT_EQ(notReady->pollCount, 1u);
  EXPECT_EQ(timer.tryGetElapsedNanos({.commandBuffer = nullptr, .timer = notReady}),
            std::optional<uint64_t>(7));
  EXPECT_EQ(notReady->pollCount, 2u);
  auto zero = std::make_shared<FakeTimer>();
  EXPECT_FALSE(timer.tryGetElapsedNanos({.commandBuffer = nullptr, .timer = zero}).has_value());
  EXPECT_EQ(zero->pollCount, 1u);
  EXPECT_FALSE(timer.tryGetElapsedNanos({}).has_value());
}

} // namespace iglu::gpu_benchmark::tests
