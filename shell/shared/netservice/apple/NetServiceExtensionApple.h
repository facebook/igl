/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/netservice/NetServiceExtension.h>
#include <igl/Common.h>

#import <Foundation/Foundation.h>

namespace igl::shell::netservice {

class NetServiceExtensionApple final : public NetServiceExtension {
 public:
  NetServiceExtensionApple() = default;
  ~NetServiceExtensionApple() final;

  bool initialize(igl::shell::Platform& platform) noexcept final;

  [[nodiscard]] std::unique_ptr<NetService> create(std::string_view domain,
                                                   std::string_view type,
                                                   std::string_view name,
                                                   int port) const noexcept final;

  void search(std::string_view domain, std::string_view type) const noexcept final;
  void stopSearch() const noexcept;

 private:
  NSNetServiceBrowser* netServiceBrowser_ = nil;
  id<NSNetServiceBrowserDelegate> netServiceBrowserDelegate_ = nil;
};

IGL_API IGLShellExtension* IGLShellExtension_NewIglShellNetService();

} // namespace igl::shell::netservice
