/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <memory>
#include <shell/shared/extension/Extension.h>
#include <shell/shared/netservice/NetService.h>

namespace igl::shell::netservice {

class NetServiceExtension : public igl::shell::Extension {
 public:
  static const char* Name() noexcept;

  // Return true to keep searching or false to stop searching.
  using DidFindService = std::function<
      bool(NetServiceExtension& sender, std::unique_ptr<NetService> service, bool moreComing)>;

  virtual std::unique_ptr<NetService> create(std::string_view domain,
                                             std::string_view type,
                                             std::string_view name,
                                             int port) const noexcept = 0;

  virtual void search(std::string_view domain, std::string_view type) const noexcept = 0;

  [[nodiscard]] const DidFindService& delegate() const noexcept {
    return delegate_;
  }

  void setDelegate(DidFindService delegate) noexcept {
    delegate_ = std::move(delegate);
  }

 private:
  DidFindService delegate_;
};

} // namespace igl::shell::netservice
