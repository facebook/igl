/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <string>

namespace igl::shell {

enum class IntentType : uint8_t {
  ActionView,
};

struct IntentEvent {
  IntentType type;
  std::string data;
};

class IIntentListener {
 public:
  virtual bool process(const IntentEvent& event) = 0;
  virtual ~IIntentListener() = default;
};

} // namespace igl::shell
