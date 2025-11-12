/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <Foundation/Foundation.h>
#include <shell/shared/netservice/NetServiceExtension.h>
#include <igl/Common.h>

namespace igl::shell::netservice {

class NetServiceExtensionApple final : public NetServiceExtension {
 public:
  NetServiceExtensionApple() = default;
  ~NetServiceExtensionApple() final;
  NetServiceExtensionApple(const NetServiceExtensionApple&) = delete;
  NetServiceExtensionApple& operator=(const NetServiceExtensionApple&) = delete;
  NetServiceExtensionApple(NetServiceExtensionApple&&) = delete;
  NetServiceExtensionApple& operator=(NetServiceExtensionApple&&) = delete;

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

// NOLINTNEXTLINE(readability-identifier-naming)
IGL_API IGLShellExtension* IGLShellExtension_NewIglShellNetService();

} // namespace igl::shell::netservice
