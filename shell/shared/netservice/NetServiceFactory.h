/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/netservice/NetService.h>
#include <string>

namespace igl::shell::netservice {

class NetServiceFactory {
 public:
  virtual ~NetServiceFactory() = default;
  [[nodiscard]] virtual std::unique_ptr<NetService> create(std::string domain,
                                                           std::string type,
                                                           std::string name,
                                                           int port) const noexcept = 0;

  [[nodiscard]] virtual std::unique_ptr<NetService> search(std::string domain,
                                                           std::string type) const noexcept = 0;
};

} // namespace igl::shell::netservice
