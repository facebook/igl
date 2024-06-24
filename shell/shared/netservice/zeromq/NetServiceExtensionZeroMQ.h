/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <shell/shared/netservice/NetServiceExtension.h>

namespace igl::shell::netservice {

class NetServiceExtensionZeroMQ final : public NetServiceExtension {
 public:
  NetServiceExtensionZeroMQ() = default;
  ~NetServiceExtensionZeroMQ() override = default;

  bool initialize(igl::shell::Platform& platform) noexcept override;

  [[nodiscard]] std::unique_ptr<NetService> create(std::string_view domain,
                                                   std::string_view type,
                                                   std::string_view name,
                                                   int port) const noexcept override;

  void search(std::string_view domain, std::string_view type) const noexcept override;
  void stopSearch() noexcept;

 private:
  // NSNetServiceBrowser* netServiceBrowser_ = nil;
  // id<NSNetServiceBrowserDelegate> netServiceBrowserDelegate_ = nil;
};

IGL_API IGLShellExtension* IGLShellExtension_NewIglShellNetService();

} // namespace igl::shell::netservice
